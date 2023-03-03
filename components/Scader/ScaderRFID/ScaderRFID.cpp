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
    _actLedPin = configGetLong("actLedPin", -1);

    // Debug
    LOG_I(MODULE_PREFIX, "setup moduleName %s scaderUIName %s ACT LED %d", 
            _scaderCommon.getModuleName().c_str(),
            _scaderCommon.getFriendlyName().c_str(),
            _actLedPin);

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

void ScaderRFID::service()
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader rfid");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRFID::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Result
    bool rslt = false;

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
