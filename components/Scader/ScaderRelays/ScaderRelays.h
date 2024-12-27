/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderRelays
//
// Rob Dobson 2013-2020
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftUtils.h"
#include "RaftJsonIF.h"
#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "SPIDimmer.h"

class APISourceInfo;

class ScaderRelays : public RaftSysMod
{
public:
    static const int DEFAULT_MAX_ELEMS = 24;
    // static const int ELEMS_PER_CHIP = 8;
    // static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
    ScaderRelays(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderRelays(pModuleName, sysConfig);
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
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // Dimmer
    SPIDimmer _spiDimmer;

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // On/Off Key
    int _onOffKey = -1;

    // Names of control elements
    std::vector<String> _elemNames;

    // Current state of elements
    std::vector<uint8_t> _elemStates;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Relay states, etc
    RaftJsonNVS _scaderModuleState;

    // Helper
    uint32_t getElemStateFromString(const String &elemStateStr)
    {
        if (elemStateStr.equalsIgnoreCase("on") || elemStateStr.equalsIgnoreCase("1"))
            return 100;
        if (elemStateStr.equalsIgnoreCase("off") || elemStateStr.equalsIgnoreCase("0"))
            return 0;
        return elemStateStr.toInt();
    }

    // Debug
    static constexpr const char *MODULE_PREFIX = "ScaderRelays";
};
