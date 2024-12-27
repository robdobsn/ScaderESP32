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
            double speedPC = paramsJSON.getDouble("speed", 100.0);
            if (speedPC <= 0)
                speedPC = 100;
            if (speedPC > 500)
                speedPC = 500;
            double durationMins = paramsJSON.getDouble("duration", 10.0);
            if (durationMins < 0)
                durationMins = 10;
            double extendFactor = speedPC / 100.0;
            // Get motor device
            RaftDevice* pMotor = getMotorDevice();
            if (pMotor)
            {
                rslt = true;
                rsltStr = "Run";
                String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"rel":1,"nosplit":1,"feedrate":__SPEED__,"pos":[{"a":0,"p":__POS__}]})";
                moveCmd.replace("__POS__", String(12000 * durationMins * extendFactor));
                moveCmd.replace("__SPEED__", String(speedPC*2));
                // Send command
                pMotor->sendCmdJSON(moveCmd.c_str());
                // Debug
                LOG_I(MODULE_PREFIX, "api Run %s", moveCmd.c_str());
            }
        }
        // Stop
        else if (params[1].equalsIgnoreCase("stop"))
        {
            rsltStr = "Stopped";
            rslt = true;
            // Get motor device
            RaftDevice* pMotor = getMotorDevice();
            if (pMotor)
            {
                // Send command
                String stopCmd = R"({"cmd":"motion","stop":1})";
                pMotor->sendCmdJSON(stopCmd.c_str());
                // Debug
                LOG_I(MODULE_PREFIX, "api Stop %s", stopCmd.c_str());
            }
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
