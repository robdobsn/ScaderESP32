/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderWaterer
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RaftArduino.h"
#include "RaftUtils.h"
#include "ScaderWaterer.h"
#include "ConfigPinMap.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"

static const char *MODULE_PREFIX = "ScaderWaterer";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderWaterer::ScaderWaterer(const char *pModuleName, RaftJsonIF& sysConfig)
    : SysModBase(pModuleName, sys)
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
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    String paramsJSON = RaftJson::getJSONFromNVPairs(nameValues, true);

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
