/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MotionArgs
//
// Rob Dobson 2021-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotionArgs.h"

static const char* MODULE_PREFIX = "MotionArgs";

std::vector<MotionArgs::FieldDefType> MotionArgs::getFieldDefs()
{
    std::vector<FieldDefType> fieldDefs;
    fieldDefs.push_back(FieldDefType("rel", &_isRelative, "bool"));
    fieldDefs.push_back(FieldDefType("lin", &_linearNoRamp, "bool"));
    fieldDefs.push_back(FieldDefType("steps", &_unitsAreSteps, "bool"));
    fieldDefs.push_back(FieldDefType("nosplit", &_dontSplitMove, "bool"));
    fieldDefs.push_back(FieldDefType("exDistOk", &_extrudeValid, "bool"));
    fieldDefs.push_back(FieldDefType("speedOk", &_targetSpeedValid, "bool"));
    fieldDefs.push_back(FieldDefType("cw", &_moveClockwise, "bool"));
    fieldDefs.push_back(FieldDefType("rapid", &_moveRapid, "bool"));
    fieldDefs.push_back(FieldDefType("OoBOk", &_allowOutOfBounds, "bool"));
    fieldDefs.push_back(FieldDefType("more", &_moreMovesComing, "bool"));
    fieldDefs.push_back(FieldDefType("homing", &_isHoming, "bool"));
    fieldDefs.push_back(FieldDefType("idxOk", &_motionTrackingIndexValid, "bool"));
    fieldDefs.push_back(FieldDefType("feedPerMin", &_feedrateUnitsPerMin, "bool"));
    fieldDefs.push_back(FieldDefType("speed", &_targetSpeed, "double"));
    fieldDefs.push_back(FieldDefType("exDist", &_extrudeDistance, "double"));
    fieldDefs.push_back(FieldDefType("feedrate", &_feedrate, "double"));
    fieldDefs.push_back(FieldDefType("idx", &_motionTrackingIdx, "double"));
    return fieldDefs;
}

void MotionArgs::fromJSON(const char* jsonStr)
{
    // Get json
    JSONParams cmdJson(jsonStr);
    clear();

    // Get field definitions
    std::vector<FieldDefType> fieldDefs = getFieldDefs();

    // Get fields
    for (auto &fieldDef : fieldDefs)
    {
        // Check if value is present
        if (!cmdJson.contains(fieldDef._name.c_str()))
            continue;

        // Get value
        if (fieldDef._dataType.equalsIgnoreCase("bool"))
        {
            bool fieldVal = cmdJson.getLong(fieldDef._name.c_str(), 0) != 0;
            *((bool*)fieldDef._pValue) = fieldVal;
        }
        else if (fieldDef._dataType.equalsIgnoreCase("double"))
        {
            double fieldVal = cmdJson.getDouble(fieldDef._name.c_str(), 0);
            *((double*)fieldDef._pValue) = fieldVal;
        }
    }

    // // Extract flags
    // std::vector<String> flagsList;
    // cmdJson.getArrayElems("flags", flagsList);
    // for (const ConfigBase flag : flagsList)
    // {
    //     String flagName = flag.getString("n", "");
    //     bool flagValue = flag.getLong("v", 0) != 0;
    //     if (flagName.length() == 0)
    //         continue;
    //     _isRelative               = flagName.equalsIgnoreCase("isRelative")               ? flagValue : _isRelative              ;
    //     _linearNoRamp             = flagName.equalsIgnoreCase("linearNoRamp")             ? flagValue : _linearNoRamp            ;
    //     _unitsAreSteps            = flagName.equalsIgnoreCase("unitsAreSteps")            ? flagValue : _unitsAreSteps           ;
    //     _dontSplitMove            = flagName.equalsIgnoreCase("dontSplitMove")            ? flagValue : _dontSplitMove           ;
    //     _extrudeValid             = flagName.equalsIgnoreCase("extrudeValid")             ? flagValue : _extrudeValid            ;
    //     _targetSpeedValid         = flagName.equalsIgnoreCase("targetSpeedValid")         ? flagValue : _targetSpeedValid        ;
    //     _moveClockwise            = flagName.equalsIgnoreCase("moveClockwise")            ? flagValue : _moveClockwise           ;
    //     _moveRapid                = flagName.equalsIgnoreCase("moveRapid")                ? flagValue : _moveRapid               ;
    //     _allowOutOfBounds         = flagName.equalsIgnoreCase("allowOutOfBounds")         ? flagValue : _allowOutOfBounds        ;
    //     _moreMovesComing          = flagName.equalsIgnoreCase("moreMovesComing")          ? flagValue : _moreMovesComing         ;
    //     _isHoming                 = flagName.equalsIgnoreCase("isHoming")                 ? flagValue : _isHoming                ;
    //     _motionTrackingIndexValid = flagName.equalsIgnoreCase("motionTrackingIndexValid") ? flagValue : _motionTrackingIndexValid;
    //     _feedrateUnitsPerMin      = flagName.equalsIgnoreCase("feedrateUnitsPerMin")      ? flagValue : _feedrateUnitsPerMin     ;
    // }

    // Extract speed, etc
    // _targetSpeed = cmdJson.getDouble("targetSpeed", _targetSpeed);
    // _extrudeDistance = cmdJson.getDouble("extrudeDistance", _extrudeDistance);
    // _feedrate = cmdJson.getDouble("feedrate", _feedrate);
    // _motionTrackingIdx = cmdJson.getDouble("motionTrackingIdx", _motionTrackingIdx);

    // Extract endstops
    _endstops.fromJSON(cmdJson, "endstops");

    // Extract position
    std::vector<String> posList;
    cmdJson.getArrayElems("pos", posList);
    for (const ConfigBase pos : posList)
    {
        int32_t axisIdx = pos.getLong("a", -1);
        double axisPos = pos.getDouble("p", 0);
        
        LOG_I(MODULE_PREFIX, "cmdJson %s pos %s axisIdx: %d, axisPos: %f", cmdJson.getConfigString().c_str(), pos.getConfigString().c_str(), axisIdx, axisPos);

        if ((axisIdx < 0) || (axisIdx >= MULTISTEPPER_MAX_AXES))
            continue;
        _axisValid[axisIdx] = true;
        _axisPos[axisIdx] = axisPos;
    }
}

String MotionArgs::toJSON()
{
    // Get field definitions
    std::vector<FieldDefType> fieldDefs = getFieldDefs();

    // Output string
    String jsonStr;

    // Expand fields
    for (auto &fieldDef : fieldDefs)
    {
        // Get value
        if (fieldDef._dataType.equalsIgnoreCase("bool"))
        {
            jsonStr += "\"" + fieldDef._name + "\":" + String(*((bool*)fieldDef._pValue)) + ",";
        }
        else if (fieldDef._dataType.equalsIgnoreCase("double"))
        {
            jsonStr += "\"" + fieldDef._name + "\":" + String(*((double*)fieldDef._pValue)) + ",";
        }
    }

    // Expand endstops
    jsonStr += _endstops.toJSON("endstops");

    // Expand position
    jsonStr += ",\"pos\":[";
    bool firstAxis = true;
    for (int32_t axisIdx = 0; axisIdx < MULTISTEPPER_MAX_AXES; axisIdx++)
    {
        if (!_axisValid[axisIdx])
            continue;
        if (!firstAxis)
        {
            jsonStr += ",";
            firstAxis = false;
        }
        jsonStr += "{\"a\":" + String(axisIdx) + ",\"p\":" + String(_axisPos[axisIdx]) + "}";
    }
    jsonStr += "]";
    return "{" + jsonStr + "}";
}
