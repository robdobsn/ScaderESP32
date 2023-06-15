/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Door Opener
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DoorOpener.h"
#include "ConfigPinMap.h"
#include <MotorControl.h>
#include <BusSerial.h>
#include <HWElemConsts.h>

static const char* MODULE_PREFIX = "DoorOpener";

DoorOpener::DoorOpener()
{
}

DoorOpener::~DoorOpener()
{
    clear();
}

void DoorOpener::clear()
{
    if (_pStepper)
        delete _pStepper;
    _pStepper = nullptr;
    if (_pBusSerial)
        delete _pBusSerial;
    _pBusSerial = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::setup(ConfigBase& config)
{
    // Record config
    recordConfig(config);

    // Clear
    clear();

    // Configure serial bus for stepper
    _pBusSerial = new BusSerial(nullptr, nullptr);
    if (_pBusSerial)
    {
        _pBusSerial->setup(config, "bus");
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup failed to create BusSerial");
    }

    // Configure stepper
    _pStepper = new MotorControl();
    if (_pStepper)
    {
        _pStepper->setup(config, nullptr, "stepper");
        _pStepper->setBusNameIfValid(_pBusSerial ? _pBusSerial->getBusName().c_str() : nullptr);
        _pStepper->connectToBus(_pBusSerial);
        _pStepper->postSetup();
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup failed to create HWElemSteppers");
    }

    // Configure GPIOs
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("consvButtonPin", ConfigPinMap::GPIO_INPUT_PULLUP, &_conservatoryButtonPin),
        ConfigPinMap::PinDef("consvPirPin", ConfigPinMap::GPIO_INPUT_PULLDOWN, &_consvPirSensePin)
    };
    ConfigPinMap::configMultiple(config, gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Setup conservatory button
    _conservatoryButton.setup(_conservatoryButtonPin, true, 0, 
                std::bind(&DoorOpener::onConservatoryButtonPressed, this, std::placeholders::_1));

    // Move and open times
    _doorTimeToOpenSecs = config.getLong("DoorTimeToOpenSecs", DEFAULT_DOOR_TIME_TO_OPEN_SECS);
    _doorRemainOpenTimeSecs = config.getLong("DoorRemainOpenTimeSecs", DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS);
    
    // Get door closed angle offset from config
    _doorClosedAngleOffsetDegrees = config.getLong("doorClosedAngleOffsetDegrees", 0);
    _doorSwingAngleDegrees = config.getLong("doorSwingAngleDegrees", 0);

    // Calculate movement speed
    _doorMoveSpeedDegsPerSec = calcDoorMoveSpeedDegsPerSec(_doorTimeToOpenSecs, _doorSwingAngleDegrees);

    // Get motor on time after move (secs)
    _motorOnTimeAfterMoveSecs = config.getLong("MotorOnTimeAfterMoveSecs", 0);
    if (_pStepper)
        _pStepper->setMotorOnTimeAfterMoveSecs(_motorOnTimeAfterMoveSecs);

    // Get motor current threshold
    float maxMotorCurrentAmps = config.getDouble("MaxMotorCurrentAmps", 0.1);
    if (_pStepper)
        _pStepper->setMaxMotorCurrentAmps(0, maxMotorCurrentAmps);

    // Setup I2C
    uint32_t i2cPort = config.getLong("i2cPort", 0);
    int i2cSdaPin = config.getLong("sdaPin", -1);
    int i2cSclPin = config.getLong("sclPin", -1);
    uint32_t i2cBusFreqHz = config.getLong("i2cBusFreqHz", 100000);
    if (i2cSdaPin != -1 && i2cSclPin != -1)
    {
        _i2cEnabled = _i2cCentral.init(i2cPort, i2cSdaPin, i2cSclPin, i2cBusFreqHz);
    }

    // MT6701 address
    _mt6701Addr = config.getLong("mt6701addr", 0x06);

    // Reverse angle
    _reverseSensorAngle = config.getLong("reverseSensorAngle", 0);

    // Read from non-volatile storage
    readFromNVS();

    // Debug
    // LOG_I(MODULE_PREFIX, "setup HBridgeEn=%d HBridgePhase=%d HBridgeVissen=%d", _hBridgeEn, _hBridgePhase, _hBridgeVissen);
    LOG_I(MODULE_PREFIX, "setup Conservatory: buttonPin %d pirPin %d reverseSensorAngle %s",
                _conservatoryButtonPin, _consvPirSensePin, _reverseSensorAngle ? "Y" : "N");
    LOG_I(MODULE_PREFIX, "setup DoorClosedAngleOffset %ddegs DoorSwingAngle %ddegs DoorTimeToOpen %ds DoorRemainOpenTime %ds", 
                _doorClosedAngleOffsetDegrees, _doorSwingAngleDegrees, _doorTimeToOpenSecs, _doorRemainOpenTimeSecs);
    LOG_I(MODULE_PREFIX, "setup DoorMoveSpeed %.2fdegs/s MaxMotorCurrent %.2fA MotorOnTimeAfterMove %ds", 
                _doorMoveSpeedDegsPerSec, maxMotorCurrentAmps, _motorOnTimeAfterMoveSecs);
    LOG_I(MODULE_PREFIX, "setup i2cPort %d sdaPin %d sclPin %d i2cBusFreqHz %d mt6701addr 0x%02x i2cEnabled %s", 
                i2cPort, i2cSdaPin, i2cSclPin, i2cBusFreqHz, _mt6701Addr, _i2cEnabled ? "Y" : "FAILED");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::service()
{
    // Service the stepper
    if (_pStepper)
        _pStepper->service();

    // Service the button
    _conservatoryButton.service();

    // Update PIRs
    bool pinVal = digitalRead(_consvPirSensePin);
    servicePIR("IN", pinVal, _pirSenseInActive, _pirSenseInActiveMs, _pirSenseInLast, _inEnabled);
    servicePIR("OUT", _pirOutValue, _pirSenseOutActive, _pirSenseOutActiveMs, _pirSenseOutLast, _outEnabled);

    // Handle closing after a time period
    handleClosingDoorAfterTimePeriod();

    // Service calibration
    serviceCalibration();

    // Handle end of movement
    if ((_doorMoveStartTimeMs != 0) && (_doorTimeToOpenSecs != 0) && Raft::isTimeout(millis(), 
                    _doorMoveStartTimeMs, _doorTimeToOpenSecs*1000 + 2000))
    {
        LOG_I(MODULE_PREFIX, "service door move timeout - no longer moving");
        _doorMoveStartTimeMs = 0;
    }

    // Update rotation angle
    updateRotationAngleFromSensor();

    // Update opener status
    uiModuleSetOpenStatus(isClosed(), isMotorActive());

    // Update UI module with angle, etc
    uiModuleSetStatusStr(1, String(calculateDegreesFromClosed(_rotationAngleCorrected)));

    // Save to non-volatile storage
    saveToNVSIfRequired();

    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, 5000))
    {
        _debugLastDisplayMs = millis();
        bool isValid = false;
        LOG_I(MODULE_PREFIX, "svc fromClosed %ddegs (raw %d) stepPos %.2f iPIR[v%d actv%d lst%d en%d] oPIR[v%d actv%d lst%d en%d] conBtn%d open%lldms stayOpen%ds",
            calculateDegreesFromClosed(_rotationAngleCorrected),
            _rotationAngleCorrected,
            _pStepper ? _pStepper->getNamedValue("x", isValid) : 0,
            digitalRead(_consvPirSensePin), _pirSenseInActive, _pirSenseInLast, _inEnabled, 
            _pirOutValue, _pirSenseOutActive, _pirSenseOutLast, _outEnabled,
            _conservatoryButtonState,
            _doorOpenedTimeMs != 0 ? Raft::timeElapsed(millis(), _doorOpenedTimeMs) : 0,
            _doorRemainOpenTimeSecs
            );
        // if (_pStepper)
        // {
        //     LOG_I(MODULE_PREFIX, "service stepper %s", _pStepper->getDataJSON().c_str());
        // }
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop and disable door
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::stopAndDisableDoor()
{
    if (!_pStepper)
        return;

    // Send command
    _pStepper->sendCmdJSON(R"({"cmd":"supv","en":0,"clearQ":1})");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conservatory button pressed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::onConservatoryButtonPressed(int val)
{
    _conservatoryButtonState = val == 0 ? 0 : 1;
    // _isOverCurrent = false;
    LOG_I(MODULE_PREFIX, "onConservatoryButtonPressed %d", val);

    // Check if motor already active
    if (isMotorActive())
    {
        // Stop and disable door - maybe something is wrong
        LOG_I(MODULE_PREFIX, "onConservatoryButtonPressed is moving - stopping");
        stopAndDisableDoor();
        return;
    }

    // Moving to open position
    motorMoveToPosition(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service PIRs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::servicePIR(const char* pName, bool pirValue, bool& pirActive, 
            uint32_t& pirActiveMs, bool& pirLast, bool dirEnabled)
{
    // Debug
    if (pirValue != pirLast)
    {
        LOG_I(MODULE_PREFIX, "servicePIR %s PIR now %d", pName, !pirLast);
        pirLast = !pirLast;
    }
    
    // Handle PIRs
    if (pirValue && dirEnabled)
    {
        if (!pirActive && !isMotorActive())
        {
            if (isOpenSufficientlyForCatAccess())
            {
                LOG_I(MODULE_PREFIX, "servicePIR %s triggered but door already open (or at least ajar)", pName);
            }
            else
            {
                LOG_I(MODULE_PREFIX, "servicePIR %s triggered - door opening", pName);
                motorMoveToPosition(true);
            }
            pirActive = true;
        }
        pirActiveMs = millis();
    }

    // Handle end of PIR sense
    if (pirActive && Raft::isTimeout(millis(), pirActiveMs, 5000))
        pirActive = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle closing door after time period
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::handleClosingDoorAfterTimePeriod()
{
    if (!isClosed() && (_doorOpenedTimeMs != 0) && 
            Raft::isTimeout(millis(), _doorOpenedTimeMs, _doorRemainOpenTimeSecs*1000) && (_inEnabled || _outEnabled))
    {
        LOG_I(MODULE_PREFIX, "service was open for %d secs - closing", _doorRemainOpenTimeSecs);
        motorMoveToPosition(false);
        _doorOpenedTimeMs = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update rotation angle from sensor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::updateRotationAngleFromSensor()
{
    // Check ready
    if (!_i2cEnabled || !Raft::isTimeout(millis(), _lastRotationAngleMs, ROTATION_ANGLE_SAMPLE_TIME_MS))
        return;

    // Update last time
    _lastRotationAngleMs = millis();

    // Get rotation angle
    uint8_t rotationRegNum = 0x03;
    uint8_t rotationAngleBytes[2];
    uint32_t numBytesRead = 0;
    RaftI2CCentralIF::AccessResultCode rslt = _i2cCentral.access(_mt6701Addr, &rotationRegNum, 1, rotationAngleBytes, 2, numBytesRead);

    // Check for error
    if (rslt == RaftI2CCentralIF::ACCESS_RESULT_OK)
    {
        // Convert to angle
        int32_t rawRotationAngle = (rotationAngleBytes[0] << 6) | (rotationAngleBytes[1] >> 2);
        _rotationAngleFromMagSensor = (360*rawRotationAngle)/16384;

        // Check for reverse
        _rotationAngleCorrected = _reverseSensorAngle ? 360 - _rotationAngleFromMagSensor : _rotationAngleFromMagSensor;

        // LOG_I(MODULE_PREFIX, "service rotation angle %ddegrees (raw %ddegrees, %drawunits)", 
        //             _rotationAngleCorrected, _rotationAngleFromMagSensor, rawRotationAngle);
    }
    else
    {
        // LOG_E(MODULE_PREFIX, "service error reading rotation angle - rslt=%s", RaftI2CCentralIF::getAccessResultStr(rslt));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// is motor active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DoorOpener::isMotorActive()
{
    bool isValid = false;
    return _pStepper && _pStepper->getNamedValue("b", isValid) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calibrate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::calibrate()
{
    // Debug
    LOG_I(MODULE_PREFIX, "calibrate %s", _pStepper ? "" : "FAILED NO STEPPER ");

    // Check valid
    if (!_pStepper)
        return;

    // Set motor current
    _pStepper->sendCmdJSON(R"({"cmd":"supv","holdcurrent":0.2})");

    // Assume we are starting at the closed position
    _doorClosedAngleOffsetDegrees = _rotationAngleCorrected;
    LOG_I(MODULE_PREFIX, "calibrate angle %u this is closed position - step 1 - detect motion direction",
                _doorClosedAngleOffsetDegrees);

    // Set to first state
    setCalState(CAL_STATE_START_ROTATION_1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move door to position open/closed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::motorMoveToPosition(bool open)
{    
    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveToPosition %s%s angles: [open %ddegs close %ddegs] current %ddeg", 
                _pStepper ? "" : "FAILED NO STEPPER ",
                open ? "opening" : "closing", 
                _doorSwingAngleDegrees, _doorClosedAngleOffsetDegrees,
                _rotationAngleCorrected);

    // Check valid
    if (!_pStepper)
        return;

    // Record last move request
    if (open)
        _doorOpenedTimeMs = millis();

    // Calculate angle to turn (in degrees)
    int32_t targetDegrees = open ? _doorSwingAngleDegrees : _doorClosedAngleOffsetDegrees;
    int32_t currentDegrees = _rotationAngleCorrected;
    int32_t degreesToTurn = targetDegrees - currentDegrees;
    // Add a little to ensure door fully closed
    if (!open)
        degreesToTurn = (int)(degreesToTurn * 1.02);

    // Form command
    String moveCmd = R"({"cmd":"motion","rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(degreesToTurn));
    moveCmd.replace("__SPEED__", String(_doorMoveSpeedDegsPerSec));

    // Send command
    UtilsRetCode::RetCode retc = _pStepper->sendCmdJSON(moveCmd.c_str());

    // Time movement started
    _doorMoveStartTimeMs = millis();

    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveToPosition target=%ddegs speed=%.2fdegs/s doorTimeToOpen %ds cmd %s retc %s", 
                targetDegrees, _doorMoveSpeedDegsPerSec, _doorTimeToOpenSecs, moveCmd.c_str(), UtilsRetCode::getRetcStr(retc));

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move door to specific angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::motorMoveAngle(int32_t angleDegs, bool relative, uint32_t moveTimeSecs)
{
    // Debug
    LOG_I(MODULE_PREFIX, "moveDoorAngle %s%d%s", _pStepper ? "" : "FAILED NO STEPPER ", angleDegs, 
                    relative ? " relative" : "");

    // Check valid
    if (!_pStepper)
        return;

    // Check if motor is active
    if (isMotorActive())
    {
        LOG_I(MODULE_PREFIX, "moveDoorToAngle motor is active - stopping first");
        stopAndDisableDoor();
    }

    // Form command
    String moveCmd = R"({"cmd":"motion","rel":__REL__,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__REL__", String(relative ? 1 : 0));
    moveCmd.replace("__POS__", String(angleDegs));
    moveCmd.replace("__SPEED__", String(_doorMoveSpeedDegsPerSec));

    // Send command
    UtilsRetCode::RetCode retc = _pStepper->sendCmdJSON(moveCmd.c_str());

    // Time movement started
    _doorMoveStartTimeMs = millis();

    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveAngle angle=%d %s speed=%.2fdegs/s moveTime %ds cmd %s retc %s", 
                angleDegs, relative ? "relative" : "absolute",
                _doorMoveSpeedDegsPerSec, moveTimeSecs, moveCmd.c_str(), UtilsRetCode::getRetcStr(retc));

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate door move speed degs per sec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double DoorOpener::calcDoorMoveSpeedDegsPerSec(double timeSecs, double angleDegs)
{
    double moveTimeSecs = timeSecs;
    if (moveTimeSecs == 0)
        moveTimeSecs = DEFAULT_DOOR_TIME_TO_OPEN_SECS;
    double turnAngle = angleDegs;
    if (turnAngle == 0)
    {
        turnAngle = DEFAULT_DOOR_SWING_ANGLE;
    }
    double doorOpenSpeedDegsPerSec = turnAngle / moveTimeSecs;
    LOG_I(MODULE_PREFIX, "calcDoorMoveSpeed angleDegs %.2f turnAngle %.2f timeSecs %.2f moveTimeSecs %.2f speedDegsPerSec %.2f", 
                angleDegs, turnAngle, timeSecs, moveTimeSecs, doorOpenSpeedDegsPerSec);
    return doorOpenSpeedDegsPerSec;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String DoorOpener::getStatusJSON(bool includeBraces)
{
    bool isValid = false;
    String json = "";
    json += "\"motorActive\":" + String(isMotorActive() ? 1 : 0);
    json += ",\"inEnabled\":" + String(_inEnabled ? 1 : 0);
    json += ",\"outEnabled\":" + String(_outEnabled ? 1 : 0);
    json += ",\"consButtonPressed\":" + String(_conservatoryButton.isButtonPressed() ? 1 : 0);
    json += ",\"pirSenseInActive\":" + String(_pirSenseInActive ? 1 : 0);
    json += ",\"pirSenseInTriggered\":" + String(digitalRead(_consvPirSensePin) ? 1 : 0);
    json += ",\"pirSenseOutActive\":" + String(_pirSenseOutActive ? 1 : 0);
    json += ",\"pirSenseOutTriggered\":" + String(_pirOutValue ? 1 : 0);
    json += ",\"rawSensorAngleCorrected\":" + String(_rotationAngleCorrected);
    json += ",\"angleFromClosed\":" + String(calculateDegreesFromClosed(_rotationAngleCorrected));
    json += ",\"closedAngleTolerance\":" + String(_doorClosedAngleTolerance);
    json += ",\"openEnoughForCatAccessAngle\":" + String(_doorOpenEnoughForCatAccessAngle);
    json += ",\"stepperCurAngle\":" + String(_pStepper ? _pStepper->getNamedValue("x", isValid) : 0);
    json += ",\"doorSwingAngleDegrees\":" + String(_doorSwingAngleDegrees);
    json += ",\"doorClosedAngleOffsetDegrees\":" + String(_doorClosedAngleOffsetDegrees);
    json += ",\"timeBeforeCloseSecs\":" + String(getSecsBeforeClose());
    if (includeBraces)
        json = "{" + json + "}";
    return json;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::getStatusHash(std::vector<uint8_t>& stateHash)
{
    // Add state
    stateHash.push_back(isMotorActive() ? 1 : 0);
    stateHash.push_back(_inEnabled ? 1 : 0);
    stateHash.push_back(_outEnabled ? 1 : 0);
    stateHash.push_back(_conservatoryButton.isButtonPressed() ? 1 : 0);
    stateHash.push_back(_conservatoryButtonState);
    stateHash.push_back(digitalRead(_consvPirSensePin) ? 1 : 0);
    stateHash.push_back(_pirSenseInActive ? 1 : 0);
    stateHash.push_back(_pirOutValue ? 1 : 0);
    stateHash.push_back(_pirSenseOutActive ? 1 : 0);
    stateHash.push_back(getSecsBeforeClose() & 0xFF);
    stateHash.push_back(_rotationAngleCorrected & 0xff);
    stateHash.push_back(_doorSwingAngleDegrees & 0xff);
    stateHash.push_back(_doorClosedAngleOffsetDegrees & 0xff);
    bool isValid = false;
    stateHash.push_back(_pStepper ? (uint8_t)(_pStepper->getNamedValue("x", isValid)) : 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Calibration
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::serviceCalibration()
{
    // Check overall timeout
    if ((_calState != CAL_STATE_NONE) && Raft::isTimeout(millis(), _calStartTimeMs, 30000))
    {
        LOG_I(MODULE_PREFIX, "serviceCalibration overall timeout");
        setCalState(CAL_STATE_NONE);
        return;
    }

    switch(_calState)
    {
        case CAL_STATE_NONE:
            break;
        case CAL_STATE_START_ROTATION_1:
        {
            // Move to angle to check if motion is inhibited by door frame
            motorMoveAngle(10, true, 0);
            setCalState(CAL_STATE_WAIT_ROTATION_1);
            break;
        }
        case CAL_STATE_WAIT_ROTATION_1:
        {
            // Check if door has moved
            if (!isMotorActive())
            {
                setCalState(CAL_STATE_WAIT_ROTATION_1A);
            }
            break;
        }
        case CAL_STATE_WAIT_ROTATION_1A:
        {
            if (Raft::isTimeout(millis(), _calStartTimeMs, 5000))
            {
                // Check if door has moved
                if (abs(_rotationAngleCorrected - _doorClosedAngleOffsetDegrees) > 5)
                {
                    // Door has moved - so rotation direction to open is positive
                    _calDoorOpenRotationDirection = 1;
                    LOG_I(MODULE_PREFIX, "serviceCalibration door moved - so rotation direction to open is positive");
                    // Rotate back to original position
                    motorMoveAngle(-10, true, 0);
                }
                else
                {
                    // Door has not moved - so rotation direction to open is negative
                    _calDoorOpenRotationDirection = -1;
                    LOG_I(MODULE_PREFIX, "serviceCalibration door not moved - so rotation direction to open is negative");
                }
                setCalState(CAL_STATE_WAIT_ROTATION_2);
            }
            break;
        }
        case CAL_STATE_WAIT_ROTATION_2:
        {
            // Check for door movement finished
            if (!isMotorActive())
            {
                setCalState(CAL_STATE_WAIT_ROTATION_2A);
            }
            break;
        }
        case CAL_STATE_WAIT_ROTATION_2A:
        {
            if (Raft::isTimeout(millis(), _calStartTimeMs, 5000))
            {
                // Now rotate to the position where the door hits the end stop
                LOG_I(MODULE_PREFIX, "serviceCalibration rotating to open end stop");
                motorMoveAngle(_calDoorOpenRotationDirection * CAL_DOOR_OPEN_ANGLE_MAX, true, 0);
                setCalState(CAL_STATE_WAIT_OPEN_1);
            }
            break;
        }
        case CAL_STATE_WAIT_OPEN_1:
        {
            // Check for door movement finished
            if (!isMotorActive())
            {
                setCalState(CAL_STATE_WAIT_OPEN_1A);
            }
            break;
        }
        case CAL_STATE_WAIT_OPEN_1A:
        {
            if (Raft::isTimeout(millis(), _calStartTimeMs, 5000))
            {
                LOG_I(MODULE_PREFIX, "serviceCalibration rotate to open end stop complete");
            
                // Rotate back a few degrees from the end stop
                motorMoveAngle(-_calDoorOpenRotationDirection * 10, true, 0);
                setCalState(CAL_STATE_WAIT_ROTATION_3);
            }
            break;
        }
        case CAL_STATE_WAIT_ROTATION_3:
        {
            // Check for door movement finished
            if (!isMotorActive())
            {
                setCalState(CAL_STATE_WAIT_ROTATION_3A);
            }
            break;
        }
        case CAL_STATE_WAIT_ROTATION_3A:
        {
            if (Raft::isTimeout(millis(), _calStartTimeMs, 5000))
            {
                // This is the angle where the door hits the end stop
                _doorSwingAngleDegrees = _rotationAngleCorrected;
                LOG_I(MODULE_PREFIX, "serviceCalibration open position %d degrees", _doorSwingAngleDegrees);
                setCalState(CAL_STATE_NONE);

                // Move door back to closed position
                motorMoveAngle(_doorClosedAngleOffsetDegrees - _doorSwingAngleDegrees, true, 0);
            }
            break;
        }
    }
}
