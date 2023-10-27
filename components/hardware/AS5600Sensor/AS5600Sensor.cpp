/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// AS5600Sensor.cpp
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AS5600Sensor.h"
#include "BusBase.h"
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <BusRequestInfo.h>
#include <BusRequestResult.h>

// Module prefix
static const char *MODULE_PREFIX = "AS5600Sensor";

// #define DEBUG_POLL_RESULT

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AS5600Sensor::AS5600Sensor()
{
}

AS5600Sensor::~AS5600Sensor()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AS5600Sensor::setup(ConfigBase& config, const char* pConfigPrefix, BusBase* pBus)
{
    // Store bus
    _pBus = pBus;

    // Get address
    _i2cAddr = config.getLong("i2cAddr", AS5600_DEFAULT_I2C_ADDR, pConfigPrefix);

    // Get poll rate
    _pollRatePerSec = config.getLong("pollsPerSec", AS5600_DEFAULT_POLL_RATE_PER_SEC, pConfigPrefix);

    // Check if rotation direction is reversed (default to reverse as AS5600 values increase CCW normally)
    _rotationDirectionReversed = config.getBool("reverse", true, pConfigPrefix);

    // Get hysteresis
    double hysteresis = config.getDouble("hysteresis", 150.0, pConfigPrefix);
    _angleFilter.setHysteresis(hysteresis);

    // Log
    LOG_I(MODULE_PREFIX, "setup i2cAddr 0x%x reversed %s pollRatePerSec %d hysteresis %0.1f", 
                (int)_i2cAddr,
                _rotationDirectionReversed ? "Y" : "N",
                (int)_pollRatePerSec, hysteresis);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AS5600Sensor::service()
{
    // Check if initialisation is done
    if (_isInitialised)
        return;

    // Command to get AS5600 data
    const HWElemReq AS5600GetDataCommand = {{AS5600_ROTATION_REG_NUMBER}, AS5600_BYTES_TO_READ_FOR_ROTATION, HWElemReq::UNNUM, "Rotation", 0};

    // Setup polling
    BusRequestInfo busRequestInfo("AS5600", _i2cAddr);
    busRequestInfo.set(BUS_REQ_TYPE_POLL, AS5600GetDataCommand, _pollRatePerSec, pollResultCallbackStatic, this);
    _pBus->addRequest(busRequestInfo);
    _isInitialised = true;

    LOG_I(MODULE_PREFIX, "service i2cAddr %x pollRatePerSec %d", (int)_i2cAddr, (int)_pollRatePerSec);

    // Send setup command
    // if ((_configSendTimeMs != 0) && (Raft::isTimeout(millis(), _configSendTimeMs, 100)))
    // {
        // // Command to get AS5600 data
        // const HWElemReq AS5600GetDataCommand = {{AS5600_ROTATION_REG_NUMBER}, AS5600_BYTES_TO_READ_FOR_ROTATION, HWElemReq::UNNUM, "Rotation", 0};

        // // Setup polling
        // BusRequestInfo busRequestInfo("AS5600", _i2cAddr);
        // busRequestInfo.set(BUS_REQ_TYPE_POLL, AS5600GetDataCommand, _pollRatePerSec, pollResultCallbackStatic, this);
        // _pBus->addRequest(busRequestInfo);
        // _isInitialised = true;
    // }
    // else
    // {
    //     uint16_t confRegContents = 3 << AS5600_CONF_HYSTERESIS_BIT_POS;
    //     const HWElemReq confRegContentsCommand = {{AS5600_CONF_REG_NUMBER, (uint8_t)(confRegContents & 0xff), uint8_t((confRegContents >> 8) & 0xff)}, 0, HWElemReq::UNNUM, "Setup", 0};
    //     BusRequestInfo busRequestInfo("AS5600", _i2cAddr);
    //     busRequestInfo.set(BUS_REQ_TYPE_STD, confRegContentsCommand, 0, 0, 0);
    //     _pBus->addRequest(busRequestInfo);
    //     _configSendTimeMs = millis();
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Poll Result Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Poll result callback
void AS5600Sensor::pollResultCallbackStatic(void *pCallbackData, BusRequestResult &reqResult)
{
    if (pCallbackData)
        ((AS5600Sensor *)pCallbackData)->pollResultCallback(reqResult);
}

// Poll result callback
void AS5600Sensor::pollResultCallback(BusRequestResult &reqResult)
{
    // Get received data
    uint8_t* pData = reqResult.getReadData();
    if (!pData)
        return;
    if (reqResult.getReadDataLen() == AS5600_BYTES_TO_READ_FOR_ROTATION)
    {
        // Extract raw sensor data
        int32_t sensorAngle = (((uint32_t)pData[0] << 8) | pData[1]) & 0x0fff;

        // Check for reverse rotation
        if (_rotationDirectionReversed)
            sensorAngle = AS5600_RAW_RANGE - sensorAngle;

        // Filter
        _angleFilter.sample(sensorAngle);

        // Add to sample collector
        if (_pSampleCollector)
            _pSampleCollector->addSample(sensorAngle);

#ifdef DEBUG_POLL_RESULT
        if (Raft::isTimeout(millis(), _debugLastShowTimeMs, 100))
        {
            String debugStr;
            Raft::getHexStrFromBytes(pData, reqResult.getReadDataLen(), debugStr);
            LOG_I(MODULE_PREFIX, "pollResultCallback angleDegrees %0.1f angleRadians %0.1f rawReading %d rawBytes %s", 
                    _angleFilter.getAverage(false, false) * AS5600_ANGLE_CONVERSION_FACTOR_DEGREES,
                    _angleFilter.getAverage(false, false) * AS5600_ANGLE_CONVERSION_FACTOR_RADIANS,
                    sensorAngle, 
                    debugStr.c_str());
            _debugLastShowTimeMs = millis();
        }
#endif
    }
}
