// RBotFirmware
// Rob Dobson 2016-2021

#pragma once

#include "AxisGeomBase.h"

class AxisGeomXYZ : public AxisGeomBase
{
  public:
    virtual bool ptToActuator(AxesPosValues targetPt, 
                    AxesParamVals<AxisStepsDataType> &outActuator, 
                    const AxesPosition &curPos, 
                    const AxesParams &axesParams, 
                    bool allowOutOfBounds) override final;
    virtual bool actuatorToPt(const AxesParamVals<AxisStepsDataType> &targetActuator, 
                    AxesPosValues &outPt, 
                    const AxesPosition &curPos, 
                    const AxesParams &axesParams) override final;

// typedef bool (*ptToActuatorFnType)(AxisFloats &targetPt, AxisFloats &outActuator, AxesPosition &curPos, AxesParams &axesParams, bool allowOutOfBounds);
// typedef void (*actuatorToPtFnType)(AxisInt32s &targetActuator, AxisFloats &outPt, AxesPosition &curPos, AxesParams &axesParams);
// typedef void (*correctStepOverflowFnType)(AxesPosition &curPos, AxesParams &axesParams);
// typedef void (*convertCoordsFnType)(RobotCommandArgs& cmdArgs, AxesParams &axesParams);
// typedef void (*setRobotAttributesFnType)(AxesParams& axesParams, String& robotAttributes);

};

