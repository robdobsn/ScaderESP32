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
        ConfigBase busConfig = config.getString("bus", "{}");
        _pBusSerial->setup(busConfig);
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup failed to create BusSerial");
    }

    // Configure stepper
    _pStepper = new HWElemSteppers();
    if (_pStepper)
    {
        ConfigBase stepperSetup = config.getString("stepper", "{}");
        _pStepper->setup(stepperSetup, nullptr);
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
    _doorMoveTimeMs = config.getLong("DoorMoveTimeMs", DOOR_MOVE_TIME_MS);
    _doorOpenTimeSecs = config.getLong("DoorOpenTimeSecs", DEFAULT_DOOR_OPEN_TIME_SECS);
    
    // Get door open angle from config
    _doorOpenAngleDegrees = config.getLong("DoorOpenAngle", 0);

    // Get motor on time after move (secs)
    _motorOnTimeAfterMoveSecs = config.getLong("MotorOnTimeAfterMoveSecs", 0);
    if (_pStepper)
        _pStepper->setMotorOnTimeAfterMoveSecs(_motorOnTimeAfterMoveSecs);

    // Debug
    // LOG_I(MODULE_PREFIX, "setup HBridgeEn=%d HBridgePhase=%d HBridgeVissen=%d", _hBridgeEn, _hBridgePhase, _hBridgeVissen);
    LOG_I(MODULE_PREFIX, "setup KitchenButtonPin=%d ConservatoryButtonPin=%d",                 
                _kitchenButtonPin, _conservatoryButtonPin);
    LOG_I(MODULE_PREFIX, "setup LedDownPin=%d LedUpPin=%d", 
                _ledOutEnPin, _ledInEnPin);
    LOG_I(MODULE_PREFIX, "setup PirSenseInPin=%d PirSenseOutPin=%d", 
                _pirSenseInPin, _pirSenseOutPin);
    LOG_I(MODULE_PREFIX, "setup DoorMoveTimeMs=%d DoorOpenTimeSecs=%d", 
                _doorMoveTimeMs, _doorOpenTimeSecs);
    LOG_I(MODULE_PREFIX, "setup DoorOpenAngle=%d MotorOnTimeAfterMoveSecs=%d", 
                _doorOpenAngleDegrees, _motorOnTimeAfterMoveSecs);

    // LED state
    setLEDs();
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

    // Handle opener mode
    if ((_kitchenButtonState == KITCHEN_BUTTON_STATE_TOGGLE) && 
        Raft::isTimeout(millis(), _buttonPressTimeMs, 750))
    {
        LOG_I(MODULE_PREFIX, "service TOGGLE and timeout - toggling and going idle");
        moveDoor(!_isOpen);
        _kitchenButtonState = KITCHEN_BUTTON_STATE_IDLE;
    }
    else if ((_kitchenButtonState >= KITCHEN_BUTTON_STATE_LOOP_START) && 
        Raft::isTimeout(millis(), _buttonPressTimeMs, 3000))
    {
        LOG_I(MODULE_PREFIX, "service LOOP_START and timeout - going idle");
        _kitchenButtonState = KITCHEN_BUTTON_STATE_IDLE;
    }

    // Service PIRs
    servicePIR("IN", _pirSenseInPin, _pirSenseInActive, _pirSenseInActiveMs, _pirSenseInLast, _inEnabled);
    servicePIR("OUT", _pirSenseOutPin, _pirSenseOutActive, _pirSenseOutActiveMs, _pirSenseOutLast, _outEnabled);

    // Handle closing
    if (_isOpen && Raft::isTimeout(millis(), _doorOpenedTimeMs, _doorOpenTimeSecs*1000) && (_inEnabled || _outEnabled))
    {
        LOG_I(MODULE_PREFIX, "service was open for %d secs - closing", _doorOpenTimeSecs);
        moveDoor(false);
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
    if (Raft::isTimeout(millis(), _currentLastSampleMs, CURRENT_SAMPLE_TIME_MS))
    {
        _currentLastSampleMs = millis();
        // TODO 2022 average current
        uint16_t avgCurrent = _avgCurrent.getAverage();

        // Check for overcurrent
        if (avgCurrent > CURRENT_OVERCURRENT_THRESHOLD)
        {
            if (!_isOpen || Raft::isTimeout(millis(), _overCurrentStartTimeMs, OVER_CURRENT_IGNORE_AT_START_OF_OPEN_MS))
            {
                LOG_I(MODULE_PREFIX, "service overcurrent detected - avgCurrent=%d", avgCurrent);
                _isOverCurrent = true;
                _overCurrentStartTimeMs = millis();
                _overCurrentBlinkTimeMs = 0;
                _overCurrentBlinkCurOn = true;
                motorControl(false, 0);
            }
        }
    }

    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, 1000))
    {
        _debugLastDisplayMs = millis();
        LOG_I(MODULE_PREFIX, "svc %savg I=%d iPIR: v=%d actv=%d lst=%d en=%d oPIR: v=%d actv=%d lst=%d en=%d kitBut=%d conBut=%d timeOpenMs=%lld doorOpenTimeSecs=%d",
            _isOpen ? "OPEN " : "CLOSED ", 
            _avgCurrent.getAverage(),
            digitalRead(_pirSenseInPin), _pirSenseInActive, _pirSenseInLast, _inEnabled, 
            digitalRead(_pirSenseOutPin), _pirSenseOutActive, _pirSenseOutLast, _outEnabled,
            _kitchenButtonState,
            _conservatoryButtonState,
            _doorOpenedTimeMs != 0 ? Raft::timeElapsed(millis(), _doorOpenedTimeMs) : 0,
            _doorOpenTimeSecs
            );
        // if (_pStepper)
        // {
        //     LOG_I(MODULE_PREFIX, "service stepper %s", _pStepper->getDataJSON().c_str());
        // }
    }
}
void DoorOpener::moveDoor(bool openDoor)
{
    // Record last move request
    _isOpen = openDoor;
    if (_isOpen)
        _doorOpenedTimeMs = millis();

    // Move door
    motorControl(true, openDoor);
}
void DoorOpener::stopDoor()
{
    motorControl(false, 0);
}
void DoorOpener::onKitchenButtonPressed(int val)
{
    _isOverCurrent = false;
    LOG_I(MODULE_PREFIX, "onKitchenButtonPressed %d", val);
    if (!val)
        return;
    _kitchenButtonState++;
    if (_kitchenButtonState > KITCHEN_BUTTON_STATE_LOOP_END)
        _kitchenButtonState = KITCHEN_BUTTON_STATE_LOOP_START;
    if (_kitchenButtonState >= KITCHEN_BUTTON_STATE_LOOP_START)
    {
        _inEnabled = (_kitchenButtonState == KITCHEN_BUTTON_STATE_IN_ONLY) || 
                    (_kitchenButtonState == KITCHEN_BUTTON_STATE_IN_AND_OUT);
        _outEnabled = (_kitchenButtonState == KITCHEN_BUTTON_STATE_OUT_ONLY) || 
                    (_kitchenButtonState == KITCHEN_BUTTON_STATE_IN_AND_OUT);
        setLEDs();
    }
    _buttonPressTimeMs = millis();
}
void DoorOpener::onConservatoryButtonPressed(int val)
{
    _conservatoryButtonState = val == 0 ? 0 : 1;
    _isOverCurrent = false;
    LOG_I(MODULE_PREFIX, "onConservatoryButtonPressed %d", val);
    moveDoor(true);
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
        if (!pirActive)
        {
            if (_isOpen)
            {
                LOG_I(MODULE_PREFIX, "servicePIR %s triggered but door already open", pName);
            }
            else
            {
                LOG_I(MODULE_PREFIX, "servicePIR %s triggered - door opening", pName);
                moveDoor(true);
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
    return false;
}
void DoorOpener::setMode(bool enIntoKitchen, bool enOutOfKitchen)
{
    _inEnabled = enIntoKitchen;
    _outEnabled = enOutOfKitchen;
    setLEDs();
}

void DoorOpener::motorControl(bool isEn, bool dirn)
{
    // digitalWrite(_hBridgeEn, isEn);
    // digitalWrite(_hBridgePhase, dirn);
    // _pStepper->setDir(dirn);
    // _pStepper->setEnable(isEn);
    // _pStepper->
    LOG_I(MODULE_PREFIX, "motorControl %d %d", isEn, dirn);
    if (isEn)
    {
        if (dirn)
        {
            String moveCmd = R"({"rel":0,"nosplit":1,"pos":[{"a":0,"p":__POS__}]})";
            moveCmd.replace("__POS__", String(_doorOpenAngleDegrees));
            if (_pStepper)
                _pStepper->sendCmdJSON(moveCmd.c_str());
        }
        else
        {
            String moveCmd = R"({"rel":0,"nosplit":1,"pos":[{"a":0,"p":0}]})";
            if (_pStepper)
                _pStepper->sendCmdJSON(moveCmd.c_str());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String DoorOpener::getStatusJSON(bool includeBraces)
{
    String json = "";
    json += "\"isOpen\":" + String(_isOpen ? 1 : 0);
    json += ",\"inEnabled\":" + String(_inEnabled ? 1 : 0);
    json += ",\"outEnabled\":" + String(_outEnabled ? 1 : 0);
    json += ",\"isOverCurrent\":" + String(_isOverCurrent ? 1 : 0);
    json += ",\"kitchenButtonState\":" + String(_kitchenButtonState);
    json += ",\"consButtonPressed\":" + String(_conservatoryButtonState);
    json += ",\"pirSenseInActive\":" + String(_pirSenseInActive ? 1 : 0);
    json += ",\"pirSenseOutActive\":" + String(_pirSenseOutActive ? 1 : 0);
    json += ",\"avgCurrent\":" + String(_avgCurrent.getAverage());
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
    stateHash.push_back(_isOpen ? 1 : 0);
    stateHash.push_back(_inEnabled ? 1 : 0);
    stateHash.push_back(_outEnabled ? 1 : 0);
    stateHash.push_back(_isOverCurrent ? 1 : 0);
    stateHash.push_back(_kitchenButtonState);
    stateHash.push_back(_conservatoryButtonState);
    stateHash.push_back(_pirSenseInActive ? 1 : 0);
    stateHash.push_back(_pirSenseOutActive ? 1 : 0);
    stateHash.push_back(_avgCurrent.getAverage());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::testMotorEnable(bool en)
{

}

void DoorOpener::testMotorTurnTo(double degrees)
{
    String moveCmd = R"({"rel":0,"nosplit":1,"speed":10,"speedOk":1,"pos":[{"a":0,"p":__POS__}]})";
    moveCmd.replace("__POS__", String(degrees));
    if (_pStepper)
        _pStepper->sendCmdJSON(moveCmd.c_str());
}
