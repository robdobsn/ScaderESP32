/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderCommon
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <ConfigBase.h>
#include <RaftUtils.h>
#include <SysModBase.h>

class APISourceInfo;

class ScaderCommon : public SysModBase
{
public:
    ScaderCommon(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

protected:
    // Setup
    virtual void setup() override final;

    // Service (called frequently)
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

private:
    // Scader config
    ConfigBase* _pScaderConfig;

    // Helpers
    void apiScaderGet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiScaderSet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiScaderSetBody(const String& reqStr, const uint8_t *pData, size_t len, 
                size_t index, size_t total, const APISourceInfo& sourceInfo);
};
