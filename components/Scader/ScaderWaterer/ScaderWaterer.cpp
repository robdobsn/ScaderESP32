/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderWaterer
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ScaderWaterer.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>

static const char *MODULE_PREFIX = "ScaderWaterer";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderWaterer::ScaderWaterer(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Init
    _isEnabled = false;
    _isInitialised = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderWaterer::setup()
{
    // Get settings
    _isEnabled = configGetLong("enable", false) != 0;

    // Check enabled
    if (_isEnabled)
    {
        // Configure
        _moistureSensors.setup(configGetConfig());
        _pumpControl.setup(configGetConfig());

        // Setup publisher with callback functions
        SysManager* pSysManager = getSysManager();
        if (pSysManager)
        {
            // Register publish message generator
            pSysManager->sendMsgGenCB("Publish", "ScaderWaterer", 
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

void ScaderWaterer::service()
{
    if (!_isInitialised)
        return;
    _moistureSensors.service();
    _pumpControl.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderWaterer::isBusy()
{
    return _pumpControl.isBusy();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderWaterer::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("waterer", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderWaterer::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control waterer");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderWaterer::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
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
        // Control pump
        if (params[1] == "pump")
        {
            if (params.size() > 4)
            {
                uint32_t pumpNum = params[2].toInt();
                float flowRate = params[3].toFloat();
                float durationSecs = params[4].toFloat();
                _pumpControl.setFlow(pumpNum-1, flowRate, durationSecs);
            }
        }
    //     // Open
    //     if (params[1] == "open")
    //     {
    //         _doorOpener.moveDoor(true);
    //         rsltStr = "Opened";
    //     }
    //     // Close
    //     else if (params[1] == "close")
    //     {
    //         _doorOpener.moveDoor(false);
    //         rsltStr = "Closed";
    //     }
    //     // Stop
    //     else if (params[1] == "stop")
    //     {
    //         _doorOpener.stopDoor();
    //         rsltStr = "Stopped";
    //     }
    //     // Mode
    //     else if (params[1] == "mode")
    //     {
    //         if (params.size() > 1)
    //         {
    //             if (params[2] == "in")
    //             {
    //                 _doorOpener.setMode(true, false);
    //                 rsltStr = "Mode set to IN";
    //             }
    //             else if (params[2] == "out")
    //             {
    //                 _doorOpener.setMode(false, true);
    //                 rsltStr = "Mode set to OUT";
    //             }
    //             else if (params[2] == "both")
    //             {
    //                 _doorOpener.setMode(true, true);
    //                 rsltStr = "Mode set to BOTH";
    //             }
    //             else if (params[2] == "none")
    //             {
    //                 _doorOpener.setMode(false, false);
    //                 rsltStr = "Mode set to NONE";
    //             }
    //             else
    //             {
    //                 rsltStr = "Invalid mode";
    //                 rslt = false;
    //             }
    //         }
    //         else
    //         {
    //             rsltStr = "Mode not specified";
    //             rslt = false;
    //         }
    //     }
    //     // Unknown
    //     else
    //     {
    //         rsltStr = "Unknown command";
    //         rslt = false;
    //     }
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderWaterer::getStatusJSON()
{
    // Get Json
    return _moistureSensors.getStatusJSON(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderWaterer::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (uint32_t i = 0; i < _moistureSensors.getCount(); i++)
        stateHash.push_back(_moistureSensors.getMoisturePercentageHash(i));
}

