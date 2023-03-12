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
#include <CommsCoreIF.h>
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
    bool isAnyDoorUnlocked = false;
    for (int i = 0; i < _doorStrikes.size(); i++)
    {
        _doorStrikes[i].service();
        isAnyDoorUnlocked |= !_doorStrikes[i].isLocked();
    }

    // Check for change of state
    if (Raft::isTimeout(millis(), _isAnyDoorUnlockedLastMs, STATE_CHANGE_MIN_MS))
    {
        _isAnyDoorUnlockedLastMs = millis();
        if (isAnyDoorUnlocked != _isAnyDoorUnlocked)
        {
            // Update state
            _isAnyDoorUnlocked = isAnyDoorUnlocked;

            // Publish state change to CommandSerial
            publishStateChangeToCommandSerial();
        }
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

    // RFID
    endpointManager.addEndpoint("RFIDTagRead", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderDoors::apiTagRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "RFID tag has been read on door, RFIDTagRead?tagID=XXXX tagID is the tag read");

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
        LOG_I(MODULE_PREFIX, "apiControl module disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Get list of doors to control
    bool rslt = false;
    String elemNumStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    std::vector<int> elemNums;
    if (elemNumStr.length() > 0)
    {
        // Get the numbers
        Raft::parseIntList(elemNumStr.c_str(), elemNums);
    }
    else
    {
        // No numbers so do all
        for (int i = 0; i < _elemNames.size(); i++)
            elemNums.push_back(i + 1);
    }

    // Get newState
    String elemCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool newState = (elemCmdStr == "1") || (elemCmdStr.equalsIgnoreCase("on") || elemCmdStr.equalsIgnoreCase("unlock"));

    // Execute unlock/lock
    uint32_t numElemsSet = executeUnlockLock(elemNums, newState);

    // Check something changed
    if (numElemsSet > 0)
    {
        // Set
        _mutableDataDirty = true;
        rslt = true;

        // Debug
        LOG_I(MODULE_PREFIX, "apiControl req %s numSet %d newState %d num doors affected %d", 
                    reqStr.c_str(), numElemsSet, newState, elemNums.size());
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
// Scader doors
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::apiTagRead(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiTagRead module disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    JSONParams paramsJSON = RdJson::getJSONFromNVPairs(nameValues, true);

    // Publish to MQTT
    String tagID = paramsJSON.getString("tagID", "");
    if (tagID.length() > 0)
    {
        // Add to tag queue only if it is empty
        bool queueEmpty = (_tagReadQueue.count() == 0);
        if (queueEmpty)
            _tagReadQueue.put(tagID);
        LOG_I(MODULE_PREFIX, "apiTagRead tagID %s %s", tagID.c_str(), 
                    queueEmpty ? "added to queue" : "queue not empty");
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiTagRead no tagID in req %s", reqStr.c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execute unlock / lock command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t ScaderDoors::executeUnlockLock(std::vector<int> elemNums, bool unlock)
{
    // Count number of elements set
    uint32_t numElemsSet = 0;
    for (auto elemNum : elemNums)
    {
        // Check element number
        int elemIdx = elemNum - 1;
        if (elemIdx >= 0 && elemIdx < _doorStrikes.size())
        {
            // Set state
            if (unlock)
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
        else
        {
            LOG_W(MODULE_PREFIX, "executeUnlockLock invalid door number %d", elemNum);
        }
    }

    return numElemsSet;
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

    // Get RFID tag read
    String rfidTagRead;
    if (_tagReadQueue.get(rfidTagRead))
    {
        rfidTagRead = ",\"RFIDTag\":\"" + rfidTagRead + "\"";
    }

    // Bell status str
    String bellStatus = _bellPressedPin >= 0 ? (digitalRead(_bellPressedPin) == _bellPressedPinLevel ? "Y" : "N") : "K";

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + rfidTagRead + 
            ",\"bell\":\"" + bellStatus + 
            "\",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::getStatusHash(std::vector<uint8_t>& stateHash)
{
    // Clear hash initially
    stateHash.clear();

    // Add door strike status to the hash
    for (const DoorStrike& doorStrike : _doorStrikes)
        doorStrike.getStatusHash(stateHash);

    // Add RFID tag list length to the hash
    uint8_t tagListLen = _tagReadQueue.count();
    stateHash.push_back(tagListLen);
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
// Publish state change to CommandSerial
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderDoors::publishStateChangeToCommandSerial()
{
    // If any door locked/unlocked then send message over CommandSerial to inform of change
    int channelID = getCommsCore()->getChannelIDByName("CommandSerial", "RICSerial");
    // LOG_I(MODULE_PREFIX, "service channelID %d", channelID);

    // Send message
    CommsChannelMsg msg(channelID, MSG_PROTOCOL_RAWCMDFRAME, 0, MSG_TYPE_COMMAND);
    String cmdStr = R"("cmdName":"InfoDoorStatusChange","doorStatus":")";
    cmdStr += _isAnyDoorUnlocked ? "unlocked" : "locked";
    cmdStr += R"(")";
    cmdStr = "{" + cmdStr + "}";
    msg.setFromBuffer((uint8_t*)cmdStr.c_str(), cmdStr.length());
    getCommsCore()->handleOutboundMessage(msg);
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Parse list of integers
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// int ScaderDoors::parseIntList(const String &str, int *pIntList, int maxInts)
// {
//     int numInts = 0;
//     int idx = 0;
//     while (idx < str.length())
//     {
//         int nextCommaIdx = str.indexOf(',', idx);
//         if (nextCommaIdx < 0)
//             nextCommaIdx = str.length();
//         String intStr = str.substring(idx, nextCommaIdx);
//         int intVal = intStr.toInt();
//         if (numInts < maxInts)
//             pIntList[numInts++] = intVal;
//         idx = nextCommaIdx + 1;
//     }
//     return numInts;
// }