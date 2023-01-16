/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit tests of RampGenerator
//
// Rob Dobson 2017-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Utils.h>
#include <Logger.h>
#include <unity.h>

#include <ConfigBase.h>
#include "RampGenerator/RampGenerator.h"
#include "RampGenerator/RampGenTimer.h"
#include "RampGenerator/MotionPipeline.h"
#include "RampGenerator/MotionBlock.h"
#include "StepDrivers/StepDriverBase.h"
#include "EndStops/EndStops.h"
#include <ArduinoOrAlt.h>

static const char* MODULE_PREFIX = "RampGeneratorTest";

#define TEST_USING_TIMER_ISR
// #define TEST_PERFORMANCE_ONLY

struct StepInfo
{
    uint64_t timerCount;
    uint32_t ticksPerStep;
};

class StepDriverTest : public StepDriverBase
{

};

#ifdef RAMPGENIO_TEST
class RampGenIOTest : public RampGenIOIF
{
public:
    RampGenIOTest()
    {
        _testClockCount = 0;
        _lastStepClockCount = 0;
        _stepStarted = false;
        _lastTicksBetweenSteps = 0;
        _firstStep = true;
        _ticksToCompletion = 0;
        _minTicksPerStep = 1000000;
    }
    virtual void setup(const ConfigBase& config) override final
    {
        // LOG_I(MODULE_PREFIX, "setup with %s", config.getConfigString());
        _testClockCount = 0;
        _lastStepClockCount = 0;
        _stepStarted = false;
        _lastTicksBetweenSteps = 0;
        _firstStep = true;
        _ticksToCompletion = 0;
        _minTicksPerStep = 1000000;
        _stepPositions.clear();
    }
    // Tick indicator - called to indicate ticks elapsing
    virtual void IRAM_ATTR tickIndicator() override final
    {
#ifndef TEST_PERFORMANCE_ONLY
        _testClockCount++;
#endif
    }
    virtual void IRAM_ATTR service() override final
    {

    }
    virtual void IRAM_ATTR getEndStopStatus(AxisEndstopChecks& axisEndStopVals) override final
    {
    }
    virtual bool IRAM_ATTR getEndStopConfig(uint32_t axisIdx, bool isMin, int& pin, bool& activeLevel) override final
    {
        return false;
    }
    virtual void IRAM_ATTR setDirection(uint32_t axisIdx, bool direction) override final
    {
    }
    virtual bool IRAM_ATTR readEndStopViaPinNum(uint32_t pinNum) override final
    {
        return false;
    }
    virtual void IRAM_ATTR stepStart(uint32_t axisIdx) override final
    {
#ifndef TEST_PERFORMANCE_ONLY
        _stepStarted = true;
        _ticksToCompletion = _testClockCount;

        // Get time (in clock ticks) between steps (velocity)
        uint32_t ticksPerStep = _testClockCount - _lastStepClockCount;
        _lastStepClockCount = _testClockCount;

        // Get difference (acceleration)
        bool isChange = (_firstStep || (std::abs(int(ticksPerStep - _lastTicksBetweenSteps)) >= 2));
        // if (isChange)
        // {
        //     LOG_I(MODULE_PREFIX, "%d %d axisIdx %d ticks %lld ticksPerStep %d", 
        //                 isChange, 
        //                 std::abs(int(ticksPerStep - _lastTicksBetweenSteps)),
        //                 axisIdx, _testClockCount, ticksPerStep);
        // }
        if (isChange)
        {
            if (_minTicksPerStep > ticksPerStep)
                _minTicksPerStep = ticksPerStep;
            if (_stepPositions.size() == 1)
                _earlyTicksPerStep = ticksPerStep;
            StepInfo stepInfo = { _testClockCount, ticksPerStep };
            _stepPositions.push_back(stepInfo);
        }
        if (isChange)
            _lastTicksBetweenSteps = ticksPerStep;
        _firstStep = false;
#endif
    }
    virtual bool IRAM_ATTR stepEnd(uint32_t axisIdx) override final
    {
#ifndef TEST_PERFORMANCE_ONLY
        bool wasStarted = _stepStarted;
        _stepStarted = false;
        return wasStarted;
#else
        return false;
#endif
    }
    std::vector<StepInfo>& getTestResults(uint32_t& minTicksPerStep, 
            uint32_t& earlyTicksPerStep, uint32_t& lateTicksPerStep,
            uint32_t& ticksToCompletion)
    {
        if (_stepPositions.size() > 1)
            earlyTicksPerStep = _stepPositions[1].ticksPerStep;
        if (_stepPositions.size() > 2)
            lateTicksPerStep = _stepPositions[_stepPositions.size()-2].ticksPerStep;
        minTicksPerStep = _minTicksPerStep;
        ticksToCompletion = _ticksToCompletion;
        return _stepPositions;
    }
private:
    uint64_t _testClockCount;
    std::vector<StepInfo> _stepPositions;
    bool _stepStarted;
    uint64_t _lastStepClockCount;
    uint64_t _lastTicksBetweenSteps;
    bool _firstStep;
    uint32_t _minTicksPerStep;
    uint32_t _earlyTicksPerStep;
    uint32_t _lateTicksPerStep;
    uint32_t _ticksToCompletion;
};
#endif

static const char* RAMP_GEN_TEST_STEPPER_JSON =
{
#ifdef TEST_USING_TIMER_ISR    
    R"("timerIntr":1,)"
#else
    R"("timerIntr":0,)"
#endif
    R"("axes":[)"
        R"({)"
            R"("name":"X",)"
            R"("params":)"
            R"({)"
                R"("maxSpeed":75,)"
                R"("maxAcc":"100",)"
                R"("maxRPM":"600",)"
                R"("stepsPerRot":"1000",)"
                R"("maxVal":"100")"
            R"(},)"
            R"("driver":)"
            R"({)"            
                R"("driver":"TMC2209",)"
                R"("hw":"local",)"
                R"("stepPin":"15",)"
                R"("dirnPin":"12",)"
                R"("invDirn":0)"
            R"(})"
        R"(})"
    R"(])"
};

TEST_CASE("test_RampGenerator", "[RampGenerator]")
{
    // Debug
    LOG_I(MODULE_PREFIX, "RampGenerator Test");

    // Steppers
    std::vector<StepDriverBase*> steppers;
    steppers.push_back(new StepDriverTest());

    // End stops
    std::vector<EndStops*> endStops;

    // Axes
    AxesParams axesParams;
    axesParams.setupAxes(RAMP_GEN_TEST_STEPPER_JSON);
    MotionPipeline motionPipeline;
    motionPipeline.setup(10);
    RampGenTimer rampGenTimer;

    RampGenerator rampGen(motionPipeline, rampGenTimer, steppers, endStops);
    rampGen.setup(RAMP_GEN_TEST_STEPPER_JSON);
    rampGen.enable(true);
    rampGen.pause(false);

    LOG_I(MODULE_PREFIX, "TESTING ..........................")
//     MotionBlock motionBlock;
//     // Set tick time
//     motionBlock.setTimerPeriodNs(rampGenTimer.getPeriodUs() * 1000);
//     // Requested speed in mm/s
//     motionBlock._requestedVelocity = 100;
//     // Move distance
//     motionBlock._moveDistPrimaryAxesMM = 10;
//     // Primary axis vector
//     motionBlock._unitVecAxisWithMaxDist = 1;
//     // Max extry speed mm/s
//     motionBlock._maxEntrySpeedMMps = 100;
//     // Actual entry speed mm/s
//     motionBlock._entrySpeedMMps = 0;
//     // Actual exit speed mm/s
//     motionBlock._exitSpeedMMps = 0;
//     // No end stops for now
//     motionBlock._endStopsToCheck.clear();
//     // Keeps track of block
//     motionBlock._motionTrackingIndex = 1234;
//     // Block is stand-alone
//     motionBlock._blockIsFollowed = false;
//     // Set can execute flag
//     motionBlock._canExecute = true;
//     // Set the motion steps
//     motionBlock.setStepsToTarget(0, 10000);
//     motionBlock.prepareForStepping(axesParams, false);
//     motionBlock.debugShowTimingConsts();
//     motionBlock.debugShowBlkHead();
//     motionBlock.debugShowBlock(0, axesParams);

//     motionPipeline.add(motionBlock);

// #ifdef TEST_USING_TIMER_ISR
//     rampGenTimer.setup(20, TIMER_GROUP_0, TIMER_0);
//     rampGenTimer.enable(true);
//     delayMicroseconds(2000000);
//     rampGenTimer.enable(false);
// #else
//     for (uint64_t i = 0; i < 1000000; i++)
//     {
//         rampGen.service();
//     }
// #endif

//     // uint32_t minTicksPerStep = 0;
//     // uint32_t earlyTicksPerStep = 0;
//     // uint32_t lateTicksPerStep = 0;
//     // uint32_t ticksToCompletion = 0;
//     // std::vector<StepInfo>& stepInfo = rampGenIOTest.getTestResults(minTicksPerStep, 
//     //                     earlyTicksPerStep, lateTicksPerStep, ticksToCompletion);
//     // LOG_I(MODULE_PREFIX, "Steps found %d, maxSpeed (ticksPerStep) %d earlySpeed %d lateSpeed %d ticksToCompletion %d",
//     //                     stepInfo.size(), minTicksPerStep, earlyTicksPerStep, lateTicksPerStep, ticksToCompletion);
//     // for (auto& step : stepInfo)
//     // {
//     //     LOG_I(MODULE_PREFIX, "ticks %lld ticksPerStep %d", 
//     //         step.timerCount, step.ticksPerStep);
//     // }

//     rampGen.debugShowStats();

//     // TEST_ASSERT_MESSAGE (std::abs(int(minTicksPerStep - 5)) < 2, "Invalid max speed (min ticks per step)");
//     // TEST_ASSERT_MESSAGE (std::abs(int(earlyTicksPerStep - 90)) < 10, "Invalid step rate early in the move");
//     // TEST_ASSERT_MESSAGE (std::abs(int(lateTicksPerStep - 36)) < 10, "Invalid step rate towards end of move");
}
