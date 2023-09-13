/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderDoors
//
// Rob Dobson 2013-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RaftArduino.h>
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
}

ScaderDoors::~ScaderDoors()
{
    if (_bellPressedPin >= 0)
    {
        pinMode(_bellPressedPin, INPUT);
    }
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

    // Get array of door configs
    std::vector<String> doorConfigs;
    configGetArrayElems("doors", doorConfigs);
    if (doorConfigs.size() == 0)
    {
        LOG_E(MODULE_PREFIX, "setup no doors");
        return;
    }

    // Handle bell config
    String bellSensePinStr = configGetString("bellSensePin", "");
    _bellPressedPin = ConfigPinMap::getPinFromName(bellSensePinStr.c_str());
    if (_bellPressedPin >= 0)
    {
        pinMode(_bellPressedPin, INPUT_PULLUP);
    }
    _bellPressedPinLevel = configGetBool("bellSenseLevel", false);

    // Master door index and reporting
    _masterDoorIndex = configGetLong("masterDoorIdx", 0);
    _reportMasterDoorOnly = configGetBool("reportMasterDoorOnly", false);

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

    // Iterate through door configs
    uint32_t doorIdx = 0;
    for (String& doorConfigStr : doorConfigs)
    {
        // Check if valid
        if (doorIdx >= _maxElems)
        {
            LOG_E(MODULE_PREFIX, "setup too many doors");
            break;
        }

        // Extract door config
        ConfigBase doorConfig(doorConfigStr);

        // Get active/sense levels of pins
        bool strikePinUnlockLevel = doorConfig.getBool("strikeOn", false);
        bool openSensePinLevel = doorConfig.getBool("openSenseLevel", false);
        uint32_t unlockForSecs = doorConfig.getLong("unlockForSecs", 10);
        uint32_t delayRelockSecs = doorConfig.getLong("delayRelockSecs", 0);

        // Configure GPIOs
        int strikeControlPin = -1;
        int openSensePin = -1;
        int closedSensePin = -1;
        ConfigPinMap::PinDef gpioPins[] = {
            ConfigPinMap::PinDef("strikePin", ConfigPinMap::GPIO_OUTPUT, &strikeControlPin, !strikePinUnlockLevel),
            ConfigPinMap::PinDef("openSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &openSensePin, 0),
            ConfigPinMap::PinDef("closedSensePin", ConfigPinMap::GPIO_INPUT_PULLUP, &closedSensePin, 0)
        };
        ConfigPinMap::configMultiple(doorConfig, gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

        // Setup the door strike
        if (strikeControlPin >= 0)
        {
            _doorStrikes[doorIdx].setup(strikeControlPin, strikePinUnlockLevel, openSensePin, 
                    closedSensePin, openSensePinLevel, unlockForSecs, delayRelockSecs);
        }
 
        LOG_I(MODULE_PREFIX, "setup enabled doorIdx %d strikePin %d strikeOpen %d openSensePin %d openSenseLevel %d closedSensePin %d unlockForSecs %d", 
                    doorIdx, strikeControlPin, strikePinUnlockLevel, openSensePin, openSensePinLevel, closedSensePin, unlockForSecs);

        // Next door
        doorIdx++;
    }

    LOG_I(MODULE_PREFIX, "setup enabled scaderUIName %s bellSensePin %d bellSenseLevel %d masterDoorIdx %d", 
                    _scaderCommon.getUIName().c_str(),
                    _bellPressedPin, _bellPressedPinLevel, _masterDoorIndex);

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

    // Check for change of state
    if (Raft::isTimeout(millis(), _lockedStateChangeTestLastMs, STATE_CHANGE_MIN_MS))
    {
        _lockedStateChangeTestLastMs = millis();

        // Determine if door lock state reporting is required
        bool changeDetected = false;
        for (int i = 0; i < _doorStrikes.size(); i++)
        {
            _doorStrikes[i].service();
            if (_reportMasterDoorOnly && (i != _masterDoorIndex))
                continue;
            if (_lockedStateChangeLastLocked != _doorStrikes[i].isLocked())
                changeDetected = true;
            _lockedStateChangeLastLocked = _doorStrikes[i].isLocked();
        }

        // Report if required
        if (changeDetected)
        {
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

RaftRetCode ScaderDoors::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl module disabled");
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
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
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scader doors
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderDoors::apiTagRead(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiTagRead module disabled");
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
    }

    // Extract params
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    JSONParams paramsJSON = RaftJson::getJSONFromNVPairs(nameValues, true);

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
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
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
                _doorStrikes[elemIdx].unlockWithTimeout("API");
            }
            else
            {
                _doorStrikes[elemIdx].lock(false);
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
    int channelID = getCommsCore()->getChannelIDByName("Serial1", "RICSerial");

    // Send message
    CommsChannelMsg msg(channelID, MSG_PROTOCOL_RAWCMDFRAME, 0, MSG_TYPE_COMMAND);
    String cmdStr = R"("cmdName":"InfoDoorStatusChange","doorStatus":")";
    cmdStr += _lockedStateChangeLastLocked ? "locked" : "unlocked";
    cmdStr += R"(")";
    cmdStr = "{" + cmdStr + "}";
    msg.setFromBuffer((uint8_t*)cmdStr.c_str(), cmdStr.length());
    getCommsCore()->outboundHandleMsg(msg);

    // Debug
    LOG_I(MODULE_PREFIX, "publishStateChangeToCommandSerial channelID %d msg %s", 
                channelID, cmdStr.c_str());
}
