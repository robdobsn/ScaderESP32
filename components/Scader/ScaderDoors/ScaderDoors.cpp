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
#include <JSONParams.h>
#include <ESPUtils.h>
#include <time.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderDoors";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderDoors::ScaderDoors(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
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
    // Get settings
    _isEnabled = configGetLong("enable", false) != 0;
    _maxElems = configGetLong("maxElems", DEFAULT_MAX_ELEMS);
    if (_maxElems > DEFAULT_MAX_ELEMS)
        _maxElems = DEFAULT_MAX_ELEMS;

    // Check enabled
    if (_isEnabled)
    {
        
        // Configure GPIOs
        ConfigPinMap::PinDef gpioPins[] = {
            ConfigPinMap::PinDef("doors[0]/strikePin", ConfigPinMap::GPIO_OUTPUT, &_strikeControlPins[0], 0),
            ConfigPinMap::PinDef("doors[1]/strikePin", ConfigPinMap::GPIO_OUTPUT, &_strikeControlPins[1], 0),
            ConfigPinMap::PinDef("doors[0]/openSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_openSensePins[0], 0),
            ConfigPinMap::PinDef("doors[1]/openSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_openSensePins[1], 0),
            ConfigPinMap::PinDef("doors[0]/closedSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_closedSensePins[0], 0),
            ConfigPinMap::PinDef("doors[1]/closedSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_closedSensePins[1], 0),
            ConfigPinMap::PinDef("bellSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &_bellPressedPin, 0),
        };
        ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

        // Get sense level of strike pins
        _strikePinLevel[0] = configGetLong("doors[0]/strikeOn", 0) != 0;
        _strikePinLevel[1] = configGetLong("doors[1]/strikeOn", 0) != 0;
        _strikeTimeSecs[0] = configGetLong("doors[0]/strikeTimeSecs", 1);
        _strikeTimeSecs[1] = configGetLong("doors[1]/strikeTimeSecs", 1);
        _openSensePinLevel[0] = configGetLong("doors[0]/senseLevel", 0) != 0;
        _openSensePinLevel[1] = configGetLong("doors[1]/senseLevel", 0) != 0;

        // Bell pressed pin sense
        _bellPressedPinLevel = configGetLong("bellSensePress", 0) != 0;

        // Master door index
        _masterDoorIndex = configGetLong("masterDoorIdx", 0);

        // Name set in UI
        _scaderFriendlyName = configGetString("ScaderCommon/name", "Scader");
        LOG_I(MODULE_PREFIX, "setup scaderUIName %s", _scaderFriendlyName.c_str());

        // Element names
        std::vector<String> elemInfos;
        if (configGetArrayElems("ScaderDoors/elems", elemInfos))
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
        for (int i = 0; i < _maxElems; i++)
        {
            if (_strikeControlPins[i] >= 0)
            {
                DoorStrike doorStrike(_strikeControlPins[i], _strikePinLevel[i], _openSensePins[i], 
                        _closedSensePins[i], _openSensePinLevel[i], _strikeTimeSecs[i]);
                _doorStrikes.push_back(doorStrike);
            }
        }

        // Debug
        for (int i = 0; i < _maxElems; i++)
        {
            LOG_I(MODULE_PREFIX, "setup enabled door %d strikePin %d strikeOn %d openSensePin %d openSenseLevel %d closedSensePin %d openForSecs %d", 
                        i, _strikeControlPins[i], _strikePinLevel[i], _openSensePins[i], _openSensePinLevel[i], 
                        _closedSensePins[i], _strikeTimeSecs[i]);
        }
        LOG_I(MODULE_PREFIX, "setup enabled bellSensePin %d", _bellPressedPin);

        // Debug show states
        debugShowCurrentState();

        // Setup publisher with callback functions
        SysManager* pSysManager = getSysManager();
        if (pSysManager)
        {
            // Register publish message generator
            pSysManager->sendMsgGenCB("Publish", "ScaderDoors", 
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
    else
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::service()
{
    // Check enabled
    if (!_isEnabled)
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
    // Control shade
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
    // Check enabled
    if (!_isEnabled)
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
            elemNums[i] = i;
        numElemsInList = _elemNames.size();
    }

    // Get newState
    String elemCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool newState = (elemCmdStr == "1") || (elemCmdStr.equalsIgnoreCase("on"));

    // Execute command
    uint32_t numElemsSet = 0;
    for (int i = 0; i < numElemsInList; i++)
    {
        int elemIdx = elemNums[i] - 1;
        if (elemIdx >= 0 && elemIdx < _elemNames.size())
        {
            // Set state
            if (newState)
            {
                _doorStrikes[elemIdx].unlockWithTimeout("API");
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
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiControl no valid door specified");
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderDoors::getStatusJSON()
{
    // Get length of JSON
    uint32_t jsonLen = 200 + _scaderFriendlyName.length();
    for (int i = 0; i < _elemNames.size(); i++)
        jsonLen += 30 + _elemNames[i].length();
    static const uint32_t MAX_JSON_STR_LEN = 4500;
    if (jsonLen > MAX_JSON_STR_LEN)
    {
        LOG_E(MODULE_PREFIX, "getStatusJSON jsonLen %d > MAX_JSON_STR_LEN %d", jsonLen, MAX_JSON_STR_LEN);
        return "{}";
    }

    // Get network information
    JSONParams networkJson = sysModGetStatusJSON("NetMan");

    // Extract hostname
    String hostname = networkJson.getString("Hostname", "");

    // Check if ethernet connected
    bool ethConnected = networkJson.getLong("ethConn", false) != 0;

    // Extract MAC address
    String macAddress;
    String ipAddress;
    if (ethConnected)
    {
        macAddress = getSystemMACAddressStr(ESP_MAC_ETH, ":").c_str(),
        ipAddress = networkJson.getString("ethIP", "");
    }
    else
    {
        macAddress = getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str(),
        ipAddress = networkJson.getString("IP", "");
    }

    // Create JSON string
    char* pJsonStr = new char[jsonLen];
    if (!pJsonStr)
        return "{}";

    // Format JSON
    snprintf(pJsonStr, jsonLen, R"({"name":"%s","version":"%s","hostname":"%s","IP":"%s","MAC":"%s","elems":[)", 
                _scaderFriendlyName.c_str(), 
                getSysManager()->getSystemVersion().c_str(),
                hostname.c_str(), ipAddress.c_str(), macAddress.c_str());
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            strncat(pJsonStr, ",", jsonLen);
        String jsonStatus = _doorStrikes[i].getStatusJson(false);
        DoorStrike::DoorLockedEnum lockedEnum = DoorStrike::LockStateUnknown;
        DoorStrike::DoorOpenSense openEnum = DoorStrike::DoorSenseUnknown;
        int timeBeforeLockMs = 0;
        _doorStrikes[i].getStatus(lockedEnum, openEnum, timeBeforeLockMs);

        snprintf(pJsonStr + strlen(pJsonStr), jsonLen - strlen(pJsonStr), R"({"name":"%s","locked":%s,"open":"%s","timeBeforeLockMs":%d})", 
                            _elemNames[i].c_str(),
                            _doorStrikes[i].getDoorLockedStr(lockedEnum), 
                            _doorStrikes[i].getDoorOpenSenseStr(openEnum),
                            timeBeforeLockMs);
    }
    snprintf(pJsonStr + strlen(pJsonStr), jsonLen - strlen(pJsonStr), R"(],"bell":"%s"})", 
                _bellPressedPin >= 0 ? (digitalRead(_bellPressedPin) == _bellPressedPinLevel ? "1" : "0") : "X");
    String jsonStr = pJsonStr;
    delete[] pJsonStr;
    return jsonStr;
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