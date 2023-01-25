/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Door Opener
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DoorOpener.h"
#include "ConfigPinMap.h"
#include <HWElemSteppers.h>
#include <BusSerial.h>
#include <HWElemConsts.h>

static const char* MODULE_PREFIX = "DoorOpener";

DoorOpener::DoorOpener()
{
    // Init INA219 current sensor
    // esp_err_t initOk = ina219_init_desc(&_ina219, INA219_ADDR_VS_GND, (i2c_port_t)0, (gpio_num_t)21, (gpio_num_t)22);
    // if (initOk == ESP_OK)
    //     initOk = ina219_init(&_ina219);
    // if (initOk != ESP_OK)
    // {
    //     LOG_E(MODULE_PREFIX, "constructor failed to init INA219");
    // }
    // else
    // {
    //     LOG_I(MODULE_PREFIX, "constructor INA219 init OK");
    // }
}

DoorOpener::~DoorOpener()
{
    // ina219_deinit(&_ina219);
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
    _pStepper = new HWElemSteppers();
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
        // ConfigPinMap::PinDef("H_BRIDGE_EN_PIN", ConfigPinMap::GPIO_OUTPUT, &_hBridgeEn),
        // ConfigPinMap::PinDef("H_BRIDGE_PHASE_PIN", ConfigPinMap::GPIO_OUTPUT, &_hBridgePhase),
        // ConfigPinMap::PinDef("H_BRIDGE_VISSEN_PIN", ConfigPinMap::GPIO_INPUT, &_hBridgeVissen),
        ConfigPinMap::PinDef("KITCHEN_BUTTON_PIN", ConfigPinMap::GPIO_INPUT_PULLUP, &_kitchenButtonPin),
        ConfigPinMap::PinDef("CONSV_BUTTON_PIN", ConfigPinMap::GPIO_INPUT_PULLUP, &_conservatoryButtonPin),
        ConfigPinMap::PinDef("LED_OUT_EN_PIN", ConfigPinMap::GPIO_OUTPUT, &_ledOutEnPin),
        ConfigPinMap::PinDef("LED_IN_EN_PIN", ConfigPinMap::GPIO_OUTPUT, &_ledInEnPin),
        ConfigPinMap::PinDef("PIR_SENSE_IN_PIN", ConfigPinMap::GPIO_INPUT_PULLDOWN, &_pirSenseInPin),
        ConfigPinMap::PinDef("PIR_SENSE_OUT_PIN", ConfigPinMap::GPIO_INPUT_PULLDOWN, &_pirSenseOutPin)
    };
    ConfigPinMap::configMultiple(config, gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    _kitchenButton.setup(_kitchenButtonPin, true, 0, 
                std::bind(&DoorOpener::onKitchenButtonPressed, this, std::placeholders::_1));
    _conservatoryButton.setup(_conservatoryButtonPin, true, 0, 
                std::bind(&DoorOpener::onConservatoryButtonPressed, this, std::placeholders::_1));

    // Move and open times
    _doorTimeToOpenSecs = config.getLong("DoorTimeToOpenSecs", DEFAULT_DOOR_TIME_TO_OPEN_SECS);
    _doorRemainOpenTimeSecs = config.getLong("DoorRemainOpenTimeSecs", DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS);
    
    // Get door open angle from config
    _doorOpenAngleDegrees = config.getLong("DoorOpenAngle", 0);
    _doorClosedAngleDegrees = config.getLong("DoorClosedAngle", 0);

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

    // Debug
    // LOG_I(MODULE_PREFIX, "setup HBridgeEn=%d HBridgePhase=%d HBridgeVissen=%d", _hBridgeEn, _hBridgePhase, _hBridgeVissen);
    LOG_I(MODULE_PREFIX, "setup KitchenButtonPin %d ConservatoryButtonPin %d",
                _kitchenButtonPin, _conservatoryButtonPin);
    LOG_I(MODULE_PREFIX, "setup LedDownPin %d LedUpPin %d", 
                _ledOutEnPin, _ledInEnPin);
    LOG_I(MODULE_PREFIX, "setup PirSenseInPin %d PirSenseOutPin %d", 
                _pirSenseInPin, _pirSenseOutPin);
    LOG_I(MODULE_PREFIX, "setup DoorOpenAngle %ddegs DoorClosedAngle %ddegs DoorTimeToOpen %ds DoorRemainOpenTime %ds", 
                _doorOpenAngleDegrees, _doorClosedAngleDegrees, _doorTimeToOpenSecs, _doorRemainOpenTimeSecs);
    LOG_I(MODULE_PREFIX, "setup MaxMotorCurrent %.2fA MotorOnTimeAfterMove %ds", 
                maxMotorCurrentAmps, _motorOnTimeAfterMoveSecs);
    LOG_I(MODULE_PREFIX, "setup i2cPort %d sdaPin %d sclPin %d i2cBusFreqHz %d mt6701addr 0x%02x i2cEnabled %s", 
                i2cPort, i2cSdaPin, i2cSclPin, i2cBusFreqHz, _mt6701Addr, _i2cEnabled ? "Y" : "FAILED");

    // Mode
    bool inEn = config.getBool("inEn", false);
    bool outEn = config.getBool("outEn", false);
    LOG_I(MODULE_PREFIX, "setup inEn %d outEn %d", inEn, outEn);

    bool inEn2 = config.getBool("openerState/inEn", false);
    bool outEn2 = config.getBool("openerState/outEn", false);
    LOG_I(MODULE_PREFIX, "setup inEn2 %d outEn2 %d", inEn2, outEn2);

    setMode(inEn, outEn, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::service()
{
    // Service the stepper
    if (_pStepper)
        _pStepper->service();

    // Service the buttons
    _kitchenButton.service();
    _conservatoryButton.service();

    // Handle opener mode changes - these are made using the OUT button
    if ((_openerInOutMode == OPENER_IN_OUT_MODE_TOGGLE) && 
        Raft::isTimeout(millis(), _buttonPressTimeMs, 750))
    {
        // Close door if it was open / open door if it was closed
        LOG_I(MODULE_PREFIX, "service TOGGLE and timeout - toggling and going idle");
        motorMoveToPosition(!_isOpen);
        _openerInOutMode = OPENER_IN_OUT_MODE_IDLE;
    }
    else if ((_openerInOutMode >= OPENER_IN_OUT_MODE_LOOP_START) && 
        Raft::isTimeout(millis(), _buttonPressTimeMs, 3000))
    {
        // No longer expecting button presses to change mode
        LOG_I(MODULE_PREFIX, "service LOOP_START and timeout - going idle");
        _openerInOutMode = OPENER_IN_OUT_MODE_IDLE;
    }

    // Service PIRs
    servicePIR("IN", _pirSenseInPin, _pirSenseInActive, _pirSenseInActiveMs, _pirSenseInLast, _inEnabled);
    servicePIR("OUT", _pirSenseOutPin, _pirSenseOutActive, _pirSenseOutActiveMs, _pirSenseOutLast, _outEnabled);

    // Handle closing after a time period
    if (_isOpen && (_doorOpenedTimeMs != 0) && Raft::isTimeout(millis(), _doorOpenedTimeMs, _doorRemainOpenTimeSecs*1000) && (_inEnabled || _outEnabled))
    {
        LOG_I(MODULE_PREFIX, "service was open for %d secs - closing", _doorRemainOpenTimeSecs);
        motorMoveToPosition(false);
        _doorOpenedTimeMs = 0;
    }

    // Handle warning LED
    if (_isOverCurrent)
    {
        if (Raft::isTimeout(millis(), _overCurrentBlinkTimeMs, OVER_CURRENT_BLINK_TIME_MS))
        {
            _overCurrentBlinkTimeMs = millis();
            _overCurrentBlinkCurOn = !_overCurrentBlinkCurOn;
            setLEDs();
        }
        if (Raft::isTimeout(millis(), _overCurrentStartTimeMs, OVER_CURRENT_CLEAR_TIME_MS))
        {
            _isOverCurrent = false;
            _overCurrentBlinkCurOn = false;
            setLEDs();
        }
        // else
        // {
        //     _tinyPico.DotStar_CycleColor(25);
        // }
    }

    // Calculate window average of vissen analogRead
    // if (Raft::isTimeout(millis(), _currentLastSampleMs, CURRENT_SAMPLE_TIME_MS))
    // {
    //     _currentLastSampleMs = millis();
    //     // TODO 2022 average current
    //     uint16_t avgCurrent = _avgCurrent.getAverage();

    //     // Check for overcurrent
    //     if (avgCurrent > CURRENT_OVERCURRENT_THRESHOLD)
    //     {
    //         if (!_isOpen || Raft::isTimeout(millis(), _overCurrentStartTimeMs, OVER_CURRENT_IGNORE_AT_START_OF_OPEN_MS))
    //         {
    //             LOG_I(MODULE_PREFIX, "service overcurrent detected - avgCurrent=%d", avgCurrent);
    //             _isOverCurrent = true;
    //             _overCurrentStartTimeMs = millis();
    //             _overCurrentBlinkTimeMs = 0;
    //             _overCurrentBlinkCurOn = true;
    //             motorControl(false, 0, _doorOpenAngleDegrees, _doorClosedAngleDegrees);
    //         }
    //     }
    // }

    // Handle end of movement
    if ((_doorMoveStartTimeMs != 0) && (_doorTimeToOpenSecs != 0) && Raft::isTimeout(millis(), _doorMoveStartTimeMs, _doorTimeToOpenSecs*1000 + 2000))
    {
        LOG_I(MODULE_PREFIX, "service door move timeout - no longer moving");
        _doorMoveStartTimeMs = 0;
    }

    // Get rotation angle
    if (_i2cEnabled && Raft::isTimeout(millis(), _lastRotationAngleMs, ROTATION_ANGLE_SAMPLE_TIME_MS))
    {
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
            uint32_t rawRotationAngle = (rotationAngleBytes[0] << 6) | (rotationAngleBytes[1] >> 2);
            _rotationAngle = (360*rawRotationAngle)/16384;

            // LOG_I(MODULE_PREFIX, "service rotation angle = %.2f (raw %d)", 360.0*rotationAngle/16384.0, rotationAngle);
        }
        else
        {
            // LOG_E(MODULE_PREFIX, "service error reading rotation angle - rslt=%s", RaftI2CCentralIF::getAccessResultStr(rslt));
        }
    }


    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, 5000))
    {
        _debugLastDisplayMs = millis();
        bool isValid = false;
        LOG_I(MODULE_PREFIX, "svc %s rotDegs %d stepAng %.2f iPIR: v=%d actv=%d lst=%d en=%d oPIR: v=%d actv=%d lst=%d en=%d kitBut=%d conBut=%d timeOpenMs=%lld doorStayOpenSecs=%d",
            _isOpen ? "OPEN " : "CLOSED ", 
            _rotationAngle,
            _pStepper ? _pStepper->getNamedValue("x", isValid) : 0,
            digitalRead(_pirSenseInPin), _pirSenseInActive, _pirSenseInLast, _inEnabled, 
            digitalRead(_pirSenseOutPin), _pirSenseOutActive, _pirSenseOutLast, _outEnabled,
            _openerInOutMode,
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

void DoorOpener::stopAndDisableDoor()
{
    if (!_pStepper)
        return;

    // Send command
    _pStepper->sendCmdJSON(R"({"en":0,"clearQ":1})");
}

void DoorOpener::onKitchenButtonPressed(int val)
{
    _isOverCurrent = false;
    LOG_I(MODULE_PREFIX, "onKitchenButtonPressed %d", val);
    if (!val)
        return;
    _openerInOutMode++;
    if (_openerInOutMode > OPENER_IN_OUT_MODE_LOOP_END)
        _openerInOutMode = OPENER_IN_OUT_MODE_LOOP_START;
    if (_openerInOutMode >= OPENER_IN_OUT_MODE_LOOP_START)
    {
        bool inEn = (_openerInOutMode == OPENER_IN_OUT_MODE_IN_ONLY) || 
                    (_openerInOutMode == OPENER_IN_OUT_MODE_IN_AND_OUT);
        bool outEn = (_openerInOutMode == OPENER_IN_OUT_MODE_OUT_ONLY) || 
                    (_openerInOutMode == OPENER_IN_OUT_MODE_IN_AND_OUT);
        setMode(inEn, outEn, true);
    }
    _buttonPressTimeMs = millis();
}
void DoorOpener::onConservatoryButtonPressed(int val)
{
    _conservatoryButtonState = val == 0 ? 0 : 1;
    _isOverCurrent = false;
    LOG_I(MODULE_PREFIX, "onConservatoryButtonPressed %d", val);
    motorMoveToPosition(true);
}
void DoorOpener::setLEDs()
{
    digitalWrite(_ledInEnPin, _isOverCurrent ? _overCurrentBlinkCurOn : (_inEnabled ? HIGH : LOW));
    digitalWrite(_ledOutEnPin, _isOverCurrent ? !_overCurrentBlinkCurOn : (_outEnabled ? HIGH : LOW));

    // Echo onto TinyPico LED
    // uint32_t dotStarColor = _inEnabled ? 0x005000 : 0x000000;
    // dotStarColor |= _outEnabled ? 0x000050 : 0x000000;
    // _tinyPico.DotStar_SetPixelColor(dotStarColor);
    // LOG_I(MODULE_PREFIX, "setLEDs in=%d out=%d", _inEnabled, _outEnabled);
}
void DoorOpener::servicePIR(const char* pName, uint32_t pirPin, bool& pirActive, 
            uint32_t& pirActiveMs, bool& pirLast, bool dirEnabled)
{
    // Debug
    if (digitalRead(pirPin) != pirLast)
    {
        LOG_I(MODULE_PREFIX, "servicePIR %s PIR now %d", pName, !pirLast);
        pirLast = !pirLast;
    }
    
    // Handle PIRs
    if (digitalRead(pirPin) && dirEnabled)
    {
        if (!pirActive && !isBusy())
        {
            if (_isOpen)
            {
                LOG_I(MODULE_PREFIX, "servicePIR %s triggered but door already open", pName);
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
bool DoorOpener::isBusy()
{
    return _doorMoveStartTimeMs != 0;
}
void DoorOpener::setMode(bool enIntoKitchen, bool enOutOfKitchen, bool recordChange)
{
    if (_inEnabled == enIntoKitchen && _outEnabled == enOutOfKitchen)
        return;
    _inEnabled = enIntoKitchen;
    _outEnabled = enOutOfKitchen;
    if (recordChange)
        _modeChanged = true;
    setLEDs();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move door to position open/closed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::motorMoveToPosition(bool open)
{    
    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveToPosition %s%s angles: [open %.2fdegs close %.2fdegs]", 
                _pStepper ? "" : "FAILED NO STEPPER ",
                open ? "open" : "closed", 
                _doorOpenAngleDegrees, _doorClosedAngleDegrees);

    // Check valid
    if (!_pStepper)
        return;

    // Record last move request
    _isOpen = open;
    if (_isOpen)
        _doorOpenedTimeMs = millis();

    // Calculate angle to turn (in degrees)
    int32_t targetDegrees = open ? _doorOpenAngleDegrees : _doorClosedAngleDegrees;
    int32_t currentDegrees = _rotationAngle;
    int32_t degreesToTurn = targetDegrees - currentDegrees;
    // Add a little to ensure door fully open/closed
    degreesToTurn = (int)(degreesToTurn * 1.02);

    // Form command
    String moveCmd = R"({"rel":1,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(degreesToTurn));
    moveCmd.replace("__SPEED__", String(calcDoorMoveSpeedDegsPerSec()));

    // Send command
    UtilsRetCode::RetCode retc = _pStepper->sendCmdJSON(moveCmd.c_str());

    // Time movement started
    _doorMoveStartTimeMs = millis();

    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveToPosition speed=%.2fdegs/s doorTimeToOpen %ds cmd %s retc %s", 
                calcDoorMoveSpeedDegsPerSec(), _doorTimeToOpenSecs, moveCmd.c_str(), UtilsRetCode::getRetcStr(retc));

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move door to specific angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::motorMoveToAngle(uint32_t angleDegs)
{
    // Debug
    LOG_I(MODULE_PREFIX, "moveDoorToAngle %s%d", _pStepper ? "" : "FAILED NO STEPPER ", angleDegs);

    // Check valid
    if (!_pStepper)
        return;

    // Check if door is already moving
    if (isBusy())
    {
        LOG_I(MODULE_PREFIX, "moveDoorToAngle already moving");
        return;
    }

    // Form command
    String moveCmd = R"({"rel":0,"nosplit":1,"speed":__SPEED__,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(angleDegs));
    moveCmd.replace("__SPEED__", String(calcDoorMoveSpeedDegsPerSec()));

    // Send command
    UtilsRetCode::RetCode retc = _pStepper->sendCmdJSON(moveCmd.c_str());

    // Time movement started
    _doorMoveStartTimeMs = millis();

    // Debug
    LOG_I(MODULE_PREFIX, "motorMoveToAngle angle=%d speed=%.2fdegs/s doorTimeToOpen %ds cmd %s retc %s", 
                angleDegs, calcDoorMoveSpeedDegsPerSec(), _doorTimeToOpenSecs, moveCmd.c_str(), UtilsRetCode::getRetcStr(retc));

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate door move speed degs per sec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float DoorOpener::calcDoorMoveSpeedDegsPerSec()
{
    uint32_t maxTurnAngle = abs((int)_doorOpenAngleDegrees-(int)_doorClosedAngleDegrees);
    if (maxTurnAngle == 0)
        maxTurnAngle = abs((int)DEFAULT_DOOR_OPEN_ANGLE - (int)DEFAULT_DOOR_CLOSED_ANGLE);
    float doorOpenSpeedDegsPerSec = float(maxTurnAngle) / (_doorTimeToOpenSecs == 0 ? DEFAULT_DOOR_TIME_TO_OPEN_SECS : _doorTimeToOpenSecs);
    return doorOpenSpeedDegsPerSec;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String DoorOpener::getStatusJSON(bool includeBraces)
{
    bool isValid = false;
    String json = "";
    json += "\"isMoving\":" + String(isBusy() ? 1 : 0);
    json += ",\"isOpen\":" + String(_isOpen ? 1 : 0);
    json += ",\"inEnabled\":" + String(_inEnabled ? 1 : 0);
    json += ",\"outEnabled\":" + String(_outEnabled ? 1 : 0);
    json += ",\"isOverCurrent\":" + String(_isOverCurrent ? 1 : 0);
    json += ",\"inOutMode\":\"" + String(getOpenerInOutModeStr(_openerInOutMode));
    json += "\",\"kitButtonPressed\":" + String(_kitchenButton.isButtonPressed() ? 1 : 0);
    json += ",\"consButtonPressed\":" + String(_conservatoryButton.isButtonPressed() ? 1 : 0);
    json += ",\"pirSenseInActive\":" + String(_pirSenseInActive ? 1 : 0);
    json += ",\"pirSenseInTriggered\":" + String(digitalRead(_pirSenseInPin) ? 1 : 0);
    json += ",\"pirSenseOutActive\":" + String(_pirSenseOutActive ? 1 : 0);
    json += ",\"pirSenseOutTriggered\":" + String(digitalRead(_pirSenseOutPin) ? 1 : 0);
    json += ",\"avgCurrent\":" + String(_avgCurrent.getAverage());
    json += ",\"rotationAngleDegs\":" + String(_rotationAngle);
    json += ",\"stepperCurAngle\":" + String(_pStepper ? _pStepper->getNamedValue("x", isValid) : 0);
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
    stateHash.push_back(isBusy() ? 1 : 0);
    stateHash.push_back(_isOpen ? 1 : 0);
    stateHash.push_back(_inEnabled ? 1 : 0);
    stateHash.push_back(_outEnabled ? 1 : 0);
    stateHash.push_back(_isOverCurrent ? 1 : 0);
    stateHash.push_back(_kitchenButton.isButtonPressed() ? 1 : 0);
    stateHash.push_back(_openerInOutMode);
    stateHash.push_back(_conservatoryButton.isButtonPressed() ? 1 : 0);
    stateHash.push_back(_conservatoryButtonState);
    stateHash.push_back(digitalRead(_pirSenseInPin) ? 1 : 0);
    stateHash.push_back(_pirSenseInActive ? 1 : 0);
    stateHash.push_back(digitalRead(_pirSenseOutPin) ? 1 : 0);
    stateHash.push_back(_pirSenseOutActive ? 1 : 0);
    stateHash.push_back(_avgCurrent.getAverage());
    stateHash.push_back(getSecsBeforeClose() & 0xFF);
    stateHash.push_back(_rotationAngle & 0xff);
    bool isValid = false;
    stateHash.push_back(_pStepper ? (uint8_t)(_pStepper->getNamedValue("x", isValid)) : 0);
}
