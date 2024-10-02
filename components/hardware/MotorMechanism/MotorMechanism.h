/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Motor and Angle Sensor
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Logger.h"
#include "RaftArduino.h"
#include "MotorControl.h"
#include "BusSerial.h"
#include "BusI2C.h"
#include "MovingRate.h"
#include "DeviceManager.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @note The MotorMechanism class handles the motor, angle sensor and force sensor to provide absolute angle
// selection and motor disablement when force threshold is exceeded. See README for more details.

class MotorMechanism
{
public:
    // Constructor and destructor
    MotorMechanism();
    virtual ~MotorMechanism();

    // Setup
    void setup(DeviceManager* pDevMan, RaftJsonIF& config);

    // Service
    void loop();

    // Motor move speed
    void setMotorSpeedFromDegreesAndSecs(float angleDegs, float timeSecs)
    {
        _reqMotorSpeedDegsPerSec = calcMoveSpeedDegsPerSec(angleDegs, timeSecs);
        LOG_I(MODULE_PREFIX, "setMotorSpeedFromDegreesAndSecs angleDegs %f timeSecs %f speedDegsPerSec %f",
              angleDegs, timeSecs, _reqMotorSpeedDegsPerSec);
    }
    float getMotorSpeedDegsPerSec() { return _reqMotorSpeedDegsPerSec; }

    // Set motor max current
    void setMaxMotorCurrentAmps(float maxMotorCurrentAmps);

    // Set motor on time after move
    void setMotorOnTimeAfterMoveSecs(float motorOnTimeAfterMoveSecs);

    // Motor force offset and threshold
    void setForceOffsetAndThreshold(float forceOffsetN, float forceThresholdN)
    {
        _forceOffsetN = forceOffsetN;
        _forceThresholdN = forceThresholdN;
        LOG_I(MODULE_PREFIX, "setForceOffsetAndThreshold forceOffsetN %.2f forceThresholdN %.2f",
              forceOffsetN, forceThresholdN);
    }
    float getForceOffsetN() const { return _forceOffsetN; }
    float getForceThresholdN() const { return _forceThresholdN; }

    // Move to angle in degrees (pass 0 as speed to use default speed)
    void moveToAngleDegs(float angleDegrees, float movementSpeedDegreesPerSec = 0);

    // Stop
    void stop();

    // Is motor active
    bool isMotorActive() const;

    // Get measured angle
    float getMeasuredAngleDegs() const;

    // Get measured angular speed
    float getMeasuredAngularSpeedDegsPerSec() const;

    // Check if angle is within tolerance of target
    bool isNearTargetAngle(float targetAngleDegs, float posToleranceDegs, float negToleranceDegs) const;

    // Check if motor has stopped for more than a given time (ms)
    bool isStoppedForTimeMs(uint32_t timeMs, float expectedMotorSpeedDegsPerSec = 0) const;

    // Get measured force
    float getMeasuredForceN() const
    {
        return _rawForceN - _forceOffsetN;
    }

    // Get raw force
    float getRawForceN() const
    {
        return _rawForceN;
    }

private:
    // Device manager
    DeviceManager* _pDevMan = nullptr;

    // Semaphore for angle updates
    SemaphoreHandle_t _angleUpdateSemaphore = nullptr;

    // Requested motor speed degrees per second
    float _reqMotorSpeedDegsPerSec = 5;

    // Time of last motor stopped check
    uint32_t _lastMotorStoppedCheckTimeMs = 0;

    // Measured door angle and speed degrees per second (protected by semaphore)
    float _measuredDoorAngleDegs = 0;
    MovingRate<5, float, float> _measuredDoorSpeedDegsPerSec;

    // Raw force (newtons)
    float _rawForceN = 0;

    // Force offset and threshold (newtons)
    float _forceOffsetN = 0;
    float _forceThresholdN = 5;

    // Decode state
    RaftBusDeviceDecodeState _decodeStateAs5600;
    RaftBusDeviceDecodeState _decodeStateHX711;

    // Calculate move speed degs per sec
    float calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs) const;

    // Get motor device
    RaftDevice* getMotorDevice() const
    {
        // Get motor device
        if (!_pDevMan)
            return nullptr;
        RaftDevice* pMotor = _pDevMan->getDevice("Motor");
        if (!pMotor)
            return nullptr;
        return pMotor;
    }

    // Debug
    static constexpr const char *MODULE_PREFIX = "MotorMechanism";
    uint32_t _debugLastPrintTimeMs = 0;
    uint32_t _debugCount = 0;
};