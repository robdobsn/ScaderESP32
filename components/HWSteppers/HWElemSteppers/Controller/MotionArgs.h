/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionArgs
//
// Rob Dobson 2021-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Axes/AxisValues.h"
#include "HWElemMultiStepperFormat.h"
#include "JSONParams.h"
#include <vector>

// This must be packed as it is used for binary communication
#pragma pack(push, 1)

class MotionArgs
{
public:
    MotionArgs()
    {
        clear();
    }
    void clear()
    {
        // Versioning - as this structure might be used in a binary
        // fashion
        _motionArgsStructVersion = MULTISTEPPER_MOTION_ARGS_BINARY_FORMAT_1;

        // Flags
        _isRelative = false;
        _linearNoRamp = false;
        _unitsAreSteps = false;
        _dontSplitMove = false;
        _extrudeValid = false;
        _targetSpeedValid = false;
        _moveClockwise = false;
        _moveRapid = false;
        _allowOutOfBounds = false;
        _moreMovesComing = false;
        _motionTrackingIndexValid = false;
        _isHoming = false;
        _feedrateUnitsPerMin = false;
        _enableMotors = true;
        _preClearMotionQueue = false;        

        // Reset values to sensible levels
        _targetSpeed = 0;
        _feedrate = 100.0;
        _extrudeDistance = 1;
        _motionTrackingIdx = 0;

        // Clear position
        for (uint32_t i = 0; i < MULTISTEPPER_MAX_AXES; i++)
        {
            _axisValid[i] = false;
            _axisPos[i] = 0;
        }

        // _pause = false;
        // // Command control
        // _queuedCommands = 0;
        // _numberedCommandIndex = RobotConsts::NUMBERED_COMMAND_NONE;
        // // Coords, etc
        // _ptInMM.clear();
        // _ptInCoordUnits.clear();
        // _ptInSteps.clear();
        // _extrudeValue = 0.0;
        // _feedrateValue = 0.0;
        // _moveType = RobotMoveTypeArg_None;
        // _endstops.clear();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Motion Flags
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setLinear(bool flag)
    {
        _linearNoRamp = flag;
    }
    bool isLinear() const
    {
        return _linearNoRamp;
    }
    void setRelative(bool flag)
    {
        _isRelative = flag;
    }
    bool isRelative() const
    {
        return _isRelative;
    }
    void setDoNotSplitMove(bool flag)
    {
        _dontSplitMove = flag;
    }
    bool dontSplitMove() const
    {
        return _dontSplitMove;
    }
    void setMoveRapid(bool flag)
    {
        _moveRapid = flag;
    }
    bool isMoveRapid()
    {
        return _moveRapid;
    }
    void setClockwise(bool flag)
    {
        _moveClockwise = flag;
    }
    bool isMoveClockwise()
    {
        return _moveClockwise;
    }
    void setUnitsSteps(bool flag)
    {
        _unitsAreSteps = flag;
    }
    bool areUnitsSteps()
    {
        return _unitsAreSteps;
    }
    bool isEnableMotors() const
    {
        return _enableMotors;
    }
    bool isClearQueue() const
    {
        return _preClearMotionQueue;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Axis values
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setAxesPositions(const AxesPosValues& axisPositions)
    {
        for (uint32_t axisIdx = 0; axisIdx < axisPositions.numAxes(); axisIdx++)
        {
            if (axisIdx >= MULTISTEPPER_MAX_AXES)
                break;
            bool isValid = axisPositions.isValid(axisIdx);
            _axisPos[axisIdx] = isValid ? axisPositions.getVal(axisIdx) : 0;
            _axisValid[axisIdx] = isValid;
        }
    }

    AxesPosValues getAxesPositions() const
    {
        AxesPosValues axesPositions;
        for (uint32_t axisIdx = 0; axisIdx < axesPositions.numAxes(); axisIdx++)
        {
            if (axisIdx >= MULTISTEPPER_MAX_AXES)
                break;
            if (_axisValid[axisIdx])
                axesPositions.setVal(axisIdx, _axisPos[axisIdx]);
            else
                axesPositions.setValid(axisIdx, false);
        }
        return axesPositions;
    }

    void setAxisPosition(int axisIdx, double value, bool isValid)
    {
        if (axisIdx >= MULTISTEPPER_MAX_AXES)
            return;
        if (isValid)
        {
            _axisPos[axisIdx] = value;
            _axisValid[axisIdx] = true;
        }
        else
        {
            _axisPos[axisIdx] = 0;
            _axisValid[axisIdx] = false;
        }
    }

    AxisDistDataType getAxisPos(uint32_t axisIdx) const
    {
        if (axisIdx >= MULTISTEPPER_MAX_AXES)
            return 0;
        return _axisPos[axisIdx];
    }

    bool isAxisPosValid(uint32_t axisIdx) const
    {
        if (axisIdx >= MULTISTEPPER_MAX_AXES)
            return false;
        return _axisValid[axisIdx];
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Target speed
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setTargetSpeed(AxisVelocityDataType targetSpeed)
    {
        _targetSpeed = targetSpeed;
        _targetSpeedValid = true;
    }
    bool isTargetSpeedValid() const
    {
        return _targetSpeedValid;
    }
    AxisVelocityDataType getTargetSpeed() const
    {
        return _targetSpeed;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Feedrate percent
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setFeedratePercent(double feedrate)
    {
        _feedrate = feedrate;
        _feedrateUnitsPerMin = false;
    }
    void setFeedrateUnitsPerMin(double feedrate)
    {
        _feedrate = feedrate;
        _feedrateUnitsPerMin = true;
    }
    double getFeedrate() const
    {
        return _feedrate;
    }
    bool isFeedrateUnitsPerMin() const
    {
        return _feedrateUnitsPerMin;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Extrusion
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setExtrudeDist(AxisDistDataType extrude)
    {
        _extrudeDistance = extrude;
        _extrudeValid = true;
    }
    bool isExtrudeValid() const
    {
        return _extrudeValid;
    }
    AxisDistDataType getExtrudeDist() const
    {
        return _extrudeDistance;
    }    

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Motion tracking
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setMotionTrackingIndex(uint32_t motionTrackingIdx)
    {
        _motionTrackingIdx = motionTrackingIdx;
        _motionTrackingIndexValid = true;
    }
    bool isMotionTrackingIndexValid() const
    {
        return _motionTrackingIndexValid;
    }
    uint32_t getMotionTrackingIndex() const
    {
        return _motionTrackingIdx;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Hint that more movement is expected (allows optimization of pipeline processing)
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setMoreMovesComing(bool moreMovesComing)
    {
        _moreMovesComing = moreMovesComing;
    }
    bool getMoreMovesComing() const
    {
        return _moreMovesComing;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Out of bounds
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setAllowOutOfBounds(bool allowOutOfBounds = true)
    {
        _allowOutOfBounds = allowOutOfBounds;
    }
    bool getAllowOutOfBounds() const
    {
        return _allowOutOfBounds;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // End stops
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void setEndStops(AxisEndstopChecks endstops)
    {
        _endstops = endstops;
    }
    void setTestAllEndStops()
    {
        _endstops.all();
        LOG_I("MotionArgs", "Test all endstops");
    }

    void setTestNoEndStops()
    {
        _endstops.clear();
    }

    void setTestEndStopsDefault()
    {
        _endstops.clear();
    }

    void setTestEndStop(int axisIdx, int endStopIdx, AxisEndstopChecks::AxisMinMaxEnum checkType)
    {
        _endstops.set(axisIdx, endStopIdx, checkType);
    }

    const AxisEndstopChecks &getEndstopCheck() const
    {
        return _endstops;
    }

    // void reverseEndstopChecks()
    // {
    //     _endstops.reverse();
    // }

    void fromJSON(const char* jsonStr);
    String toJSON();

    // String toJSON(bool includeBraces = true)
    // {
    //     char jsonStr[500];
    //     snprintf(jsonStr, sizeof(jsonStr), R"(%s"pos":%s,"Fpc":%.2f%s,"rel":%s,%s,"OoB":%d,"mIdx":%d,  %s)",
    //         includeBraces ? "{" : "",
    //         axisPosToJsonArray().c_str(),
    //         _feedratePercent,
    //         _extrudeValid ? (R"(,"E":)" + String(_extrudeDistance, 2)).c_str() : "",
    //         _isRelative ? "rel" : "abs",
    //         _endstops.toJSON("endstops").c_str(),
    //         _allowOutOfBounds,
    //         _motionTrackingIdx,
    //         includeBraces ? "} : ""
    //         )
    //     jsonStr += String(",\"Hmd\":") + (_hasHomed ? "1" : "0");
    //     if (_isHoming)
    //         jsonStr += ",\"Homing\":1";
    //     jsonStr += String(",\"pause\":") + (_pause ? "1" : "0");
    //     if (includeBraces)
    //         jsonStr += "}";
    //     return jsonStr;
    // }

private:

    // Version of this structure
    uint8_t _motionArgsStructVersion;

    // Flags
    bool _isRelative;
    bool _linearNoRamp;
    bool _unitsAreSteps;
    bool _dontSplitMove;
    bool _extrudeValid;
    bool _targetSpeedValid;
    bool _moveClockwise;
    bool _moveRapid;
    bool _allowOutOfBounds;
    bool _moreMovesComing;
    bool _isHoming;
    bool _motionTrackingIndexValid;
    bool _feedrateUnitsPerMin;
    bool _enableMotors;
    bool _preClearMotionQueue;

    // Boolean flags
    class FieldDefType {
    public:
        FieldDefType(const char* name, void* pValue, const char* dataType)
        {
            _name = name;
            _pValue = pValue;
            _dataType = dataType;
        }
        String _name;
        void* _pValue;
        String _dataType;
    };
    std::vector<FieldDefType> getFieldDefs();

    // Target speed (like an absolute feedrate)
    double _targetSpeed;

    // Extrude distance
    double _extrudeDistance;

    // Feedrate is a percentage (unless _feedrateUnitsPerMin is set)
    double _feedrate;

    // Current as percentage of max current
    double _ampsPercentOfMax;

    // Motion tracking index - used to track execution of motion requests
    uint32_t _motionTrackingIdx;

    // End stops
    AxisEndstopChecks _endstops;

    // Coords
    // When _unitsAreSteps flag is true these represent the position in steps
    // When _unitsAreSteps flag is false units are axes units (defined in axes config)
    bool _axisValid[MULTISTEPPER_MAX_AXES];
    double _axisPos[MULTISTEPPER_MAX_AXES];

    // // Helpers
    // String axisPosToJsonArray()
    // {
    //     char jsonArray[200];
    //     strlcpy(jsonArray, sizeof(jsonArray), "[");
    //     for (uint32_t i = 0; i < MULTISTEPPER_MAX_AXES; i++)
    //     {
    //         char axisPosStr[30];
    //         if (_axisValid[i])
    //             snprintf(axisPosStr, sizeof(axisPosStr), "%.2f", _axisPos[i]);
    //         else
    //             strlcpy(axisPosStr, "0", sizeof(axisPosStr));
    //         if (i != 0)
    //             strlcat(jsonArray, ",", sizeof(jsonArray));
    //         strlcat(jsonArray, axisPosStr, sizeof(jsonArray));
    //     }
    //     strlcat(jsonArray, sizeof(jsonArray), "]");
    //     return jsonArray;
    // }

public:
    // MotionArgs& operator=(const MotionArgs& copyFrom)
    // {
    //     copy(copyFrom);
    //     return *this;
    // }

    // bool operator==(const MotionArgs& other)
    // {
    //     bool isEqual =
    //         // Flags
    //         (_unitsAreSteps == other._unitsAreSteps) &&
    //         (_dontSplitMove == other._dontSplitMove) &&
    //         (_extrudeValid == other._extrudeValid) &&
    //         (_feedrateValid == other._feedrateValid) &&
    //         (_moveClockwise == other._moveClockwise) &&
    //         (_moveRapid == other._moveRapid) &&
    //         (_allowOutOfBounds == other._allowOutOfBounds) &&
    //         (_pause == other._pause) &&
    //         (_moreMovesComing == other._moreMovesComing) &&
    //         // Command control
    //         (_queuedCommands == other._queuedCommands) &&
    //         (_numberedCommandIndex == other._numberedCommandIndex) &&
    //         // Endstops etc
    //         (_extrudeValue == other._extrudeValue) &&
    //         (_feedrateValue == other._feedrateValue) &&
    //         (_moveType == other._moveType) &&
    //         (_endstops == other._endstops);
    //     if (!isEqual)
    //         return false;
    //     // Coords, etc
    //     for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    //     {
    //         if (_ptInMM.isValid(axisIdx) != _ptInMM.isValid(axisIdx))
    //             return false;
    //         if (_ptInMM.isValid(axisIdx) &&
    //             ((_ptInMM != other._ptInMM) ||
    //             (_ptInCoordUnits != other._ptInCoordUnits) ||
    //             (_ptInSteps != other._ptInSteps)))
    //             return false;
    //     }
    //     return true;
    // }

    // bool operator!=(const MotionArgs& other)
    // {
    //     return !(*this == other);
    // }

    // MotionArgs(const MotionArgs &other)
    // {
    //     copy(other);
    // }
    // void setAxisValMM(int axisIdx, float value, bool isValid)
    // {
    //     if (axisIdx >= 0 && axisIdx < AXIS_VALUES_MAX_AXES)
    //     {
    //         _ptInMM.setVal(axisIdx, value);
    //         _ptInMM.setValid(axisIdx, isValid);
    //         _unitsAreSteps = false;
    //     }
    // }
    // void setAxisSteps(int axisIdx, int32_t value, bool isValid)
    // {
    //     if (axisIdx >= 0 && axisIdx < AXIS_VALUES_MAX_AXES)
    //     {
    //         _ptInSteps.setVal(axisIdx, value);
    //         // Piggy-back on MM validity flags
    //         _ptInMM.setValid(axisIdx, isValid);
    //         _unitsAreSteps = true;
    //     }
    // }
    // void reverseStepDirection()
    // {
    //     for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    //     {
    //         if (isValid(axisIdx))
    //             _ptInSteps.setVal(axisIdx, -_ptInSteps.getVal(axisIdx));
    //     }
    // }
    // bool isValid(int axisIdx)
    // {
    //     return _ptInMM.isValid(axisIdx);
    // }
    // bool anyValid()
    // {
    //     return _ptInMM.anyValid();
    // }

    // // Indicate that all axes need to be homed
    // void setAllAxesNeedHoming()
    // {
    //     for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    //         setAxisValMM(axisIdx, 0, true);
    // }
    // inline float getValNoCheckMM(int axisIdx)
    // {
    //     return _ptInMM.getValNoCheck(axisIdx);
    // }
    // float getValMM(int axisIdx)
    // {
    //     return _ptInMM.getVal(axisIdx);
    // }
    // float getValCoordUnits(int axisIdx)
    // {
    //     return _ptInCoordUnits.getVal(axisIdx);
    // }
    // AxesPosMM &getPointMM()
    // {
    //     return _ptInMM;
    // }
    // void setPointMM(AxesPosMM &ptInMM)
    // {
    //     _ptInMM = ptInMM;
    // }
    // void setPointSteps(AxisInt32s &ptInSteps)
    // {
    //     _ptInSteps = ptInSteps;
    // }
    // AxisInt32s &getPointSteps()
    // {
    //     return _ptInSteps;
    // }
    // void setMoveType(RobotMoveTypeArg moveType)
    // {
    //     _moveType = moveType;
    // }
    // RobotMoveTypeArg getMoveType()
    // {
    //     return _moveType;
    // }
    // void setMoveRapid(bool moveRapid)
    // {
    //     _moveRapid = moveRapid;
    // }


    // void setDontSplitMove(bool dontSplitMove = true)
    // {
    //     _dontSplitMove = dontSplitMove;
    // }
    // bool getDontSplitMove()
    // {
    //     return _dontSplitMove;
    // }
    // void setNumberedCommandIndex(int cmdIdx)
    // {
    //     _numberedCommandIndex = cmdIdx;
    // }
    // int getNumberedCommandIndex()
    // {
    //     return _numberedCommandIndex;
    // }
    // void setNumQueued(int numQueued)
    // {
    //     _queuedCommands = numQueued;
    // }
    // void setPause(bool pause)
    // {
    //     _pause = pause;
    // }
    // void setIsHoming(bool isHoming)
    // {
    //     _isHoming = isHoming;
    // }
    // void setHasHomed(bool hasHomed)
    // {
    //     _hasHomed = hasHomed;
    // }

private:

    // bool _pause;
    // // Command control
    // int _queuedCommands;
    // int _numberedCommandIndex;
    // // Coords etc
    // AxesPosMM _ptInMM;
    // AxisFloats _ptInCoordUnits;
    // AxisInt32s _ptInSteps;
    // float _extrudeValue;
    // float _feedrateValue;
    // RobotMoveTypeArg _moveType;

    // // Helpers
    // void copy(const MotionArgs &copyFrom)
    // {
    //     clear();
    //     // Flags
    //     _unitsAreSteps = copyFrom._unitsAreSteps;
    //     _dontSplitMove = copyFrom._dontSplitMove;
    //     _extrudeValid = copyFrom._extrudeValid;
    //     _feedrateValid = copyFrom._feedrateValid;
    //     _moveClockwise = copyFrom._moveClockwise;
    //     _moveRapid = copyFrom._moveRapid;
    //     _allowOutOfBounds = copyFrom._allowOutOfBounds;
    //     _pause = copyFrom._pause;
    //     _moreMovesComing = copyFrom._moreMovesComing;
    //     // Command control
    //     _queuedCommands = copyFrom._queuedCommands;
    //     _numberedCommandIndex = copyFrom._numberedCommandIndex;
    //     // Coords, etc
    //     _ptInMM = copyFrom._ptInMM;
    //     _ptInCoordUnits = copyFrom._ptInCoordUnits;
    //     _ptInSteps = copyFrom._ptInSteps;
    //     _extrudeValue = copyFrom._extrudeValue;
    //     _feedrateValue = copyFrom._feedrateValue;
    //     _moveType = copyFrom._moveType;
    //     _endstops = copyFrom._endstops;
    // }    
};

#pragma pack(pop)
