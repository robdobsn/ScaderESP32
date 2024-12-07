////////////////////////////////////////////////////////////////////////////////
//
// DeviceMMWave.h
//
// MMWave device read using UART protocol
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"
#include <vector>
#include "sdkconfig.h"
#include "esp_idf_version.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// #define DEBUG_DEVICE_READING

class RestAPIEndpointManager;
class CommsCoreIF;
class APISourceInfo;

class DeviceMMWave : public RaftDevice
{
public:
    /// @brief Constructor
    /// @param pClassName device class name
    /// @param pDevConfigJson device configuration JSON
    DeviceMMWave(const char* pClassName, const char *pDevConfigJson)
    : RaftDevice(pClassName, pDevConfigJson)
    {
    }

    /// @brief Destructor
    virtual ~DeviceMMWave()
    {
    }

    /// @brief Create function for device factory
    /// @param pClassName device class name
    /// @param pDevConfigJson device configuration JSON
    /// @return RaftDevice* pointer to the created device
    static RaftDevice* create(const char* pClassName, const char* pDevConfigJson)
    {
        return new DeviceMMWave(pClassName, pDevConfigJson);
    }

    // @brief Setup the device
    virtual void setup()
    {
        // Get uart config
        _uartRx = deviceConfig.getInt("uartRx", -1);
        _uartTx = deviceConfig.getInt("uartTx", -1);
        _baudrate = deviceConfig.getInt("baudrate", 115200);
        _uartNum = deviceConfig.getInt("uartNum", 1);
        if (_uartRx < 0)
        {
            LOG_E(MODULE_PREFIX, "setup: UART Rx pin not specified");
            return;
        }

        // Configure UART
        const uart_config_t uart_config = {
                .baud_rate = _baudrate,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .rx_flow_ctrl_thresh = 122,
                .source_clk = UART_SCLK_DEFAULT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
                .flags = {
                    .backup_before_sleep = 0,
                },
#endif
        };
        esp_err_t err = uart_param_config((uart_port_t)_uartNum, &uart_config);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setup FAILED uartNum %d can't initialize uart, err %d", _uartNum, err);
            return;
        }

        // Set UART pins
        err = uart_set_pin((uart_port_t)_uartNum, _uartTx, _uartRx, -1, -1);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setup FAILED uartNum %d can't set pins Rx %d Tx %d, err %d", _uartNum, _uartRx, _uartTx, err);
            return;
        }

        // Get the decode function
        DeviceTypeRecord deviceTypeRecord;
        deviceTypeRecords.getDeviceInfo(getPublishDeviceType(), deviceTypeRecord, _deviceTypeIdx);
        _pDecodeFn = deviceTypeRecord.pollResultDecodeFn;

        // Set initialised
        _isInitialised = true;

        // Debug
        LOG_I(MODULE_PREFIX, "setup: Rx %d Tx %d baud %d uartNum %d", _uartRx, _uartTx, _baudrate, _uartNum);
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
#ifdef DEBUG_DEVICE_READING
        if (Raft::isTimeout(millis(), _debugLastMs, 1000))
        {
            // Convert value to range
            float rangeM = getRangeInMeters();

            // Debug
            LOG_I(MODULE_PREFIX, "loop range %.2fm", rangeM);

            // Update last debug time
            _debugLastMs = millis();
        }
#endif
    }

    /// @brief Get the device type record for this device so that it can be added to the device type records
    /// @param devTypeRec (out) Device type record
    /// @return true if the device has a device type record
    virtual bool getDeviceTypeRecord(DeviceTypeRecordDynamic& devTypeRec) const override final
    {
        // Keep track of length of poll data size
        uint16_t pollDataSizeBytes = 0;

        // Generate the JSON for the range
        String devInfoJson = R"~({"n":"range","t":">H","u":"m","r":[0,65536],"d":100,"f":".0f","o":"uint16"})~";
        pollDataSizeBytes += 2;

        // Device info JSON
        devInfoJson = R"~({"name":"MMWave","desc":"mmWave Sensor","manu":"Waveshare","type":"MMWave","resp":{"b":)~" + 
                    String(pollDataSizeBytes*2) +
                    R"~(,"a":[)~" + 
                    devInfoJson + 
                    "]}}";

        // Set the device type record
        devTypeRec = DeviceTypeRecordDynamic(
            getPublishDeviceType().c_str(),
            "",
            "",
            "",
            "",
            pollDataSizeBytes,
            devInfoJson.c_str(),
            nullptr
        );

        // Return
        return true;
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
        String json = "\"rangeM\":" + String(getRangeInMeters());
        return includeBraces ? "{" + json + "}" : json;
    }

    // @brief Get the range in metres
    // @return Range in metres
    float getRangeInMeters() const
    {
        return _rangeM;
    }

private:

    // Initialised flag
    bool _isInitialised = false;

    // Serial config
    int _uartRx = -1;
    int _uartTx = -1;
    int _baudrate = 115200;
    int _uartNum = 1;

    // Device data
    float _rangeM = 0;

    // Last value
    bool _valueValid = false;

    // Decode function and state
    DeviceTypeRecordDecodeFn _pDecodeFn = nullptr;
    RaftBusDeviceDecodeState _decodeState;

    // Read operation
    uint32_t _readLastMs = 0;
    String _readLine;

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
        // Check initialised
        if (!_isInitialised)
            return;

        // Read from UART
        static const uint32_t MAX_READ_LINE_LENGTH = 100;
        static const uint32_t MAX_UART_READ_BYTES = 100;
        uint8_t rxData[MAX_UART_READ_BYTES];
        int numRead = uart_read_bytes((uart_port_t)_uartNum, rxData, MAX_UART_READ_BYTES, 0);

        // Add to received string
        for (int i = 0; i < numRead; i++)
        {
            if (rxData[i] == '\n')
            {
                // Process line
                LOG_I(MODULE_PREFIX, "read: uart read line %s", _readLine.c_str());
                _readLine = "";
                continue;
            }
            else if (rxData[i] == '\r')
                continue;
            else if (_readLine.length() < MAX_READ_LINE_LENGTH)
                _readLine += (char)rxData[i];
            else
                _readLine = "";
        }

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
        uint32_t value = (uint32_t)(getRangeInMeters() * 1000);
        data.push_back(value >> 24);
        data.push_back((value >> 16) & 0xFF);
        data.push_back((value >> 8) & 0xFF);
        data.push_back(value & 0xFF);
        data.push_back(_valueValid ? 1 : 0);
    }

    // Debug
    static constexpr const char *MODULE_PREFIX = "DeviceMMWave";
};
