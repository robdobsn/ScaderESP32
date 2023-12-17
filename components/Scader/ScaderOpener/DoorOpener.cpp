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
#include <HWElemConsts.h>

#define DEBUG_DOOR_OPENER_STATUS_RATE_MS 5000

static const char* MODULE_PREFIX = "DoorOpener";

DoorOpener::DoorOpener() :
    _consvPIRChangeDetector(std::bind(&DoorOpener::onConservatoryPIRChanged, this, std::placeholders::_1, std::placeholders::_2))
{
}

DoorOpener::~DoorOpener()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::setup(ConfigBase& config)
{
    // Setup motor and angle sensor
    _motorAndAngleSensor.setup(config, "MotorAndAngleSensor");

    // Record config
    recordConfig(config);

    // Configure conservatory button and PIR GPIO pins
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("consvButtonPin", ConfigPinMap::GPIO_INPUT_PULLUP, &_conservatoryButtonPin),
        ConfigPinMap::PinDef("consvPirPin", ConfigPinMap::GPIO_INPUT_PULLDOWN, &_consvPirSensePin)
    };
    ConfigPinMap::configMultiple(config, gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Setup conservatory button
    _conservatoryButton.setup(_conservatoryButtonPin, true, 0, 
                std::bind(&DoorOpener::onConservatoryButtonPressed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                DebounceButton::DEFAULT_PIN_DEBOUNCE_MS,
                5000);

    // Setup conservatory PIR change detector
    _consvPIRChangeDetector.setup(true);

    // Move and open times
    _doorTimeToOpenSecs = config.getLong("DoorTimeToOpenSecs", DEFAULT_DOOR_TIME_TO_OPEN_SECS);
    _doorRemainOpenTimeSecs = config.getLong("DoorRemainOpenTimeSecs", DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS);
    
    // Get open and closed door angles
    _doorClosedAngleDegs = config.getLong("DoorClosedAngleDegs", 0);
    _doorOpenAngleDegs = config.getLong("DoorOpenAngleDegs", 0);

    // Read from non-volatile storage
    readFromNVS();

    // Set motor speed
    _motorAndAngleSensor.setMotorSpeedFromDegreesAndSecs(std::abs(_doorOpenAngleDegs - _doorClosedAngleDegs), _doorTimeToOpenSecs);

    // Debug
    // // LOG_I(MODULE_PREFIX, "setup HBridgeEn=%d HBridgePhase=%d HBridgeVissen=%d", _hBridgeEn, _hBridgePhase, _hBridgeVissen);
    LOG_I(MODULE_PREFIX, "setup Conservatory: buttonPin %d pirPin %d",
                _conservatoryButtonPin, _consvPirSensePin);
    LOG_I(MODULE_PREFIX, "setup DoorClosedAngle %ddegs DoorOpenAngle %ddegs DoorTimeToOpen %ds DoorRemainOpenTime %ds DoorMoveSpeed %.2fdegs/s", 
                _doorClosedAngleDegs, _doorOpenAngleDegs, _doorTimeToOpenSecs, _doorRemainOpenTimeSecs, 
                _motorAndAngleSensor.getMotorSpeedDegsPerSec());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::service()
{
    // Service motor and angle sensor
    _motorAndAngleSensor.service();

    // Service the conservatory button
    _conservatoryButton.service();

    // Service the conservatory PIR change detector
    _consvPIRChangeDetector.service(digitalRead(_consvPirSensePin) != 0);
    
    // Handle kitchen PIR state changes
    bool kitchenPIRValue = false;
    if (getKitchenPIRStateChangedAndClear(kitchenPIRValue))
    {
        LOG_I(MODULE_PREFIX, "service kitchenPIRValue %d", kitchenPIRValue);
        onKitchenPIRChanged(kitchenPIRValue);
    }

    // Handle open/close toggle state changes
    bool openCloseToggleValue = false;
    if (getOpenCloseToggleStateChangedAndClear(openCloseToggleValue))
    {
        LOG_I(MODULE_PREFIX, "service openCloseToggleValue %d", openCloseToggleValue);
        onOpenCloseToggleChanged(openCloseToggleValue);
    }

    // Service door state
    serviceDoorState();

    // Update UI button label
    String buttonLabel;
    switch(getOpenerState())
    {
        case DOOR_STATE_CLOSED:
            buttonLabel = "Open";
            break;
        case DOOR_STATE_OPENING:
        case DOOR_STATE_CLOSING:
            buttonLabel = "Stop";
            break;
        case DOOR_STATE_AJAR:
        case DOOR_STATE_OPEN:
            buttonLabel = "Close";
            break;
    }
    uiModuleSetOpenCloseButtonLabel(buttonLabel);

    // Update UI module with angle, etc
    String uiStatusLineA = getOpenerStateStr(getOpenerState());
    uiStatusLineA += " " + String(calcDegreesFromClosed(_motorAndAngleSensor.getMeasuredAngleDegs()), 0) + "d";
    uiModuleSetStatusStr(1, uiStatusLineA);
    if (calcTimeBeforeCloseSecs() > 0)
    {
        String uiStatusLineB = "Close in ";
        uiStatusLineB += String((int)calcTimeBeforeCloseSecs()) + "s";
        uiModuleSetStatusStr(2, uiStatusLineB);
    }
    else
    {
        uiModuleSetStatusStr(2, "");
    }

    // Save to non-volatile storage
    saveToNVSIfRequired();

#ifdef DEBUG_DOOR_OPENER_STATUS_RATE_MS
    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, DEBUG_DOOR_OPENER_STATUS_RATE_MS))
    {
        _debugLastDisplayMs = millis();
        LOG_I(MODULE_PREFIX, "service angle %.2f speed %.2fdegs/sec state %s timeInState %ds", 
                _motorAndAngleSensor.getMeasuredAngleDegs(),
                _motorAndAngleSensor.getMeasuredAngularSpeedDegsPerSec(),
                getOpenerStateStr(getOpenerState()),
                (int)Raft::timeElapsed(millis(), getOpenerStateLastMs()) / 1000);
    }
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Open/Close door
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::startDoorOpening(String debugMsg)
{
    // Initiate door opening
    _motorAndAngleSensor.moveToAngleDegs(_doorOpenAngleDegs);
    setOpenerState(DOOR_STATE_OPENING, debugMsg + " (go-to-angle " + String(_doorOpenAngleDegs) + ")");
}

void DoorOpener::startDoorClosing(String debugMsg)
{
    // Increment angle by a small amount to ensure the door is fully closed
    float requiredDoorAngleDegs = _doorClosedAngleDegs < _doorOpenAngleDegs ? 
                    _doorClosedAngleDegs - DOOR_CLOSED_ANGLE_ADDITIONAL_DEGS : 
                    _doorClosedAngleDegs + DOOR_CLOSED_ANGLE_ADDITIONAL_DEGS;
        
    // Initiate door closing
    _motorAndAngleSensor.moveToAngleDegs(requiredDoorAngleDegs);
    setOpenerState(DOOR_STATE_CLOSING, debugMsg + " (go-to-angle " + String(requiredDoorAngleDegs) + ")");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop and disable door
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::doorStopAndDisable(String debugMsg)
{
    // WebUI command
    _motorAndAngleSensor.stop();
    setOpenerState(DOOR_STATE_AJAR, debugMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move door to specified angle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::debugMoveToAngle(int32_t angleDegs)
{
    // Check if angle in range
    if (angleDegs < 30)
        angleDegs = 30;
    if (angleDegs > 300)
        angleDegs = 300;
    _motorAndAngleSensor.moveToAngleDegs(angleDegs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conservatory button pressed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::onConservatoryButtonPressed(int isActive, uint32_t timeSinceLastPressMs, uint16_t repeatCount)
{
    // Debug
    LOG_I(MODULE_PREFIX, "onConservatoryButtonPressed %s", isActive ? "Y" : "N");

    // Handle new press
    if (isActive)
    {
        switch(getOpenerState())
        {
            case DOOR_STATE_AJAR:
            case DOOR_STATE_CLOSED:
                // When closed or ajar - open fully
                startDoorOpening("onConservatoryButtonPressed door opening");
                break;
            case DOOR_STATE_OPENING:
            case DOOR_STATE_CLOSING:
                // When opening or closing - stop
                doorStopAndDisable("onConservatoryButtonPressed door stopped");
                break;
            case DOOR_STATE_OPEN:
                // When open - close
                startDoorClosing("onConservatoryButtonPressed door closing");
                break;
        }        
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conservatory PIR changed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::onConservatoryPIRChanged(bool isActive, uint32_t timeSinceLastChange)
{
    // Debug
    LOG_I(MODULE_PREFIX, "onConservatoryPIRChanged %s", isActive ? "Y" : "N");

    // Check for currently closed or ajar, PIR active and in enabled
    if (((getOpenerState() == DOOR_STATE_CLOSED) || (getOpenerState() == DOOR_STATE_AJAR)) && isActive && _inEnabled)
    {
        // Open the door
        startDoorOpening("onConservatoryPIRChanged door opening");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Kitchen PIR changed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::onKitchenPIRChanged(bool isActive)
{
    // Debug
    LOG_I(MODULE_PREFIX, "onKitchenPIRChanged %s", isActive ? "Y" : "N");

    // Check for currently closed or ajar, PIR active and out enabled
    if (((getOpenerState() == DOOR_STATE_CLOSED) || (getOpenerState() == DOOR_STATE_AJAR)) && isActive && _outEnabled)
    {
        // Open the door
        startDoorOpening("onKitchenPIRChanged door opening");
    }

    // Remember current state
    _isKitchenPIRActive = isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Open/Close toggle changed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::onOpenCloseToggleChanged(bool isActive)
{
    // Debug
    LOG_I(MODULE_PREFIX, "onOpenCloseToggleChanged %s", isActive ? "Y" : "N");

    // Handle the command based on current state
    switch(getOpenerState())
    {
        case DOOR_STATE_CLOSED:
            // When closed - open fully
            startDoorOpening("onOpenCloseToggleChanged door opening");
            break;
        case DOOR_STATE_OPENING:
        case DOOR_STATE_CLOSING:
            // When opening or closing - stop
            doorStopAndDisable("onOpenCloseToggleChanged door stopped");
            break;
        case DOOR_STATE_AJAR:
        case DOOR_STATE_OPEN:
            // When open or ajar - close
            startDoorClosing("onOpenCloseToggleChanged door closing");
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service door state
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorOpener::serviceDoorState()
{
    switch(getOpenerState())
    {
        case DOOR_STATE_AJAR:
            // Check if moved (manually) to open or closed positions
            if (_motorAndAngleSensor.isNearTargetAngle(_doorOpenAngleDegs, DOOR_OPEN_ANGLE_TOLERANCE_DEGS, -100))
                setOpenerState(DOOR_STATE_OPEN, "serviceDoorState door opened manually");
            else if (_motorAndAngleSensor.isNearTargetAngle(_doorClosedAngleDegs, 100, -DOOR_CLOSED_ANGLE_TOLERANCE_DEGS))
                setOpenerState(DOOR_STATE_CLOSED, "serviceDoorState door closed manually");
            // Check if maximum time in AJAR state exceeded
            else if (Raft::isTimeout(millis(), getOpenerStateLastMs(), _doorRemainOpenTimeSecs*1000))
            {
                // Check if in enabled
                if (_inEnabled || _outEnabled)
                {
                    startDoorClosing("serviceDoorState door ajar for " + String(_doorRemainOpenTimeSecs) + "s - closing");
                }
            }
            break;
        case DOOR_STATE_CLOSED:
            // Check if the door is opening (manually)
            if (!_motorAndAngleSensor.isNearTargetAngle(_doorClosedAngleDegs, 100, -DOOR_CLOSED_ANGLE_TOLERANCE_DEGS))
                setOpenerState(DOOR_STATE_AJAR, "serviceDoorState door opening manually - set to AJAR curAngle " + 
                            String(_motorAndAngleSensor.getMeasuredAngleDegs()) + " closedAngle " + String(_doorClosedAngleDegs) +
                            " tolerance pos +100 neg -1");
            break;
        case DOOR_STATE_OPENING:
            // Check if reached open position
            if (_motorAndAngleSensor.isNearTargetAngle(_doorOpenAngleDegs, DOOR_OPEN_ANGLE_TOLERANCE_DEGS, -100))
                setOpenerState(DOOR_STATE_OPEN, "serviceDoorState door at open pos");
            // Check if motor has stopped moving for some time
            else if (_motorAndAngleSensor.isStoppedForTimeMs(1000))
                setOpenerState(DOOR_STATE_AJAR, "serviceDoorState door stopped opening");
            break;
        case DOOR_STATE_CLOSING:
            // Check if reached closed position
            if (_motorAndAngleSensor.isNearTargetAngle(_doorClosedAngleDegs, 100, -DOOR_CLOSED_ANGLE_TOLERANCE_DEGS))
                setOpenerState(DOOR_STATE_CLOSED, "serviceDoorState door at closed pos");
            // Check if motor has stopped moving for some time
            else if (_motorAndAngleSensor.isStoppedForTimeMs(1000))
                setOpenerState(DOOR_STATE_AJAR, "serviceDoorState door stopped closing");
            break;
        case DOOR_STATE_OPEN:
            // Check if the door is closing (manually)
            if (!_motorAndAngleSensor.isNearTargetAngle(_doorOpenAngleDegs, DOOR_OPEN_ANGLE_TOLERANCE_DEGS, -100))
                setOpenerState(DOOR_STATE_AJAR, "serviceDoorState door closing manually");
            // Check for maximum time in this state and either in or out enabled
            else if (Raft::isTimeout(millis(), getOpenerStateLastMs(), _doorRemainOpenTimeSecs*1000) && (_inEnabled || _outEnabled))
            {
                startDoorClosing("serviceDoorState door open for " + String(_doorRemainOpenTimeSecs) + "s - closing");
            }
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate angle from closed (degrees)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float DoorOpener::calcDegreesFromClosed(float measuredAngleDegrees)
{
    // Calculate angle (in degrees)
    return std::abs(measuredAngleDegrees - _doorClosedAngleDegs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate time before closing (secs)
// Will return 0 if the door is not in OPEN state
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t DoorOpener::calcTimeBeforeCloseSecs()
{
    // Check if door is open
    if (getOpenerState() != DOOR_STATE_OPEN)
        return 0;

    // Check if the door will close (either in or out enabled)
    if (!_inEnabled && !_outEnabled)
        return 0;

    // Calculate time before closing
    return _doorRemainOpenTimeSecs - Raft::timeElapsed(millis(), getOpenerStateLastMs()) / 1000;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String DoorOpener::getStatusJSON(bool includeBraces)
{
    // bool isValid = false;
    String json = "";
    json += "\"motorActive\":" + String(_motorAndAngleSensor.isMotorActive() ? 1 : 0);
    json += ",\"inEnabled\":" + String(_inEnabled ? 1 : 0);
    json += ",\"outEnabled\":" + String(_outEnabled ? 1 : 0);
    json += ",\"consButtonPressed\":" + String(_conservatoryButton.isButtonPressed() ? 1 : 0);
    json += ",\"pirSenseInActive\":" + String(_consvPIRChangeDetector.getState() ? 1 : 0);
    json += ",\"pirSenseOutActive\":" + String(_isKitchenPIRActive ? 1 : 0);
    json += ",\"doorCurAngle\":" + String(_motorAndAngleSensor.getMeasuredAngleDegs());
    json += ",\"doorOpenAngleDegs\":" + String(_doorOpenAngleDegs);
    json += ",\"doorClosedAngleDegs\":" + String(_doorClosedAngleDegs);
    json += ",\"timeBeforeCloseSecs\":" + String(calcTimeBeforeCloseSecs());
    json += ",\"doorStateCode\":" + String(getOpenerState());
    json += ",\"doorStateStr\":\"" + getOpenerStateStr(getOpenerState()) + "\"";
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
    stateHash.push_back(_motorAndAngleSensor.isMotorActive() ? 1 : 0);
    stateHash.push_back(_inEnabled ? 1 : 0);
    stateHash.push_back(_outEnabled ? 1 : 0);
    stateHash.push_back(_conservatoryButton.isButtonPressed() ? 1 : 0);
    stateHash.push_back(_consvPIRChangeDetector.getState() ? 1 : 0);
    stateHash.push_back(_isKitchenPIRActive ? 1 : 0);
    stateHash.push_back(calcTimeBeforeCloseSecs() & 0xFF);
    stateHash.push_back(int(_motorAndAngleSensor.getMeasuredAngleDegs()) & 0xff);
    stateHash.push_back(_doorOpenAngleDegs & 0xff);
    stateHash.push_back(_doorClosedAngleDegs & 0xff);
    stateHash.push_back(getOpenerState());
}
