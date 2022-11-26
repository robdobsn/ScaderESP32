/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderShades
//
// Rob Dobson 2013-2021
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SysModBase.h>

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
    int _shadeIdx;
    WindowShadesSeqStep _steps[MAX_SEQ_STEPS];

    WindowShadesSeq()
    {
        _isBusy = false;
        _curStep = 0;
        _numSteps = 0;
        _shadeIdx = 0;
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

class ScaderShades : public SysModBase
{
public:
    static const uint32_t PULSE_ON_MILLISECS = 500;
    static const uint32_t MAX_SHADE_ON_MILLSECS = 60000;
    static const int SHADE_UP_BIT_MASK = 1;
    static const int SHADE_STOP_BIT_MASK = 2;
    static const int SHADE_DOWN_BIT_MASK = 4;
    static const int BITS_PER_SHADE = 3;
    static const int MAX_WINDOW_SHADES = 6;

    ScaderShades(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

    // Perform commands
    bool doCommand(int shadeIdx, String &cmdStr, String &durationStr);

    // Number of shades
    int getMaxNumShades()
    {
        return MAX_WINDOW_SHADES;
    }

    // Check if the shade is moving
    bool isBusy(int shadeIdx);

protected:

    // Setup
    virtual void setup() override final;

    // Service (called frequently)
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:

    // Enabled and initalised flags
    bool _isEnabled;
    bool _isInitialised;

    // Shift register control
    int _hc595_SER;
    int _hc595_SCK;
    int _hc595_LATCH;
    int _hc595_RST;

    // Light levels
    int _lightLevel0;
    int _lightLevel1;
    int _lightLevel2;

    // Timing
    uint32_t _msTimeouts[MAX_WINDOW_SHADES];
    int _tickCounts[MAX_WINDOW_SHADES];

    // Lux reporting interval
    int _luxReportSecs;
    uint32_t _luxLastReportMs;

    // Shade control bits - each shade has 3 control bits (UP, STOP, DOWN) in that order
    // The shift register contains the bits end-to-end for each shade in sequence
    int _curShadeCtrlBits;

    // Sequence handling
    bool _sequenceRunning;
    WindowShadesSeq _sequence;

    // get light levels
    void getLightLevels(int &l1, int &l2, int &l3);

    // check if status report is required
    bool isStatusReportRequired()
    {
        // Lux reporting
        if (Raft::isTimeout(millis(), _luxLastReportMs, _luxReportSecs * 1000))
        {
            _luxLastReportMs = millis();
            return true;
        }
        return false;
    }

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
    void apiShadesControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
