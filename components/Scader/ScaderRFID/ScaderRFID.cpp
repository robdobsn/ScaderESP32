/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderRFID
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <ScaderRFID.h>
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
#include <CommsCoreIF.h>
#include "RFIDModule_EccelA1SPI.h"

static const char *MODULE_PREFIX = "ScaderRFID";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderRFID::ScaderRFID(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig, NULL, true),
          _scaderCommon(*this, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // ACT LED pin
    int actLedPin = configGetLong("actLedPin", -1);
    _actLed.setup("ACT", actLedPin, 1, 100, 250, 1500);

    // Tag LED pin
    _tagLedPin = configGetLong("tagLedPin", -1);
    if (_tagLedPin >= 0)
    {
        pinMode(_tagLedPin, OUTPUT);
        digitalWrite(_tagLedPin, LOW);
    }

    // Buzzer
    int buzzerPin = configGetLong("buzzerPin", -1);
    int buzzerOnLevel = configGetLong("buzzerOnLevel", 1);
    int buzzerOnMs = configGetLong("buzzerOnMs", 100);
    int buzzerShortOffMs = configGetLong("buzzerShortOffMs", 900);
    int buzzerLongOffMs = configGetLong("buzzerLongOffMs", 1800);
    _buzzer.setup("Buzzer", buzzerPin, buzzerOnLevel, buzzerOnMs, buzzerShortOffMs, buzzerLongOffMs);

    // RFID module pins
    int rfidSPIMOSIPin = configGetLong("rfidSPIMOSIPin", -1);
    int rfidSPIMISOPin = configGetLong("rfidSPIMISOPin", -1);
    int rfidSPIClkPin = configGetLong("rfidSPIClkPin", -1);
    int rfidSPICS0Pin = configGetLong("rfidSPICS0Pin", -1);
    int rfidNBusyPin = configGetLong("rfidNBusyPin", -1);
    int rfidResetPin = configGetLong("rfidResetPin", -1);
    int rfidResetActive = configGetLong("rfidResetActive", LOW);
    int rfidSPIHostID = configGetLong("rfidSPIHostID", 1);

    // Setup RFID module
    if (rfidSPIMOSIPin >= 0 && rfidSPIMISOPin >= 0 && rfidSPIClkPin >= 0 && rfidSPICS0Pin >= 0)
    {
        // Setup RFID module
        _pRFIDModule = new RFIDModule_EccelA1SPI(rfidSPIMOSIPin, rfidSPIMISOPin, 
                    rfidSPIClkPin, rfidSPICS0Pin, 
                    rfidSPIHostID, rfidNBusyPin,
                    rfidResetPin, rfidResetActive);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup moduleName %s scaderUIName %s ACT LED %d TAG LED %d", 
            _scaderCommon.getModuleName().c_str(),
            _scaderCommon.getUIName().c_str(),
            actLedPin, _tagLedPin);

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
    _isInitialised = _pRFIDModule != nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::service()
{
    // Check if initialised
    if (!_isInitialised)
        return;

    // Set ACT LED
    // 1 pulse when connected, 3 pulses when disconnected (long gap after sequence)
    _actLed.setStatusCode(networkSystem.isIPConnected() ? 1 : 3);

    // // Check if connected to WiFi
    // static const uint32_t ACT_LED_FLASH_TOTAL_TIME_MS = 1500;
    // uint32_t actLedFlashOnTime = networkSystem.isIPConnected() ? ACT_LED_FLASH_TOTAL_TIME_MS/2 : ACT_LED_FLASH_TOTAL_TIME_MS/10;

    // Get RFID status
    if (_pRFIDModule && Raft::isTimeout(millis(), _rfidStatusLastMs, RFID_STATUS_CHECK_MS))
    {
        // Update last time
        _rfidStatusLastMs = millis();

        String tagID;
        uint32_t tagPresentedMs = 0;
        bool changeOfTagState = false;
        bool tagPresent = false;
        _pRFIDModule->getTag(tagID, tagPresent, changeOfTagState, tagPresentedMs);
        if (changeOfTagState)
        {
            LOG_I(MODULE_PREFIX, "service tagID %s", tagID.c_str());

            // Send message over command serial
            int channelID = getCommsCore()->getChannelIDByName("CommandSerial", "RICSerial");
            // LOG_I(MODULE_PREFIX, "service channelID %d", channelID);

            // Send message
            CommsChannelMsg msg(channelID, MSG_PROTOCOL_RAWCMDFRAME, 0, MSG_TYPE_COMMAND);
            String cmdStr = R"("cmdName":"RFIDTagRead")";
            if (tagID.length() > 0)
                cmdStr += R"(,"tagID":")" + tagID + R"(")";
            cmdStr = "{" + cmdStr + "}";
            msg.setFromBuffer((uint8_t*)cmdStr.c_str(), cmdStr.length());
            getCommsCore()->handleOutboundMessage(msg);
        }
        digitalWrite(_tagLedPin, tagPresent ? HIGH : LOW);
    }

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

    // Service RFID
    if (_pRFIDModule)
        _pRFIDModule->service();

    // Service indicators
    _buzzer.service();
    _actLed.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Door status change
    endpointManager.addEndpoint("InfoDoorStatusChange", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderRFID::apiDoorStatusChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "InformDoorStatusChange?doorStatus=<locked|unlocked>");

    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader rfid");

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Door status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::apiDoorStatusChange(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiDoorStatusChange module disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    JSONParams paramsJSON = RdJson::getJSONFromNVPairs(nameValues, true);

    // Result
    bool rslt = true;

    // Start beeping to indicate door unlocked if appropriate
    String doorStatus = paramsJSON.getString("doorStatus", "");
    if (doorStatus.equalsIgnoreCase("unlocked"))
    {
        _buzzer.setStatusCode(20, 10000);
        LOG_I(MODULE_PREFIX, "apiDoorStatusChange ================================ door unlocked");
    }
    else
    {
        _buzzer.setStatusCode(0);
        LOG_I(MODULE_PREFIX, "apiDoorStatusChange ================================ door locked");
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderRFID::getStatusJSON()
{
    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + "}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write the mutable config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::saveMutableData()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// debug show relay states
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::debugShowCurrentState()
{
    String elemsStr;
    LOG_I(MODULE_PREFIX, "debugShowCurrentState %s", elemsStr.c_str());
}
