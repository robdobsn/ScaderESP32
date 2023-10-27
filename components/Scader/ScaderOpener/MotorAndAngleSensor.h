/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Motor and Angle Sensor
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <RaftArduino.h>
#include <MotorControl.h>
#include <BusSerial.h>
#include <BusI2C.h>
#include <AS5600Sensor.h>
#include <MovingRate.h>

class MotorAndAngleSensor
{
public:
    // Constructor and destructor
    MotorAndAngleSensor();
    virtual ~MotorAndAngleSensor();

    // Setup
    void setup(ConfigBase& config, String configPrefix);

    // Service
    void service();

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
    bool isMotorActive();

    // Get measured angle
    float getMeasuredAngleDegs();

    // Get measured angular speed
    float getMeasuredAngularSpeedDegsPerSec();

    // Check if angle is within tolerance of target
    bool isNearTargetAngle(float targetAngleDegs, float posToleranceDegs, float negToleranceDegs);

    // Check if motor has stopped for more than a given time (ms)
    bool isStoppedForTimeMs(uint32_t timeMs, float expectedMotorSpeedDegsPerSec = 0);

private:
    // Stepper motor
    MotorControl* _pStepper = nullptr;
    BusSerial* _pBusSerial = nullptr;

    // Magnetic rotation sensor
    AS5600Sensor _rotationSensor;
    BusI2C _busI2C;

    // Requested motor speed degrees per second
    float _reqMotorSpeedDegsPerSec = 5;

    // Time of last motor stopped check
    uint32_t _lastMotorStoppedCheckTimeMs = 0;

    // Measured door speed degrees per second
    MovingRate<20, float, float> _measuredDoorSpeedDegsPerSec;

    // I2C bus element status callback
    void busElemStatusCB(BusBase& bus, const std::vector<BusElemAddrAndStatus>& statusChanges);

    // I2C bus operation status callback
    void busOperationStatusCB(BusBase& bus, BusOperationStatus busOperationStatus);

    // Calculate move speed degs per sec
    float calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs);

    // Debug
    uint32_t _debugLastPrintTimeMs = 0;
    uint32_t _debugCount = 0;
};