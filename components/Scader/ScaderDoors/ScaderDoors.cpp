/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderDoors
//
// Rob Dobson 2013-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <ScaderDoors.h>
#include <ConfigPinMap.h>
#include <RaftUtils.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>
#include <NetworkSystem.h>
#include <CommsChannelMsg.h>
#include <JSONParams.h>
#include <ESPUtils.h>
#include <time.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderDoors";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderDoors::ScaderDoors(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig, NULL, true),
          _scaderCommon(*this, pModuleName)
{
    // Clear strike pins to -1
    std::fill(_strikeControlPins, _strikeControlPins + DEFAULT_MAX_ELEMS, -1);
    std::fill(_openSensePins, _openSensePins + DEFAULT_MAX_ELEMS, -1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Get settings
    _maxElems = configGetLong("maxElems", DEFAULT_MAX_ELEMS);
    if (_maxElems > DEFAULT_MAX_ELEMS)
        _maxElems = DEFAULT_MAX_ELEMS;

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Get active/sense levels of pins
    _strikePinUnlockLevel[0] = configGetBool("doors[0]/strikeOn", false);
    _strikePinUnlockLevel[1] = configGetBool("doors[1]/strikeOn", false);
    _unlockForSecs[0] = configGetLong("doors[0]/unlockForSecs", 1);
    _unlockForSecs[1] = configGetLong("doors[1]/unlockForSecs", 1);
    _openSensePinLevel[0] = configGetBool("doors[0]/openSenseLevel", false);
    _openSensePinLevel[1] = configGetBool("doors[1]/openSenseLevel", false);

    // Configure GPIOs
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("doors[0]/strikePin", ConfigPinMap::GPIO_OUTPUT, &_strikeControlPins[0], !_strikePinUnlockLevel[0]),
        ConfigPinMap::PinDef("doors[1]/strikePin", ConfigPinMap::GPIO_OUTPUT, &_strikeControlPins[1], !_strikePinUnlockLevel[1]),
        ConfigPinMap::PinDef("doors[0]/openSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_openSensePins[0], 0),
        ConfigPinMap::PinDef("doors[1]/openSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_openSensePins[1], 0),
        ConfigPinMap::PinDef("doors[0]/closedSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_closedSensePins[0], 0),
        ConfigPinMap::PinDef("doors[1]/closedSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_closedSensePins[1], 0),
        ConfigPinMap::PinDef("bellSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_bellPressedPin, 0),
    };
    ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Bell pressed pin sense
    _bellPressedPinLevel = configGetBool("bellSenseLevel", false);

    // Master door index
    _masterDoorIndex = configGetLong("masterDoorIdx", 0);

    // Debug
    LOG_I(MODULE_PREFIX, "setup scaderUIName %s", _scaderCommon.getFriendlyName().c_str());

    // Element names
    std::vector<String> elemInfos;
    if (configGetArrayElems("elems", elemInfos))
    {
        // Names array
        uint32_t numNames = elemInfos.size() > _maxElems ? _maxElems : elemInfos.size();
        _elemNames.resize(numNames);

        // Set names
        for (int i = 0; i < numNames; i++)
        {
            JSONParams elemInfo = elemInfos[i];
            _elemNames[i] = elemInfo.getString("name", ("Door " + String(i+1)).c_str());
            LOG_I(MODULE_PREFIX, "Door %d name %s", i+1, _elemNames[i].c_str());
        }
    }

    // Setup door strikes
    _doorStrikes.clear();
    _doorStrikes.resize(_maxElems);
    for (int i = 0; i < _maxElems; i++)
    {
        if (_strikeControlPins[i] >= 0)
        {
            _doorStrikes[i].setup(_strikeControlPins[i], _strikePinUnlockLevel[i], _openSensePins[i], 
                    _closedSensePins[i], _openSensePinLevel[i], _unlockForSecs[i]);
        }
    }

    // Debug
    for (int i = 0; i < _maxElems; i++)
    {
        LOG_I(MODULE_PREFIX, "setup enabled door %d strikePin %d strikeOpen %d openSensePin %d openSenseLevel %d closedSensePin %d unlockForSecs %d", 
                    i, _strikeControlPins[i], _strikePinUnlockLevel[i], _openSensePins[i], _openSensePinLevel[i], 
                    _closedSensePins[i], _unlockForSecs[i]);
    }
    LOG_I(MODULE_PREFIX, "setup enabled name %s bellSensePin %d masterDoorIdx %d", 
                    _scaderCommon.getFriendlyName().c_str(),
                    _bellPressedPin, _masterDoorIndex);

    // Debug show states
    debugShowCurrentState();

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->sendMsgGenCB("Publish", _scaderCommon.getModuleName().c_str(), 
            [this](const char* messageName, CommsChannelMsg& msg) {
                String statusStr = getStatusJSON();
                msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
                return true;
            },
            [this](const char* messageName, std::vector<uint8_t>& stateHash) {
                return getStatusHash(stateHash);
            }
        );
    }

    // HW Now initialised
    _isInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::service()
{
    // Check if initialised
    if (!_isInitialised)
        return;

    // Check if mutable data changed
    if (_mutableDataDirty)
    {
        // Check if min time has passed
        if (Raft::isTimeout(millis(), _mutableDataChangeLastMs, MUTABLE_DATA_SAVE_MIN_MS))
        {
            // Save mutable data
            saveMutableData();
            _mutableDataDirty = false;
        }
    }

    // Service strikes
    for (int i = 0; i < _doorStrikes.size(); i++)
    {
        _doorStrikes[i].service();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control door
    endpointManager.addEndpoint("door", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderDoors::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "control doors, door/<door>/<state> where door is 1-based and state is 0 or 1 for off or on");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader door");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Get list of doors to control
    bool rslt = false;
    String elemNumStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    int elemNums[DEFAULT_MAX_ELEMS];    
    int numElemsInList = 0;
    if (elemNumStr.length() > 0)
    {
        // Get the numbers
        numElemsInList = parseIntList(elemNumStr, elemNums, _elemNames.size());
    }
    else
    {
        // No numbers so do all
        for (int i = 0; i < _elemNames.size(); i++)
            elemNums[i] = i + 1;
        numElemsInList = _elemNames.size();
    }

    // Get newState
    String elemCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool newState = (elemCmdStr == "1") || (elemCmdStr.equalsIgnoreCase("on") || elemCmdStr.equalsIgnoreCase("unlock"));

    // Execute command
    uint32_t numElemsSet = 0;
    for (int i = 0; i < numElemsInList; i++)
    {
        int elemIdx = elemNums[i] - 1;
        if (elemIdx >= 0 && elemIdx < _doorStrikes.size())
        {
            // Set state
            if (newState)
            {
                _doorStrikes[elemIdx].unlockWithTimeout("API", _unlockForSecs[elemIdx]);
            }
            else
            {
                _doorStrikes[elemIdx].lock();
            }
            _mutableDataChangeLastMs = millis();
            numElemsSet++;
        }
    }

    // Check something changed
    if (numElemsSet > 0)
    {
        // Set
        _mutableDataDirty = true;
        rslt = true;

        // Debug
        LOG_I(MODULE_PREFIX, "apiControl req %s numSet %d newState %d doors %s, %s", 
                    reqStr.c_str(), numElemsSet, newState,
                    numElemsInList > 0 && (elemNums[0]-1 >= 0) && (elemNums[0]-1) < _elemNames.size() ? _elemNames[elemNums[0]-1] : "-",
                    numElemsInList > 1 && (elemNums[1]-1 >= 0) && (elemNums[1]-1) < _elemNames.size() ? _elemNames[elemNums[1]-1] : "-");
    }
    else
    {
        // Debug
        LOG_I(MODULE_PREFIX, "apiControl no valid door in req %s", reqStr.c_str());
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderDoors::getStatusJSON()
{
    // Get status
    String elemStatus;
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i >= _doorStrikes.size())
            break;
        if (i > 0)
            elemStatus += ",";
        elemStatus += R"({"name":")" + _elemNames[i] + "\"," + _doorStrikes[i].getStatusJson(false) + "}";
    }

    // Bell status str
    String bellStatus = _bellPressedPin >= 0 ? (digitalRead(_bellPressedPin) == _bellPressedPinLevel ? "Y" : "N") : "K";

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"bell\":\"" + bellStatus + "\",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (const DoorStrike& doorStrike : _doorStrikes)
        doorStrike.getStatusHash(stateHash);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write the mutable config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::saveMutableData()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// debug show relay states
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::debugShowCurrentState()
{
    String elemsStr;
    for (const DoorStrike& doorStrike : _doorStrikes)
    {
        if (elemsStr.length() > 0)
            elemsStr += ",";
        elemsStr += String(doorStrike.getDebugStr());
    }
    LOG_I(MODULE_PREFIX, "debugShowCurrentState %s", elemsStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse list of integers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ScaderDoors::parseIntList(const String &str, int *pIntList, int maxInts)
{
    int numInts = 0;
    int idx = 0;
    while (idx < str.length())
    {
        int nextCommaIdx = str.indexOf(',', idx);
        if (nextCommaIdx < 0)
            nextCommaIdx = str.length();
        String intStr = str.substring(idx, nextCommaIdx);
        int intVal = intStr.toInt();
        if (numInts < maxInts)
            pIntList[numInts++] = intVal;
        idx = nextCommaIdx + 1;
    }
    return numInts;
}