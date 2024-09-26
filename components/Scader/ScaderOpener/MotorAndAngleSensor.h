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

class DeviceManager;

class MotorAndAngleSensor
{
public:
    // Constructor and destructor
    MotorAndAngleSensor();
    virtual ~MotorAndAngleSensor();

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
    // // Stepper motor
    // MotorControl* _pStepper = nullptr;
    // BusSerial* _pBusSerial = nullptr;

    // // Magnetic rotation sensor
    // AS5600Sensor _rotationSensor;
    // BusI2C _busI2C;

    // Requested motor speed degrees per second
    float _reqMotorSpeedDegsPerSec = 5;

    // Time of last motor stopped check
    uint32_t _lastMotorStoppedCheckTimeMs = 0;

    // Measured door speed degrees per second
    MovingRate<20, float, float> _measuredDoorSpeedDegsPerSec;

    // TODO - remove

    // // I2C bus element status callback
    // void busElemStatusCB(RaftBus& bus, const std::vector<BusElemAddrAndStatus>& statusChanges);

    // // I2C bus operation status callback
    // void busOperationStatusCB(RaftBus& bus, BusOperationStatus busOperationStatus);

    // Calculate move speed degs per sec
    float calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs) const;

    // Debug
    uint32_t _debugLastPrintTimeMs = 0;
    uint32_t _debugCount = 0;
};