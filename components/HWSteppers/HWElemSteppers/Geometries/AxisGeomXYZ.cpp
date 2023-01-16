// RBotFirmware
// Rob Dobson 2016-18

#include "AxisGeomXYZ.h"
#include "AxisGeomBase.h"
#include "Axes/AxisValues.h"
#include "Axes/AxesParams.h"

static const char* MODULE_PREFIX = "GeomXYZ";

#define DEBUG_XYBOT_MOTION

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Coord transform from real-world coords to actuator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool AxisGeomXYZ::ptToActuator(AxesPosValues targetPt, 
                AxesParamVals<AxisStepsDataType> &outActuator, 
                const AxesPosition &curPos, 
                const AxesParams &axesParams, 
                bool allowOutOfBounds)
{
    // Check machine bounds and fix the value if required
    bool ptWasValid = axesParams.ptInBounds(targetPt, !allowOutOfBounds);

    // Perform conversion
    for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    {
        // Axis val from home point
        float axisValFromHome = targetPt.getVal(axisIdx) - axesParams.getHomeOffsetVal(axisIdx);
        // Convert to steps and add offset to home in steps
        outActuator.setVal(axisIdx, round(axisValFromHome * axesParams.getStepsPerUnit(axisIdx)
                        + axesParams.gethomeOffSteps(axisIdx)));

#ifdef DEBUG_XYBOT_MOTION
        LOG_I(MODULE_PREFIX, "ptToActuator axis%d %f%s-> %d (homeOffVal %f, homeOffSteps %d)", 
                axisIdx,
                targetPt.getVal(axisIdx), 
                ptWasValid ? " " : "(OOB) ",
                outActuator.getVal(axisIdx),
                axesParams.getHomeOffsetVal(axisIdx), 
                axesParams.gethomeOffSteps(axisIdx));
#endif
    }
    return ptWasValid;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Coord transform from actuator to real-world coords
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool AxisGeomXYZ::actuatorToPt(const AxesParamVals<AxisStepsDataType> &targetActuator, 
            AxesPosValues &outPt, 
            const AxesPosition &curPos, 
            const AxesParams &axesParams)
{
    // Perform conversion
    for (int axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    {
        double ptVal = targetActuator.getVal(axisIdx) - axesParams.gethomeOffSteps(axisIdx);
        ptVal = ptVal / axesParams.getStepsPerUnit(axisIdx) + axesParams.getHomeOffsetVal(axisIdx);
        outPt.setVal(axisIdx, ptVal);
#ifdef DEBUG_XYBOT_MOTION
        LOG_I(MODULE_PREFIX, "actuatorToPt axis%d %d -> %f (perunit %f, homeOffSteps %d, homeOffVal %f)",
                        axisIdx, 
                        targetActuator.getVal(axisIdx),
                        ptVal, 
                        axesParams.getStepsPerUnit(axisIdx), 
                        axesParams.gethomeOffSteps(axisIdx),
                        axesParams.getHomeOffsetVal(axisIdx));
#endif
    }
    return true;
}

