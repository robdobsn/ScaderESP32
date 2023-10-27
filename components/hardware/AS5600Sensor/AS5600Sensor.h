/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// AS5600 Sensor
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <AngleMovingAverage.h>
#include <SampleCollector.h>

class BusBase;
class ConfigBase;
class BusRequestResult;

class AS5600Sensor
{
public:
    AS5600Sensor();
    virtual ~AS5600Sensor();

    // Setup
    void setup(ConfigBase& config, const char* pConfigPrefix, BusBase* pBus);

    // Service
    void service();

    // Set hysteresis for angle measurement
    void setHysteresis(float hysteresis)
    {
        _angleFilter.setHysteresis(hysteresis);
    }
    
    // Get angle radians
    float getAngleRadians(bool withHysteresis, bool clamped)
    {
        // Get data
        int32_t filteredData = _angleFilter.getAverage(withHysteresis, clamped);
        return filteredData * AS5600_ANGLE_CONVERSION_FACTOR_RADIANS;
    }

    // Get angle degrees
    float getAngleDegrees(bool withHysteresis, bool clamped)
    {
        // Get data
        int32_t filteredData = _angleFilter.getAverage(withHysteresis, clamped);
        return filteredData * AS5600_ANGLE_CONVERSION_FACTOR_DEGREES;
    }

    // Set sample collector
    void setSampleCollector(SampleCollector<int32_t>* pSampleCollector)
    {
        _pSampleCollector = pSampleCollector;
    }

    // Get address
    uint32_t getI2CAddr()
    {
        return _i2cAddr;
    }

private:
    // AS5600 Address and Registers
    static const uint32_t AS5600_DEFAULT_I2C_ADDR = 0x36;
    static const uint32_t AS5600_ROTATION_REG_NUMBER = 0x0e;
    static const uint32_t AS5600_STATUS_REG_NUMBER = 0x0b;
    static const uint32_t AS5600_CONF_REG_NUMBER = 0x07;
    static const uint32_t AS5600_BYTES_TO_READ_FOR_ROTATION = 2;
    static const uint32_t AS5600_CONF_HYSTERESIS_BIT_POS = 2;
    
    // Default poll rate
    static const uint32_t AS5600_DEFAULT_POLL_RATE_PER_SEC = 1000;

    // Raw range and angle conversion factors
    static constexpr int32_t AS5600_RAW_RANGE = 4096;
    static constexpr float AS5600_ANGLE_CONVERSION_FACTOR_DEGREES = 360.0 / AS5600_RAW_RANGE;
    static constexpr float AS5600_ANGLE_CONVERSION_FACTOR_RADIANS = 2.0 * 3.14159265358979323846 / AS5600_RAW_RANGE;

    // I2C address
    uint32_t _i2cAddr = AS5600_DEFAULT_I2C_ADDR;

    // Poll rate / sec
    uint32_t _pollRatePerSec = AS5600_DEFAULT_POLL_RATE_PER_SEC;

    // Rotation direction reversed
    bool _rotationDirectionReversed = false;

    // Is init
    bool _isInitialised = false;

    // Config send time
    uint32_t _configSendTimeMs = 0;

    // I2C Bus
    BusBase* _pBus = nullptr;

    // Numerical filter
    AngleMovingAverage<1, AS5600_RAW_RANGE> _angleFilter;

    // Sample collector
    SampleCollector<int32_t>* _pSampleCollector = nullptr;

    // Debug
    uint32_t _debugLastShowTimeMs = 0;

    // Callbacks
    static void pollResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult);
    void pollResultCallback(BusRequestResult& reqResult);    
};
