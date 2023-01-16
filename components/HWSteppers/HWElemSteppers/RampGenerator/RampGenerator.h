/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RampGenerator
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MotionBlock.h"
#include <Logger.h>
// #include "driver/timer.h"

#define DEBUG_GENERATE_DETAILED_STATS

class MotionPipelineIF;
class RampGenTimer;
class StepDriverBase;
class EndStops;

class RampGenerator
{
public:
    // Constructor / destructor
    RampGenerator(MotionPipelineIF& motionPipeline, RampGenTimer& rampGenTimer);
    virtual ~RampGenerator();

    // Setup ramp generator
    void setup(bool useRampGenTimer, std::vector<StepDriverBase*> stepperDrivers,
            std::vector<EndStops*> axisEndStops);

    // Must be called frequently - if useRampGenTimer is false (in setup) then
    // this function generates stepping pulses
    void service();

    // Enable
    void enable(bool en);

    // Control
    void stop();
    void pause(bool pauseIt);

    // Access to current state
    void resetTotalStepPosition();
    void getTotalStepPosition(AxisInt32s& actuatorPos);
    void setTotalStepPosition(int axisIdx, int32_t stepPos);

    // End stop handling
    void clearEndstopReached();
    void getEndStopStatus(AxisEndstopChecks& axisEndStopVals);
    bool isEndStopReached();

    // Progress
    // int getLastCompletedNumberedCmdIdx();

    // Stats
    class RampGenStats
    {
    public:
        RampGenStats();
        void clear();
        void IRAM_ATTR startMotionProcessing();
        void IRAM_ATTR endMotionProcessing();
        void IRAM_ATTR update(uint32_t curAccumulatorStep, 
                uint32_t curStepRatePerTTicks,
                uint32_t curAccumulatorNS,
                int axisIdxWithMaxSteps,
                uint32_t accStepsPerTTicksPerMS,
                uint32_t curStepCountMajorAxis,
                uint32_t stepsBeforeDecel,
                uint32_t maxStepRatePerTTicks);
        void IRAM_ATTR stepDirn(uint32_t axisIdx, bool dirnPositive);
        void IRAM_ATTR stepStart(uint32_t axisIdx);
        String getStatsStr();

    private:
        // Stats
        uint64_t _isrStartUs;
        uint64_t _isrAccUs;
        uint32_t _isrCount;
        float _isrAvgUs;
        bool _isrAvgValid;
        uint32_t _isrMaxUs;
#ifdef DEBUG_GENERATE_DETAILED_STATS
        uint32_t _curAccumulatorStep;
        uint32_t _curStepRatePerTTicks;
        uint32_t _curAccumulatorNS;
        int _axisIdxWithMaxSteps;
        uint32_t _accStepsPerTTicksPerMS;
        uint32_t _curStepCountMajorAxis;
        uint32_t _stepsBeforeDecel;
        uint32_t _maxStepRatePerTTicks;
#endif
    };
    RampGenStats& getStats()
    {
        return _stats;
    }
    void debugShowStats();

private:
    // If this is true nothing will move
    volatile bool _isPaused;

    // Steps moved in total and increment based on direction
    volatile int32_t _axisTotalSteps[AXIS_VALUES_MAX_AXES];
    volatile int32_t _totalStepsInc[AXIS_VALUES_MAX_AXES];

    // Pipeline of blocks to be processed
    MotionPipelineIF& _motionPipeline;

    // Ramp generation timer
    RampGenTimer& _rampGenTimer;
    bool _useRampGenTimer;
    uint32_t _stepGenPeriodNs;
    uint32_t _minStepRatePerTTicks;

    // Steppers
    std::vector<StepDriverBase*> _stepperDrivers;
    
    // Endstops
    std::vector<EndStops*> _axisEndStops;

    // Ramp generation enabled
    bool _rampGenEnabled;
    // Last completed numbered command
    // volatile int _lastDoneNumberedCmdIdx;
    // Steps
    volatile uint32_t _stepsTotalAbs[AXIS_VALUES_MAX_AXES];
    volatile uint32_t _curStepCount[AXIS_VALUES_MAX_AXES];
    // Current step rate (in steps per K ticks)
    volatile uint32_t _curStepRatePerTTicks;
    // Accumulators for stepping and acceleration increments
    volatile uint32_t _curAccumulatorStep;
    volatile uint32_t _curAccumulatorNS;
    volatile uint32_t _curAccumulatorRelative[AXIS_VALUES_MAX_AXES];

    // End stop handling
    volatile bool _endStopReached;
    volatile int _endStopCheckNum;
    struct EndStopChecks
    {
        uint8_t axisIdx;
        bool isMax;
        bool checkHit;
    };
    volatile EndStopChecks _endStopChecks[AXIS_VALUES_MAX_AXES];

    // Stats
    RampGenStats _stats;

    // Helpers
    void IRAM_ATTR generateMotionPulses();
    bool IRAM_ATTR handleStepEnd();
    void setupNewBlock(MotionBlock *pBlock);
    void updateMSAccumulator(MotionBlock *pBlock);
    bool handleStepMotion(MotionBlock *pBlock);
    void endMotion(MotionBlock *pBlock);

    // Timer callback
    static void IRAM_ATTR rampGenTimerCallback(void* pObject);

    // ISR count
    volatile uint32_t _isrCount;

    // Debug
    uint32_t _debugLastQueuePeekMs;
};
