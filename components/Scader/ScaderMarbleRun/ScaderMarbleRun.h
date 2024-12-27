/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderMarbleRun
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ScaderCommon.h"
#include "DoorOpener.h"
#include "UIModule.h"

class APISourceInfo;

class ScaderMarbleRun : public RaftSysMod
{
public:
    ScaderMarbleRun(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderMarbleRun(pModuleName, sysConfig);
    }
    
protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

private:

    // Common
    ScaderCommon _scaderCommon;

    // Initalised flag
    bool _isInitialised = false;

    // NVS State
    RaftJsonNVS _scaderModuleState;
    
    // Get motor device
    RaftDevice* getMotorDevice() const;

    // Helpers
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // Debug
    static constexpr const char* MODULE_PREFIX = "ScaderMarbleRun";
};
