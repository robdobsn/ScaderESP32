/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderOpener
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderOpener.h"
#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>
#include <esp_attr.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderOpener";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderOpener::ScaderOpener(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Init
    _isEnabled = false;
    _isInitialised = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::setup()
{
    // Get settings
    _isEnabled = configGetLong("enable", false) != 0;

    // Check enabled
    if (_isEnabled)
    {
        // Configure
        _doorOpener.setup(configGetConfig());

        // HW Now initialised
        _isInitialised = true;

        // Debug
        LOG_I(MODULE_PREFIX, "setup enabled");
    }
    else
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::service()
{
    if (!_isInitialised)
        return;
    _doorOpener.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderOpener::isBusy()
{
    return _doorOpener.isBusy();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("opener", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderOpener::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Opener - /open or /close or /mode/in or /mode/out or /mode/both or /mode/none or mode/open");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    String paramsJSON = RdJson::getJSONFromNVPairs(nameValues, true);

    // Handle commands
    bool rslt = true;
    String rsltStr;
    if (params.size() > 0)
    {
        // Open
        if (params[1] == "open")
        {
            _doorOpener.moveDoor(true);
            rsltStr = "Opened";
        }
        // Close
        else if (params[1] == "close")
        {
            _doorOpener.moveDoor(false);
            rsltStr = "Closed";
        }
        // Stop
        else if (params[1] == "stop")
        {
            _doorOpener.stopDoor();
            rsltStr = "Stopped";
        }
        // Mode
        else if (params[1] == "mode")
        {
            if (params.size() > 1)
            {
                if (params[2] == "in")
                {
                    _doorOpener.setMode(true, false);
                    rsltStr = "Mode set to IN";
                }
                else if (params[2] == "out")
                {
                    _doorOpener.setMode(false, true);
                    rsltStr = "Mode set to OUT";
                }
                else if (params[2] == "both")
                {
                    _doorOpener.setMode(true, true);
                    rsltStr = "Mode set to BOTH";
                }
                else if (params[2] == "none")
                {
                    _doorOpener.setMode(false, false);
                    rsltStr = "Mode set to NONE";
                }
                else
                {
                    rsltStr = "Invalid mode";
                    rslt = false;
                }
            }
            else
            {
                rsltStr = "Mode not specified";
                rslt = false;
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
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
        LOG_I(MODULE_PREFIX, "apiControl: reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    }
    else
    {
        Raft::setJsonErrorResult(reqStr.c_str(), respStr, rsltStr.c_str());
        LOG_E(MODULE_PREFIX, "apiControl: FAILED reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    }
}
