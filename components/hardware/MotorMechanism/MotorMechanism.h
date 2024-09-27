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
#include "AS5600Sensor.h"
#include "MovingRate.h"
#include "DeviceManager.h"

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
    }
    float getMotorSpeedDegsPerSec() { return _reqMotorSpeedDegsPerSec; }

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

    // Decode state
    RaftBusDeviceDecodeState _decodeState;

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
    uint32_t _debugLastPrintTimeMs = 0;
    uint32_t _debugCount = 0;
};