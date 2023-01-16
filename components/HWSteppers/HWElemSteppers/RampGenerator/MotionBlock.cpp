/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionBlock
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotionBlock.h"
#include "Axes/AxisValues.h"
#include "Axes/AxesParams.h"
#include <Logger.h>
#include <RaftUtils.h>
#include "RampGenTimer.h"

// #define DEBUG_ENDSTOPS

static const char* MODULE_PREFIX = "MotionBlock";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MotionBlock::MotionBlock()
{
    clear();
    _ticksPerSec = calcTicksPerSec(RampGenTimer::rampGenPeriodUs_default * 1000);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set timer period
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionBlock::setTimerPeriodNs(uint32_t stepGenPeriodNs)
{
    _ticksPerSec = calcTicksPerSec(stepGenPeriodNs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionBlock::clear()
{
    // Clear values
    _requestedVelocity = 0;
    _moveDistPrimaryAxesMM = 0;
    _maxEntrySpeedMMps = 0;
    _entrySpeedMMps = 0;
    _exitSpeedMMps = 0;
    _debugStepDistMM = 0;
    _isExecuting = false;
    _canExecute = false;
    _blockIsFollowed = false;
    _axisIdxWithMaxSteps = 0;
    _unitVecAxisWithMaxDist = 0;
    _accStepsPerTTicksPerMS = 0;
    _finalStepRatePerTTicks = 0;
    _initialStepRatePerTTicks = 0;
    _maxStepRatePerTTicks = 0;
    _stepsBeforeDecel = 0;
    _motionTrackingIndex = 0;
    _endStopsToCheck.clear();
    for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        _stepsTotalMaybeNeg[axisIdx] = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command tracking
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionBlock::setMotionTrackingIndex(uint32_t motionTrackingIndex)
{
    _motionTrackingIndex = motionTrackingIndex;
}
uint32_t IRAM_ATTR MotionBlock::getMotionTrackingIndex()
{
    return _motionTrackingIndex;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t MotionBlock::getStepsToTarget(int axisIdx) const
{
    if ((axisIdx >= 0) && (axisIdx < AXIS_VALUES_MAX_AXES))
    {
        return _stepsTotalMaybeNeg[axisIdx];
    }
    return 0;
}

int32_t MotionBlock::getAbsStepsToTarget(int axisIdx) const
{
    if ((axisIdx >= 0) && (axisIdx < AXIS_VALUES_MAX_AXES))
    {
        return abs(_stepsTotalMaybeNeg[axisIdx]);
    }
    return 0;
}

void MotionBlock::setStepsToTarget(int axisIdx, int32_t steps)
{
    if ((axisIdx >= 0) && (axisIdx < AXIS_VALUES_MAX_AXES))
    {
        _stepsTotalMaybeNeg[axisIdx] = steps;
        if (abs(steps) > abs(_stepsTotalMaybeNeg[_axisIdxWithMaxSteps]))
            _axisIdxWithMaxSteps = axisIdx;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block params
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t MotionBlock::getExitStepRatePerTTicks()
{
    return _finalStepRatePerTTicks;
}

float MotionBlock::maxAchievableSpeed(AxisAccDataType acceleration, 
                AxisVelocityDataType target_velocity, 
                AxisDistDataType distance)
{
    return sqrtf(target_velocity * target_velocity + 2.0F * acceleration * distance);
}

template<typename T>
void MotionBlock::forceInBounds(T &val, T lowBound, T highBound)
{
    if (val < lowBound)
        val = lowBound;
    if (val > highBound)
        val = highBound;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End-stops
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionBlock::setEndStopsToCheck(const AxisEndstopChecks &endStopCheck)
{
#ifdef DEBUG_ENDSTOPS
    LOG_I(MODULE_PREFIX, "Set test endstops %x", endStopCheck.debugGetRawValue());
#endif
    _endStopsToCheck = endStopCheck;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Prepare a block for stepping
// If the block is "stepwise" this means that there is no acceleration and deceleration - just steps
//     at the requested rate - this is generally used for homing, etc
// If not "stepwise" then the block's entry and exit speed are now known
//     so the block can accelerate and decelerate as required as long as these criteria are met -
//     we now compute the stepping parameters to make motion happen
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotionBlock::prepareForStepping(const AxesParams &axesParams, bool isLinear)
{
    // If block is currently being executed don't change it
    if (_isExecuting)
        return false;

    // Find the max number of steps for any axis
    uint32_t absMaxStepsForAnyAxis = abs(_stepsTotalMaybeNeg[_axisIdxWithMaxSteps]);

    // Check if stepwise movement (see note above)
    float initialStepRatePerSec = 0;
    float finalStepRatePerSec = 0;
    float maxAccStepsPerSec2 = 0;
    float axisMaxStepRatePerSec = 0;
    uint32_t stepsDecelerating = 0; 
    double stepDistMM = 0;
    if (isLinear)
    {
        // requestedVelocity is in steps per second in this case
        float stepRatePerSec = _requestedVelocity;
        if (stepRatePerSec > axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps))
            stepRatePerSec = axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps);
        initialStepRatePerSec = stepRatePerSec;
        finalStepRatePerSec = stepRatePerSec;
        maxAccStepsPerSec2 = stepRatePerSec;
        axisMaxStepRatePerSec = stepRatePerSec;
        stepsDecelerating = 0;
    }
    else
    {
        // Get the initial step rate, final step rate and max acceleration for the axis with max steps
        stepDistMM = fabs(_moveDistPrimaryAxesMM / _stepsTotalMaybeNeg[_axisIdxWithMaxSteps]);
        initialStepRatePerSec = fabs(_entrySpeedMMps / stepDistMM);
        if (initialStepRatePerSec > axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps))
            initialStepRatePerSec = axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps);
        finalStepRatePerSec = fabs(_exitSpeedMMps / stepDistMM);
        if (finalStepRatePerSec > axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps))
            finalStepRatePerSec = axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps);
        maxAccStepsPerSec2 = fabs(axesParams.getMaxAccel(_axisIdxWithMaxSteps) / stepDistMM);

        // Calculate the distance decelerating and ensure within bounds
        // Using the facts for the block ... (assuming max accleration followed by max deceleration):
        //		Vmax * Vmax = Ventry * Ventry + 2 * Amax * Saccelerating
        //		Vexit * Vexit = Vmax * Vmax - 2 * Amax * Sdecelerating
        //      Stotal = Saccelerating + Sdecelerating
        // And solving for Saccelerating (distance accelerating)
        uint32_t stepsAccelerating = 0;
        float stepsAcceleratingFloat =
            ceilf((powf(finalStepRatePerSec, 2) - powf(initialStepRatePerSec, 2)) / 4 /
                        maxAccStepsPerSec2 +
                    absMaxStepsForAnyAxis / 2);
        if (stepsAcceleratingFloat > 0)
        {
            stepsAccelerating = uint32_t(stepsAcceleratingFloat);
            if (stepsAccelerating > absMaxStepsForAnyAxis)
                stepsAccelerating = absMaxStepsForAnyAxis;
        }

        // Decelerating steps
        stepsDecelerating = 0;

        // Find max possible rate for axis with max steps
        axisMaxStepRatePerSec = fabs(_requestedVelocity / stepDistMM);
        if (axisMaxStepRatePerSec > axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps))
            axisMaxStepRatePerSec = axesParams.getMaxStepRatePerSec(_axisIdxWithMaxSteps);

        // See if max speed will be reached
        uint32_t stepsToMaxSpeed =
            uint32_t((powf(axisMaxStepRatePerSec, 2) - powf(initialStepRatePerSec, 2)) /
                        2 / maxAccStepsPerSec2);
        if (stepsAccelerating > stepsToMaxSpeed)
        {
            // Max speed will be reached
            stepsAccelerating = stepsToMaxSpeed;

            // Decelerating steps
            stepsDecelerating =
                uint32_t((powf(axisMaxStepRatePerSec, 2) - powf(finalStepRatePerSec, 2)) /
                            2 / maxAccStepsPerSec2);
        }
        else
        {
            // Calculate max speed that will be reached
            axisMaxStepRatePerSec =
                sqrtf(powf(initialStepRatePerSec, 2) + 2.0F * maxAccStepsPerSec2 * stepsAccelerating);

            // Decelerating steps
            stepsDecelerating = absMaxStepsForAnyAxis - stepsAccelerating;
        }
    }

    // Fill in the step values for this axis
    _initialStepRatePerTTicks = uint32_t((initialStepRatePerSec * TTICKS_VALUE) / _ticksPerSec);
    _maxStepRatePerTTicks = uint32_t((axisMaxStepRatePerSec * TTICKS_VALUE) / _ticksPerSec);
    _finalStepRatePerTTicks = uint32_t((finalStepRatePerSec * TTICKS_VALUE) / _ticksPerSec);
    _accStepsPerTTicksPerMS = uint32_t((maxAccStepsPerSec2 * TTICKS_VALUE) / _ticksPerSec / 1000);
    _stepsBeforeDecel = absMaxStepsForAnyAxis - stepsDecelerating;
    _debugStepDistMM = stepDistMM;

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionBlock::debugShowTimingConsts() const
{
    LOG_I(MODULE_PREFIX, "TTicksValue (accumulator) %u, TicksPerSec %0.0f", TTICKS_VALUE, _ticksPerSec);
}

void MotionBlock::debugShowBlkHead() const
{
    LOG_I(MODULE_PREFIX, "#i EntMMps ExtMMps StTot0 StTot1 StTot2 St>Dec    Init     (perTT)      Pk     (perTT)     Fin     (perTT)     Acc     (perTT) UnitVecMax   FeedRtMMps StepDistMM  MaxStepRate");
}

void MotionBlock::debugShowBlock(int elemIdx, const AxesParams &axesParams) const
{
    char baseStr[100];
    snprintf(baseStr, sizeof(baseStr), "%2d%8.3f%8.3f%7d%7d%7d%7u",                 
                elemIdx,
                _entrySpeedMMps,
                _exitSpeedMMps,
                getStepsToTarget(0),
                getStepsToTarget(1),
                getStepsToTarget(2),
                _stepsBeforeDecel);
    char extStr[200];
    snprintf(extStr, sizeof(extStr), "%8.3f(%10d)%8.3f(%10d)%8.3f(%10d)%8.3f(%10u)%13.8f%11.6f%11.8f%11.3f",
                debugStepRateToMMps(_initialStepRatePerTTicks), _initialStepRatePerTTicks,
                debugStepRateToMMps(_maxStepRatePerTTicks), _maxStepRatePerTTicks,
                debugStepRateToMMps(_finalStepRatePerTTicks), _finalStepRatePerTTicks,
                debugStepRateToMMps2(_accStepsPerTTicksPerMS),_accStepsPerTTicksPerMS,
                _unitVecAxisWithMaxDist,
                _requestedVelocity,
                _debugStepDistMM,
                axesParams.getMaxStepRatePerSec(0));
    LOG_I(MODULE_PREFIX, "%s%s", baseStr, extStr);
}
