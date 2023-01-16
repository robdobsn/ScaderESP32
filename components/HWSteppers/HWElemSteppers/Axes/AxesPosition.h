// RBotFirmware
// Rob Dobson 2016-18

#pragma once

#include "AxisValues.h"

class AxesPosition
{
public:
    AxesPosition()
    {
        clear();
    }
    void clear()
    {
        unitsFromHome.clear();
        stepsFromHome.clear();
        _unitsFromHomeValid = false;
    }
    bool unitsFromHomeValid()
    {
        return _unitsFromHomeValid;
    }
    void setUnitsFromHomeValidity(bool valid)
    {
        _unitsFromHomeValid = valid;
    }
    // Axis position in axes units and steps
    AxesPosValues unitsFromHome;
    AxesParamVals<AxisStepsDataType> stepsFromHome;
private:
    bool _unitsFromHomeValid;
};
