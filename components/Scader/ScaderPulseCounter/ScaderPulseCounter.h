/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderPulseCounter.h
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftUtils.h"
#include "RaftJsonIF.h"
#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "DebounceButton.h"

class APISourceInfo;

class ScaderPulseCounter : public RaftSysMod
{
public:
    static const int DEFAULT_MAX_ELEMS = 24;
    static const int ELEMS_PER_CHIP = 8;
    static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
    ScaderPulseCounter(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderPulseCounter(pModuleName, sysConfig);
    }

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // Names of control elements
    String _pulseCountName;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Pulse counter functionality
    int _pulseCounterPin = -1;
    DebounceButton _pulseCounterButton;
    void pulseCounterCallback(bool val, uint32_t msSinceLastChange, uint16_t repeatCount);
    uint32_t _pulseCount = 0;

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Pulse count
    RaftJsonNVS _scaderModuleState;
};
