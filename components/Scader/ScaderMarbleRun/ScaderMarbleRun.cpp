/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderMarbleRun
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderMarbleRun.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderMarbleRun::ScaderMarbleRun(const char* pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName),
          _scaderModuleState("ScaderMarbleRun")
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderMarbleRun::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Get speed input
    _speedInputPin = configGetLong("speedInputPin", -1);
    if (_speedInputPin >= 0)
    {
        pinMode(_speedInputPin, ANALOG);
        LOG_I(MODULE_PREFIX, "setup speedInputPin %d", _speedInputPin);
    }

    // Get run mode on/off input pin
    _runModeInputPin = configGetLong("runModeInputPin", -1);
    if (_runModeInputPin >= 0)
    {
        pinMode(_runModeInputPin, INPUT_PULLUP);
        LOG_I(MODULE_PREFIX, "setup runModeInputPin %d", _runModeInputPin);
    }

    // Get default speed value
    _currentSpeedValue = configGetLong("defaultSpeedValue", DEFAULT_SPEED_VALUE);
    if (_currentSpeedValue > MAX_SPEED_VALUE)
        _currentSpeedValue = MAX_SPEED_VALUE;

    // Get default duration mins
    _defaultDurationMins = configGetLong("defaultDurationMins", DEFAULT_DURATION_MINS);

    // Set initial speed from pot and switch
    setSpeedFromPotAndSwitch();

    // HW Now initialised
    _isInitialised = true;

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled scaderUIName %s", 
                _scaderCommon.getUIName().c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// loop - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderMarbleRun::loop()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Check if time to read inputs
    if (Raft::isTimeout(millis(), _lastInputCheckMs, INPUT_CHECK_MS))
    {
        _lastInputCheckMs = millis();

        // Set speed from pot and switch if it has changed
        setSpeedFromPotAndSwitch();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderMarbleRun::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("marbles", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderMarbleRun::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Marble Run - marbles/run?speed=N&duration=M (where N is percent and M is mins - if not specified run forever), marbles/stop");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderMarbleRun::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    RaftJson paramsJSON = RaftJson::getJSONFromNVPairs(nameValues, true);

    // Handle commands
    bool rslt = false;
    String rsltStr = "Failed";
    LOG_I(MODULE_PREFIX, "apiControl: params size %d", params.size());
    if (params.size() > 1)
    {
        // Run
        if (params[1].equalsIgnoreCase("run"))
        {
            // Get optional speed and duration
            double speedPC = paramsJSON.getDouble("speed", DEFAULT_SPEED_VALUE);
            if (speedPC <= 0)
                speedPC = DEFAULT_SPEED_VALUE;
            if (speedPC > MAX_SPEED_VALUE)
                speedPC = MAX_SPEED_VALUE;
            double durationMins = paramsJSON.getDouble("duration", _defaultDurationMins);
            if (durationMins < 0)
                durationMins = _defaultDurationMins;

            // Run motor
            rslt = true;
            rsltStr = "Run";
            setMotorSpeed(speedPC);
        }
        // Stop
        else if (params[1].equalsIgnoreCase("stop"))
        {
            rsltStr = "Stopped";
            rslt = true;
            // Stop motor
            setMotorSpeed(0);
        }
        // Speed
        else if ((params.size() > 2) && params[1].equalsIgnoreCase("raw"))
        {
            // Get motor device
            RaftDevice* pMotor = getMotorDevice();
            if (pMotor)
            {
                rslt = true;
                rsltStr = "Raw";
                String moveCmd = R"({"cmd":"motion",)" + params[2] + R"(})";
                // Send command
                pMotor->sendCmdJSON(moveCmd.c_str());
                // Debug
                LOG_I(MODULE_PREFIX, "api Raw %s", moveCmd.c_str());
            }
        }
        // Unknown
        else
        {
            rsltStr = "Unknown command";
            rslt = false;
        }
    }
    else
    {
        rsltStr = "No command specified";
        rslt = false;
    }    

    // Result
    if (rslt)
    {
        LOG_I(MODULE_PREFIX, "apiControl: reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    LOG_E(MODULE_PREFIX, "apiControl: FAILED reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    return Raft::setJsonErrorResult(reqStr.c_str(), respStr, rsltStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Get motor device
RaftDevice* ScaderMarbleRun::getMotorDevice() const
{
    // Get motor device
    auto pDevMan = getSysManager()->getDeviceManager();
    if (!pDevMan)
        return nullptr;
    RaftDevice* pMotor = pDevMan->getDevice("Motor");
    if (!pMotor)
        return nullptr;
    return pMotor;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set speed from potentiometer and switch if it has changed
void ScaderMarbleRun::setSpeedFromPotAndSwitch()
{
    int32_t newSpeedValue = _currentSpeedValue;
    bool forcedStop = false;

    // Check run mode from switch first
    if (_runModeInputPin >= 0)
    {
        if (digitalRead(_runModeInputPin) == HIGH)
        {
            // Log change
            if (_currentSpeedValue != 0)
            {
                LOG_I(MODULE_PREFIX, "readSpeedFromPotAndSwitch not in run mode - switch off");
            }

            // Not in run mode
            forcedStop = true;
            newSpeedValue = 0;
        }
    }

    // Read speed pot
    if ((_speedInputPin >= 0) && !forcedStop)
    {
        int32_t analogVal = analogRead(_speedInputPin);

        // Map to speed value (analogVal range 0-4095 to speed range 0-MAX_SPEED_VALUE)
        newSpeedValue = (analogVal * MAX_SPEED_VALUE) / 4095;
        
        // Only update if change exceeds threshold (hysteresis)
        int32_t speedDiff = abs(newSpeedValue - _currentSpeedValue);
        if (speedDiff >= SPEED_CHANGE_THRESHOLD)
        {
            LOG_I(MODULE_PREFIX, "setSpeedFromPotAndSwitch analogVal %d mappedSpeed %d (diff %d)", 
                    analogVal, newSpeedValue, speedDiff);
            _currentSpeedValue = newSpeedValue;
            setMotorSpeed(_currentSpeedValue);
        }
    }
    else if (forcedStop && (_currentSpeedValue != 0))
    {
        // Update speed to 0 when forced stop
        _currentSpeedValue = 0;
        setMotorSpeed(0);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Set motor speed
void ScaderMarbleRun::setMotorSpeed(double speedValue, double durationMins)
{
    RaftDevice* pMotor = getMotorDevice();
    if (!pMotor)
        return;
    if (speedValue <= 0)
    {
        // Send command
        String stopCmd = R"({"cmd":"motion","stop":1})";
        pMotor->sendCmdJSON(stopCmd.c_str());
        // Debug
        LOG_I(MODULE_PREFIX, "api Stop %s", stopCmd.c_str());
        return;
    }

    // Get motor device
    double extendFactor = speedValue / 100.0;
    String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"rel":1,"nosplit":1,"feedrate":__SPEED__,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(12000 * durationMins * extendFactor));
    moveCmd.replace("__SPEED__", String(speedValue*2));
    // Send command
    pMotor->sendCmdJSON(moveCmd.c_str());
    // Debug
    LOG_I(MODULE_PREFIX, "api Run %s", moveCmd.c_str());
}
