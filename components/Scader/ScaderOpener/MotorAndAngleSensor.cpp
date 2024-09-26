/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Motor and Angle Sensor
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotorAndAngleSensor.h"
#include "DeviceManager.h"
// TODO REMOVE
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"
RaftBusDeviceDecodeState decodeState;

static const char *MODULE_PREFIX = "MotorAndAngleSensor";

// #define DEBUG_SENSOR_ANGLE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MotorAndAngleSensor::MotorAndAngleSensor()
{
    // Angle update semaphore
    _angleUpdateSemaphore = xSemaphoreCreateBinary();
}

MotorAndAngleSensor::~MotorAndAngleSensor()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::setup(DeviceManager* pDevMan, RaftJsonIF& config)
{
    // Store device manager
    _pDevMan = pDevMan;

    // Register for device data notifications from the angle sensor
    if (pDevMan)
        pDevMan->registerForDeviceData("I2CA_0x36@0", 
                [this](uint32_t deviceTypeIdx, std::vector<uint8_t> data, const void* pCallbackInfo) {

                    // Decode device data
                    poll_AS5600 deviceData;
                    DeviceTypeRecordDecodeFn pDecodeFn = deviceTypeRecords.getPollDecodeFn(deviceTypeIdx);
                    if (pDecodeFn)
                        pDecodeFn(data.data(), data.size(), &deviceData, sizeof(deviceData), 1, decodeState);

                    // Debug
                    LOG_I(MODULE_PREFIX, "setup deviceDataChangeCB devTypeIdx %d data bytes %d callbackInfo %p tims %d angle %.2f",
                            deviceTypeIdx, data.size(), pCallbackInfo, deviceData.timeMs, deviceData.angle);

                    // Take the angle semaphore and update the angle
                    if (_angleUpdateSemaphore)
                    {
                        if (xSemaphoreTake(_angleUpdateSemaphore, 10) == pdTRUE)
                        {
                            // Update the angle
                            _measuredDoorAngleDegs = deviceData.angle;

                            // Feed the speed averaging
                            _measuredDoorSpeedDegsPerSec.sample(deviceData.angle);

                            // Give back the semaphore
                            xSemaphoreGive(_angleUpdateSemaphore);
                        }
                    }
                },
                100
            );



    // Get motor on time after move (secs)
    float motorOnTimeAfterMoveSecs = config.getDouble("MotorOnTimeAfterMoveSecs", 0.0);

    // TODO implement
    // if (_pStepper)
    //     _pStepper->setMotorOnTimeAfterMoveSecs(motorOnTimeAfterMoveSecs);

    // Get motor current threshold
    float maxMotorCurrentAmps = config.getDouble("MaxMotorCurrentAmps", 0.1);

    // TODO implement
    // if (_pStepper)
    //     _pStepper->setMaxMotorCurrentAmps(0, maxMotorCurrentAmps);

    // TODO implement
    // // Set hysteresis for angle filter
    // _rotationSensor.setHysteresis(1);

    // Debug
    LOG_I(MODULE_PREFIX, "setup MaxMotorCurrent %.2fA MotorOnTimeAfterMoveSecs %.2fs", 
                maxMotorCurrentAmps, motorOnTimeAfterMoveSecs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::loop()
{
    // Debug
#ifdef DEBUG_SENSOR_ANGLE
    if (Raft::isTimeout(millis(), _debugLastPrintTimeMs, 1000))
    {
        LOG_I(MODULE_PREFIX, "service angle %.2fdegs avgSpeed %.2fdegs/sec", 
                    _measuredDoorAngleDegs, _measuredDoorSpeedDegsPerSec.getRatePerSec());
        _debugLastPrintTimeMs = millis();
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get measured angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::getMeasuredAngleDegs() const
{
    return _measuredDoorAngleDegs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get measured angular speed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::getMeasuredAngularSpeedDegsPerSec() const
{
    return _measuredDoorSpeedDegsPerSec.getRatePerSec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move motor to angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::moveToAngleDegs(float angleDegrees, float movementSpeedDegreesPerSec)
{
    // TODO - remove
    return;

    // if (!_pStepper)
    //     return;

    // // Get the current angle
    // float currentAngleDegrees = _rotationSensor.getAngleDegrees(false, false);

    // // Calculate the difference to required angle
    // float angleDiffDegrees = angleDegrees - currentAngleDegrees;

    // // Form command
    // // ,"clearQ":1
    // String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    // moveCmd.replace("__POS__", String(angleDiffDegrees));
    // moveCmd.replace("__SPEED__", String(movementSpeedDegreesPerSec == 0 ? _reqMotorSpeedDegsPerSec : movementSpeedDegreesPerSec));

    // // Send command
    // _pStepper->sendCmdJSON(moveCmd.c_str());

    // // Reset check on motor stopped
    // _lastMotorStoppedCheckTimeMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::stop()
{
    // TODO - remove
    return;

    // if (!_pStepper)
    //     return;
    // _pStepper->sendCmdJSON(R"({"cmd":"motion","stop":1,"clearQ":1})");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if motor active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isMotorActive() const
{
    // TODO - remove
    return false;

    // bool isValid = false;
    // return _pStepper && _pStepper->getNamedValue("b", isValid) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if angle is within tolerance of target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isNearTargetAngle(float targetAngleDegs, float posToleranceDegs, float negToleranceDegs) const
{
    // Calculate the difference to required angle
    float angleDiffDegrees = targetAngleDegs - _measuredDoorAngleDegs;

    // Check if within tolerance
    if (angleDiffDegrees > 0)
        return angleDiffDegrees < posToleranceDegs;
    else
        return angleDiffDegrees > negToleranceDegs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if motor has stopped for more than a given time (ms)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isStoppedForTimeMs(uint32_t timeMs, float expectedMotorSpeedDegsPerSec) const
{
    // Check motor speed < expected motor speed
    double motorSpeedDegsPerSec = _measuredDoorSpeedDegsPerSec.getRatePerSec();
    if (std::abs(motorSpeedDegsPerSec) < 
            (expectedMotorSpeedDegsPerSec == 0 ? _reqMotorSpeedDegsPerSec / 2 : expectedMotorSpeedDegsPerSec / 2))
    {
        // Check if stopped for more than a given time
        if (Raft::isTimeout(millis(), _lastMotorStoppedCheckTimeMs, timeMs))
        {
            LOG_I(MODULE_PREFIX, "isStoppedForTimeMs motor IS stopped for %dms (peedDegs/s meas %.2f expected %.2f reqd %.2f) lastMovingTime %d", 
                        timeMs, motorSpeedDegsPerSec, expectedMotorSpeedDegsPerSec, _reqMotorSpeedDegsPerSec, _lastMotorStoppedCheckTimeMs);
            return true;
        }
    }
    else
    {
        // Reset stopped time - casting away const as this is just debugging info
        const_cast<uint32_t&>(_lastMotorStoppedCheckTimeMs) = millis();
    }
    return false;
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // I2CBus element status callback
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void MotorAndAngleSensor::busElemStatusCB(RaftBus& bus, const std::vector<BusElemAddrAndStatus>& statusChanges)
// {
//     String statusStr;
//     int seqStartAddr = -1;
//     bool lastWasOnline = false;
//     int lastAddr = 0;
//     for (auto& statusChange : statusChanges)
//     {
//         if (seqStartAddr == -1)
//         {
//             seqStartAddr = statusChange.address;
//             lastWasOnline = statusChange.isChangeToOnline;
//         }
//         else if (lastWasOnline != statusChange.isChangeToOnline)
//         {
//             if (!statusStr.isEmpty())
//                 statusStr += ", ";
//             statusStr += "0x" + String(seqStartAddr, 16) + "..0x" + String(statusChange.address - 1, 16) + (lastWasOnline ? ":online" : ":offline");
//             seqStartAddr = statusChange.address;
//             lastWasOnline = statusChange.isChangeToOnline;
//         }
//         lastAddr = statusChange.address;
//     }
//     if (seqStartAddr != -1)
//     {
//         if (!statusStr.isEmpty())
//             statusStr += ", ";
//         statusStr += "0x" + String(seqStartAddr, 16) + "..0x" + String(lastAddr, 16) + (lastWasOnline ? ":online" : ":offline");
//     }
//     LOG_I(MODULE_PREFIX, "busElemStatusCB I2C addr %s", statusStr.c_str());
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // I2CBus operation status callback
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void MotorAndAngleSensor::busOperationStatusCB(RaftBus& bus, BusOperationStatus busOperationStatus)
// {
//     LOG_I(MODULE_PREFIX, "busOperationStatusCB I2C bus %s", RaftBus::busOperationStatusToString(busOperationStatus));
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate move speed degs per sec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs) const
{
    if (timeSecs == 0)
        timeSecs = 1;
    if (angleDegs == 0)
        angleDegs = 1;
    float speedDegsPerSec = angleDegs / timeSecs;
    LOG_I(MODULE_PREFIX, "calcMoveSpeed angleDegs %.2f timeSecs %.2f speedDegsPerSec %.2f", 
                angleDegs, timeSecs, speedDegsPerSec);
    return speedDegsPerSec;
}
