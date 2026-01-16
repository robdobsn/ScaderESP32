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

    // Speed and run mode input pins
    int32_t _speedInputPin = -1;
    int32_t _runModeInputPin = -1;
    int32_t _currentSpeedValue = 0;
    static constexpr int32_t DEFAULT_SPEED_VALUE = 100;
    static constexpr int32_t MAX_SPEED_VALUE = 500;
    static constexpr int32_t DEFAULT_DURATION_MINS = 10;
    static constexpr int32_t SPEED_CHANGE_THRESHOLD = 5;

    // Default duration mins
    double _defaultDurationMins = DEFAULT_DURATION_MINS;
    
    // Initalised flag
    bool _isInitialised = false;

    // NVS State
    RaftJsonNVS _scaderModuleState;

    // Last time inputs checked
    static constexpr uint32_t INPUT_CHECK_MS = 200;
    uint32_t _lastInputCheckMs = 0;
    
    // Get motor device
    RaftDevice* getMotorDevice() const;

    // Helpers
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void setSpeedFromPotAndSwitch();
    void setMotorSpeed(double speedValue, double durationMins = -1);

    // Debug
    static constexpr const char* MODULE_PREFIX = "ScaderMarbleRun";
};
