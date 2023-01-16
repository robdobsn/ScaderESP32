// RBotFirmware
// Rob Dobson 2016-21

#pragma once

#include "Axes/AxesParams.h"
#include "Axes/AxesPosition.h"
#include "Controller/MotionArgs.h"
#include "RampGenerator/MotionPipeline.h"
#include "MotorEnabler/MotorEnabler.h"
#include "Controller/MotionPlanner.h"

class AxisGeomBase;

class MotionBlockManager
{
public:
    // Constructor / Destructor
    MotionBlockManager(MotionPipeline& motionPipeline, MotorEnabler& motorEnabler, AxesParams& axesParams);
    virtual ~MotionBlockManager();

    // Clear
    void clear();

    // Setup
    void setup(const String& geometry, bool allowAllOutOfBounds, double junctionDeviation, 
                bool homingNeededBeforeAnyMove, uint32_t stepGenPeriodUs);

    // pumpBlockSplitter - should be called regularly 
    // A single moveTo command can be split into blocks - this function checks if such
    // splitting is in progress and adds the split-up motion blocks accordingly
    void pumpBlockSplitter();

    // Check is busy
    bool isBusy()
    {
        return _numBlocks != 0;
    }

    // Add linear motion block
    bool addLinearBlock(const MotionArgs& args);

    // Add rampled block (which may be split up)
    bool addRampedBlock(const MotionArgs& args, 
                const AxesPosValues& targetPosition, 
                uint32_t numBlocks);

    // Get last commanded position in axes units
    AxesPosValues getLastPos()
    {
        return _lastCommandedAxesPositions.unitsFromHome;
    }

    // Check last commanded position is valid
    bool lastPosValid()
    {
        return _lastCommandedAxesPositions.unitsFromHomeValid();
    }

    // Pre-process coordinates (used for coordinate systems like Theta-Rho which are position dependent)
    // Note that values are modified in-place
    void preProcessCoords(AxesPosValues& axisPositions, const AxesParams& axesParams);

    // Set current position as home
    void setCurPositionAsHome(bool allAxes = true, uint32_t axisIdx = 0);

    // Homing needed before any move
    bool isHomingNeededBeforeMove()
    {
        return _homingNeededBeforeAnyMove;
    }
    
private:
    // Args for motion
    MotionArgs _blockMotionArgs;

    // Current position
    AxesPosition _curPosition;

    // Target position
    AxesPosValues _targetPosition;

    // Block delta distance
    AxesPosValues _blockDeltaDistance;

    // Num blocks to split over
    uint32_t _numBlocks;

    // Next block to return
    uint32_t _nextBlockIdx;

    // Motion pipeline to add blocks to
    MotionPipeline& _motionPipeline;

    // Planner used to plan the pipeline of motion
    MotionPlanner _motionPlanner;

    // Motor enabler
    MotorEnabler& _motorEnabler;

    // Axis geometry
    AxisGeomBase* _pAxisGeometry;

    // Axes parameters
    AxesParams& _axesParams;

    // Last commanded axes positions
    AxesPosition _lastCommandedAxesPositions;

    // Allow all out of bounds movement
    bool _allowAllOutOfBounds;

    // Homing is needed before any movement
    bool _homingNeededBeforeAnyMove;

    // Helpers
    bool addToPlanner(const MotionArgs &args);
};
