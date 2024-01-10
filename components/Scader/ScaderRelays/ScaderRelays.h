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
#include "SysModBase.h"
#include "ScaderCommon.h"
#include "DebounceButton.h"
#include "driver/spi_master.h"

class APISourceInfo;

class ScaderRelays : public SysModBase
{
  public:
    static const int DEFAULT_MAX_ELEMS = 24;
    static const int ELEMS_PER_CHIP = 8;
    static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
    ScaderRelays(const char *pModuleName, RaftJsonIF& sysConfig);

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

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
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

    // Mains sync detection
    int _mainsSyncPin = -1;
    bool _enableMainsSync = false;

    // Names of control elements
    std::vector<String> _elemNames;

    // Current state of elements
    std::vector<bool> _elemStates;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Pulse counter functionality
    bool _pulseCounterEnabled = false;
    int _pulseCounterPin = -1;
    DebounceButton _pulseCounterButton;
    void pulseCounterCallback(bool val, uint32_t msSinceLastChange, uint16_t repeatCount);
    uint32_t _pulseCount = 0;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Mains sync ISR
    static void IRAM_ATTR mainsSyncISRStatic(void *pArg)
    {
        // if (pArg)
        //     ((ScaderRelays *)pArg)->mainsSyncISR();
    }
    // void IRAM_ATTR mainsSyncISR();

    // Relay states, etc
    RaftJsonNVS _scaderModuleState;

    // TODO
    // Debug count of ISR
    volatile uint32_t _isrCount = 0;
    uint32_t _debugServiceLastMs = 0;
};
