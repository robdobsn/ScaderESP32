////////////////////////////////////////////////////////////////////////////////
//
// DeviceHX711.h
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "SimpleMovingAverage.h"
#include <vector>
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"

#define DEBUG_WEIGHT_DEVICE_READING
// #define DEBUG_WEIGHT_RAW_VALUE

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

        // Setup pins
        pinMode(_clockPin, OUTPUT);
        digitalWrite(_clockPin, LOW);
        pinMode(_dataPin, INPUT);

        // Get the decode function
        DeviceTypeRecord deviceTypeRecord;
        deviceTypeRecords.getDeviceInfo(getPublishDeviceType(), deviceTypeRecord);
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

        // Debug
#ifdef DEBUG_WEIGHT_DEVICE_READING
        if (Raft::isTimeout(millis(), _debugLastMs, 1000))
        {
            // Convert value to grams
            float grams = getWeightInGrams();

            // Debug
            LOG_I(MODULE_PREFIX, "deviceDataChangeCB weight %.2f", grams);

            // Update last debug time
            _debugLastMs = millis();
        }
#endif
    }

    // @brief Get time of last device status update
    // @param includeElemOnlineStatusChanges Include element online status changes in the status update time
    // @param includePollDataUpdates Include poll data updates in the status update time
    // @return Time of last device status update in milliseconds
    virtual uint32_t getLastStatusUpdateMs(bool includeElemOnlineStatusChanges, bool includePollDataUpdates) const override final
    {
        if (includePollDataUpdates)
            return _readLastMs;
        return 0;
    }

    // @brief Get the device status as JSON
    // @return JSON string
    virtual String getStatusJSON() const override final
    {
        // JSON strings
        String hexDataStr;
        char tmpTimeStr[10];
        uint16_t timeVal = (uint16_t)(_readLastMs & 0xFFFF);
        sprintf(tmpTimeStr, "%04X", timeVal);
        hexDataStr += tmpTimeStr;

        // Add sensor reading
        char tmpStr[10];
        sprintf(tmpStr, "%04lX%02X", (int32_t)_filter.getAverage(), _valueValid ? 1 : 0);
        hexDataStr += tmpStr;

        // Return JSON
        return "{\"00\":{\"x\":\"" + hexDataStr + "\",\"_t\":\"" + getPublishDeviceType() + "\"}}";
    }

    // @brief Get the weight in grams
    // @return Weight in grams
    float getWeightInGrams()
    {
        // Buffer
        std::vector<uint8_t> data;

        // Get average and store in format expected by conversion function
        uint16_t timeVal = (uint16_t)(_readLastMs & 0xFFFF);
        data.push_back((timeVal >> 8) & 0xFF);
        data.push_back(timeVal & 0xFF);
        int32_t value = _filter.getAverage();
        data.push_back(0);
        data.push_back((value >> 16) & 0xFF);
        data.push_back((value >> 8) & 0xFF);
        data.push_back(value & 0xFF);
        data.push_back(_valueValid ? 1 : 0);

        // Decode device data
        poll_HX711 deviceData;
        if (_pDecodeFn)
            _pDecodeFn(data.data(), data.size(), &deviceData, sizeof(deviceData), 1, _decodeState);
        return deviceData.weight;
    }

private:

    // Initialised flag
    bool _isInitialised = false;

    // Averaging loops
    static const int NUM_AVG_LOOPS = 10;

    // Clock and data pins
    int _clockPin = -1;
    int _dataPin = -1;

    // Last value
    bool _valueValid = false;

    // Data filter
    SimpleMovingAverage<NUM_AVG_LOOPS> _filter;

    // Decode function and state
    DeviceTypeRecordDecodeFn _pDecodeFn = nullptr;
    RaftBusDeviceDecodeState _decodeState;

    // Read rate
    uint32_t _readLastMs = 0;

    // Debug
    uint32_t _debugLastMs = 0;

    // Read
    void read()
    {
        // Disable interrupts
        portDISABLE_INTERRUPTS();

        // Read
        uint32_t value = 0;
        for (int i = 0; i < 24; i++)
        {
            digitalWrite(_clockPin, HIGH);
            delayMicroseconds(2);
            value = value << 1;
            if (digitalRead(_dataPin))
                value++;
            digitalWrite(_clockPin, LOW);
            delayMicroseconds(2);
        }

        // Enable interrupts
        portENABLE_INTERRUPTS();

        // Filter
        _filter.sample(value);

        // Update validity
        _valueValid = true;

        // Update last read time
        _readLastMs = millis();

        // Debug
#ifdef DEBUG_WEIGHT_RAW_VALUE
        LOG_I(MODULE_PREFIX, "read: raw value %08x", value);
#endif
    }

    // Debug
    static constexpr const char *MODULE_PREFIX = "DeviceHX711";
};
