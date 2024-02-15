/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderElecMeters
//
// Rob Dobson 2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "RaftUtils.h"
#include "RaftJsonIF.h"
#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "ExpMovingAverage.h"
#include "driver/spi_master.h"

class APISourceInfo;

class ScaderElecMeters : public RaftSysMod
{
public:
    static const int DEFAULT_MAX_ELEMS = 16;
    static const int ELEMS_PER_CHIP = 8;
    static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
    ScaderElecMeters(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderElecMeters(pModuleName, sysConfig);
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

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // SPI control
    int _spiMosi = -1;
    int _spiMiso = -1;
    int _spiClk = -1;
    int _spiChipSelects[SPI_MAX_CHIPS] = {};

    // SPI device handles
    spi_device_handle_t _spiDeviceHandles[SPI_MAX_CHIPS] = {};

    // Names of control elements
    std::vector<String> _elemNames;

    // Data acquisition worker task
    volatile TaskHandle_t _dataAcqWorkerTaskStatic = nullptr;
    static const int DEFAULT_TASK_CORE = 1;
    static const int DEFAULT_TASK_PRIORITY = 1;
    static const int DEFAULT_TASK_STACK_SIZE_BYTES = 5000;

    // Data acquisition
    static const uint32_t DATA_ACQ_SIGNAL_FREQ_HZ = 50;
    static const uint32_t DATA_ACQ_SAMPLES_PER_CYCLE = 50;
    static const uint32_t DATA_ACQ_SAMPLES_PER_SECOND = DATA_ACQ_SIGNAL_FREQ_HZ * DATA_ACQ_SAMPLES_PER_CYCLE;
    static const uint32_t DATA_ACQ_SAMPLES_FOR_MEAN_LEVEL = DATA_ACQ_SAMPLES_PER_CYCLE * 10;

    // Data aggregation
    std::vector<ExpMovingAverage<1>> _dataAcqMeanLevels;

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Worker task (static version calls the other)
    static void dataAcqWorkerTaskStatic(void* pvParameters);
    void dataAcqWorkerTask();
};
