/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionController
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotionController.h"
#include <ConfigPinMap.h>
#include <math.h>
#include <RaftUtils.h>
#include "Axes/AxisValues.h"
#include <StepDrivers/StepDriverBase.h>
#include <StepDrivers/StepDriverTMC2209.h>
#include "EndStops/EndStops.h"
#include <ArduinoOrAlt.h>

#define DEBUG_STEPPER_SETUP_CONFIG
#define DEBUG_RAMP_SETUP_CONFIG
#define DEBUG_MOTION_CONTROLLER
#define INFO_LOG_AXES_PARAMS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* MODULE_PREFIX = "MotionController";

// Ramp generator timer
RampGenTimer MotionController::_rampGenTimer;

#ifdef DEBUG_MOTION_CONTROL_TIMER
uint32_t MotionController::_testRampGenCount = 0;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup serial bus and bus reversal
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupSerialBus(BusBase* pBus, bool useBusForDirectionReversal)
{
    // Setup bus
    for (StepDriverBase* pStepDriver : _stepperDrivers)
    {
        if (pStepDriver)
            pStepDriver->setupSerialBus(pBus, useBusForDirectionReversal);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MotionController::MotionController() : 
            _rampGenerator(_motionPipeline, _rampGenTimer),
            _blockManager(_motionPipeline, _motorEnabler, _axesParams)
{
    // Debug
    _debugLastLoopMs = 0;

    _blockDistance = 0;
    _rampTimerEn = false;
    _homingNeededBeforeAnyMove = true;

    // // Init
    // _isPaused = false;
    // _moveRelative = false;
    // // Clear axis current location
    // _lastCommandedAxisPos.clear();
    // _rampGenerator.resetTotalStepPosition();
    // _trinamicsController.resetTotalStepPosition();
    // // Coordinate conversion management
    // _ptToActuatorFn = NULL;
    // _actuatorToPtFn = NULL;
    // _correctStepOverflowFn = NULL;
    // // Handling of splitting-up of motion into smaller blocks
    // _blocksToAddTotal = 0;    
    // // Init callbacks
    // _ptToActuatorFn = nullptr;
    // _actuatorToPtFn = nullptr;
    // _correctStepOverflowFn = nullptr;
    // _convertCoordsFn = nullptr;
    // _setRobotAttributes = nullptr;
}

MotionController::~MotionController()
{
    // Clear all steppers
    deinit();

    // Clear block manager
    _blockManager.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
// Setup the motor and pipeline parameters using a JSON input string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setup(const ConfigBase& config, const char* pConfigPrefix)
{
    // Stop any motion
    _rampGenerator.stop();
    _motorEnabler.deinit();

    // Remove any config
    deinit();

    // Check if using ramp timer
    _rampTimerEn = config.getBool("ramp/rampTimerEn", 0, pConfigPrefix);

    // Setup axes (and associated hardware)
    setupAxes(config, pConfigPrefix);
#ifdef INFO_LOG_AXES_PARAMS
    _axesParams.debugLog();
#endif

    // Setup ramp generator and pipeline
    setupRampGenerator("ramp", config, pConfigPrefix);
    _rampGenerator.pause(false);
    _rampGenerator.enable(true);

    // Setup motor enabler
    setupMotorEnabler("motorEn", config, pConfigPrefix);

    // Setup motion control
    setupMotionControl("motion", config, pConfigPrefix);

#ifdef DEBUG_MOTION_CONTROL_TIMER
    // Debug
    _rampGenTimer.hookTimer(rampGenTimerCallback, this);
#endif

    // Start timer if required
    _rampGenTimer.enable(_rampTimerEn);

    // If no homing required then set the current position as home
    if (!_homingNeededBeforeAnyMove)
        setCurPositionAsHome(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
// Called regularly to allow the MotionController to do background work such as adding split-up blocks to the
// pipeline and checking if motors should be disabled after a period of no motion
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::service()
{
    // LOG_I(MODULE_PREFIX, "service stepper drivers %d", _stepperDrivers.size());

    // Service stepper drivers
    for (StepDriverBase* pStepDriver : _stepperDrivers)
    {
        // LOG_I(MODULE_PREFIX, "service stepper driver %d", pStepDriver->getSerialAddress());
        if (pStepDriver)
            pStepDriver->service();
    }

    // 
    if (Raft::isTimeout(millis(), _debugLastLoopMs, 1000))
    {
#ifdef DEBUG_MOTION_CONTROL_TIMER
        LOG_I(MODULE_PREFIX, "test count %d", _testRampGenCount);
#endif
        _debugLastLoopMs = millis();
    }

    // // Check if stop requested
    // if (_stopRequested)
    // {
    //     if (Raft::isTimeout(millis(), _stopRequestTimeMs, MAX_TIME_BEFORE_STOP_COMPLETE_MS))
    //     {
    //         _blocksToAddTotal = 0;
    //         _rampGenerator.stop();
    //         _trinamicsController.stop();
    //         _motionPipeline.clear();
    //         pause(false);
    //         setCurPosActualPosition();
    //         _stopRequested = false;
    //     }
    // }

    // Service motor enabler
    _motorEnabler.service();
    
    // Call process on motion actuator - only really used for testing as
    // motion is handled by ISR
    _rampGenerator.service();

    // // Process for trinamic devices
    // _trinamicsController.process();

    // Process any split-up blocks to be added to the pipeline
    _blockManager.pumpBlockSplitter();

    // // Service homing
    // _motionHoming.service(_axesParams);

    // Ensure motors enabled when homing or moving
    if ((_motionPipeline.count() > 0) 
        // TODO 2021
        //  || _motionHoming.isHomingInProgress()
         )
    {
        _motorEnabler.enableMotors(true, false);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle MoveTo
// Command the robot to move (adding a command to the pipeline of motion)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotionController::moveTo(const MotionArgs &args)
{
    // Handle clear queue
    if (args.isClearQueue())
    {
        _blockManager.clear();
    }

    // Handle disable motors
    if (!args.isEnableMotors())
    {
        _motorEnabler.enableMotors(false, false);
        return true;
    }

    // Handle linear motion (no ramp) - motion is defined in terms of steps (not mm)
    if (args.isLinear())
        return _blockManager.addLinearBlock(args);
    return moveToRamped(args);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause (or un-pause) all motion
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::pause(bool pauseIt)
{
    _rampGenerator.pause(pauseIt);
    // _trinamicsController.pause(pauseIt);
    _isPaused = pauseIt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Motion helper for ramped motion
// Ramped motion (variable acceleration) is used for normal motion
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotionController::moveToRamped(const MotionArgs& args)
{
    // Check not busy
    if (_blockManager.isBusy())
    {
#ifdef DEBUG_MOTION_CONTROLLER
        LOG_I(MODULE_PREFIX, "moveTo busy");
#endif
        return false;
    }

    // Check the last commanded position is valid (homing complete, no stepwise movement, etc)
    if (_blockManager.isHomingNeededBeforeMove() && (!_blockManager.lastPosValid()))
    {
#ifdef DEBUG_MOTION_CONTROLLER
        LOG_I(MODULE_PREFIX, "moveTo lastPos invalid - need to home (initially and after linear moves)");
#endif
        return false;
    }

    // Get the target axis position
    AxesPosValues targetAxisPos = args.getAxesPositions();

    // Convert coords to real-world if required - this depends on the coordinate type and
    // does nothing
    _blockManager.preProcessCoords(targetAxisPos, _axesParams);

    // Fill in the targetAxisPos for axes for which values not specified
    // Handle relative motion calculatiuon if required
    // Setup flags to indicate if each axis should be included in distance calculation
    bool includeAxisDistCalc[AXIS_VALUES_MAX_AXES];
    for (int i = 0; i < AXIS_VALUES_MAX_AXES; i++)
    {
        if (!targetAxisPos.isValid(i))
        {
            targetAxisPos.setVal(i, _blockManager.getLastPos().getVal(i));
#ifdef DEBUG_MOTION_CONTROLLER
            LOG_I(MODULE_PREFIX, "moveTo ax %d, pos %0.2f NoMovementOnThisAxis", 
                    i, 
                    targetAxisPos.getVal(i));
#endif
        }
        else
        {
            // Check relative motion - override current options if this command
            // explicitly states a moveType
            if (args.isRelative())
            {
                targetAxisPos.setVal(i, _blockManager.getLastPos().getVal(i) + targetAxisPos.getVal(i));
            }
#ifdef DEBUG_MOTION_CONTROLLER
            LOG_I(MODULE_PREFIX, "moveTo ax %d, pos %0.2f %s", 
                    i, 
                    targetAxisPos.getVal(i), 
                    args.isRelative() ? "RELATIVE" : "ABSOLUTE");
#endif
        }
        includeAxisDistCalc[i] = _axesParams.isPrimaryAxis(i);
    }

    // Get maximum length of block (for splitting up into blocks if required)
    double lineLen = targetAxisPos.distanceTo(_blockManager.getLastPos(), includeAxisDistCalc);

    // Ensure at least one block
    uint32_t numBlocks = 1;
    if (_blockDistance > 0.01f && !args.dontSplitMove())
        numBlocks = int(ceil(lineLen / _blockDistance));
    if (numBlocks == 0)
        numBlocks = 1;

    // Add to the block splitter
    _blockManager.addRampedBlock(args, targetAxisPos, numBlocks);

    // Pump the block splitter to prime the pipeline with blocks
    _blockManager.pumpBlockSplitter();

    // Ok
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set current position as home
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setCurPositionAsHome(bool allAxes, uint32_t axisIdx)
{
    _rampGenerator.setTotalStepPosition(axisIdx, _axesParams.gethomeOffSteps(axisIdx));
    _blockManager.setCurPositionAsHome(allAxes, axisIdx);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Go to home position
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::goHome(const MotionArgs &args)
{
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Data JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String MotionController::getDataJSON(HWElemStatusLevel_t level)
{
    if (level >= ELEM_STATUS_LEVEL_MIN)
    {
        return _rampGenerator.getStats().getStatsStr();
    }
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get queue slots (buffers) available for streaming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t MotionController::streamGetQueueSlots()
{
    return _motionPipeline.remaining();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// De-init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::deinit()
{
    // Remote steppers
    for (StepDriverBase*& pDriver : _stepperDrivers)
    {
        if (pDriver)
            delete pDriver;
    }
    _stepperDrivers.clear();

    // Remote endstops
    for (EndStops*& pEndStops : _axisEndStops)
    {
        if (pEndStops)
            delete pEndStops;
    }
    _axisEndStops.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup axes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupAxes(const ConfigBase& config, const char* pConfigPrefix)
{
    // Setup axes params
    _axesParams.setupAxes(config, pConfigPrefix);

    // Extract hardware related to axes
    std::vector<String> axesVec;
    if (config.getArrayElems("axes", axesVec, pConfigPrefix))
    {
        uint32_t axisIdx = 0;
        for (String& axisConfigStr : axesVec)
        {
            // Setup driver
            setupAxisHardware(axisConfigStr);

            // Next
            axisIdx++;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup axis hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupAxisHardware(const ConfigBase& config)
{
    // Axis name
    String axisName = config.getString("name", "");

    // Configure the driver
    setupStepDriver(axisName, "driver", config);

    // Configure the endstops
    setupEndStops(axisName, "endstops", config);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup stepper driver
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupStepDriver(const String& axisName, const char* jsonElem, const ConfigBase& mainConfig)
{
    // TODO refactor to use JSON paths

    // Extract element config
    ConfigBase config = mainConfig.getString(jsonElem, "{}");

    // Stepper handler
    String hwLocation = config.getString("hw", DEFAULT_HARDWARE_LOCATION);
    String driverType = config.getString("driver", DEFAULT_DRIVER_CHIP);

    // Stepper parameters
    StepDriverParams stepperParams;

    // Get step controller settings
    stepperParams.microsteps = config.getLong("microsteps", StepDriverParams::MICROSTEPS_DEFAULT);
    stepperParams.writeOnly = config.getBool("writeOnly", 0);

    // Get hardware stepper params
    String stepPinName = config.getString("stepPin", "-1");
    stepperParams.stepPin = ConfigPinMap::getPinFromName(stepPinName.c_str());
    String dirnPinName = config.getString("dirnPin", "-1");
    stepperParams.dirnPin = ConfigPinMap::getPinFromName(dirnPinName.c_str());
    stepperParams.invDirn = config.getBool("invDirn", 0);
    stepperParams.extSenseOhms = config.getDouble("extSenseOhms", StepDriverParams::EXT_SENSE_OHMS_DEFAULT);
    stepperParams.extVRef = config.getBool("extVRef", false);
    stepperParams.extMStep = config.getBool("extMStep", false);
    stepperParams.intpol = config.getBool("intpol", false);
    stepperParams.minPulseWidthUs = config.getLong("minPulseWidthUs", 1);
    stepperParams.rmsAmps = config.getDouble("rmsAmps", StepDriverParams::RMS_AMPS_DEFAULT);
    stepperParams.holdDelay = config.getLong("holdDelay", StepDriverParams::IHOLD_DELAY_DEFAULT);
    stepperParams.pwmFreqKHz = config.getDouble("pwmFreqKHz", StepDriverParams::PWM_FREQ_KHZ_DEFAULT);
    stepperParams.address = config.getLong("addr", 0);

    // Hold mode
    String holdModeStr = config.getString("holdModeOrFactor", "1.0");
    if (holdModeStr.equalsIgnoreCase("freewheel"))
    {
        stepperParams.holdMode = StepDriverParams::HOLD_MODE_FREEWHEEL;
        stepperParams.holdFactor = 0;
    }
    else if (holdModeStr.equalsIgnoreCase("passive"))
    {
        stepperParams.holdMode = StepDriverParams::HOLD_MODE_PASSIVE_BREAKING;
        stepperParams.holdFactor = 0;
    }
    else
    {
        stepperParams.holdMode = StepDriverParams::HOLD_MODE_FACTOR;
        stepperParams.holdFactor = strtof(holdModeStr.c_str(), NULL);
    }

    // Handle location
    StepDriverBase* pStepDriver = nullptr; 
    if (hwLocation.equalsIgnoreCase("local"))
    {
        // Check driver type
        if (driverType.equalsIgnoreCase("tmc2209"))
        {
            pStepDriver = new StepDriverTMC2209();
        }
        if (pStepDriver)
        {
            pStepDriver->setup(axisName, stepperParams, _rampTimerEn);
        }
        // Debug
#ifdef DEBUG_STEPPER_SETUP_CONFIG
        LOG_I(MODULE_PREFIX, "setupStepDriver %s axisName %s address %02x driver %s stepPin %d(%s) dirnPin %d(%s) invDirn %s microsteps %d writeOnly %s extSenseOhms %.2f extVRef %s extMStep %s intpol %s rmsAmps %0.2f holdMode %d holdFactor %0.2f holdDelay %d pwmFreqKHz %0.2f",
                pStepDriver ? "local" : "FAILED",
                axisName.c_str(), 
                stepperParams.address,
                driverType.c_str(),
                stepperParams.stepPin, stepPinName.c_str(), 
                stepperParams.dirnPin, dirnPinName.c_str(), 
                stepperParams.invDirn ? "Y" : "N",
                stepperParams.microsteps,
                stepperParams.writeOnly ? "Y" : "N",
                stepperParams.extSenseOhms,
                stepperParams.extVRef ? "Y" : "N",
                stepperParams.extMStep ? "Y" : "N",
                stepperParams.intpol ? "Y" : "N",
                stepperParams.rmsAmps,
                uint32_t(stepperParams.holdMode),
                stepperParams.holdFactor,
                stepperParams.holdDelay,
                stepperParams.pwmFreqKHz);
    #endif
    }

    // Add driver
    _stepperDrivers.push_back(pStepDriver);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup end stops
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupEndStops(const String& axisName, const char* jsonElem, const ConfigBase& mainConfig)
{
    // Endstops
    EndStops* pEndStops = new EndStops();

    // Config
    std::vector<String> endstopVec;
    if (mainConfig.getArrayElems(jsonElem, endstopVec))
    {
        for (String& endstopConfigStr : endstopVec)
        {
            // Debug
            // LOG_I(MODULE_PREFIX, "setupEndStops %s", endstopConfigStr.c_str());

            // Setup endstop
            ConfigBase config = endstopConfigStr;
            bool isMax = config.getBool("isMax", false);
            String name = config.getString("name", "");
            String endstopPinName = config.getString("sensePin", "-1");
            int pin = ConfigPinMap::getPinFromName(endstopPinName.c_str());
            bool activeLevel = config.getBool("actLvl", false);
            String inputTypeStr = config.getString("inputType", "INPUT_PULLUP");
            int inputType = ConfigPinMap::getInputType(inputTypeStr.c_str());
            if (pEndStops)
                pEndStops->add(isMax, name.c_str(), pin, activeLevel, inputType);
            LOG_I(MODULE_PREFIX, "setupEndStops isMax %d name %s pin %d, activeLevel %d, pinMode %d", 
                        isMax, name.c_str(), pin, activeLevel, inputType);
        }
    }

    // Add
    _axisEndStops.push_back(pEndStops);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup ramp generator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupRampGenerator(const char* jsonElem, const ConfigBase& mainConfig, const char* pConfigPrefix)
{
    // TODO refactor to use JSON paths

    // Configure the driver
    ConfigBase rampGenConfig = mainConfig.getString(jsonElem, "{}", pConfigPrefix);

    // Ramp generator config
    long rampTimerUs = rampGenConfig.getLong("rampTimerUs", RampGenTimer::rampGenPeriodUs_default);

    // Ramp generator timer
    bool timerSetupOk = _rampGenTimer.setup(rampTimerUs, TIMER_GROUP_0, TIMER_0);

    // Ramp generator
    _rampGenerator.setup(_rampTimerEn, _stepperDrivers, _axisEndStops);

    // Pipeline
    uint32_t pipelineLen = rampGenConfig.getLong("pipelineLen", pipelineLen_default);
    _motionPipeline.setup(pipelineLen);

#ifdef DEBUG_RAMP_SETUP_CONFIG
    LOG_I(MODULE_PREFIX, "setupRampGenerator timerEn %d timerUs %ld timertimerSetupOk %d pipelineLen %d",
        _rampTimerEn, rampTimerUs, timerSetupOk, pipelineLen);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup motor enabler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupMotorEnabler(const char* jsonElem, const ConfigBase& mainConfig, const char* pConfigPrefix)
{
    // TODO refactor to use JSON paths

    // Configure the motor enabler
    ConfigBase enablerConfig = mainConfig.getString(jsonElem, "{}", pConfigPrefix);

    // Setup
    _motorEnabler.setup(enablerConfig);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup motion control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setupMotionControl(const char* jsonElem, const ConfigBase& mainConfig, const char* pConfigPrefix)
{
    // TODO refactor to use JSON paths

    // Configuration
    ConfigBase motionConfig = mainConfig.getString(jsonElem, "{}", pConfigPrefix);

    // Params
    String geometry = motionConfig.getString("geom", "XYZ");
    _blockDistance = motionConfig.getDouble("blockDist", _blockDistance_default);
    bool allowAllOutOfBounds = motionConfig.getLong("allowOutOfBounds", false) != 0;
    double junctionDeviation = motionConfig.getDouble("junctionDeviation", junctionDeviation_default);
    _homingNeededBeforeAnyMove = motionConfig.getLong("homeBeforeMove", true) != 0;

    // Debug
    LOG_I(MODULE_PREFIX, "setupMotion geom %s blockDist %0.2f (0=no-max) allowOoB %s homeBefMove %s jnDev %0.2f",
               geometry.c_str(), _blockDistance, allowAllOutOfBounds ? "Y" : "N", 
               _homingNeededBeforeAnyMove ? "Y" : "N",
               junctionDeviation);

    // Block manager
    _blockManager.setup(geometry, allowAllOutOfBounds, junctionDeviation, 
            _homingNeededBeforeAnyMove, _rampGenTimer.getPeriodUs());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set max motor current
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionController::setMaxMotorCurrentAmps(uint32_t axisIdx, float maxMotorCurrentAmps)
{
    // Set max motor current
    if (axisIdx < _stepperDrivers.size())
        _stepperDrivers[axisIdx]->setMaxMotorCurrentAmps(maxMotorCurrentAmps);
}

#ifdef asdasdasd

// Each robot has a set of functions that transform points from real-world coordinates
// to actuator coordinates
// There is also a function to correct step overflow which is important in robots
// which have continuous rotation as step counts would otherwise overflow 32bit integer values
void MotionController::setTransforms(ptToActuatorFnType ptToActuatorFn, actuatorToPtFnType actuatorToPtFn,
                                 correctStepOverflowFnType correctStepOverflowFn,
                                 convertCoordsFnType convertCoordsFn, setRobotAttributesFnType setRobotAttributes)
{
    // Store callbacks
    _ptToActuatorFn = ptToActuatorFn;
    _actuatorToPtFn = actuatorToPtFn;
    _correctStepOverflowFn = correctStepOverflowFn;
    _convertCoordsFn = convertCoordsFn;
    _setRobotAttributes = setRobotAttributes;
}


// Check if a command can be accepted into the motion pipeline
bool MotionController::canAccept()
{
    // Check if homing in progress
    if (_motionHoming.isHomingInProgress())
        return false;
    // Check that the motion pipeline can accept new data
    return (_blocksToAddTotal == 0) && _motionPipeline.canAccept();
}

// Stop
void MotionController::stop()
{
    _blocksToAddTotal = 0;
    _stopRequested = true;
    _stopRequestTimeMs = millis();
    _rampGenerator.stop();
    _trinamicsController.stop();
    _motionPipeline.clear();
    pause(false);
    setCurPosActualPosition();
 }

// Check if idle
bool MotionController::isIdle()
{
    return !_motionPipeline.canGet();
}

void MotionController::setCurPosActualPosition()
{
    // Get final position of actuator after a short delay to attempt to
    // ensure any final step is completed
    delayMicroseconds(100);
    AxisInt32s actuatorPos;
    if (_trinamicsController.isRampGenerator())
        _trinamicsController.getTotalStepPosition(actuatorPos);
    else
        _rampGenerator.getTotalStepPosition(actuatorPos);
    AxesPosMM curPosMM;
    if (_actuatorToPtFn)
        _actuatorToPtFn(actuatorPos, curPosMM, _lastCommandedAxisPos, _axesParams);
    _lastCommandedAxisPos.unitsFromHome = curPosMM;
    _lastCommandedAxisPos.stepsFromHome = actuatorPos;
#ifdef DEBUG_MOTION_CONTROLLER
    LOG_I(MODULE_PREFIX, "stop absAxes X%0.2f Y%0.2f Z%0.2f steps %d,%d,%d",
                _lastCommandedAxisPos.unitsFromHome.getVal(0),
                _lastCommandedAxisPos.unitsFromHome.getVal(1),
                _lastCommandedAxisPos.unitsFromHome.getVal(2),
                _lastCommandedAxisPos.stepsFromHome.getVal(0),
                _lastCommandedAxisPos.stepsFromHome.getVal(1),
                _lastCommandedAxisPos.stepsFromHome.getVal(2));
#endif
}

// Set parameters such as relative vs absolute motion
void MotionController::setMotionParams(RobotCommandArgs &args)
{
    // Check for relative movement specified and set accordingly
    if (args.getMoveType() != RobotMoveTypeArg_None)
        _moveRelative = (args.getMoveType() == RobotMoveTypeArg_Relative);
}

// Get current status of robot
void MotionController::getCurStatus(RobotCommandArgs &args)
{
    // Get current position
    AxisInt32s curActuatorPos;
    if (_trinamicsController.isRampGenerator())
        _trinamicsController.getTotalStepPosition(curActuatorPos);
    else
        _rampGenerator.getTotalStepPosition(curActuatorPos);
    args.setPointSteps(curActuatorPos);
    // Use reverse kinematics to get location
    AxesPosMM curMMPos;
    if (_actuatorToPtFn)
        _actuatorToPtFn(curActuatorPos, curMMPos, _lastCommandedAxisPos, _axesParams);
    args.setPointMM(curMMPos);
    // Get end-stop values
    AxisEndstopChecks endstops;
    _rampGenerator.getEndStopStatus(endstops);
    args.setEndStops(endstops);
    // Absolute/Relative movement
    args.setMoveType(_moveRelative ? RobotMoveTypeArg_Relative : RobotMoveTypeArg_Absolute);
    // flags
    args.setPause(_isPaused);
    args.setIsHoming(_motionHoming.isHomingInProgress());
    args.setHasHomed(_motionHoming.isHomedOk());
    // Queue length
    args.setNumQueued(_motionPipeline.count());
}

// Get attributes of robot
void MotionController::getRobotAttributes(String& robotAttrs)
{
    robotAttrs = _robotAttributes;
}

// Command the robot to home one or more axes
void MotionController::goHome(RobotCommandArgs &args)
{
    _motionHoming.homingStart(args);
}

// Debug helper methods
void MotionController::debugShowBlocks()
{
    _motionPipeline.debugShowBlocks(_axesParams);
}

void MotionController::debugShowTopBlock()
{
    _motionPipeline.debugShowTopBlock(_axesParams);
}

// String MotionController::getDebugStr()
// {
//     return _rampGenerator.getDebugStr();
// }

int MotionController::testGetPipelineCount()
{
    return _motionPipeline.count();
}

bool MotionController::testGetPipelineBlock(int elIdx, MotionBlock &block)
{
    if ((int)_motionPipeline.count() <= elIdx)
        return false;
    block = *_motionPipeline.peekNthFromPut(_motionPipeline.count() - 1 - elIdx);
    return true;
}

#endif