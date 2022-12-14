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
    static const int DEFAULT_MAX_ELEMS = 24;
    static const int ELEMS_PER_CHIP = 8;
    static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
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
    bool _isEnabled = false;
    bool _isInitialised = false;

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // SPI control
    int _spiMosi = -1;
    int _spiMiso = -1;
    int _spiClk = -1;
    int _spiChipSelects[SPI_MAX_CHIPS] = {};

    // SPI device handles
    spi_device_handle_t _spiDeviceHandles[SPI_MAX_CHIPS] = {};

    // On/Off Key
    int _onOffKey = -1;

    // Name set in Scader UI
    String _scaderFriendlyName;

    // Names of control elements
    std::vector<String> _elemNames;

    // Current state of elements
    std::vector<bool> _elemStates;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    int parseIntList(const String &str, int *pIntList, int maxInts);
};
