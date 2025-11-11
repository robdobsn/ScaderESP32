/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderPulseCounter.cpp
//
// Rob Dobson 2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <time.h>
#include "Logger.h"
#include "RaftArduino.h"
#include "ScaderCommon.h"
#include "ScaderPulseCounter.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"
#include "NetworkSystem.h"
#include "CommsChannelMsg.h"
#include "PlatformUtils.h"
#include "driver/gpio.h"

// #define DEBUG_PULSE_COUNTER_MUTABLE_DATA

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderPulseCounter::ScaderPulseCounter(const char *pModuleName, RaftJsonIF& sysConfig)
        : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName),
          _scaderModuleState("scaderPulses")
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::setup()
{
    // Common
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Check if pulse counter enabled
    _pulseCounterPin = configGetLong("pulseCounterPin", -1);
    if (_pulseCounterPin < 0)
    {
        LOG_E(MODULE_PREFIX, "setup pulseCounterPin %d invalid", _pulseCounterPin);
    }
    else
    {
        _pulseCounterButton.setup(_pulseCounterPin,false, 1, 
                std::bind(&ScaderPulseCounter::pulseCounterCallback, this, 
                        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                DebounceButton::DEFAULT_PIN_DEBOUNCE_MS, 0);
    }

    // Set pulse count from scader state
    _pulseCount = _scaderModuleState.getLong("pulseCount", 0);

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled scaderUIName %s pulseCounter %s",
                _scaderCommon.getUIName().c_str(),
                ("pin " + String(_pulseCounterPin)).c_str());

    // Debug show state
    debugShowCurrentState();

    // Setup publisher with callback functions
    SysManagerIF* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->registerDataSource("Publish", _scaderCommon.getModuleName().c_str(), 
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
// Loop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::loop()
{
    // Check init
    if (!_isInitialised)
        return;

    // Service pulse counter
    _pulseCounterButton.loop();

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

void ScaderPulseCounter::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("pulsecount", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderPulseCounter::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "pulsecount/value - get/set pulse count");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader pulse counter");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderPulseCounter::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check for set pulse count
    String cmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    if (cmdStr.startsWith("value"))
    {
        // Get pulse count
        String pulseCountStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
        if (pulseCountStr.length() > 0)
        {
            // Set pulse count
            _pulseCount = pulseCountStr.toInt();
            // Save mutable data
            saveMutableData();
            _mutableDataDirty = false;
            LOG_I(MODULE_PREFIX, "apiControl pulseCount %d", _pulseCount);
        }
        String pulseCountJson = R"("pulseCount":)" + String(_pulseCount);
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true, pulseCountJson.c_str());
    }

    // Set result
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderPulseCounter::getStatusJSON() const
{
    // Pulse count JSON if enabled
    String pulseCountStr = R"(,"pulseCount":)" + String(_pulseCount);

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + pulseCountStr + "}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    stateHash.push_back(_pulseCount & 0xff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write mutable data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::saveMutableData()
{
    // Save pulse count
    String jsonConfig = "\"pulseCount\":" + String(_pulseCount);

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";
#ifdef DEBUG_PULSE_COUNTER_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
#endif
    _scaderModuleState.setJsonDoc(jsonConfig.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// debug show states
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::debugShowCurrentState()
{
    LOG_I(MODULE_PREFIX, "debugShowCurrentState pulseCount %d", _pulseCount);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pulse counter callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderPulseCounter::pulseCounterCallback(bool val, uint32_t msSinceLastChange, uint16_t repeatCount)
{
    if (val == 1)
    {
        _pulseCount++;
        _mutableDataDirty = true;
        _mutableDataChangeLastMs = millis();
        LOG_I(MODULE_PREFIX, "pulseCounterCallback count %d", _pulseCount);
    }
}
