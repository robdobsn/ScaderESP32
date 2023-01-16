/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StepDriverParams
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

class StepDriverParams
{
public:
    static const uint32_t MICROSTEPS_DEFAULT = 256;
    static constexpr float EXT_SENSE_OHMS_DEFAULT = 0.11;
    static constexpr float RMS_AMPS_DEFAULT = 1.0;
    static constexpr float HOLD_MULT_DEFAULT = 1.0;
    static const uint32_t IHOLD_DELAY_DEFAULT = 0;
    static constexpr uint32_t TOFF_VALUE_DEFAULT = 5;
    static const uint32_t PWM_FREQ_KHZ_DEFAULT = 35;

    enum HoldModeEnum
    {
        HOLD_MODE_FACTOR,
        HOLD_MODE_FREEWHEEL,
        HOLD_MODE_PASSIVE_BREAKING
    };
    static const HoldModeEnum HOLD_MODE_DEFAULT = HOLD_MODE_FACTOR;
    
    StepDriverParams()
    {
        invDirn = false;
        extSenseOhms = EXT_SENSE_OHMS_DEFAULT;
        writeOnly = false;
        extVRef = false;
        extMStep = false;
        intpol = false;
        microsteps = MICROSTEPS_DEFAULT;
        minPulseWidthUs = 1;
        stepPin = -1;
        dirnPin = -1;
        rmsAmps = RMS_AMPS_DEFAULT;
        holdFactor = 1.0;
        holdDelay = 0;
        holdMode = HOLD_MODE_DEFAULT;
        pwmFreqKHz = PWM_FREQ_KHZ_DEFAULT;
        address = 0;
    }
    bool invDirn : 1;
    bool writeOnly : 1;
    bool extVRef : 1;
    bool extMStep : 1;
    bool intpol : 1;
    float extSenseOhms;
    uint16_t microsteps;
    uint16_t minPulseWidthUs;
    int stepPin;
    int dirnPin;
    float rmsAmps;
    float holdFactor;
    HoldModeEnum holdMode;
    uint32_t holdDelay;
    float pwmFreqKHz;
    uint8_t address;
};

