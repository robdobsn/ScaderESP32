/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Motor Mechanism
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotorMechanism.h"
#include "DeviceManager.h"
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"

#define WARN_FORCE_THRESHOLD_EXCEEDED
// #define DEBUG_SENSOR_ANGLE
// #define DEBUG_ANGLE_DEVICE_CALLBACK
// #define DEBUG_FORCE_DEVICE_CALLBACK

// Uncomment to use stepwise movement
// #define USE_STEPWISE_MOVEMENT_STEPS_PER_DEGREE 12

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Constructor
MotorMechanism::MotorMechanism()
{
    // Angle update semaphore
    _angleUpdateSemaphore = xSemaphoreCreateMutex();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Destructor
MotorMechanism::~MotorMechanism()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Setup
// @param pDevMan Device manager
// @param config Configuration
void MotorMechanism::setup(DeviceManager* pDevMan, RaftJsonIF& config)
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
                        pDecodeFn(data.data(), data.size(), &deviceData, sizeof(deviceData), 1, _decodeStateAs5600);

                    // Take the angle semaphore and update the angle
                    if (_angleUpdateSemaphore && (xSemaphoreTake(_angleUpdateSemaphore, 10) == pdTRUE))
                    {

                        // Update the angle
                        _measuredDoorAngleDegs = deviceData.angle;

                        // Feed the speed averaging
                        _measuredDoorSpeedDegsPerSec.sample(deviceData.angle);

                        // Give back the semaphore
                        xSemaphoreGive(_angleUpdateSemaphore);
                    }

                    // Debug
#ifdef DEBUG_ANGLE_DEVICE_CALLBACK
                    LOG_I(MODULE_PREFIX, "deviceDataChangeCB devTypeIdx %d data bytes %d callbackInfo %p tims %d angle %.2f",
                            deviceTypeIdx, data.size(), pCallbackInfo, deviceData.timeMs, deviceData.angle);
#endif

                },
                50
            );

    // Register for device data notifications from the force sensor
    if (pDevMan)
        pDevMan->registerForDeviceData("HX711", 
                [this](uint32_t deviceTypeIdx, std::vector<uint8_t> data, const void* pCallbackInfo) {

                    // Decode device data
                    poll_HX711 deviceData;
                    DeviceTypeRecordDecodeFn pDecodeFn = deviceTypeRecords.getPollDecodeFn(deviceTypeIdx);
                    if (pDecodeFn)
                        pDecodeFn(data.data(), data.size(), &deviceData, sizeof(deviceData), 1, _decodeStateHX711);

                    // Store raw force value - this allows getMeasuredForceN to return the force relative to the offset
                    _rawForceN = deviceData.force;

                    // Check if the abs value of the measured force is greater than the threshold, if so stop the motor
                    if ((std::abs(getMeasuredForceN()) > _forceThresholdN) && isMotorActive())
                    {
                        stop();

#ifdef WARN_FORCE_THRESHOLD_EXCEEDED
                        LOG_W(MODULE_PREFIX, "STOPPING Force threshold exceeded %.2fN", deviceData.force);
#endif
                    }
                    // Debug
#ifdef DEBUG_FORCE_DEVICE_CALLBACK
                    LOG_I(MODULE_PREFIX, "deviceDataChangeCB devTypeIdx %d data bytes %d callbackInfo %p tims %d measured force %.2f raw force %.2f",
                            deviceTypeIdx, data.size(), pCallbackInfo, deviceData.timeMs, getMeasuredForceN(), _rawForceN);
#endif
                },
                50
            );



    // TODO implement
    // // Set hysteresis for angle filter
    // _rotationSensor.setHysteresis(1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Loop
void MotorMechanism::loop()
{
    // Debug
#ifdef DEBUG_SENSOR_ANGLE
    if (Raft::isTimeout(millis(), _debugLastPrintTimeMs, 1000))
    {
        LOG_I(MODULE_PREFIX, "loop angle %.1fdegs avgSpeed %.2fdegs/sec %s", 
                    getMeasuredAngleDegs(), getMeasuredAngularSpeedDegsPerSec(),
                    _pDevMan ? _pDevMan->getDebugJSON().c_str() : "");
        _debugLastPrintTimeMs = millis();
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Set motor max current
// @param maxMotorCurrentAmps Max motor current in amps
void MotorMechanism::setMaxMotorCurrentAmps(float maxMotorCurrentAmps)
{
    RaftDevice* pMotor = getMotorDevice();
    if (pMotor)
        pMotor->sendCmdJSON((R"({"cmd":"maxCurrent","maxCurrentA":)" + String(maxMotorCurrentAmps) + R"(,"axisIdx":0})").c_str());
    
    // Debug
    LOG_I(MODULE_PREFIX, "setMaxMotorCurrentAmps %.2fA", maxMotorCurrentAmps);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Set motor on time after move
// @param motorOnTimeAfterMoveSecs Motor on time after move in seconds
void MotorMechanism::setMotorOnTimeAfterMoveSecs(float motorOnTimeAfterMoveSecs)
{
    RaftDevice* pMotor = getMotorDevice();
    if (pMotor)
        pMotor->sendCmdJSON((R"({"cmd":"offAfter","offAfterS":)" + String(motorOnTimeAfterMoveSecs) + "}").c_str());
    
    // Debug
    LOG_I(MODULE_PREFIX, "setMotorOnTimeAfterMoveSecs %.2fs", motorOnTimeAfterMoveSecs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Get measured angle in degrees
// @return Measured angle in degrees
float MotorMechanism::getMeasuredAngleDegs() const
{
    // Take the angle semaphore and get the angle
    if (_angleUpdateSemaphore && (xSemaphoreTake(_angleUpdateSemaphore, 10) == pdTRUE))
    {
        float measuredAngleDegs = _measuredDoorAngleDegs;
        xSemaphoreGive(_angleUpdateSemaphore);
        return measuredAngleDegs;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Get measured angular speed in degrees per second
// @return Measured angular speed in degrees per second
float MotorMechanism::getMeasuredAngularSpeedDegsPerSec() const
{
    // Take the angle semaphore and get the rate
    if (_angleUpdateSemaphore && (xSemaphoreTake(_angleUpdateSemaphore, 10) == pdTRUE))
    {
        float rate = _measuredDoorSpeedDegsPerSec.getRatePerSec();
        xSemaphoreGive(_angleUpdateSemaphore);
        return rate;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Move to angle in degrees
// @param angleDegrees Angle in degrees
// @param movementSpeedDegreesPerSec Movement speed in degrees per second
void MotorMechanism::moveToAngleDegs(float angleDegrees, float movementSpeedDegreesPerSec)
{
    // Get motor device
    RaftDevice* pMotor = getMotorDevice();
    if (!pMotor)
        return;

    // Get the current angle
    float currentAngleDegrees = getMeasuredAngleDegs();

    // Calculate the difference to required angle
    float angleDiffDegrees = angleDegrees - currentAngleDegrees;

    // Form command
    float reqDegsPerSec = movementSpeedDegreesPerSec == 0 ? _reqMotorSpeedDegsPerSec : movementSpeedDegreesPerSec;
#ifdef USE_STEPWISE_MOVEMENT_STEPS_PER_DEGREE
    String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"ramped":0,"steps":1,"rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(angleDiffDegrees * USE_STEPWISE_MOVEMENT_STEPS_PER_DEGREE));
    moveCmd.replace("__SPEED__", String(reqDegsPerSec * USE_STEPWISE_MOVEMENT_STEPS_PER_DEGREE));
    // Debug
    LOG_I(MODULE_PREFIX, "moveToAngleDegs %.2f current %.2fdegs diff %.2fdegs (steps %.2f) speed %.2fd/s (%.2f steps/s)", 
                angleDegrees, currentAngleDegrees, 
                angleDiffDegrees, angleDiffDegrees * STEPS_PER_DEGREE, 
                reqDegsPerSec, reqDegsPerSec * STEPS_PER_DEGREE);
#else
    String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(angleDiffDegrees));
    moveCmd.replace("__SPEED__", String(reqDegsPerSec));
    // Debug
    LOG_I(MODULE_PREFIX, "moveToAngleDegs %.2f current %.2fdegs diff %.2fdegs speed %.2fd/s", 
                angleDegrees, currentAngleDegrees, 
                angleDiffDegrees, reqDegsPerSec);
#endif


    // Send command
    pMotor->sendCmdJSON(moveCmd.c_str());

    // Reset check on motor stopped
    _lastMotorStoppedCheckTimeMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Stop
void MotorMechanism::stop()
{
    // Get motor device
    RaftDevice* pMotor = getMotorDevice();
    if (!pMotor)
        return;
    pMotor->sendCmdJSON(R"({"cmd":"motion","stop":1,"clearQ":1,"en":0})");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Check if motor is active
// @return true if motor is active
bool MotorMechanism::isMotorActive() const
{
    // Get motor device
    RaftDevice* pMotor = getMotorDevice();
    if (!pMotor)
        return false;
    bool isValid = false;
    return pMotor && pMotor->getNamedValue("b", isValid) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Check if angle is within tolerance of target
// @param targetAngleDegs Target angle in degrees
// @param posToleranceDegs Positive tolerance in degrees
// @param negToleranceDegs Negative tolerance in degrees
// @return true if angle is within tolerance of target
bool MotorMechanism::isNearTargetAngle(float targetAngleDegs, float posToleranceDegs, float negToleranceDegs) const
{
    // Calculate the difference to required angle
    float angleDiffDegrees = targetAngleDegs - getMeasuredAngleDegs();

    // Check if within tolerance
    if (angleDiffDegrees > 0)
        return angleDiffDegrees < posToleranceDegs;
    else
        return angleDiffDegrees > negToleranceDegs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Check if motor has stopped for more than a given time (ms)
// @param timeMs Time in milliseconds
// @param expectedMotorSpeedDegsPerSec Expected motor speed in degrees per second
// @return true if motor has stopped for more than a given time
bool MotorMechanism::isStoppedForTimeMs(uint32_t timeMs, float expectedMotorSpeedDegsPerSec) const
{
    // Check motor speed < expected motor speed
    double motorSpeedDegsPerSec = getMeasuredAngularSpeedDegsPerSec();
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @brief Calculate move speed degs per sec
// @param angleDegs Angle in degrees
// @param timeSecs Time in seconds
// @return Speed in degrees per second
float MotorMechanism::calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs) const
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
