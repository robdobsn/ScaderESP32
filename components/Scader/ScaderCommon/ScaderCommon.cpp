/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderCommon
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderCommon.h"
#include <RestAPIEndpointManager.h>

static const char *MODULE_PREFIX = "ScaderCommon";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderCommon::ScaderCommon(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _pScaderConfig = pMutableConfig;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCommon::setup()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCommon::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCommon::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Get settings
    endpointManager.addEndpoint("scaderget", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderCommon::apiScaderGet, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Get settings for scader");

    // Post settings
    endpointManager.addEndpoint("scaderset", 
                            RestAPIEndpoint::ENDPOINT_CALLBACK, 
                            RestAPIEndpoint::ENDPOINT_POST,
                            std::bind(&ScaderCommon::apiScaderGet, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Set settings for scader",
                            "application/json", 
                            NULL,
                            RestAPIEndpoint::ENDPOINT_CACHE_NEVER,
                            NULL, 
                            std::bind(&ScaderCommon::apiScaderSetBody, this, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    std::placeholders::_5, std::placeholders::_6),
                            NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get scader settings
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCommon::apiScaderGet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    LOG_V(MODULE_PREFIX, "scaderGet %s", respStr.c_str());

    // Get JSON response
    String configStr;
    if (_pScaderConfig)
        configStr = _pScaderConfig->getConfigString();
    if (configStr.length() == 0)
        configStr = "{}";
    String settingsResp = R"("cfg":)" + configStr;
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, true, settingsResp.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set scader settings
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderCommon::apiScaderSet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "apiScaderSet request %s", reqStr.c_str());
    // Result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

void ScaderCommon::apiScaderSetBody(const String& reqStr, const uint8_t *pData, size_t len, 
                size_t index, size_t total, const APISourceInfo& sourceInfo)
{
    String configStr;
    Raft::strFromBuffer(pData, len, configStr, true);
    LOG_I(MODULE_PREFIX, "apiScaderSetBody %s", configStr.c_str());
    // Store the settings
    if (_pScaderConfig)
        _pScaderConfig->writeConfig(configStr);
}
