// RBotFirmware
// Rob Dobson 2016-18

#pragma once

#include "AxisParams.h"
#include "AxisValues.h"
#include "ConfigBase.h"
#include <vector>

class AxesParams
{
public:
    AxesParams()
    {
        clearAxes();
    }

    void clearAxes()
    {
        _masterAxisIdx = -1;
        _masterAxisMaxAccUnitsPerSec2 = AxisParams::acceleration_default;
        _axisParams.clear();
    }

    double getStepsPerUnit(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::stepsPerRot_default / AxisParams::posUnitsPerRot_default;
        return _axisParams[axisIdx].stepsPerUnit();
    }

    double getStepsPerRot(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::stepsPerRot_default;
        return _axisParams[axisIdx]._stepsPerRot;
    }

    double getunitsPerRot(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::posUnitsPerRot_default;
        return _axisParams[axisIdx]._unitsPerRot;
    }

    AxisStepsDataType gethomeOffSteps(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return 0;
        return _axisParams[axisIdx]._homeOffSteps;
    }

    void sethomeOffSteps(uint32_t axisIdx, AxisStepsDataType newVal)
    {
        if (axisIdx >= _axisParams.size())
            return;
        _axisParams[axisIdx]._homeOffSteps = newVal;
    }

    double getHomeOffsetVal(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return 0;
        return _axisParams[axisIdx]._homeOffsetVal;
    }

    double getMaxVal(uint32_t axisIdx, double &maxVal) const
    {
        if (axisIdx >= _axisParams.size())
            return false;
        if (!_axisParams[axisIdx]._maxValValid)
            return false;
        maxVal = _axisParams[axisIdx]._maxVal;
        return true;
    }

    double getMinVal(uint32_t axisIdx, double &minVal) const
    {
        if (axisIdx >= _axisParams.size())
            return false;
        if (!_axisParams[axisIdx]._minValValid)
            return false;
        minVal = _axisParams[axisIdx]._minVal;
        return true;
    }

    double getMaxSpeed(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::maxVelocity_default;
        return _axisParams[axisIdx]._maxVelocityUnitsPerSec;
    }

    double getMinSpeed(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::minVelocity_default;
        return _axisParams[axisIdx]._minVelocityUnitsPerSec;
    }

    AxisStepRateDataType getMaxStepRatePerSec(uint32_t axisIdx, bool forceRecalc = false) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::maxRPM_default * AxisParams::stepsPerRot_default / 60;
        if (forceRecalc)
            return _axisParams[axisIdx]._maxRPM * _axisParams[axisIdx]._stepsPerRot / 60;
        return _maxStepRatesPerSec.getVal(axisIdx);
    }

    double getMaxAccel(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return AxisParams::acceleration_default;
        return _axisParams[axisIdx]._maxAccelUnitsPerSec2;
    }

    bool isPrimaryAxis(uint32_t axisIdx) const
    {
        if (axisIdx >= _axisParams.size())
            return false;
        return _axisParams[axisIdx]._isPrimaryAxis;
    }

    bool ptInBounds(AxesPosValues &pt, bool correctValueInPlace) const
    {
        bool wasValid = true;
        for (uint32_t axisIdx = 0; (axisIdx < _axisParams.size()) && (axisIdx < pt.numAxes()); axisIdx++)
            wasValid = wasValid && _axisParams[axisIdx].ptInBounds(pt._pt[axisIdx], correctValueInPlace);
        return wasValid;
    }

    bool setupAxes(const ConfigBase& config, const char* pConfigPrefix)
    {
        // Clear existing
        _axisParams.clear();
        
        // TODO REFACTOR TO USE JSON PATHS
        
        // Extract sub-system elements
        std::vector<String> axesVec;
        if (config.getArrayElems("axes", axesVec, pConfigPrefix))
        {
            // Check index ok
            uint32_t numAxesToAdd = axesVec.size();
            if (numAxesToAdd >= AXIS_VALUES_MAX_AXES)
                numAxesToAdd = AXIS_VALUES_MAX_AXES;

            // Resize appropriately
            _axisParams.resize(numAxesToAdd);
            uint32_t axisIdx = 0;
            for (ConfigBase axisConfig : axesVec)
            {
                // Get params
                String paramsJson = axisConfig.getString("params", "{}");

                // Set the axis parameters
                _axisParams[axisIdx].setFromJSON(paramsJson.c_str());

                // Find the master axis (dominant one, or first primary - or just first)
                setMasterAxis(axisIdx);

                // Cache axis max step rate
                for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
                {
                    _maxStepRatesPerSec.setVal(i, getMaxStepRatePerSec(i, true));
                }

                // Next
                axisIdx++;
            }
        }
        return true;
    }

    void debugLog()
    {
        for (uint32_t axisIdx = 0; axisIdx < _axisParams.size(); axisIdx++)
            _axisParams[axisIdx].debugLog(axisIdx);
    }
    // bool configureAxis(const char *robotConfigJSON, uint32_t axisIdx, String &axisJSON)
    // {
    //     if (axisIdx >= AXIS_VALUES_MAX_AXES)
    //         return false;

    //     // Get params
    //     String axisIdStr = "axis" + String(axisIdx);
    //     axisJSON = RdJson::getString(axisIdStr.c_str(), "{}", robotConfigJSON);
    //     if (axisJSON.length() == 0 || axisJSON.equals("{}"))
    //         return false;

    //     // Set the axis parameters
    //     _axisParams[axisIdx].setFromJSON(axisJSON.c_str());
    //     _axisParams[axisIdx].debugLog(axisIdx);

    //     // Find the master axis (dominant one, or first primary - or just first)
    //     setMasterAxis(axisIdx);

    //     // Cache axis max step rate
    //     for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    //     {
    //         _maxStepRatesPerSec.setVal(axisIdx, getMaxStepRatePerSec(axisIdx, true));
    //     }
    //     return true;
    // }

    // Set the master axis either to the dominant axis (if there is one)
    // or just the first one found
    void setMasterAxis(int fallbackAxisIdx)
    {
        int dominantIdx = -1;
        int primaryIdx = -1;
        for (uint32_t i = 0; i < _axisParams.size(); i++)
        {
            if (_axisParams[i]._isDominantAxis)
            {
                dominantIdx = i;
                break;
            }
            if (_axisParams[i]._isPrimaryAxis)
            {
                if (primaryIdx == -1)
                    primaryIdx = i;
            }
        }
        if (dominantIdx != -1)
            _masterAxisIdx = dominantIdx;
        else if (primaryIdx != -1)
            _masterAxisIdx = primaryIdx;
        else if (_masterAxisIdx == -1)
            _masterAxisIdx = fallbackAxisIdx;

        // Cache values for master axis
        _masterAxisMaxAccUnitsPerSec2 = getMaxAccel(_masterAxisIdx);
    }

    AxisAccDataType masterAxisMaxAccel() const
    {
        return _masterAxisMaxAccUnitsPerSec2;
    }

    AxisVelocityDataType masterAxisMaxSpeed() const
    {
        if (_masterAxisIdx != -1)
            return getMaxSpeed(_masterAxisIdx);
        return getMaxSpeed(0);
    }

private:
    // Axis parameters
    std::vector<AxisParams> _axisParams;

    // Master axis
    int _masterAxisIdx;

    // Cache values for master axis as they are used frequently in the planner
    AxisAccDataType _masterAxisMaxAccUnitsPerSec2;

    // Cache max step rate
    AxesParamVals<AxisStepRateDataType> _maxStepRatesPerSec;
};
