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
#include "SimpleMovingAverage.h"
#include "driver/spi_master.h"

#define DEBUG_ELEC_METER_ANALYZE_TIMING_INTERVAL

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

    // Names of control elements - the size of this array is the number of elements
    std::vector<String> _elemNames;

    // Data acquisition worker task
    volatile TaskHandle_t _dataAcqWorkerTaskStatic = nullptr;
    static const int DEFAULT_TASK_CORE = 1;
    static const int DEFAULT_TASK_PRIORITY = 1;
    static const int DEFAULT_TASK_STACK_SIZE_BYTES = 5000;

    // Data acquisition constants
    static const uint32_t DATA_ACQ_SIGNAL_FREQ_HZ = 50;
    static const uint32_t DATA_ACQ_SAMPLES_PER_CYCLE = 50;
    static const uint32_t DATA_ACQ_SAMPLES_PER_SECOND = DATA_ACQ_SIGNAL_FREQ_HZ * DATA_ACQ_SAMPLES_PER_CYCLE;
    static const uint32_t DATA_ACQ_SAMPLE_INTERVAL_US = 1000000 / DATA_ACQ_SAMPLES_PER_SECOND;
    static const uint32_t DATA_ACQ_SAMPLES_FOR_MEAN_LEVEL = DATA_ACQ_SAMPLES_PER_CYCLE * 10;
    static const uint32_t DATA_ACQ_SAMPLES_FOR_BATCH = DATA_ACQ_SAMPLES_PER_CYCLE * 2;
    static const uint32_t DATA_ACQ_TIME_BETWEEN_BATCHES_MS = 5000;

    // ADC data
    std::vector<std::vector<uint16_t>> _dataAcqSamples;

    // Mean levels
    std::vector<ExpMovingAverage<8>> _dataAcqMeanLevels;

    // Data acq timer
    SemaphoreHandle_t _dataAcqSemaphore = nullptr;
    esp_timer_handle_t _dataAcqTimer = nullptr;

    // Data acq progress
    uint32_t _dataAcqNumSamples = 0;
    uint32_t _dataAcqBatchStartTimeMs = 0;

    // Debug timing
#ifdef DEBUG_ELEC_METER_ANALYZE_TIMING_INTERVAL
    SimpleMovingAverage<100, int16_t, int32_t> _dataAcqSampleIntervals;
    uint64_t _debugLastDataAcqSampleTimeUs = 0;
    uint32_t _debugSampleTimeUsCount = 0;
#endif

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Worker task (static version calls the other)
    static void dataAcqWorkerTaskStatic(void* pvParameters)
    {
        ((ScaderElecMeters*)pvParameters)->dataAcqWorkerTask();
    }
    void dataAcqWorkerTask();

    // Timer for data acquisition
    static void dataAcqTimerCallbackStatic(void* pArg) 
    {
        // Set the semaphore to signal data acquisition can occur
        xSemaphoreGive(((ScaderElecMeters*)pArg)->_dataAcqSemaphore);
    }

    // Data acquisition
    void acquireSamples(uint32_t numSamples, uint64_t betweenChSamplesUs, uint32_t numChannels, std::vector<uint16_t>& samples);
    uint16_t acquireSample(uint32_t elemIdx);
};
