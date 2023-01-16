/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionController
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Axes/AxesParams.h"
#include <BusBase.h>
#include <HWElemConsts.h>

class StepDriverBase;
class ConfigBase;
class EndStops;

// #define DEBUG_MOTION_CONTROL_TIMER

// #include "AxesPosition.h"
// #include "MotionArgs.h"
#include "Controller/MotionPlanner.h"
#include "Controller/MotionBlockManager.h"
// #include "MotorEnabler.h"
#include "RampGenerator/RampGenerator.h"
// #include "RampGenerator/RampGenIO.h"
#include "RampGenerator/RampGenTimer.h"
// #include "HWElemBase.h"
// #include "MotionHoming.h"
// #include "Trinamics/TrinamicsController.h"
// #include "MotorEnabler.h"

class AxisGeomBase;

class MotionController
{
public:
    // Constructor / Destructor
    // MotionController(RampGenIOIF* pRampGenIOTest = NULL);
    MotionController();
    ~MotionController();

    // Setup
    void setup(const ConfigBase& config);

    // Set serial bus and whether to use bus for direction reversal 
    void setupSerialBus(BusBase* pBus, bool useBusForDirectionReversal);

    // Service - called frequently
    void service();

    // Move to a specific location
    bool moveTo(const MotionArgs &args);

    // Pause (or un-pause) all motion
    void pause(bool pauseIt);

    // Check if paused
    bool isPaused()
    {
        return _isPaused;
    }

    // Set current position as home
    void setCurPositionAsHome(bool allAxes = true, uint32_t axisIdx = 0);

    // Go to previously set home position
    void goHome(const MotionArgs &args);

    // Get data (diagnostics)
    String getDataJSON(HWElemStatusLevel_t level);

    // Get queue slots (buffers) available for streaming
    uint32_t streamGetQueueSlots();

    // Motor on time after move
    void setMotorOnTimeAfterMoveSecs(float motorOnTimeAfterMoveSecs)
    {
        _motorEnabler.setMotorOnTimeAfterMoveSecs(motorOnTimeAfterMoveSecs);
    }

private:
    // Ramp generation
    static RampGenTimer _rampGenTimer;

    // Axis stepper motors
    std::vector<StepDriverBase*> _stepperDrivers;

    // Axis end-stops
    std::vector<EndStops*> _axisEndStops;

    // Axes parameters
    AxesParams _axesParams;

    // Ramp generator
    RampGenerator _rampGenerator;

    // Motion pipeline to add blocks to
    MotionPipeline _motionPipeline;

    // Motion block manager
    MotionBlockManager _blockManager;

    // Motor enabler - handles timeout of motor movement
    MotorEnabler _motorEnabler;

    // Ramp timer enabled
    bool _rampTimerEn;

    // Homing needed
    bool _homingNeededBeforeAnyMove;

    // Block distance
    double _blockDistance;

    // Pause status
    bool _isPaused;

    // Helpers
    void deinit();
    void setupAxes(const ConfigBase& config);
    void setupAxisHardware(const ConfigBase& config);
    void setupStepDriver(const String& axisName, const char* jsonElem, const ConfigBase& mainConfig);
    void setupEndStops(const String& axisName, const char* jsonElem, const ConfigBase& mainConfig);
    void setupRampGenerator(const char* jsonElem, const ConfigBase& config);
    void setupMotorEnabler(const char* jsonElem, const ConfigBase& config);
    void setupMotionControl(const char* jsonElem, const ConfigBase& config);
    bool moveToLinear(const MotionArgs& args);
    bool moveToRamped(const MotionArgs& args);

    // Defaults
    static constexpr const char* DEFAULT_DRIVER_CHIP = "TMC2209";
    static constexpr const char* DEFAULT_HARDWARE_LOCATION = "local";
    static constexpr double _blockDistance_default = 0.0f;
    static constexpr double junctionDeviation_default = 0.05f;
    static constexpr double distToTravel_ignoreBelow = 0.01f;
    static constexpr uint32_t pipelineLen_default = 100;
    static constexpr uint32_t MAX_TIME_BEFORE_STOP_COMPLETE_MS = 500;

    // Debug
    uint32_t _debugLastLoopMs;

#ifdef DEBUG_MOTION_CONTROL_TIMER
    static uint32_t _testRampGenCount;
    static IRAM_ATTR void rampGenTimerCallback(void* pObj)
    {
        _testRampGenCount++;
    }
#endif

};


#ifdef bbbbb

private:
    RampGenIO _rampGenIO;
    
    // Axes parameters
    AxesParams _axesParams;

#endif

#ifdef asdasdasd




private:
    // Robot attributes
    String _robotAttributes;
    // Callbacks for coordinate conversion etc
    ptToActuatorFnType _ptToActuatorFn;
    actuatorToPtFnType _actuatorToPtFn;
    correctStepOverflowFnType _correctStepOverflowFn;
    convertCoordsFnType _convertCoordsFn;
    setRobotAttributesFnType _setRobotAttributes;
    // Relative motion
    bool _moveRelative;
    // Motion pipeline
    MotionPipeline _motionPipeline;
    // Trinamic Controller
    // TrinamicsController _trinamicsController;
    // Actuators (motors etc)
    RampGenerator _rampGenerator;
    RampGenIO _rampGenIO;
    RampGenTimer _rampGenTimer;
    // Homing
    MotionHoming _motionHoming;
    // Motor enabler
    MotorEnabler _motorEnabler;

    // Split-up movement blocks to be added to pipeline
    // Number of blocks to add
    int _blocksToAddTotal;
    // Current block to be added
    int _blocksToAddCurBlock;
    // Start position for block generation
    AxesPosMM _blocksToAddStartPos;
    // End position for block generation
    AxesPosMM _blocksToAddEndPos;
    // Deltas for each axis for block generation
    AxesPosMM _blocksToAddDelta;
    // Command args for block generation
    RobotCommandArgs _blocksToAddCommandArgs;

    // Handling of stop
    bool _stopRequested;
    bool _stopRequestTimeMs;

    // Debug
    unsigned long _debugLastPosDispMs;

public:
    MotionController();
    ~MotionController();

    void setTransforms(ptToActuatorFnType ptToActuatorFn, actuatorToPtFnType actuatorToPtFn,
                       correctStepOverflowFnType correctStepOverflowFn,
                       convertCoordsFnType convertCoordsFn, setRobotAttributesFnType setRobotAttributes);

    void configure(const char *robotConfigJSON);

    // Can accept
    bool canAccept();


    // Stop
    void stop();
    // Check if idle
    bool isIdle();

    double getStepsPerUnit(int axisIdx)
    {
        return _axesParams.getStepsPerUnit(axisIdx);
    }

    double getStepsPerRot(int axisIdx)
    {
        return _axesParams.getStepsPerRot(axisIdx);
    }

    double getunitsPerRot(int axisIdx)
    {
        return _axesParams.getunitsPerRot(axisIdx);
    }

    long gethomeOffSteps(int axisIdx)
    {
        return _axesParams.gethomeOffSteps(axisIdx);
    }

    AxesParams &getAxesParams()
    {
        return _axesParams;
    }

    void setMotionParams(RobotCommandArgs &args);
    void getCurStatus(RobotCommandArgs &args);
    void getRobotAttributes(String& robotAttrs);
    void goHome(RobotCommandArgs &args);
    int getLastCompletedNumberedCmdIdx()
    {
        return _rampGenerator.getLastCompletedNumberedCmdIdx() >
                _trinamicsController.getLastCompletedNumberedCmdIdx() ?
                _rampGenerator.getLastCompletedNumberedCmdIdx() :
                _trinamicsController.getLastCompletedNumberedCmdIdx();
    }
    void service();

    unsigned long getLastActiveUnixTime()
    {
        return _motorEnabler.getLastActiveUnixTime();
    }

    // Test code
    void debugShowBlocks();
    void debugShowTopBlock();
    void debugShowTiming();
    String getDebugStr();
    int testGetPipelineCount();
    bool testGetPipelineBlock(int elIdx, MotionBlock &elem);
    // void setIntrumentationMode(const char *testModeStr)
    // {
    //     _rampGenerator.debugSetIntrumentationMode(testModeStr);
    // }

#ifdef UNIT_TEST
    MotionHoming* testGetMotionHoming()
    {
        return &_motionHoming;
    }
#endif

private:
    bool isInBounds(double v, double b1, double b2)
    {
        return (v > fmin(b1, b2) && v < fmax(b1, b2));
    }
    void setCurPosActualPosition();
    bool addToPlanner(RobotCommandArgs &args);
    void blocksToAddProcess();

    // TODO 2021
    _rampGenTimer.setup(1000);

#endif

