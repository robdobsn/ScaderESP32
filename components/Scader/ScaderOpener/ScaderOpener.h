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
#include "IScaderStatusBody.h"
#include "RaftUtils.h"
#include "DoorOpener.h"
#include "UIModule.h"

class APISourceInfo;

class ScaderOpener : public RaftSysMod, public IScaderStatusBody
{
public:
    ScaderOpener(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderOpener(pModuleName, sysConfig);
    }

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() const override final;

    // IScaderStatusBody
    virtual String getStatusBodyJSON() const override final;
    virtual void appendStatusBodyHash(std::vector<uint8_t>& stateHash) override final;
    virtual bool isScaderEnabled() const override final { return _scaderCommon.isEnabled(); }

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

    // Debug
    static constexpr const char *MODULE_PREFIX = "ScaderOpener";
};
