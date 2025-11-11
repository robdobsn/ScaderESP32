////////////////////////////////////////////////////////////////////////////////
//
// DeviceHX711.h
//
// Force measurement device read using SPI protocol
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "SimpleMovingAverage.h"
#include <vector>
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"

// #define DEBUG_DEVICE_FORCE_READING
// #define DEBUG_DEVICE_FORCE_RAW`

class RestAPIEndpointManager;
class CommsCoreIF;
class APISourceInfo;

class DeviceHX711 : public RaftDevice
{
public:
    /// @brief Constructor
    /// @param pClassName device class name
    /// @param pDevConfigJson device configuration JSON
    DeviceHX711(const char* pClassName, const char *pDevConfigJson)
    : RaftDevice(pClassName, pDevConfigJson)
    {
    }

    /// @brief Destructor
    virtual ~DeviceHX711()
    {
        // Deinitialise
        if (_spiDeviceHandle)
        {
            spi_bus_remove_device(_spiDeviceHandle);
            _spiDeviceHandle = nullptr;
        }
        spi_bus_free(SPI_HOST);
    }

    /// @brief Create function for device factory
    /// @param pClassName device class name
    /// @param pDevConfigJson device configuration JSON
    /// @return RaftDevice* pointer to the created device
    static RaftDevice* create(const char* pClassName, const char* pDevConfigJson)
    {
        return new DeviceHX711(pClassName, pDevConfigJson);
    }

    // @brief Setup the device
    virtual void setup()
    {
        // Get clock pin
        _clockPin = deviceConfig.getInt("clkPin", -1);
        _dataPin = deviceConfig.getInt("dataPin", -1);
        if (_clockPin < 0 || _dataPin < 0)
        {
            LOG_E(MODULE_PREFIX, "setup: clock or data pin not specified");
            return;
        }

        // Configure SPI
        const spi_bus_config_t buscfg = {
            .mosi_io_num = -1,
            .miso_io_num = _dataPin,
            .sclk_io_num = _clockPin,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .data4_io_num = -1,
            .data5_io_num = -1,
            .data6_io_num = -1,
            .data7_io_num = -1,
            .data_io_default_level = false,
            .max_transfer_sz = 0,
            .flags = 0,
            .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
            .intr_flags = 0,
        };

        //Initialize the SPI bus
        esp_err_t espErr = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_DISABLED);
        if (espErr != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setup: spi_bus_initialize failed %d clockPin %d dataPin %d", espErr, _clockPin, _dataPin);
            return;
        }

        // Set up the SPI device
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
            .clock_speed_hz = 1000000,
            .input_delay_ns = 0,
            .sample_point = SPI_SAMPLING_POINT_PHASE_0,
            .spics_io_num=-1,
            .flags = 0,
            .queue_size=3,
            .pre_cb = nullptr,
            .post_cb = nullptr
        };
        esp_err_t ret = spi_bus_add_device(SPI_HOST, &devCfg, &_spiDeviceHandle);
        if (ret != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "spi_bus_add_device failed");
            return;
        }

        // Get the decode function
        DeviceTypeRecord deviceTypeRecord;
        deviceTypeRecords.getDeviceInfo(getPublishDeviceType(), deviceTypeRecord, _deviceTypeIdx);
        _pDecodeFn = deviceTypeRecord.pollResultDecodeFn;

        // Set initialised
        _isInitialised = true;

        // Debug
        LOG_I(MODULE_PREFIX, "setup: clock %d data %d", _clockPin, _dataPin);
    }

    // @brief Main loop for the device (called frequently)
    virtual void loop() override final
    {
        // Check initialised
        if (!_isInitialised)
            return;

        // Check if ready to read
        if (!Raft::isTimeout(millis(), _readLastMs, 20))
            return;

        // Read
        read();

        // Check if ready for callback
        if (_deviceDataChangeCB && Raft::isTimeout(millis(), _deviceDataChangeCBLastTime, _deviceDataChangeCBMinTime))
        {
            // Buffer
            std::vector<uint8_t> data;
            formDeviceDataResponse(data);

            // Callback
            _deviceDataChangeCB(_deviceTypeIdx, data, _deviceDataChangeCBInfo);

            // Update last callback time
            _deviceDataChangeCBLastTime = millis();
        }

        // Debug
#ifdef DEBUG_DEVICE_FORCE_READING
        if (Raft::isTimeout(millis(), _debugLastMs, 1000))
        {
            // Convert value to newtons
            float newtons = getForceInNewtons();

            // Debug
            LOG_I(MODULE_PREFIX, "loop force %.2fN", newtons);

            // Update last debug time
            _debugLastMs = millis();
        }
#endif
    }

    // @brief Get time of last device status update
    // @param includeElemOnlineStatusChanges Include element online status changes in the status update time
    // @param includePollDataUpdates Include poll data updates in the status update time
    // @return Time of last device status update in milliseconds
    virtual uint32_t getDeviceInfoTimestampMs(bool includeElemOnlineStatusChanges, bool includePollDataUpdates) const override final
    {
        if (includePollDataUpdates)
            return _readLastMs;
        return 0;
    }

    // @brief Get the device status as JSON
    // @return JSON string
    virtual String getStatusJSON() const override final
    {
        // Buffer
        std::vector<uint8_t> data;
        formDeviceDataResponse(data);

        // Return JSON
        return "{\"0\":{\"x\":\"" + Raft::getHexStr(data.data(), data.size()) + "\",\"_t\":\"" + getPublishDeviceType() + "\"}}";
    }

    /// @brief Get device debug info JSON
    /// @param includeBraces Include braces in the JSON
    /// @return JSON string
    virtual String getDebugJSON(bool includeBraces) const override final
    {
        // Return JSON
        String json = "\"force\":" + String(getForceInNewtons());
        return includeBraces ? "{" + json + "}" : json;
    }

    // @brief Get the force in newtons
    // @return Force in newtons
    float getForceInNewtons() const
    {
        // Buffer
        std::vector<uint8_t> data;
        formDeviceDataResponse(data);

        // Decode device data
        poll_HX711 deviceData;
        if (_pDecodeFn)
            _pDecodeFn(data.data(), data.size(), &deviceData, sizeof(deviceData), 1, const_cast<RaftBusDeviceDecodeState&>(_decodeState));
        return deviceData.force;
    }

    /// @brief Register for device data notifications
    /// @param dataChangeCB Callback for data change
    /// @param minTimeBetweenReportsMs Minimum time between reports (ms)
    /// @param pCallbackInfo Callback info (passed to the callback)
    virtual void registerForDeviceData(RaftDeviceDataChangeCB dataChangeCB, uint32_t minTimeBetweenReportsMs, const void* pCallbackInfo) override final
    {
        _deviceDataChangeCB = dataChangeCB;
        _deviceDataChangeCBMinTime = minTimeBetweenReportsMs;
        _deviceDataChangeCBInfo = pCallbackInfo;
    }

private:

    // Initialised flag
    bool _isInitialised = false;

    // SPI host
    static const spi_host_device_t SPI_HOST = SPI2_HOST;

    // Averaging loops
    static const int NUM_AVG_LOOPS = 10;

    // Clock and data pins
    int _clockPin = -1;
    int _dataPin = -1;

    // SPI device handle
    spi_device_handle_t _spiDeviceHandle = nullptr;

    // Last value
    bool _valueValid = false;

    // Data filter
    SimpleMovingAverage<NUM_AVG_LOOPS, int32_t, int64_t> _filter;

    // Decode function and state
    DeviceTypeRecordDecodeFn _pDecodeFn = nullptr;
    RaftBusDeviceDecodeState _decodeState;

    // Read rate
    uint32_t _readLastMs = 0;

    // Device data change callback
    RaftDeviceDataChangeCB _deviceDataChangeCB = nullptr;
    uint32_t _deviceDataChangeCBMinTime = 0;
    const void* _deviceDataChangeCBInfo = nullptr;
    uint32_t _deviceDataChangeCBLastTime = 0;
    uint32_t _deviceTypeIdx = 0;

    // Debug
    uint32_t _debugLastMs = 0;

    // Read
    void read()
    {
        if (!_spiDeviceHandle)
            return;

        // Check if data ready (DOUT high)
        if (digitalRead(_dataPin))
            return;

        // SPI transaction
        uint32_t rxVal = 0;
        spi_transaction_t spiTransaction = {
            .flags = 0,
            .cmd = 0,
            .addr = 0,
            .length = 25 /* bits */,
            .rxlength = 0 /* bits */,
            .override_freq_hz = 0,
            .user = NULL,
            .tx_buffer = NULL,
            .rx_buffer = &rxVal,
        };

        // Send the data
        esp_err_t err = spi_device_acquire_bus(_spiDeviceHandle, portMAX_DELAY);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "read: spi_device_acquire_bus failed");
            return;
        }
        err = spi_device_transmit(_spiDeviceHandle, &spiTransaction);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "read: spi_device_transmit failed");
            spi_device_release_bus(_spiDeviceHandle);
            return;
        }
        spi_device_release_bus(_spiDeviceHandle);

        // Reverse the bits
        uint32_t data = SPI_SWAP_DATA_RX(*(uint32_t*)&rxVal, 24);

        // Sign extend
        int32_t adcReading = data;
        if (adcReading & 0x800000)
            adcReading |= 0xFF000000;

        // Filter
        _filter.sample(adcReading);

        // Update validity
        _valueValid = true;

        // Update last read time
        _readLastMs = millis();

        // Debug
#ifdef DEBUG_DEVICE_FORCE_RAW
        LOG_I(MODULE_PREFIX, "read: adc %d raw value %08x", adcReading, rxVal);
#endif
    }

    // Form device data response
    void formDeviceDataResponse(std::vector<uint8_t>& data) const
    {
        // Get average and store in format expected by conversion function
        uint16_t timeVal = (uint16_t)(_readLastMs & 0xFFFF);
        data.push_back((timeVal >> 8) & 0xFF);
        data.push_back(timeVal & 0xFF);
        uint32_t value = _filter.getAverage();
        data.push_back(value >> 24);
        data.push_back((value >> 16) & 0xFF);
        data.push_back((value >> 8) & 0xFF);
        data.push_back(value & 0xFF);
        data.push_back(_valueValid ? 1 : 0);
    }

    // Debug
    static constexpr const char *MODULE_PREFIX = "DeviceHX711";
};
