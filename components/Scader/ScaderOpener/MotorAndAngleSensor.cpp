/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Motor and Angle Sensor
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotorAndAngleSensor.h"

static const char *MODULE_PREFIX = "MotorAndAngleSensor";

// #define DEBUG_SENSOR_ANGLE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MotorAndAngleSensor::MotorAndAngleSensor() :
        _busI2C(std::bind(&MotorAndAngleSensor::busElemStatusCB, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&MotorAndAngleSensor::busOperationStatusCB, this, std::placeholders::_1, std::placeholders::_2))
{
}

MotorAndAngleSensor::~MotorAndAngleSensor()
{
    // Remove stepper and bus serial
    if (_pStepper)
        delete _pStepper;
    _pStepper = nullptr;
    if (_pBusSerial)
        delete _pBusSerial;
    _pBusSerial = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::setup(RaftJsonIF& config)
{
    // Check if already setup
    if (_pBusSerial)
    {
        LOG_W(MODULE_PREFIX, "setup already done");
        return;
    }

    // Configure serial bus for stepper
    _pBusSerial = new BusSerial(nullptr, nullptr);
    if (_pBusSerial)
    {
        RaftJsonPrefixed motorSerialConfig(config, "MotorSerial");
        _pBusSerial->setup(motorSerialConfig);
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup FAILED to create BusSerial");
    }

    // Configure stepper
    _pStepper = new MotorControl();
    if (_pStepper)
    {
        RaftJsonPrefixed doorMotorConfig(config, "DoorMotor");
        _pStepper->setup(doorMotorConfig);
        _pStepper->setBusNameIfValid(_pBusSerial ? _pBusSerial->getBusName().c_str() : nullptr);
        _pStepper->connectToBus(_pBusSerial);
        _pStepper->postSetup();
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup FAILED to create HWElemSteppers");
    }

    // Get motor on time after move (secs)
    float motorOnTimeAfterMoveSecs = config.getDouble("MotorOnTimeAfterMoveSecs", 0.0);
    if (_pStepper)
        _pStepper->setMotorOnTimeAfterMoveSecs(motorOnTimeAfterMoveSecs);

    // Get motor current threshold
    float maxMotorCurrentAmps = config.getDouble("MaxMotorCurrentAmps", 0.1);
    if (_pStepper)
        _pStepper->setMaxMotorCurrentAmps(0, maxMotorCurrentAmps);

    // Setup I2C
    RaftJsonPrefixed i2cConfig(config, "BusI2C");
    _busI2C.setup(i2cConfig);

    // Rotation sensor address
    RaftJsonPrefixed angleSensorConfig(config, "AngleSensor");
    _rotationSensor.setup(angleSensorConfig, &_busI2C);

    // Set hysteresis for angle filter
    _rotationSensor.setHysteresis(1);

    // Debug
    LOG_I(MODULE_PREFIX, "setup MaxMotorCurrent %.2fA MotorOnTimeAfterMoveSecs %.2fs", 
                maxMotorCurrentAmps, motorOnTimeAfterMoveSecs);
    // LOG_I(MODULE_PREFIX, "setup rotSensorI2CAddr 0x%02x", _rotationSensor.getI2CAddr());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::service()
{
    // Service the stepper
    if (_pStepper)
        _pStepper->service();

    // Service I2C bus
    _busI2C.service();

    // Service the sensor
    _rotationSensor.service();

    // Feed the speed averaging
    float curAngleDegs = _rotationSensor.getAngleDegrees(true, false);
    _measuredDoorSpeedDegsPerSec.sample(curAngleDegs);

    // Debug
#ifdef DEBUG_SENSOR_ANGLE
    if (Raft::isTimeout(millis(), _debugLastPrintTimeMs, 1000))
    {
        LOG_I(MODULE_PREFIX, "service angle %.2fdegs avgSpeed %.2fdegs/sec", 
                    curAngleDegs, _measuredDoorSpeedDegsPerSec.getRatePerSec());
        _debugLastPrintTimeMs = millis();
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get measured angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::getMeasuredAngleDegs()
{
    return _rotationSensor.getAngleDegrees(false, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get measured angular speed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::getMeasuredAngularSpeedDegsPerSec()
{
    return _measuredDoorSpeedDegsPerSec.getRatePerSec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move motor to angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::moveToAngleDegs(float angleDegrees, float movementSpeedDegreesPerSec)
{
    if (!_pStepper)
        return;

    // Get the current angle
    float currentAngleDegrees = _rotationSensor.getAngleDegrees(false, false);

    // Calculate the difference to required angle
    float angleDiffDegrees = angleDegrees - currentAngleDegrees;

    // Form command
    // ,"clearQ":1
    String moveCmd = R"({"cmd":"motion","stop":1,"clearQ":1,"rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(angleDiffDegrees));
    moveCmd.replace("__SPEED__", String(movementSpeedDegreesPerSec == 0 ? _reqMotorSpeedDegsPerSec : movementSpeedDegreesPerSec));

    // Send command
    _pStepper->sendCmdJSON(moveCmd.c_str());

    // Reset check on motor stopped
    _lastMotorStoppedCheckTimeMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::stop()
{
    if (!_pStepper)
        return;
    _pStepper->sendCmdJSON(R"({"cmd":"motion","stop":1,"clearQ":1})");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if motor active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isMotorActive()
{
    bool isValid = false;
    return _pStepper && _pStepper->getNamedValue("b", isValid) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if angle is within tolerance of target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isNearTargetAngle(float targetAngleDegs, float posToleranceDegs, float negToleranceDegs)
{
    // Get the current angle
    float currentAngleDegrees = _rotationSensor.getAngleDegrees(false, false);

    // Calculate the difference to required angle
    float angleDiffDegrees = targetAngleDegs - currentAngleDegrees;

    // Check if within tolerance
    if (angleDiffDegrees > 0)
        return angleDiffDegrees < posToleranceDegs;
    else
        return angleDiffDegrees > negToleranceDegs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if motor has stopped for more than a given time (ms)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotorAndAngleSensor::isStoppedForTimeMs(uint32_t timeMs, float expectedMotorSpeedDegsPerSec)
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
        // Reset stopped time
        _lastMotorStoppedCheckTimeMs = millis();
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// I2CBus element status callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::busElemStatusCB(BusBase& bus, const std::vector<BusElemAddrAndStatus>& statusChanges)
{
    String statusStr;
    int seqStartAddr = -1;
    bool lastWasOnline = false;
    int lastAddr = 0;
    for (auto& statusChange : statusChanges)
    {
        if (seqStartAddr == -1)
        {
            seqStartAddr = statusChange.address;
            lastWasOnline = statusChange.isChangeToOnline;
        }
        else if (lastWasOnline != statusChange.isChangeToOnline)
        {
            if (!statusStr.isEmpty())
                statusStr += ", ";
            statusStr += "0x" + String(seqStartAddr, 16) + "..0x" + String(statusChange.address - 1, 16) + (lastWasOnline ? ":online" : ":offline");
            seqStartAddr = statusChange.address;
            lastWasOnline = statusChange.isChangeToOnline;
        }
        lastAddr = statusChange.address;
    }
    if (seqStartAddr != -1)
    {
        if (!statusStr.isEmpty())
            statusStr += ", ";
        statusStr += "0x" + String(seqStartAddr, 16) + "..0x" + String(lastAddr, 16) + (lastWasOnline ? ":online" : ":offline");
    }
    LOG_I(MODULE_PREFIX, "busElemStatusCB I2C addr %s", statusStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// I2CBus operation status callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotorAndAngleSensor::busOperationStatusCB(BusBase& bus, BusOperationStatus busOperationStatus)
{
    LOG_I(MODULE_PREFIX, "busOperationStatusCB I2C bus %s", BusBase::busOperationStatusToString(busOperationStatus));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate move speed degs per sec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float MotorAndAngleSensor::calcMoveSpeedDegsPerSec(float angleDegs, float timeSecs)
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
