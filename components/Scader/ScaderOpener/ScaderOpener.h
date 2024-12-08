/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderOpener
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "ScaderCommon.h"
#include "RaftUtils.h"
#include "DoorOpener.h"
#include "UIModule.h"

class APISourceInfo;

class ScaderOpener : public RaftSysMod
{
public:
    ScaderOpener(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderOpener(pModuleName, sysConfig);
    }
    
    // // Check if moving
    // bool isBusy();

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() const override final;

private:

    // Common
    ScaderCommon _scaderCommon;

    // Initalised flag
    bool _isInitialised = false;

    // Opener state NVS
    RaftJsonNVS _scaderModuleState;

    // Opener hardware
    DoorOpener _doorOpener;

    // UI module
    UIModule _uiModule;

    // Helpers
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);
};
