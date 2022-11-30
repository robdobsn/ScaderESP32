/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderRelays
//
// Rob Dobson 2013-2020
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SysModBase.h>
#include <driver/spi_master.h>

class APISourceInfo;

class ScaderRelays : public SysModBase
{
  public:
    static const int DEFAULT_MAX_RELAYS = 24;
    static const int RELAYS_PER_CHIP = 8;
    static const int SPI_MAX_CHIPS = DEFAULT_MAX_RELAYS/RELAYS_PER_CHIP;
    ScaderRelays(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

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

    // Settings
    uint32_t _maxRelays;

    // SPI control
    int _spiMosi;
    int _spiMiso;
    int _spiClk;
    int _spiChipSelects[SPI_MAX_CHIPS];

    // SPI device handles
    spi_device_handle_t _spiDeviceHandles[SPI_MAX_CHIPS];

    // On/Off Key
    int _onOffKey;

    // Name of relay panel
    String _relayPanelName;

    // Names of relays
    std::vector<String> _relayNames;

    // Current state of relays
    std::vector<bool> _relayStates;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Helpers
    bool setRelays();

    // Helper functions
    void deinit();
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowRelayStates();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    int parseIntList(String &str, int *pIntList, int maxInts);
};
