/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderShades
//
// Rob Dobson 2013-2021
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftUtils.h"
#include "ScaderCommon.h"
#include "RaftSysMod.h"

class APISourceInfo;

class WindowShadesSeqStep
{
public:
    int _bitMask;
    int _pinState;
    uint32_t _msDuration;
    bool _clearExisting;

    WindowShadesSeqStep()
    {
        _bitMask = 0;
        _pinState = 0;
        _msDuration = 0;
        _clearExisting = false;
    }

    WindowShadesSeqStep(int bitMask, int pinState, long msDuration, bool clearExisting)
    {
        _bitMask = bitMask;
        _pinState = pinState;
        _msDuration = msDuration;
        _clearExisting = clearExisting;
    }
};

class WindowShadesSeq
{
public:
    static constexpr int MAX_SEQ_STEPS = 5;
    int _isBusy;
    int _curStep;
    int _numSteps;
    int _elemIdx;
    WindowShadesSeqStep _steps[MAX_SEQ_STEPS];

    WindowShadesSeq()
    {
        _isBusy = false;
        _curStep = 0;
        _numSteps = 0;
        _elemIdx = 0;
    }

    bool addStep(WindowShadesSeqStep& step)
    {
        if (_numSteps < MAX_SEQ_STEPS)
        {
            _steps[_numSteps] = step;
            if (_steps[_numSteps]._msDuration != step._msDuration)
            {
                LOG_E("WindowShadesSeq", "addStep mismatch dur %d != %d", _steps[_numSteps]._msDuration, step._msDuration);
            }
            _numSteps++;
            return true;
        }
        return false;
    }
};

class ScaderShades : public RaftSysMod
{
public:
    static const int DEFAULT_MAX_ELEMS = 5;
    static const uint32_t PULSE_ON_MILLISECS = 500;
    static const uint32_t MAX_SHADE_ON_MILLSECS = 60000;
    static const int SHADE_UP_BIT_MASK = 1;
    static const int SHADE_STOP_BIT_MASK = 2;
    static const int SHADE_DOWN_BIT_MASK = 4;
    static const int BITS_PER_SHADE = 3;

    ScaderShades(const char* pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderShades(pModuleName, sysConfig);
    }
    
    // Perform commands
    bool doCommand(int shadeIdx, String &cmdStr, String &durationStr);

    // Check if the shade is moving
    bool isBusy(int shadeIdx) const;

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() const override final;
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // Light levels
    bool _lightLevelsEnabled = false;

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // Shift register control
    int _hc595_SER = -1;
    int _hc595_SCK = -1;
    int _hc595_LATCH = -1;
    int _hc595_RST = -1;

    // Light level pins
    static const uint32_t NUM_LIGHT_LEVELS = 3;
    int _lightLevelPins[NUM_LIGHT_LEVELS] = {};

    // Timing
    uint32_t _msTimeouts[DEFAULT_MAX_ELEMS] = {};
    int _tickCounts[DEFAULT_MAX_ELEMS] = {};

    // Names of control elements
    std::vector<String> _elemNames;

    // Shade control bits - each shade has 3 control bits (UP, STOP, DOWN) in that order
    // The shift register contains the bits end-to-end for each shade in sequence
    int _curShadeCtrlBits = 0;

    // Sequence handling
    bool _sequenceRunning = false;
    WindowShadesSeq _sequence;

    // get light levels
    void getLightLevels(int lightLevels[], int numLevels) const;

    // Helper functions
    bool clearShadeBits(int shadeIdx);
    bool sendBitsToShiftRegister();
    bool setTimedOutput(int shadeIdx, int bitMask, bool bitOn, uint32_t msDuration, bool bClearExisting);
    bool setShadeBit(int shadeIdx, int bitMask, int bitIsOn);
    bool sequenceStart(int shadeIdx);
    void sequenceAdd(int mask, bool pinOn, uint32_t msDuration, bool clearExisting);
    void sequenceRun();
    void sequenceStartStep(int stepIdx);
    void sequenceStepComplete();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
