/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Moisture Sensors
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "Logger.h"
#include "RaftArduino.h"
#include "RaftUtils.h"
#include "RdI2C.h"
#include "SimpleMovingAverage.h"

class MoistureSensors
{
public:
    MoistureSensors();
    virtual ~MoistureSensors();
    void setup(const RaftJsonIF& config);
    void service();
    bool isBusy();
    uint32_t getCount() {
        return NUM_MOISTURE_SENSORS;
    }
    uint8_t getMoisturePercentageHash(uint8_t sensorIndex)
    {
        uint16_t adcAvg = _adcAvgValues[sensorIndex].cur();
        return (adcAvg % 256) | (adcAvg / 256) << 8;
    }
    String getStatusJSON(bool includeBrackets);

    // Get moisture percentage
    uint8_t getMoisturePercentage(uint8_t sensorIndex);

private:
    // I2C bus
    RdI2CIF* _pI2CBus = nullptr;

    // I2C ADC address
    uint32_t _adcI2CAddr = 0x48;

    // ADS1015 Consts
    static const uint8_t ADS1015_CONVERSION_REGISTER = 0x00;
    static const uint8_t ADS1015_CONFIG_REGISTER = 0x01;
    static const uint16_t ADS1015_CONFIG_OS_SINGLE = 0x8000;
    static const uint16_t ADS1015_CONFIG_MUX_SINGLE_ENDED = 0x4000;
    static const uint16_t ADS1015_CONFIG_MUX_BIT_POS = 12;
    static const uint16_t ADS1015_CONFIG_MUX_CHAN_MASK = 0x3000;
    static const uint16_t ADS1015_CONFIG_PGA_6_144V = 0x0000;
    static const uint16_t ADS1015_CONFIG_PGA_4_096V = 0x0200;
    static const uint16_t ADS1015_CONFIG_PGA_2_048V = 0x0400;
    static const uint16_t ADS1015_CONFIG_PGA_1_024V = 0x0600;
    static const uint16_t ADS1015_CONFIG_PGA_0_512V = 0x0800;
    static const uint16_t ADS1015_CONFIG_PGA_0_256V = 0x0A00;
    static const uint16_t ADS1015_CONFIG_MODE_CONTINUOUS = 0x0000;
    static const uint16_t ADS1015_CONFIG_MODE_SINGLE = 0x0100;
    static const uint16_t ADS1015_CONFIG_MODE_SHUTDOWN = 0x0000;
    static const uint16_t ADS1015_CONFIG_MODE_STARTUP = 0x0100;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_128SPS = 0x0000;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_250SPS = 0x0020;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_490SPS = 0x0040;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_920SPS = 0x0060;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_1600SPS = 0x0080;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_2400SPS = 0x00A0;
    static const uint16_t ADS1015_CONFIG_DATA_RATE_3300SPS = 0x00C0;
    static const uint16_t ADS1015_CONFIG_COMP_MODE_TRADITIONAL = 0x0000;
    static const uint16_t ADS1015_CONFIG_COMP_MODE_WINDOW = 0x0010;
    static const uint16_t ADS1015_CONFIG_COMP_MODE_WINDOW_LATCH = 0x0020;
    static const uint16_t ADS1015_CONFIG_COMP_MODE_WINDOW_AUTO_CLEAR = 0x0030;
    static const uint16_t ADS1015_CONFIG_COMP_POL_ACTIVE_LOW = 0x0000;
    static const uint16_t ADS1015_CONFIG_COMP_POL_ACTIVE_HIGH = 0x0008;
    static const uint16_t ADS1015_CONFIG_COMP_LATENCY_2 = 0x0000;
    static const uint16_t ADS1015_CONFIG_COMP_LATENCY_4 = 0x0004;
    static const uint16_t ADS1015_CONFIG_COMP_LATENCY_8 = 0x0008;
    static const uint16_t ADS1015_CONFIG_COMP_LATENCY_16 = 0x000C;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_DISABLE = 0x0000;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_1 = 0x0001;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_2 = 0x0002;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_4 = 0x0003;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_6 = 0x0004;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_8 = 0x0005;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_10 = 0x0006;
    static const uint16_t ADS1015_CONFIG_COMP_QUE_12 = 0x0007;

    // ADC read state
    uint32_t _adcCurChannel = 0;
    bool _adcConvInProgress = false;
    uint32_t _adcConvLastMs = 0;

    // Moisture averages
    static const uint32_t NUM_MOISTURE_SENSORS = 4;
    SimpleMovingAverage<10> _adcAvgValues[NUM_MOISTURE_SENSORS];

    // ADC Min/Max
    static const uint32_t ADC_MAX_VALUE = 3000;
    static const uint32_t ADC_MIN_VALUE = 100;

    // Debug
    uint32_t _debugLastDisplayMs = 0;
};
