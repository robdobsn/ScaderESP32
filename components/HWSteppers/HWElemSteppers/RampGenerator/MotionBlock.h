/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionBlock
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "math.h"
#include "Axes/AxisValues.h"
#include "Axes/AxesParams.h"

class MotionBlock
{
public:
    MotionBlock();

    // Set timer period
    void setTimerPeriodNs(uint32_t stepGenPeriodNs);

    // Clear
    void clear();

    // Motion tracking
    void setMotionTrackingIndex(uint32_t motionTrackingIndex);
    uint32_t IRAM_ATTR getMotionTrackingIndex();

    // Target
    AxisStepsDataType getStepsToTarget(int axisIdx) const;
    AxisStepsDataType getAbsStepsToTarget(int axisIdx) const;
    void setStepsToTarget(int axisIdx, AxisStepsDataType steps);

    // Rates
    uint32_t getExitStepRatePerTTicks();
    static AxisVelocityDataType maxAchievableSpeed(AxisAccDataType acceleration, 
                        AxisVelocityDataType target_velocity, 
                        AxisDistDataType distance);

    // End stops
    void setEndStopsToCheck(const AxisEndstopChecks &endStopCheck);

    // Prepare a block for stepping
    // If the block is "stepwise" this means that there is no acceleration and deceleration - just steps
    //     at the requested rate - this is generally used for homing, etc
    // If not "stepwise" then the block's entry and exit speed are now known
    //     so the block can accelerate and decelerate as required as long as these criteria are met -
    //     we now compute the stepping parameters to make motion happen
    bool prepareForStepping(const AxesParams &axesParams, bool isLinear);

    // Debug
    void debugShowTimingConsts() const;
    void debugShowBlkHead() const;
    void debugShowBlock(int elemIdx, const AxesParams &axesParams) const;
    double debugStepRateToMMps(uint32_t val) const
    {
        return (((val * 1.0) * _ticksPerSec) / MotionBlock::TTICKS_VALUE) * _debugStepDistMM;
    }
    double debugStepRateToMMps2(uint32_t val) const
    {
        return (((val * 1.0) * 1000 * _ticksPerSec) / MotionBlock::TTICKS_VALUE) * _debugStepDistMM;
    }

    // Minimum move distance
    static constexpr double MINIMUM_MOVE_DIST_MM = 0.0001;

    // Number of ticks to accumulate for rate actuation
    static constexpr uint32_t TTICKS_VALUE = 1000000000l;

    // Number of ns in ms
    static constexpr uint32_t NS_IN_A_MS = 1000000;

    // Calculate ticks per second
    static double calcTicksPerSec(uint32_t stepGenPeriodNs)
    {
        return 1.0e9 / stepGenPeriodNs;
    }

    // Calculate minimum step rate per TTICKS 
    static uint32_t calcMinStepRatePerTTicks(uint32_t stepGenPeriodNs)
    {
        // This is to ensure that the robot never goes to 0 tick rate - which would leave it
        // immobile forever
        static constexpr uint32_t MIN_STEP_RATE_PER_SEC = 10;
        return uint32_t((MIN_STEP_RATE_PER_SEC * 1.0 * MotionBlock::TTICKS_VALUE) / calcTicksPerSec(stepGenPeriodNs));
    }

public:
    // Flags
    struct
    {
        // Flag indicating the block is currently executing
        volatile bool _isExecuting : 1;
        // Flag indicating the block can start executing
        volatile bool _canExecute : 1;
        // Block is followed by others
        bool _blockIsFollowed : 1;
    };

    // Requested max velocity for move - either axis units-per-sec or 
    // stepsPerSec depending if move is stepwise
    AxisVelocityDataType _requestedVelocity;
    // Distance (pythagorean) to move considering primary axes only
    AxisDistDataType _moveDistPrimaryAxesMM;
    // Unit vector on axis with max movement
    AxisUnitVectorDataType _unitVecAxisWithMaxDist;
    // Computed max entry speed for a block based on max junction deviation calculation
    AxisVelocityDataType _maxEntrySpeedMMps;
    // Computed entry speed for this block
    AxisVelocityDataType _entrySpeedMMps;
    // Computed exit speed for this block
    AxisVelocityDataType _exitSpeedMMps;
    // End-stops to test
    AxisEndstopChecks _endStopsToCheck;

    // Steps to target and before deceleration
    int32_t _stepsTotalMaybeNeg[AXIS_VALUES_MAX_AXES];
    int _axisIdxWithMaxSteps;
    uint32_t _stepsBeforeDecel;

    // Stepping acceleration/deceleration profile
    uint32_t _initialStepRatePerTTicks;
    uint32_t _maxStepRatePerTTicks;
    uint32_t _finalStepRatePerTTicks;
    uint32_t _accStepsPerTTicksPerMS;    

    // Motion tracking index - to help keep track of motion execution from other processes
    // like homing
    uint32_t _motionTrackingIndex;

private:
    // Step distance in MM
    double _debugStepDistMM;

    // Ticks per second
    double _ticksPerSec;

    // Helpers
    template<typename T>
    void forceInBounds(T &val, T lowBound, T highBound);

};
