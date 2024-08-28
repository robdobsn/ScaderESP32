/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MT6701 Sensor
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "AngleMovingAverage.h"
#include "SampleCollector.h"

class RaftBus;
class BusRequestResult;

class MT6701Sensor
{
public:
    MT6701Sensor();
    virtual ~MT6701Sensor();

    // Setup
    void setup(RaftJsonIF& config, RaftBus* pBus);

    // Service
    void loop();

    // Get angle data
    float getAngleRadians(bool withHysteresis, bool clamped)
    {
        // Get data
        int32_t filteredData = _angleFilter.getAverage(withHysteresis, clamped);
        return filteredData * MT6701_ANGLE_CONVERSION_FACTOR_RADIANS;
    }

    // Set sample collector
    void setSampleCollector(SampleCollector<int32_t>* pSampleCollector)
    {
        _pSampleCollector = pSampleCollector;
    }

private:
    // MT6701 Address and Registers
    static const uint32_t MT6701_DEFAULT_I2C_ADDR = 0x06;
    static const uint32_t MT6701_ROTATION_REG_NUMBER = 0x03;
    static const uint32_t MT6701_BYTES_TO_READ_FOR_ROTATION = 2;
    
    // Default poll rate
    static const uint32_t MT6701_DEFAULT_POLL_RATE_PER_SEC = 1000;

    // Raw range and angle conversion factors
    static constexpr int32_t MT6701_RAW_RANGE = 16384;
    static constexpr float MT6701_ANGLE_CONVERSION_FACTOR_DEGREES = 360.0 / MT6701_RAW_RANGE;
    static constexpr float MT6701_ANGLE_CONVERSION_FACTOR_RADIANS = 2.0 * 3.14159265358979323846 / MT6701_RAW_RANGE;

    // Rotation direction reversed
    bool _rotationDirectionReversed = false;

    // I2C Bus
    RaftBus* _pBus = nullptr;

    // Numerical filter
    AngleMovingAverage<1, MT6701_RAW_RANGE> _angleFilter;

    // Sample collector
    SampleCollector<int32_t>* _pSampleCollector = nullptr;

    // Debug
    uint32_t _debugLastShowTimeMs = 0;

    // Callbacks
    static void pollResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult);
    void pollResultCallback(BusRequestResult& reqResult);    
};
