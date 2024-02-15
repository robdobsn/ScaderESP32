/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderWaterer
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftUtils.h"
#include "RaftSysMod.h"
#include "MoistureSensors.h"
#include "PumpControl.h"

class APISourceInfo;

class ScaderWaterer : public RaftSysMod
{
public:
    ScaderWaterer(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderWaterer(pModuleName, sysConfig);
    }
    
    // Check if moving
    bool isBusy();

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:

    // Enabled and initalised flags
    bool _isEnabled;
    bool _isInitialised;

    // Moisture sensors
    MoistureSensors _moistureSensors;

    // Pump controls
    PumpControl _pumpControl;

    // API control
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // Helpers
    void getStatusHash(std::vector<uint8_t>& stateHash);
};
