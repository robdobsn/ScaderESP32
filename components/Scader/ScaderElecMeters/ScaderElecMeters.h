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
#include "CTProcessor.h"
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
    virtual String getStatusJSON() const override final;
    
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

    // Calibration values for Current Transformer input scaling
    static constexpr float DEFAULT_ADC_TO_CURRENT_CALIBRATION_VAL = 0.089;
    std::vector<float> _ctCalibrationVals;
    static constexpr float DEFAULT_MAINS_RMS_VOLTAGE = 236.0;

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

    // CTProcessors
    std::vector<CTProcessor<uint16_t>> _ctProcessors;

    // Data acquisition semaphore
    SemaphoreHandle_t _dataAcqSemaphore = nullptr;

    // Data acq timer
    esp_timer_handle_t _dataAcqTimer = nullptr;

    // Sample queue
    static const uint32_t DATA_ACQ_SAMPLE_QUEUE_SIZE = 250;
    QueueHandle_t _dataAcqSampleQueue = nullptr;

    // Current element index for ISR
    volatile uint32_t _isrElemIdxCur = 0;
    volatile uint32_t _isrElemIdxMax = 0;

    // Module state
    RaftJsonNVS _scaderModuleState;
    uint32_t _mutableDataChangeLastMs = 0;
    static const uint32_t MUTABLE_DATA_SAVE_CHECK_MS = 1000;
    void saveMutableData();

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
    static void dataAcqTimerCallbackStatic(void* pArg);

    // Data acquisition
    void acquireSamples(uint32_t numSamples, uint64_t betweenChSamplesUs, uint32_t numChannels, std::vector<uint16_t>& samples);
    uint16_t acquireSample(uint32_t elemIdx);

    // Debug vals
    std::vector<DebugCTProcessorVals> _debugVals;

    // Debug raw samples in batches 
    uint32_t _debugBatchSampleCounter = 0;
    uint32_t _debugBatchStartTimeMs = 0;

    // Debug
    static constexpr const char *MODULE_PREFIX = "ScaderElecMeters";
};
