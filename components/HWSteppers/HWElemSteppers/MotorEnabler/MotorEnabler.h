// RBotFirmware
// Rob Dobson 2016-21

#pragma once

#include "RaftUtils.h"
#include "time.h"
#include "ArduinoOrAlt.h"
#include "ConfigPinMap.h"

class MotorEnabler
{
public:
    static constexpr float stepDisableSecs_default = 60.0f;

    MotorEnabler()
    {
        // Stepper motor enablement
        _stepEnablePin = -1;
        _stepEnLev = true;
        _stepDisableSecs = 60.0;
        _motorEnLastMillis = 0;
        _motorEnLastUnixTime = 0;
        _motorsAreEnabled = false;
    }
    ~MotorEnabler()
    {
        deinit();
    }
    void deinit()
    {
        // disable
        if (_stepEnablePin >= 0)
            pinMode(_stepEnablePin, INPUT);
    }

    bool setup(const ConfigBase& config)
    {
        static const char* MODULE_PREFIX = "MotorEnabler";

        // Get motor enable info
        String stepEnablePinName = config.getString("stepEnablePin", "-1");
        _stepEnLev = config.getLong("stepEnLev", 1);
        _stepEnablePin = ConfigPinMap::getPinFromName(stepEnablePinName.c_str());
        _stepDisableSecs = config.getDouble("stepDisableSecs", stepDisableSecs_default);
        LOG_I(MODULE_PREFIX, "MotorEnabler: (pin %d, actLvl %d, disableAfter %fs)", _stepEnablePin, _stepEnLev, _stepDisableSecs);

        // Enable pin - initially disable
        if (_stepEnablePin >= 0)
        {
            pinMode(_stepEnablePin, OUTPUT);
            digitalWrite(_stepEnablePin, !_stepEnLev);
        }
        return true;
    }

    void enableMotors(bool en, bool timeout)
    {
        static const char* MODULE_PREFIX = "MotorEnabler";
        // LOG_I(MODULE_PREFIX, "Enable %d currentlyEn %d pin %d disable level %d, disable after time %f",
        // 							en, _motorsAreEnabled, _stepEnablePin, !_stepEnLev, _stepDisableSecs);
        if (en)
        {
            if (_stepEnablePin >= 0)
            {
                if (!_motorsAreEnabled)
                    LOG_I(MODULE_PREFIX, "MotorEnabler: enabled, disable after idle %fs (enPin %d level %d)", 
                                _stepDisableSecs, _stepEnablePin, _stepEnLev);
                digitalWrite(_stepEnablePin, _stepEnLev);
            }
            _motorsAreEnabled = true;
            _motorEnLastMillis = millis();
            time(&_motorEnLastUnixTime);
        }
        else
        {
            if (_stepEnablePin >= 0)
            {
                if (_motorsAreEnabled)
                    LOG_I(MODULE_PREFIX, "MotorEnabler: motors disabled by %s", timeout ? "timeout" : "command");
                digitalWrite(_stepEnablePin, !_stepEnLev);
            }
            _motorsAreEnabled = false;
        }
    }

    unsigned long getLastActiveUnixTime()
    {
        return _motorEnLastUnixTime;
    }

    void service()
    {
        // Check for motor enable timeout
        if (_motorsAreEnabled && Raft::isTimeout(millis(), _motorEnLastMillis,
                                                    (unsigned long)(_stepDisableSecs * 1000)))
            enableMotors(false, true);
    }

    void setMotorOnTimeAfterMoveSecs(float motorOnTimeAfterMoveSecs)
    {
        _stepDisableSecs = motorOnTimeAfterMoveSecs;
    }

private:
    // Step enable
    int _stepEnablePin;
    bool _stepEnLev = true;
    // Motor enable
    float _stepDisableSecs;
    bool _motorsAreEnabled;
    unsigned long _motorEnLastMillis;
    time_t _motorEnLastUnixTime;
};
