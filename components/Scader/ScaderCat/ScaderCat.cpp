/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderCat
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderCat.h"
#include "RaftArduino.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"

static const char *MODULE_PREFIX = "ScaderCat";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderCat::ScaderCat(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Deinit
    deinit();

    // Check enabled
    if (!_isEnabled)
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Prepare timed outputs
    const char* timedOutputNames[] = {"light", "squirt", "fet1", "fet2"};
    const int numTimedOutputs = sizeof(timedOutputNames)/sizeof(timedOutputNames[0]);
    int pinVars[] = {0,0,0,0};
    bool onLevels[] = {true, true, true, true};

    // Configure GPIOs
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("LIGHT_CTRL", ConfigPinMap::GPIO_OUTPUT, &pinVars[0]),
        ConfigPinMap::PinDef("VALVE_CTRL", ConfigPinMap::GPIO_OUTPUT, &pinVars[1]),
        ConfigPinMap::PinDef("FET_1", ConfigPinMap::GPIO_OUTPUT, &pinVars[2]),
        ConfigPinMap::PinDef("FET_2", ConfigPinMap::GPIO_OUTPUT, &pinVars[3])
    };
    ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Add to the timed outputs
    for (uint32_t i = 0; i < numTimedOutputs; i++)
    {
        TimedOutput timedOutput(timedOutputNames[i], pinVars[i], onLevels[i]);
        _timedOutputs.push_back(timedOutput);
    }

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

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled squirtPin %d lightPin %d fet1 %d fet2 %d",
                pinVars[0], pinVars[1], pinVars[2], pinVars[3]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deinit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::deinit()
{
    // Check initialised
    if (_isInitialised)
    {
        // Change pin modes
        for (TimedOutput& timedOutput : _timedOutputs)
        {
            timedOutput.deinit();
        }

        // Deinit
        _timedOutputs.clear();
        _isInitialised = false;

        // Debug
        LOG_I(MODULE_PREFIX, "deinit");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::loop()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Check for timed outputs
    for (TimedOutput& timedOutput : _timedOutputs)
        timedOutput.loop();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderCat::isBusy(int shadeIdx)
{
    // Check validity
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("cat", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderCat::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Cat - /CTRL/on/durationMs or /CTRL/off/durationMs where CTRL is squirt, light, fet1 or fet2 and durationMs can be omitted for permanance");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader cat");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check initialised
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    bool rslt = false;
    
    // Get operation
    String operation = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);

    // Handle operation
    for (TimedOutput& timedOutput : _timedOutputs)
    {
        if (timedOutput._name == operation)
        {
            // Get control string
            String ctrlStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
            String durationStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
            timedOutput.cmd(ctrlStr, durationStr);
            rslt = true;
            break;
        }
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getStatusJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderCat::getStatusJSON()
{
    String statusStr;
    for (TimedOutput& timedOutput : _timedOutputs)
        statusStr += (statusStr.length() > 0 ? "," : "") + timedOutput.getStatusStrJSON();

    return "{" + _scaderCommon.getStatusJSON() + ",\"status\":{" + statusStr + "}}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (TimedOutput& timedOutput : _timedOutputs)
        stateHash.push_back(timedOutput.getStateHashByte());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output set
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::TimedOutput::set(bool turnOn, int durationMs)
{
    // Set
    _timerActive = false;
    _isOn = turnOn;
    if (_pin >= 0)
        digitalWrite(_pin, turnOn ? _onLevel : !_onLevel);

    // Handle duration
    if (durationMs > 0)
    {
        _timerActive = true;
        _durationMs = durationMs;
        _startTimeMs = millis();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::TimedOutput::cmd(const String &ctrlStr, const String &durationStr)
{
    // Check for on/off
    if ((ctrlStr.equalsIgnoreCase("on")) || (ctrlStr.equalsIgnoreCase("off")))
    {
        // Get duration
        int durationMs = 0;
        if (durationStr.length() > 0)
            durationMs = durationStr.toInt();

        // Set
        set(ctrlStr.equalsIgnoreCase("on"), durationMs);

        // Debug
        LOG_I(MODULE_PREFIX, "cmd turning %s (pin %d) %s%s%s", _name.c_str(), _pin, ctrlStr.c_str(), 
                (durationMs > 0) ? " for (ms) " : "", (durationMs > 0) ? durationStr.c_str() : "");
    }
    else
    {
        // Invalid
        LOG_E(MODULE_PREFIX, "cmd invalid ctrlStr %s", ctrlStr.c_str());
        return;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::TimedOutput::loop()
{
    // Check if timer is active
    if (_timerActive)
    {
        // Check if the output change is due
        if (Raft::isTimeout(millis(), _startTimeMs, _durationMs))
        {
            // Change the output
            _isOn = !_isOn;
            LOG_I(MODULE_PREFIX, "service turning %s %s", _name.c_str(), _isOn ? "on" : "off");
            if (_pin >= 0)
                digitalWrite(_pin, _isOn == _onLevel);
            _timerActive = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output deinit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCat::TimedOutput::deinit()
{
    // Change pin mode
    pinMode(_pin, INPUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderCat::TimedOutput::getStatusStrJSON()
{
    char statusStr[50];
    snprintf(statusStr, sizeof(statusStr), R"("%s":%d)", _name.c_str(), _isOn ? 1 : 0);
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t ScaderCat::TimedOutput::getStateHashByte()
{
    return _isOn ? 1 : 0;
}
