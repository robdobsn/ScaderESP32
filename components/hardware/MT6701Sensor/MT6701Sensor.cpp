/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MT6701Sensor.cpp
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MT6701Sensor.h"
#include "BusBase.h"
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <BusRequestInfo.h>
#include <BusRequestResult.h>

// Module prefix
static const char *MODULE_PREFIX = "MT6701Sensor";

// #define DEBUG_POLL_RESULT

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MT6701Sensor::MT6701Sensor()
{
}

MT6701Sensor::~MT6701Sensor()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MT6701Sensor::setup(ConfigBase& config, const char* pConfigPrefix, BusBase* pBus)
{
    // Store bus
    _pBus = pBus;

    // Get address
    uint32_t i2cAddr = config.getLong("i2cAddr", MT6701_DEFAULT_I2C_ADDR, pConfigPrefix);

    // Get poll rate
    uint32_t pollRatePerSec = config.getLong("pollsPerSec", MT6701_DEFAULT_POLL_RATE_PER_SEC, pConfigPrefix);

    // Check if rotation direction is reversed (default to reverse as MT6701 values increase CCW normally)
    _rotationDirectionReversed = config.getBool("reverse", true, pConfigPrefix);

    // Get hysteresis
    double hysteresis = config.getDouble("hysteresis", 150.0, pConfigPrefix);
    _angleFilter.setHysteresis(hysteresis);

    // Command to get MT6701 data
    const HWElemReq mt6701GetDataCommand = {{MT6701_ROTATION_REG_NUMBER}, MT6701_BYTES_TO_READ_FOR_ROTATION, HWElemReq::UNNUM, "Rotation", 0};

    // Setup polling
    BusRequestInfo busRequestInfo("MT6701", i2cAddr);
    busRequestInfo.set(BUS_REQ_TYPE_POLL, mt6701GetDataCommand, pollRatePerSec, pollResultCallbackStatic, this);
    _pBus->addRequest(busRequestInfo);

    // Log
    LOG_I(MODULE_PREFIX, "setup i2cAddr %x pollRatePerSec %d hyseteresis %0.1f", (int)i2cAddr, (int)pollRatePerSec, hysteresis);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MT6701Sensor::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Poll Result Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Poll result callback
void MT6701Sensor::pollResultCallbackStatic(void *pCallbackData, BusRequestResult &reqResult)
{
    if (pCallbackData)
        ((MT6701Sensor *)pCallbackData)->pollResultCallback(reqResult);
}

// Poll result callback
void MT6701Sensor::pollResultCallback(BusRequestResult &reqResult)
{
    // Get received data
    uint8_t* pData = reqResult.getReadData();
    if (!pData)
        return;
    if (reqResult.getReadDataLen() == MT6701_BYTES_TO_READ_FOR_ROTATION)
    {
        // Extract raw sensor data
        int32_t rawSensorData = ((uint32_t)pData[0] << 6) | pData[1];

        // Check for reverse rotation
        if (_rotationDirectionReversed)
            rawSensorData = MT6701_RAW_RANGE - rawSensorData;

        // Filter
        _angleFilter.sample(rawSensorData);

        // Add to sample collector
        if (_pSampleCollector)
            _pSampleCollector->addSample(rawSensorData);

#ifdef DEBUG_POLL_RESULT
        if (Raft::isTimeout(millis(), _debugLastShowTimeMs, 100))
        {
            String debugStr;
            Raft::getHexStrFromBytes(pData, reqResult.getReadDataLen(), debugStr);
            LOG_I(MODULE_PREFIX, "pollResultCallback angleDegrees %0.1f angleRadians %0.1f rawReading %d rawBytes %s", 
                    _angleFilter.getAverage(false, false) * MT6701_ANGLE_CONVERSION_FACTOR_DEGREES,
                    _angleFilter.getAverage(false, false) * MT6701_ANGLE_CONVERSION_FACTOR_RADIANS,
                    rawSensorData, 
                    debugStr.c_str());
            _debugLastShowTimeMs = millis();
        }
#endif
    }
}
