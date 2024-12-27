/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderElecMeters
//
// Rob Dobson 2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <time.h>
#include "Logger.h"
#include "RaftArduino.h"
#include "ScaderElecMeters.h"
#include "ScaderCommon.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"
#include "NetworkSystem.h"
#include "CommsChannelMsg.h"
#include "PlatformUtils.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// Debug
// #define DEBUG_IN_BATCHES_CHANNEL_NO 0
#define DEBUG_ELEC_METER_MUTABLE_DATA

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderElecMeters::ScaderElecMeters(const char *pModuleName, RaftJsonIF& sysConfig)
        : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName),
          _scaderModuleState("scaderKWh")
{
    // Initialize semaphore
    _dataAcqSemaphore = xSemaphoreCreateBinary();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::setup()
{
    // Common
    _scaderCommon.setup();

    // Max elems
    _maxElems = config.getLong("maxElems", DEFAULT_MAX_ELEMS);
    if (_maxElems > DEFAULT_MAX_ELEMS)
        _maxElems = DEFAULT_MAX_ELEMS;

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Configure GPIOs
    // The MOSI, MISO, CLK pins are configured as INPUT as they are driven by the SPI master
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("SPI_MOSI", ConfigPinMap::GPIO_INPUT, &_spiMosi),
        ConfigPinMap::PinDef("SPI_MISO", ConfigPinMap::GPIO_INPUT, &_spiMiso),
        ConfigPinMap::PinDef("SPI_CLK", ConfigPinMap::GPIO_INPUT, &_spiClk),
        ConfigPinMap::PinDef("SPI_CS1", ConfigPinMap::GPIO_OUTPUT, &_spiChipSelects[0], 1),
        ConfigPinMap::PinDef("SPI_CS2", ConfigPinMap::GPIO_OUTPUT, &_spiChipSelects[1], 1)
    };
    ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Check SPI config valid
    if ((_spiMosi < 0) || (_spiMiso < 0) || (_spiClk < 0) || (_spiChipSelects[0] < 0))
    {
        LOG_E(MODULE_PREFIX, "setup INVALID MOSI %d, MISO %d, CLK %d, CS1 %d, CS2 %d",
                _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1]);
        return;
    }

    // Configure SPI
    const spi_bus_config_t buscfg = {
        .mosi_io_num = _spiMosi,
        .miso_io_num = _spiMiso,
        .sclk_io_num = _spiClk,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0,
    };

    //Initialize the SPI bus
    esp_err_t espErr = spi_bus_initialize(SPI2_HOST, &buscfg, 1);
    if (espErr != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "setup SPI failed MOSI %d MISO %d CLK %d CS1 %d CS2 %d retc %d",
                _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], espErr);
        return;
    }

    // Initialise the SPI devices
    for (int i = 0; i < SPI_MAX_CHIPS; i++)
    {
        // Check if configured
        if (_spiChipSelects[i] < 0)
            continue;

        // From datasheet https://www.onsemi.com/pub/Collateral/NCV7240-D.PDF
        // And Wikipedia https://en.wikipedia.org/wiki/Serial_Peripheral_Interface
        // CPOL = 0, CPHA = 1
        spi_device_interface_config_t devCfg =
        {
            .command_bits = 0,
            .address_bits = 0,
            .dummy_bits = 0,
            .mode = 1,
            .clock_source = SPI_CLK_SRC_DEFAULT,
            .duty_cycle_pos = 128,
            .cs_ena_pretrans = 1,
            .cs_ena_posttrans = 0,
            .clock_speed_hz = 500000,
            .input_delay_ns = 0,
            .spics_io_num=_spiChipSelects[i],
            .flags = 0,
            .queue_size=3,
            .pre_cb = nullptr,
            .post_cb = nullptr
        };
        esp_err_t ret = spi_bus_add_device(SPI2_HOST, &devCfg, &_spiDeviceHandles[i]);
        if (ret != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setup add SPI device failed MOSI %d MISO %d CLK %d CS1 %d CS2 %d retc %d",
                    _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], ret);
            return;
        }
    }

    // Get default calibration factor
    float defaultADCToAmps = config.getDouble("calibADCToAmps", DEFAULT_ADC_TO_CURRENT_CALIBRATION_VAL);

    // Get the voltage lovel
    float mainsVoltageRMS = config.getDouble("mainsVoltage", DEFAULT_MAINS_RMS_VOLTAGE); 

    // Element names
    std::vector<String> elemInfos;
    if (configGetArrayElems("elems", elemInfos))
    {
        // Names array
        uint32_t numElems = elemInfos.size() > _maxElems ? _maxElems : elemInfos.size();
        _elemNames.resize(numElems);
        _ctCalibrationVals.resize(numElems);

        // Set names
        for (int i = 0; i < numElems; i++)
        {
            RaftJson elemInfo = elemInfos[i];
            _elemNames[i] = elemInfo.getString("name", ("CTClamp " + String(i+1)).c_str());
            _ctCalibrationVals[i] = elemInfo.getDouble("calibADCToAmps", defaultADCToAmps);
            if (_ctCalibrationVals[i] < 0.0001 || _ctCalibrationVals[i] > 1.0)
                _ctCalibrationVals[i] = defaultADCToAmps;
            LOG_I(MODULE_PREFIX, "CTClamp %d name %s calibrationADCToAmps %.4f", 
                    i+1, _elemNames[i].c_str(), _ctCalibrationVals[i]);
        }
    }

    // Max element index for ISR
    _isrElemIdxMax = _elemNames.size();

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->registerDataSource("Publish", _scaderCommon.getModuleName().c_str(), 
            [this](const char* messageName, CommsChannelMsg& msg) {
                String statusStr = getStatusJSON();
                msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
                return true;
            },
            [this](const char* messageName, std::vector<uint8_t>& stateHash) {
                return getStatusHash(stateHash);
            }
        );
    }

    // CT Processors
    _ctProcessors.resize(_elemNames.size());
    for (int i = 0; i < _elemNames.size(); i++)
    {
        // Get stored total kWh
        float totalKWh = _scaderModuleState.getDouble(("totalKWh[" + String(i) + "]").c_str(), 0);
        _ctProcessors[i].setup(_ctCalibrationVals[i], DATA_ACQ_SAMPLES_PER_SECOND, DATA_ACQ_SIGNAL_FREQ_HZ, mainsVoltageRMS, totalKWh);
    }

    // No need to save mutable data for a bit
    _mutableDataChangeLastMs = millis();

#ifdef DEBUG_IN_BATCHES_CHANNEL_NO
    _debugVals.resize(DATA_ACQ_SAMPLES_FOR_BATCH);
#endif

    BaseType_t retc = pdPASS;
    // Task settings
    UBaseType_t taskCore = config.getLong("taskCore", DEFAULT_TASK_CORE);
    BaseType_t taskPriority = config.getLong("taskPriority", DEFAULT_TASK_PRIORITY);
    int taskStackSize = config.getLong("taskStack", DEFAULT_TASK_STACK_SIZE_BYTES);

    // Start the worker task
    if (_dataAcqWorkerTaskStatic == nullptr)
    {
        retc = xTaskCreatePinnedToCore(
                    dataAcqWorkerTaskStatic,
                    "ElecTask",                                     // task name
                    taskStackSize,                                  // stack size of task
                    this,                                           // parameter passed to task on execute
                    taskPriority,                                   // priority
                    (TaskHandle_t*)&_dataAcqWorkerTaskStatic,       // task handle
                    taskCore);                                      // pin task to core N
    }

    // Start GPTimer for data acquisition
    if (retc == pdPASS)
    {
        // Start the timer
        esp_timer_create_args_t timerArgs = {
            .callback = dataAcqTimerCallbackStatic,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ElecTimer",
            .skip_unhandled_events = false
        };
        esp_timer_create(&timerArgs, &_dataAcqTimer);
        esp_timer_start_periodic(_dataAcqTimer, DATA_ACQ_SAMPLE_INTERVAL_US);
    }

    // HW Now initialised
    _isInitialised = retc == pdPASS;

    // Debug
    LOG_I(MODULE_PREFIX, "setup %s scaderUIName %s numCTClamps %d (max %d) MOSI %d MISO %d CLK %d CS1 %d CS2 %d taskRetc %d",
                _isInitialised ? "OK" : "FAILED",
                _scaderCommon.getUIName().c_str(),
                _elemNames.size(), _maxElems, 
                _spiMosi, _spiMiso, _spiClk, 
                _spiChipSelects[0], _spiChipSelects[1],
                retc);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loop (called frequently)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::loop()
{
    // Check init
    if (!_isInitialised)
        return;

    // Store total kwh when required
    if (Raft::isTimeout(millis(), _mutableDataChangeLastMs, MUTABLE_DATA_SAVE_CHECK_MS))
    {
        // Check if any of the ct processors require persistence of total kWh
        for (int i = 0; i < _elemNames.size(); i++)
        {
            if (_ctProcessors[i].isPersistenceReqd())
            {
                saveMutableData();
                _mutableDataChangeLastMs = millis();
                break;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("elecmeter", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderElecMeters::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "elecmeter/value/N - get elecmeter value, elecmeter/value/N/M - set elecmeter value");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints setup elec");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderElecMeters::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check init
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
    }

    // Get command
    String cmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    if (cmdStr.startsWith("value"))
    {
        // Get element number
        String elemNoStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
        if (elemNoStr.length() == 0)
        {
            LOG_E(MODULE_PREFIX, "apiControl invalid elemNo (1 based)");
            return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        }
        int elemNo = elemNoStr.toInt();
        if (elemNo < 1)
        {
            LOG_E(MODULE_PREFIX, "apiControl invalid elemNo (1 based)");
            return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        }

        // Get value
        String valueStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
        if (valueStr.length() > 0)
        {
            // Set value
            _ctProcessors[elemNo-1].setTotalKWh(valueStr.toFloat());
            return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
        }

        // Get value
        respStr = String(_ctProcessors[elemNo-1].getTotalKWh());
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }

    // Set result
    bool rslt = true;
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderElecMeters::getStatusJSON() const
{
    // Get status
    String elemStatus;

    // Get status for each element
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            elemStatus += ",";
        elemStatus += _ctProcessors[i].getStatusJSON();
    }

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (int i = 0; i < _elemNames.size(); i++)
    {
        std::vector<uint8_t> elemHash;
        _ctProcessors[i].getStatusHash(elemHash);
        stateHash.insert(stateHash.end(), elemHash.begin(), elemHash.end());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::dataAcqTimerCallbackStatic(void* pArg)
{
        // Set the semaphore to signal data acquisition can occur
        xSemaphoreGive(((ScaderElecMeters*)pArg)->_dataAcqSemaphore);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Worker task
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::dataAcqWorkerTask()
{
    // Acquisition loop
    while (true)
    {
        // Check init
        if (!_isInitialised)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        // Wait for the semaphore to be available
        if(xSemaphoreTake(_dataAcqSemaphore, portMAX_DELAY) == pdTRUE) 
        {
            // Get the time
            uint64_t timeNowUs = micros();

            // Acquire data
            uint16_t samples[_elemNames.size()];
            for (uint32_t elemIdx = 0; elemIdx < _elemNames.size(); elemIdx++)
            {
                // Acquire data
                samples[elemIdx] = acquireSample(elemIdx);

                // Process the sample
                _ctProcessors[elemIdx].newADCReading(samples[elemIdx], timeNowUs);
            }

#ifdef DEBUG_IN_BATCHES_CHANNEL_NO

            // Check if not processing a batch and not yet time to start a new one
            if ((_debugBatchSampleCounter == 0) && !Raft::isTimeout(millis(), _debugBatchStartTimeMs, DATA_ACQ_TIME_BETWEEN_BATCHES_MS))
                continue;

            // Check for first sample in batch and record time if so
            if (_debugBatchSampleCounter == 0)
                _debugBatchStartTimeMs = millis();

            // Get the state
            DebugCTProcessorVals debugVals;
            _ctProcessors[DEBUG_IN_BATCHES_CHANNEL_NO].getDebugInfo(debugVals);
            _debugVals[_debugBatchSampleCounter] = debugVals;

            // Increment the number of samples
            _debugBatchSampleCounter++;

            // Check if the batch is complete
            if (_debugBatchSampleCounter >= DATA_ACQ_SAMPLES_FOR_BATCH)
            {
                double sampleTimeMs = _debugBatchStartTimeMs;
                for (uint32_t sampleIdx = 0; sampleIdx < _debugBatchSampleCounter; sampleIdx++)
                {
                    sampleTimeMs += DATA_ACQ_SAMPLE_INTERVAL_US / 1000.0;
                    printf("T %.3f ADC %d max %.2f %u min %.2f %u Irms %.2f P %.0f TKWh %02f Zx %u ADCmean %.2f Offs %.2f",
                        sampleTimeMs,
                        _debugVals[sampleIdx].curADCSample,
                        _debugVals[sampleIdx].peakValPos, (int)_debugVals[sampleIdx].peakTimePos,
                        _debugVals[sampleIdx].peakValNeg, (int)_debugVals[sampleIdx].peakTimeNeg,
                        _debugVals[sampleIdx].rmsCurrentAmps, 
                        _debugVals[sampleIdx].rmsPowerW,
                        _debugVals[sampleIdx].totalKWh,
                        (int)_debugVals[sampleIdx].lastZeroCrossingTimeUs,
                        _debugVals[sampleIdx].meanADCValue, _debugVals[sampleIdx].prevACADCSample);
                    printf("\n");
                }

                // Clear the number of samples
                _debugBatchSampleCounter = 0;
            }

#endif
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Acquire one sample
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t ScaderElecMeters::acquireSample(uint32_t elemIdx)
{
    // Check init
    if (!_isInitialised)
        return 0;

    static const uint32_t NUM_BITS_TO_READ_WRITE = 24;
    // Setup tx buffer & rx buffers - 3 bytes 
    uint8_t txBuf[NUM_BITS_TO_READ_WRITE / 8] = {
        uint8_t(0x06 | ((elemIdx & 0x04) << 1)), 
        uint8_t((elemIdx & 0x03) << 6), 
        0x00};
    uint8_t rxBuf[NUM_BITS_TO_READ_WRITE / 8] = {0, 0, 0};

    // SPI transaction
    spi_transaction_t spiTransaction = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = NUM_BITS_TO_READ_WRITE /* bits */,
        .rxlength = NUM_BITS_TO_READ_WRITE /* bits */,
        .user = nullptr,
        .tx_buffer = txBuf,
        .rx_buffer = rxBuf,
    };

    // Chip select
    int chipIdx = elemIdx / ELEMS_PER_CHIP;

    // Send the data
    spi_device_acquire_bus(_spiDeviceHandles[chipIdx], portMAX_DELAY);
    spi_device_transmit(_spiDeviceHandles[chipIdx], &spiTransaction);
    spi_device_release_bus(_spiDeviceHandles[chipIdx]);

    // Extract value
    uint32_t val = ((rxBuf[1] & 0x0f) << 8) | rxBuf[2];
    return val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write mutable data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderElecMeters::saveMutableData()
{
    // Save total kWh for each element
    String jsonConfig = "\"totalKWh\":[";
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            jsonConfig += ",";
        jsonConfig += String(_ctProcessors[i].getTotalKWh());
    }
    jsonConfig += "]";

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";

#ifdef DEBUG_ELEC_METER_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
#endif

    _scaderModuleState.setJsonDoc(jsonConfig.c_str());

    // Set persistence done
    for (int i = 0; i < _elemNames.size(); i++)
        _ctProcessors[i].setPersistenceDone();
}