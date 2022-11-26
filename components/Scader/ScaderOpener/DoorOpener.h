/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Door Opener
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <DebounceButton.h>
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SimpleMovingAverage.h>
#include "stdint.h"
// #include <TinyPICO.h>
// #include <ina219.h>

class HWElemSteppers;
class BusSerial;

class DoorOpener
{
public:
    DoorOpener();
    virtual ~DoorOpener();
    void setup(ConfigBase& config);
    void service();
    void moveDoor(bool openDoor);
    void stopDoor();
    void onKitchenButtonPressed(int val);
    void onConservatoryButtonPressed(int val);
    bool isBusy();
    void setMode(bool enIntoKitchen, bool enOutOfKitchen);

    // Timing
    static const uint32_t MOVE_PENDING_TIME_MS = 500;
    static const uint32_t DOOR_MOVE_TIME_MS = 20000;
    static const uint32_t DOOR_REMAIN_OPEN_MS = 45000;
    static const uint32_t OVER_CURRENT_BLINK_TIME_MS = 200;
    static const uint32_t OVER_CURRENT_CLEAR_TIME_MS = 5000;
    static const uint32_t OVER_CURRENT_IGNORE_AT_START_OF_OPEN_MS = 5000;

    // Overcurrent threshold
    static const uint32_t CURRENT_SAMPLE_TIME_MS = 10;
    static const uint16_t CURRENT_OVERCURRENT_THRESHOLD = 800;

    // Button state (works like a menu)
    enum KitchenButtonState
    {
        KITCHEN_BUTTON_STATE_IDLE,
        KITCHEN_BUTTON_STATE_TOGGLE,
        KITCHEN_BUTTON_STATE_OUT_ONLY,
        KITCHEN_BUTTON_STATE_IN_AND_OUT,
        KITCHEN_BUTTON_STATE_IN_ONLY,
        KITCHEN_BUTTON_STATE_DISABLED,
    };
private:
    // Helpers
    void setLEDs();
    void servicePIR(const char* pName, uint32_t pirPin, bool& pirActive, 
                uint32_t& pirActiveMs, bool& pirLast, bool dirEnabled);
    void motorControl(bool isEn, bool dirn);

    // Stepper motor
    HWElemSteppers* _pStepper;
    BusSerial* _pBusSerial;

    // Pin Settings
    // int _hBridgeEn;
    // int _hBridgePhase;
    // int _hBridgeVissen;
    int _kitchenButtonPin;
    int _conservatoryButtonPin;
    int _ledOutEnPin;
    int _ledInEnPin;
    int _pirSenseInPin;
    int _pirSenseOutPin;

    // Other settings
    uint32_t _doorOpenTimeMs;
    uint32_t _doorMoveTimeMs;
    uint32_t _currentLastSampleMs;
    uint32_t _overCurrentBlinkTimeMs;
    uint32_t _overCurrentStartTimeMs;

    // State
    bool _isOpen;
    uint32_t _doorOpenedTimeMs;
    bool _inEnabled;
    bool _outEnabled;
    bool _isOverCurrent;
    bool _overCurrentBlinkCurOn;

    // Button debounce
    DebounceButton _kitchenButton;
    DebounceButton _conservatoryButton;
    uint32_t _buttonPressTimeMs;

    // Button press looping while going through modes
    static const uint32_t KITCHEN_BUTTON_STATE_LOOP_START = KITCHEN_BUTTON_STATE_OUT_ONLY;
    static const uint32_t KITCHEN_BUTTON_STATE_LOOP_END = KITCHEN_BUTTON_STATE_DISABLED;
    uint32_t _kitchenButtonState;

    // PIR
    bool _pirSenseInLast;
    bool _pirSenseInActive;
    uint32_t _pirSenseInActiveMs;
    bool _pirSenseOutActive;
    bool _pirSenseOutLast;
    uint32_t _pirSenseOutActiveMs;

    // Vissen which is the motor current sense
    SimpleMovingAverage<200> _avgCurrent;

    // // TinyPICO hardware
    // TinyPICO _tinyPico;

    // Debug
    uint32_t _debugLastDisplayMs;
};
