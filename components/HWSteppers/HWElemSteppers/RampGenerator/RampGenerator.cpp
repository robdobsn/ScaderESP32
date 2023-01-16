/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RampGenerator
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RampGenerator.h"
#include <ConfigBase.h>
#include "esp_intr_alloc.h"
#include "MotionPipelineIF.h"
#include "RampGenTimer.h"
#include <ArduinoOrAlt.h>
#include <StepDrivers/StepDriverBase.h>
#include <EndStops/EndStops.h>

#define DEBUG_GENERATE_DETAILED_STATS
// #define DEBUG_MOTION_PULSE_GEN
// #define DEBUG_MOTION_PEEK_QUEUE
// #define DEBUG_SETUP_NEW_BLOCK

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* MODULE_PREFIX = "RampGen";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RampGenerator::RampGenerator(MotionPipelineIF& motionPipeline, RampGenTimer& rampGenTimer) :
            _motionPipeline(motionPipeline), 
            _rampGenTimer(rampGenTimer)
{
    _useRampGenTimer = false;
    _stepGenPeriodNs = rampGenTimer.getPeriodUs() * 1000;
    _minStepRatePerTTicks = MotionBlock::calcMinStepRatePerTTicks(_stepGenPeriodNs);
    _rampGenEnabled = false;
    _isPaused = true;
    _endStopReached = false;
    // _lastDoneNumberedCmdIdx = RobotConsts::NUMBERED_COMMAND_NONE;
    _curStepRatePerTTicks = 0;
    _curAccumulatorStep = 0;
    _curAccumulatorNS = 0;
    _endStopCheckNum = 0;
    _isrCount = 0;
    _debugLastQueuePeekMs = 0;
    resetTotalStepPosition();

    // Debug
    LOG_I(MODULE_PREFIX, "constructed numStepperDrivers %d stepGenPeriodNs=%d", _stepperDrivers.size(), _stepGenPeriodNs);
}

RampGenerator::~RampGenerator()
{
    // Release timer hook
    _rampGenTimer.unhookTimer(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::setup(bool useRampGenTimer, std::vector<StepDriverBase*> stepperDrivers,
            std::vector<EndStops*> axisEndStops)
{
    // // Check if step gen timer is to be used
    // _useRampGenTimer = config.getLong("timerIntr", 1) != 0;
    // uint32_t stepGenPeriodUs = config.getLong("stepGenPeriodUs", RampGenTimer::rampGenPeriodUs_default);
    // Store steppers and end stops
    _stepperDrivers = stepperDrivers;
    _axisEndStops = axisEndStops;

    // Calculate ramp gen periods
    _stepGenPeriodNs = _rampGenTimer.getPeriodUs() * 1000;
    _minStepRatePerTTicks = MotionBlock::calcMinStepRatePerTTicks(_stepGenPeriodNs);

    // Hook the timer if required
    _useRampGenTimer = useRampGenTimer;
    if (_useRampGenTimer)
        _rampGenTimer.hookTimer(rampGenTimerCallback, this);

    // Debug
    LOG_I(MODULE_PREFIX, "setup useTimerInterrupt %s stepGenPeriod %dus", 
                _useRampGenTimer ? "Y" : "N", _rampGenTimer.getPeriodUs());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service method called by main program loop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::service()
{
    // Service RampGenIO
    // _rampGenIO.service();

    // Check if timer used for pulse generation - otherwise pump many times to
    // aid testing
    if (!_useRampGenTimer)
    {
        for (uint32_t i = 0; i < 100; i++)
            generateMotionPulses();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable / stop / pause
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::enable(bool en)
{
    _rampGenEnabled = en;
}

void RampGenerator::stop()
{
    _isPaused = true;
    _endStopReached = false;
}

void RampGenerator::pause(bool pauseIt)
{
    _isPaused = pauseIt;
    if (!_isPaused)
    {
        _endStopReached = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Axis position handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::resetTotalStepPosition()
{
    for (int i = 0; i < AXIS_VALUES_MAX_AXES; i++)
    {
        _axisTotalSteps[i] = 0;
        _totalStepsInc[i] = 0;
    }
}
void RampGenerator::getTotalStepPosition(AxisInt32s& actuatorPos)
{
    for (int i = 0; i < AXIS_VALUES_MAX_AXES; i++)
    {
        actuatorPos.setVal(i, _axisTotalSteps[i]);
    }
}
void RampGenerator::setTotalStepPosition(int axisIdx, int32_t stepPos)
{
    if ((axisIdx >= 0) && (axisIdx < AXIS_VALUES_MAX_AXES))
        _axisTotalSteps[axisIdx] = stepPos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End stop handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::clearEndstopReached()
{
    _endStopReached = false;
}

bool RampGenerator::isEndStopReached()
{
    return _endStopReached;
}

void RampGenerator::getEndStopStatus(AxisEndstopChecks& axisEndStopVals)
{
    // Iterate endstops
    for (int axisIdx = 0; (axisIdx < _axisEndStops.size()) && (axisIdx < AXIS_VALUES_MAX_AXES); axisIdx++)
    {
        if (!_axisEndStops[axisIdx])
            continue;
        axisEndStopVals.set(axisIdx, AxisEndstopChecks::MIN_VAL_IDX, 
            _axisEndStops[axisIdx]->isAtEndStop(false) ? 
                    AxisEndstopChecks::END_STOP_HIT : 
                    AxisEndstopChecks::END_STOP_NOT_HIT);
        axisEndStopVals.set(axisIdx, AxisEndstopChecks::MAX_VAL_IDX, 
            _axisEndStops[axisIdx]->isAtEndStop(true) ?
                    AxisEndstopChecks::END_STOP_HIT : 
                    AxisEndstopChecks::END_STOP_NOT_HIT);
    }
    // _rampGenIO.getEndStopStatus(axisEndStopVals);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracking progress
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// int RampGenerator::getLastCompletedNumberedCmdIdx()
// {
//     return _lastDoneNumberedCmdIdx;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle the end of a step for any axis
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool IRAM_ATTR RampGenerator::handleStepEnd()
{
    bool anyPinReset = false;
    for (int axisIdx = 0; (axisIdx < AXIS_VALUES_MAX_AXES) && (axisIdx < _stepperDrivers.size()); axisIdx++)
    {
        if (!_stepperDrivers[axisIdx])
            continue;
        if (_stepperDrivers[axisIdx]->stepEnd())
        {
            anyPinReset = true;
            _axisTotalSteps[axisIdx] += _totalStepsInc[axisIdx];
        }
    }
    return anyPinReset;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup new block - cache all the info needed to process the block and reset
// motion accumulators to facilitate the block's execution
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenerator::setupNewBlock(MotionBlock *pBlock)
{
    // Setup step counts, direction and endstops for each axis
    _endStopCheckNum = 0;
    for (int axisIdx = 0; (axisIdx < AXIS_VALUES_MAX_AXES) && (axisIdx < _stepperDrivers.size()); axisIdx++)
    {
        if (!_stepperDrivers[axisIdx])
            continue;

        // Total steps
        int32_t stepsTotal = pBlock->_stepsTotalMaybeNeg[axisIdx];
        _stepsTotalAbs[axisIdx] = UTILS_ABS(stepsTotal);
        _curStepCount[axisIdx] = 0;
        _curAccumulatorRelative[axisIdx] = 0;
        // Set direction for the axis
        _stepperDrivers[axisIdx]->setDirection(stepsTotal >= 0);
        _totalStepsInc[axisIdx] = (stepsTotal >= 0) ? 1 : -1;

#ifdef DEBUG_SETUP_NEW_BLOCK
        if (!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "setupNewBlock setDirection %d stepsTotal %d numSteppers %d stepType %s", 
                        stepsTotal >= 0, stepsTotal, _stepperDrivers.size(), _stepperDrivers[axisIdx]->getDriverType().c_str());
        }
#endif

        // Instrumentation
        _stats.stepDirn(axisIdx, stepsTotal >= 0);

        // Check if any endstops to setup
        if (!pBlock->_endStopsToCheck.any())
            continue;

        // Check if the axis is moving in a direction which might result in hitting an active end-stop
        for (int minMaxIdx = 0; minMaxIdx < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; minMaxIdx++)
        {
            // See if anything to check for
            AxisEndstopChecks::AxisMinMaxEnum minMaxType = pBlock->_endStopsToCheck.get(axisIdx, minMaxIdx);
            if (minMaxType == AxisEndstopChecks::END_STOP_NONE)
                continue;

            // Check for towards - this is different from MAX or MIN because the axis will still move even if
            // an endstop is hit if the movement is away from that endstop
            if (minMaxType == AxisEndstopChecks::END_STOP_TOWARDS)
            {
                // Stop at max if we're heading towards max OR
                // stop at min if we're heading towards min
                if (!(((minMaxIdx == AxisEndstopChecks::MAX_VAL_IDX) && (stepsTotal > 0)) ||
                        ((minMaxIdx == AxisEndstopChecks::MIN_VAL_IDX) && (stepsTotal < 0))))
                    continue;
            }
            
            // Config for end stop
            if ((axisIdx <= _axisEndStops.size()) && (_axisEndStops[axisIdx]))
            {
                bool isValid = _axisEndStops[axisIdx]->isValid(minMaxIdx == AxisEndstopChecks::MAX_VAL_IDX);
                if (isValid)
                {
                    _endStopChecks[_endStopCheckNum].axisIdx = axisIdx;
                    _endStopChecks[_endStopCheckNum].checkHit = minMaxType != AxisEndstopChecks::END_STOP_NOT_HIT;
                    _endStopCheckNum++;
                }
            }
        }
    }

    // Accumulator reset
    _curAccumulatorStep = 0;
    _curAccumulatorNS = 0;

    // Step rate
    _curStepRatePerTTicks = pBlock->_initialStepRatePerTTicks;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update millisecond accumulator to handle acceleration and deceleration
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenerator::updateMSAccumulator(MotionBlock *pBlock)
{
    // Bump the millisec accumulator
    _curAccumulatorNS += _stepGenPeriodNs;

    // Check for millisec accumulator overflow
    if (_curAccumulatorNS >= MotionBlock::NS_IN_A_MS)
    {
        // Subtract from accumulator leaving remainder to combat rounding errors
        _curAccumulatorNS -= MotionBlock::NS_IN_A_MS;

        // Check if decelerating
        if (_curStepCount[pBlock->_axisIdxWithMaxSteps] > pBlock->_stepsBeforeDecel)
        {
            if (_curStepRatePerTTicks > UTILS_MAX(_minStepRatePerTTicks + pBlock->_accStepsPerTTicksPerMS,
                                                 pBlock->_finalStepRatePerTTicks + pBlock->_accStepsPerTTicksPerMS))
                _curStepRatePerTTicks -= pBlock->_accStepsPerTTicksPerMS;
        }
        else if ((_curStepRatePerTTicks < _minStepRatePerTTicks) || (_curStepRatePerTTicks < pBlock->_maxStepRatePerTTicks))
        {
            if (_curStepRatePerTTicks + pBlock->_accStepsPerTTicksPerMS < MotionBlock::TTICKS_VALUE)
                _curStepRatePerTTicks += pBlock->_accStepsPerTTicksPerMS;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle start of step on each axis
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool IRAM_ATTR RampGenerator::handleStepMotion(MotionBlock *pBlock)
{
    // Complete Flag
    bool anyAxisMoving = false;

    // Axis with most steps
    int axisIdxMaxSteps = pBlock->_axisIdxWithMaxSteps;

    // Subtract from accumulator leaving remainder
    _curAccumulatorStep -= MotionBlock::TTICKS_VALUE;

    // Step the axis with the greatest step count if needed
    if (_curStepCount[axisIdxMaxSteps] < _stepsTotalAbs[axisIdxMaxSteps])
    {
        // Step this axis
        _stepperDrivers[axisIdxMaxSteps]->stepStart();
        _curStepCount[axisIdxMaxSteps]++;
        if (_curStepCount[axisIdxMaxSteps] < _stepsTotalAbs[axisIdxMaxSteps])
            anyAxisMoving = true;

        // Instrumentation
        _stats.stepStart(axisIdxMaxSteps);

#ifdef DEBUG_MOTION_PULSE_GEN
        if (!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "handleStepMotion stepStart axisIdxMaxSteps %d axisDriver %p numStepperDrivers %d driverType %s", 
                        axisIdxMaxSteps, _stepperDrivers[axisIdxMaxSteps], _stepperDrivers.size(), 
                        _stepperDrivers[axisIdxMaxSteps]->getDriverType().c_str());
        }
#endif
    }

    // Check if other axes need stepping
    for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    {
        if ((axisIdx == axisIdxMaxSteps) || (_curStepCount[axisIdx] == _stepsTotalAbs[axisIdx]))
            continue;

        // Bump the relative accumulator
        _curAccumulatorRelative[axisIdx] += _stepsTotalAbs[axisIdx];
        if (_curAccumulatorRelative[axisIdx] >= _stepsTotalAbs[axisIdxMaxSteps])
        {
            // Do the remainder calculation
            _curAccumulatorRelative[axisIdx] -= _stepsTotalAbs[axisIdxMaxSteps];

            // Step the axis
            _stepperDrivers[axisIdx]->stepStart();

#ifdef DEBUG_MOTION_PULSE_GEN
            if (!_useRampGenTimer)
            {
                LOG_I(MODULE_PREFIX, "handleStepMotion otherAxisStep ax %d cur %d tot %d", axisIdx, _curStepCount[axisIdx], _stepsTotalAbs[axisIdx]);
            }
#endif

            // Move the count onward
            _curStepCount[axisIdx]++;
            if (_curStepCount[axisIdx] < _stepsTotalAbs[axisIdx])
                anyAxisMoving = true;

            // Instrumentation
            _stats.stepStart(axisIdx);
        }
    }

    // Return indicator of block complete
    return anyAxisMoving;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End motion
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenerator::endMotion(MotionBlock *pBlock)
{
    _motionPipeline.remove();
    // Check if this is a numbered block - if so record its completion
    // if (pBlock->getMotionTrackingIndex() != RobotConsts::NUMBERED_COMMAND_NONE)
    //     _lastDoneNumberedCmdIdx = pBlock->getMotionTrackingIndex();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generate motion pulses
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenerator::generateMotionPulses()
{
    // Instrumentation code to time ISR execution (if enabled)
    _stats.startMotionProcessing();

    // Count ISR entries
    _isrCount++;

    // Indicate tick
    // TODO
    // _rampGenIO.tickIndicator();

    // Do a step-end for any motor which needs one - return here to avoid too short a pulse
    if (handleStepEnd())
    {
#ifdef DEBUG_MOTION_PULSE_GEN
        if(!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "generateMotionPulses stepEnd true exiting");
        }
#endif
        return;
    }

    // Check if paused
    if (_isPaused)
    {
#ifdef DEBUG_MOTION_PULSE_GEN
        if(!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "generateMotionPulses paused exiting");
        }
#endif
        return;
    }

    // Peek a MotionPipelineElem from the queue
    MotionBlock *pBlock = _motionPipeline.peekGet();
    if (!pBlock)
    {
#ifdef DEBUG_MOTION_PEEK_QUEUE
        if (Raft::isTimeout(millis(), _debugLastQueuePeekMs, 1000))
        {
            if(!_useRampGenTimer)
            {
                LOG_I(MODULE_PREFIX, "generateMotionPulses no block exiting");
            }
            _debugLastQueuePeekMs = millis();
        }
#endif
        return;
    }

    // Check if the element can be executed
    if (!pBlock->_canExecute)
    {
#ifdef DEBUG_MOTION_PEEK_QUEUE
        if (Raft::isTimeout(millis(), _debugLastQueuePeekMs, 1000))
        {
            if(!_useRampGenTimer)
            {
                LOG_I(MODULE_PREFIX, "generateMotionPulses can't execute exiting");
            }
            _debugLastQueuePeekMs = millis();
        }
#endif
        return;
    }

    // See if the block was already executing and set isExecuting if not
    bool newBlock = !pBlock->_isExecuting;
    pBlock->_isExecuting = true;

    // New block
    if (newBlock)
    {
        // Setup new block
        setupNewBlock(pBlock);

        // Return here to reduce the maximum time this function takes
        // Assuming this function is called frequently (<50uS intervals say)
        // then it will make little difference if we return now and pick up on the next tick
        return;
    }

    // Check endstops        
    bool endStopHit = false;
    for (int i = 0; i < _endStopCheckNum; i++)
    {
        EndStops* pEndStops = _axisEndStops[_endStopChecks[i].axisIdx];
        if (pEndStops->isAtEndStop(_endStopChecks[i].isMax) == _endStopChecks[i].checkHit)
            endStopHit = true;
    }

    // Handle end-stop hit
    if (endStopHit)
    {
        // Cancel motion (by removing the block) as end-stop reached
        _endStopReached = true;
        endMotion(pBlock);

        // Only use this debugging if not driving from ISR
#ifdef DEBUG_MOTION_PULSE_GEN
        if (!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "generateMotionPulses endStopHit - stopping");
        }
#endif
    }

    // Update the millisec accumulator - this handles the process of changing speed incrementally to
    // implement acceleration and deceleration
    updateMSAccumulator(pBlock);

    // Bump the step accumulator
    _curAccumulatorStep += UTILS_MAX(_curStepRatePerTTicks, _minStepRatePerTTicks);

#ifdef DEBUG_GENERATE_DETAILED_STATS
    _stats.update(_curAccumulatorStep, _curStepRatePerTTicks, _curAccumulatorNS,
                pBlock->_axisIdxWithMaxSteps,
                pBlock->_accStepsPerTTicksPerMS,
                _curStepCount[pBlock->_axisIdxWithMaxSteps],
                pBlock->_stepsBeforeDecel,
                pBlock->_maxStepRatePerTTicks);
#endif

    // Check for step accumulator overflow
    if (_curAccumulatorStep >= MotionBlock::TTICKS_VALUE)
    {
#ifdef DEBUG_MOTION_PULSE_GEN
        if (!_useRampGenTimer)
        {
            LOG_I(MODULE_PREFIX, "generateMotionPulses accumulator overflow");
        }
#endif

        // Flag indicating this block is finished
        bool anyAxisMoving = false;

        // Handle a step
        anyAxisMoving = handleStepMotion(pBlock);

        // Any axes still moving?
        if (!anyAxisMoving)
        {
            // This block is done
            endMotion(pBlock);
        }
    }

    // Time execution
    _stats.endMotionProcessing();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenerator::rampGenTimerCallback(void* pObject)
{
    if (pObject)
        ((RampGenerator*)pObject)->generateMotionPulses();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenerator::debugShowStats()
{
    LOG_I(MODULE_PREFIX, "%s isrCount %d", _stats.getStatsStr().c_str(), _isrCount);
}

String RampGenerator::RampGenStats::getStatsStr()
{
#ifndef DEBUG_GENERATE_DETAILED_STATS
    char dbg[100];
    sprintf(dbg, "ISR Avg %0.2fus Max %dus", _isrAvgUs, _isrMaxUs);
    return dbg;
#else
    char dbg[500];
    sprintf(dbg, "AvgISRUs %0.2f MaxISRUs %d curAccumStep %u curStepRtPerTTicks %u curAccumNS %u axisIdxMaxStp %d accStpPerTTicksPerMS %u curStepCtMajAx %u stepsBeforeDecel %u maxStepRatePerTTicks %u",
        _isrAvgUs, _isrMaxUs,
        _curAccumulatorStep, 
        _curStepRatePerTTicks, 
        _curAccumulatorNS, 
        _axisIdxWithMaxSteps, 
        _accStepsPerTTicksPerMS, 
        _curStepCountMajorAxis, 
        _stepsBeforeDecel, 
        _maxStepRatePerTTicks);
    return dbg;
#endif
}

RampGenerator::RampGenStats::RampGenStats()
{
    clear();
}

void RampGenerator::RampGenStats::clear()
{
    _isrStartUs = 0;
    _isrAccUs = 0;
    _isrCount = 0;
    _isrAvgUs = 0;
    _isrAvgValid = false;
    _isrMaxUs = 0;
#ifdef DEBUG_GENERATE_DETAILED_STATS
    _curAccumulatorStep = 0;
    _curStepRatePerTTicks = 0;
    _curAccumulatorNS = 0;
    _axisIdxWithMaxSteps = -1;
    _accStepsPerTTicksPerMS = 0;
    _curStepCountMajorAxis = 0;
    _stepsBeforeDecel = 0;
    _maxStepRatePerTTicks = 0;
#endif
}

void IRAM_ATTR RampGenerator::RampGenStats::startMotionProcessing()
{
    _isrStartUs = micros();
}

void IRAM_ATTR RampGenerator::RampGenStats::endMotionProcessing()
{
    uint32_t elapsedUs = micros() - _isrStartUs;
    // LOG_I(MODULE_PREFIX, "Elapsed uS %d", elapsedUs);
    _isrAccUs += elapsedUs;
    _isrCount++;
    if (_isrCount > 1000)
    {
        _isrAvgUs = _isrAccUs * 1.0 / _isrCount;
        _isrAvgValid = true;
        _isrCount = 0;
        _isrAccUs = 0;
    }
    if (_isrMaxUs < elapsedUs)
        _isrMaxUs = elapsedUs;
}

void IRAM_ATTR RampGenerator::RampGenStats::update(uint32_t curAccumulatorStep, 
        uint32_t curStepRatePerTTicks,
        uint32_t curAccumulatorNS,
        int axisIdxWithMaxSteps,
        uint32_t accStepsPerTTicksPerMS,
        uint32_t curStepCountMajorAxis,
        uint32_t stepsBeforeDecel,
        uint32_t maxStepRatePerTTicks)
{
#ifdef DEBUG_GENERATE_DETAILED_STATS
    _curAccumulatorNS = curAccumulatorNS;
    _curStepRatePerTTicks = curStepRatePerTTicks;
    _axisIdxWithMaxSteps = axisIdxWithMaxSteps;
    _accStepsPerTTicksPerMS = _accStepsPerTTicksPerMS;
    _curStepCountMajorAxis = _curStepCountMajorAxis;
    _stepsBeforeDecel = stepsBeforeDecel;
    _maxStepRatePerTTicks = maxStepRatePerTTicks;
#endif
}

void IRAM_ATTR RampGenerator::RampGenStats::stepDirn(uint32_t axisIdx, bool dirnPositive)
{
#ifdef DEBUG_GENERATE_DETAILED_STATS
#endif
}

void IRAM_ATTR RampGenerator::RampGenStats::stepStart(uint32_t axisIdx)
{
#ifdef DEBUG_GENERATE_DETAILED_STATS
#endif
}
