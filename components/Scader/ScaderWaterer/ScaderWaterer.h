/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderWaterer
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SysModBase.h>
#include "MoistureSensors.h"
#include "PumpControl.h"

class APISourceInfo;

class ScaderWaterer : public SysModBase
{
public:
    ScaderWaterer(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

    // Check if moving
    bool isBusy();

protected:

    // Setup
    virtual void setup() override final;

    // Service (called frequently)
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
