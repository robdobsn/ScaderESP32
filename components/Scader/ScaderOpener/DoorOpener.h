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
#include <HWElemSteppers.h>
#include "stdint.h"
// #include <TinyPICO.h>
// #include <ina219.h>

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
    String getStatusJSON(bool includeBraces);
    void getStatusHash(std::vector<uint8_t>& stateHash);
    void motorControl(bool isEn, bool dirn, double openDegrees, double closedDegrees);

    // Timing
    static const uint32_t MOVE_PENDING_TIME_MS = 500;
    static const uint32_t OVER_CURRENT_BLINK_TIME_MS = 200;
    static const uint32_t OVER_CURRENT_CLEAR_TIME_MS = 5000;
    static const uint32_t OVER_CURRENT_IGNORE_AT_START_OF_OPEN_MS = 5000;
    static const uint32_t DEFAULT_DOOR_OPEN_ANGLE = 120;
    static const uint32_t DEFAULT_DOOR_CLOSED_ANGLE = 0;
    static const uint32_t DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS = 45;
    static const uint32_t DEFAULT_DOOR_TIME_TO_OPEN_SECS = 20;

    // Overcurrent threshold
    static const uint32_t CURRENT_SAMPLE_TIME_MS = 10;
    static const uint16_t CURRENT_OVERCURRENT_THRESHOLD = 800;

    // Button state (works like a menu)
    enum OpenerInOutMode_t
    {
        OPENER_IN_OUT_MODE_IDLE,
        OPENER_IN_OUT_MODE_TOGGLE,
        OPENER_IN_OUT_MODE_OUT_ONLY,
        OPENER_IN_OUT_MODE_IN_AND_OUT,
        OPENER_IN_OUT_MODE_IN_ONLY,
        OPENER_IN_OUT_MODE_DISABLED,
    };

    static const char* getOpenerInOutModeStr(uint32_t mode)
    {
        switch(mode)
        {
            case OPENER_IN_OUT_MODE_IDLE: return "Idle";
            case OPENER_IN_OUT_MODE_TOGGLE: return "Toggle";
            case OPENER_IN_OUT_MODE_OUT_ONLY: return "OutOnly";
            case OPENER_IN_OUT_MODE_IN_AND_OUT: return "InAndOut";
            case OPENER_IN_OUT_MODE_IN_ONLY: return "InOnly";
            case OPENER_IN_OUT_MODE_DISABLED: return "Disabled";
        }
        return "Unknown";
    }

    // // Default door open angle
    // static const uint32_t DEFAULT_DOOR_OPEN_ANGLE = 30;
    // void setOpenAngleDegrees(uint32_t angleDegrees) { _doorOpenAngleDegrees = angleDegrees; }
    // uint32_t getOpenAngleDegrees() { return _doorOpenAngleDegrees; }

    // // Motor on time
    // static const uint32_t DEFAULT_MOTOR_ON_TIME_SECS = 1;
    // void setMotorOnTimeAfterMoveSecs(uint32_t secs) 
    // {
    //     if (_pStepper)
    //         _pStepper->setMotorOnTimeAfterMoveSecs(secs);
    //     _motorOnTimeAfterMoveSecs = secs; 
    // }
    // uint32_t getMotorOnTimeAfterMoveSecs() { return _motorOnTimeAfterMoveSecs; }

    // // Set door open time secs
    // static const uint32_t DEFAULT_DOOR_OPEN_TIME_SECS = 45;
    // void setDoorOpenTimeSecs(uint32_t secs) { _doorOpenTimeSecs = secs; }
    // uint32_t getDoorOpenTimeSecs() { return _doorOpenTimeSecs; }

private:
    // Helpers
    void setLEDs();
    void servicePIR(const char* pName, uint32_t pirPin, bool& pirActive, 
                uint32_t& pirActiveMs, bool& pirLast, bool dirEnabled);
    void clear();
    uint32_t getSecsBeforeClose()
    {
        return _isOpen && (_doorOpenedTimeMs != 0) && (_inEnabled || _outEnabled) ? 
                Raft::timeToTimeout(millis(), _doorOpenedTimeMs, _doorRemainOpenTimeSecs*1000) / 1000 : 0;
    }

    // Stepper motor
    HWElemSteppers* _pStepper = nullptr;
    BusSerial* _pBusSerial = nullptr;

    // Enabled
    bool _isEnabled = false;

    // Pin Settings
    int _kitchenButtonPin = -1;
    int _conservatoryButtonPin = -1;
    int _ledOutEnPin = -1;
    int _ledInEnPin = -1;
    int _pirSenseInPin = -1;
    int _pirSenseOutPin = -1;

    // Other settings
    uint32_t _doorRemainOpenTimeSecs = 0;
    uint32_t _doorTimeToOpenSecs = 0;
    uint32_t _currentLastSampleMs = 0;
    uint32_t _overCurrentBlinkTimeMs = 0;
    uint32_t _overCurrentStartTimeMs = 0;
    uint32_t _doorOpenAngleDegrees = DEFAULT_DOOR_OPEN_ANGLE;
    uint32_t _doorClosedAngleDegrees = DEFAULT_DOOR_CLOSED_ANGLE;
    uint32_t _motorOnTimeAfterMoveSecs = 1;
    float _maxMotorCurrentAmps = 0.1;

    // State
    bool _isOpen = false;
    uint32_t _doorOpenedTimeMs = 0;
    bool _inEnabled = false;
    bool _outEnabled = false;
    bool _isOverCurrent = false;
    bool _overCurrentBlinkCurOn = false;

    // Button debounce
    DebounceButton _kitchenButton;
    DebounceButton _conservatoryButton;
    uint32_t _buttonPressTimeMs = 0;
    bool _conservatoryButtonState = false;

    // Button press looping while going through modes
    static const uint32_t OPENER_IN_OUT_MODE_LOOP_START = OPENER_IN_OUT_MODE_OUT_ONLY;
    static const uint32_t OPENER_IN_OUT_MODE_LOOP_END = OPENER_IN_OUT_MODE_DISABLED;
    uint32_t _openerInOutMode = OPENER_IN_OUT_MODE_IDLE;

    // PIR
    bool _pirSenseInLast = false;
    bool _pirSenseInActive = false;
    uint32_t _pirSenseInActiveMs = 0;
    bool _pirSenseOutActive = false;
    bool _pirSenseOutLast = false;
    uint32_t _pirSenseOutActiveMs = 0;

    // Vissen which is the motor current sense
    MovingAverage<200> _avgCurrent;

    // // TinyPICO hardware
    // TinyPICO _tinyPico;

    // Debug
    uint32_t _debugLastDisplayMs = 0;
};
