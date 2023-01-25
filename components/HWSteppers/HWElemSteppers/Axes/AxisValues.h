// RBotFirmware
// Rob Dobson 2016-18

#pragma once

#include <math.h>
#include <stdint.h>
#include <ArduinoOrAlt.h>
#include "esp_attr.h"
#include <Logger.h>
#include "JSONParams.h"
#include <vector>

// Max axes and endstops supported
static const uint32_t AXIS_VALUES_MAX_AXES = 3;
static const uint32_t AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS = 2;

// Data types
typedef float AxisStepRateDataType;
typedef float AxisVelocityDataType;
typedef float AxisAccDataType;
typedef float AxisPosDataType;
typedef float AxisPosFactorDataType;
typedef float AxisRPMDataType;
typedef float AxisStepsFactorDataType;
typedef int32_t AxisStepsDataType;
typedef int32_t AxisStepsDataType;
typedef float AxisUnitVectorDataType;
typedef float AxisDistDataType;

// Utils
class AxisUtils
{
public:
    static double cosineRule(double a, double b, double c);
    static double wrapRadians(double angle);
    static double wrapDegrees(double angle);
    static double r2d(double angleRadians);
    static double d2r(double angleDegrees);
    static bool isApprox(double v1, double v2, double withinRng = 0.0001);
    static bool isApproxWrap(double v1, double v2, double wrapSize=360.0, double withinRng = 0.0001);
};

class AxesPosValues
{
public:
    typedef float AxisPosStoreType;
    static const uint32_t STORE_TO_POS_FACTOR = 1;
    AxisPosStoreType _pt[AXIS_VALUES_MAX_AXES];
    uint8_t _validityFlags;

public:
    AxesPosValues()
    {
        clear();
    }
    AxesPosValues(const AxesPosValues &other)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            _pt[i] = other._pt[i];
        _validityFlags = other._validityFlags;
    }
    AxesPosValues(AxisPosDataType x, AxisPosDataType y)
    {
        _pt[0] = x * STORE_TO_POS_FACTOR;
        _pt[1] = y * STORE_TO_POS_FACTOR;
        _pt[2] = 0;
        _validityFlags = 0x03;
    }
    AxesPosValues(AxisPosDataType x, AxisPosDataType y, AxisPosDataType z)
    {
        _pt[0] = x * STORE_TO_POS_FACTOR;
        _pt[1] = y * STORE_TO_POS_FACTOR;
        _pt[2] = z * STORE_TO_POS_FACTOR;
        _validityFlags = 0x07;
    }
    AxesPosValues(AxisPosDataType x, AxisPosDataType y, AxisPosDataType z, bool xValid, bool yValid, bool zValid)
    {
        _pt[0] = x * STORE_TO_POS_FACTOR;
        _pt[1] = y * STORE_TO_POS_FACTOR;
        _pt[2] = z * STORE_TO_POS_FACTOR;
        _validityFlags = xValid ? 0x01 : 0;
        _validityFlags |= yValid ? 0x02 : 0;
        _validityFlags |= zValid ? 0x04 : 0;
    }
    uint32_t numAxes() const
    {
        return AXIS_VALUES_MAX_AXES;
    }
    bool operator==(const AxesPosValues& other) const
    {
        if (_validityFlags != other._validityFlags)
            return false;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            if ((_validityFlags & (0x01 << i)) && (_pt[i] != other._pt[i]))
                return false;
        return true;
    }
    bool operator!=(const AxesPosValues& other) const
    {
        return !(*this == other);
    }
    void clear()
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            _pt[i] = 0;
        _validityFlags = 0;
    }
    AxisPosDataType IRAM_ATTR getVal(uint32_t axisIdx) const
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
            return _pt[axisIdx] / STORE_TO_POS_FACTOR;
        return 0;
    }
    void setVal(uint32_t axisIdx, AxisPosDataType val)
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
        {
            _pt[axisIdx] = val * STORE_TO_POS_FACTOR;
            uint32_t axisMask = 0x01 << axisIdx;
            _validityFlags |= axisMask;
        }
    }
    void set(AxisPosDataType val0, AxisPosDataType val1, AxisPosDataType val2 = 0)
    {
        _pt[0] = val0 * STORE_TO_POS_FACTOR;
        _pt[1] = val1 * STORE_TO_POS_FACTOR;
        _pt[2] = val2 * STORE_TO_POS_FACTOR;
        _validityFlags = 0x07;
    }
    void setValid(uint32_t axisIdx, bool isValid)
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
        {
            uint32_t axisMask = 0x01 << axisIdx;
            if (isValid)
                _validityFlags |= axisMask;
            else
                _validityFlags &= ~axisMask;
        }
    }
    bool isValid(uint32_t axisIdx) const
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
        {
            uint32_t axisMask = 0x01 << axisIdx;
            return (_validityFlags & axisMask) != 0;
        }
        return false;
    }
    bool anyValid() const
    {
        return (_validityFlags != 0);
    }
    AxisPosDataType X() const
    {
        return _pt[0] / STORE_TO_POS_FACTOR;
    }
    void X(AxisPosDataType val)
    {
        _pt[0] = val * STORE_TO_POS_FACTOR;
        _validityFlags |= 0x01;
    }
    AxisPosDataType Y() const
    {
        return _pt[1] / STORE_TO_POS_FACTOR;
    }
    void Y(AxisPosDataType val)
    {
        _pt[1] = val * STORE_TO_POS_FACTOR;
        _validityFlags |= 0x02;
    }
    AxisPosDataType Z() const
    {
        return _pt[2] / STORE_TO_POS_FACTOR;
    }
    void Z(AxisPosDataType val)
    {
        _pt[2] = val * STORE_TO_POS_FACTOR;
        _validityFlags |= 0x04;
    }
    AxesPosValues &operator=(const AxesPosValues &other)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            _pt[i] = other._pt[i];
        _validityFlags = other._validityFlags;
        return *this;
    }
    AxesPosValues operator-(const AxesPosValues &pt) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            result._pt[i] = _pt[i] - (pt.isValid(i) ? pt._pt[i] : 0);
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator-(AxisPosDataType val) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            result._pt[i] = _pt[i] - (val * STORE_TO_POS_FACTOR);
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator+(const AxesPosValues &pt) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            result._pt[i] = _pt[i] + (pt.isValid(i) ? pt._pt[i] : 0);
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator+(AxisPosDataType val) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            result._pt[i] = _pt[i] + (val * STORE_TO_POS_FACTOR);
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator/(const AxesPosValues &pt) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
        {
            if (pt._pt[i] != 0)
                result._pt[i] = _pt[i] / (pt.isValid(i) ? pt._pt[i] : 1);
        }
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator/(AxisPosDataType val) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
        {
            if (val != 0)
                result._pt[i] = _pt[i] / (val * STORE_TO_POS_FACTOR);
        }
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator*(const AxesPosValues &pt) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
        {
            result._pt[i] = _pt[i] * (pt.isValid(i) ? pt._pt[i] : 1);
        }
        result._validityFlags = _validityFlags;
        return result;
    }
    AxesPosValues operator*(AxisPosDataType val) const
    {
        AxesPosValues result;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
        {
            result._pt[i] = _pt[i] * (val * STORE_TO_POS_FACTOR);
        }
        result._validityFlags = _validityFlags;
        return result;
    }

    // Calculate distance between points including only axes that are indicated
    // true in the optional includeDist argument
    AxisPosDataType distanceTo(const AxesPosValues &pt, bool includeDist[] = NULL) const
    {
        double distSum = 0;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
        {
            if (isValid(i) && ((includeDist == NULL) || includeDist[i]))
            {
                double sq = _pt[i] - pt._pt[i];
                sq = sq * sq;
                distSum += sq;
            }
        }
        return sqrt(distSum) / STORE_TO_POS_FACTOR;
    }

    // Debug
    String getDebugStr()
    {
        char debugStr[40];
        snprintf(debugStr, sizeof(debugStr), "X%0.2f%s Y%0.2f%s Z%0.2f%s", 
                _pt[0],
                _validityFlags & 0x01 ? "" : "(INV)",
                _pt[1], 
                _validityFlags & 0x02 ? "" : "(INV)",
                _pt[2],
                _validityFlags & 0x04 ? "" : "(INV)");
        return debugStr;
    }

    // String toJSON()
    // {
    //     String jsonStr = "[";
    //     for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
    //     {
    //         if (axisIdx != 0)
    //             jsonStr += ",";
    //         jsonStr += String((double)(_pt[axisIdx]) / STORE_TO_POS_FACTOR, 2);
    //     }
    //     jsonStr += "]";
    //     return jsonStr;
    // }
};

template <typename T>
class AxesParamVals
{
public:
    AxesParamVals()
    {
        clear();
    }
    AxesParamVals(const AxesParamVals &other)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            _vals[i] = other._vals[i];
    }

    AxesParamVals(T x, T y)
    {
        _vals[0] = x;
        _vals[1] = y;
        _vals[2] = 0;
    }
    AxesParamVals(T x, T y, T z)
    {
        _vals[0] = x;
        _vals[1] = y;
        _vals[2] = z;
    }
    void clear()
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            _vals[i] = 0;
    }
    void setVal(uint32_t axisIdx, T val)
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
        {
            _vals[axisIdx] = val;
        }
    }
    T getVal(uint32_t axisIdx) const
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
            return _vals[axisIdx];
        return 0;
    }
    T vectorMultSum(AxesParamVals<T> other)
    {
        T result = 0;
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            result += _vals[i] * other._vals[i];
        return result;
    }
    // Debug
    String getDebugStr()
    {
        char debugStr[40];
        snprintf(debugStr, sizeof(debugStr), "X%0.2f Y%0.2f Z%0.2f", 
                (double)_vals[0],
                (double)_vals[1],
                (double)_vals[2]);
        return debugStr;
    }    
    String toJSON()
    {
        String jsonStr = "[";
        for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        {
            if (axisIdx != 0)
                jsonStr += ",";
            jsonStr += String(_vals[axisIdx]);
        }
        jsonStr += "]";
        return jsonStr;
    }

private:
    T _vals[AXIS_VALUES_MAX_AXES];
};

class AxisValidBools
{
public:
    struct BoolBitValues
    {
        bool bX : 1;
        bool bY : 1;
        bool bZ : 1;
    };
    union {
        BoolBitValues _bits;
        uint16_t _uint;
    };

    AxisValidBools()
    {
        _uint = 0;
    }
    AxisValidBools(const AxisValidBools &other)
    {
        _uint = other._uint;
    }
    AxisValidBools &operator=(const AxisValidBools &other)
    {
        _uint = other._uint;
        return *this;
    }
    bool operator==(const AxisValidBools& other)
    {
        return _uint == other._uint;
    }
    bool operator!=(const AxisValidBools& other)
    {
        return !(*this == other);
    }
    bool isValid(uint32_t axisIdx)
    {
        return _uint & (1 << axisIdx);
    }
    AxisValidBools(bool xValid, bool yValid, bool zValid)
    {
        _uint = 0;
        _bits.bX = xValid;
        _bits.bY = yValid;
        _bits.bZ = zValid;
    }
    bool XValid()
    {
        return _bits.bX;
    }
    bool YValid()
    {
        return _bits.bY;
    }
    bool ZValid()
    {
        return _bits.bZ;
    }
    bool operator[](uint32_t boolIdx)
    {
        return isValid(boolIdx);
    }
    void setVal(uint32_t boolIdx, bool val)
    {
        if (val)
            _uint |= (1 << boolIdx);
        else
            _uint &= (0xffff ^ (1 << boolIdx));
    }
    // void fromJSON(const char* jsonStr)
    // {
    //     JSONParams jsonData(jsonStr);
    //     _bits.bX = jsonData.getBool("x", 0);
    //     _bits.bY = jsonData.getBool("y", 0);
    //     _bits.bZ = jsonData.getBool("z", 0);
    // }
    // String toJSON()
    // {
    //     char jsonStr[100];
    //     snprintf(jsonStr, sizeof(jsonStr), R"("x":%d,"y":%d,"z":%d)", _bits.bX, _bits.bY, _bits.bZ);
    //     return jsonStr;
    // }
};

static const char* AxisEndstopMinMaxEnumStrs[] = {"0", "1", "T", "X"};

class AxisEndstopChecks
{
public:
    static constexpr uint32_t MIN_MAX_VALID_BIT = 31;
    static constexpr uint32_t MIN_MAX_VALUES_MASK = 0x3fffffff;
    static constexpr uint32_t MIN_VAL_IDX = 0;
    static constexpr uint32_t MAX_VAL_IDX = 1;
    static constexpr uint32_t BITS_PER_VAL = 2;
    static constexpr uint32_t BITS_PER_VAL_MASK = 0x03;
    static constexpr uint32_t MAX_AXIS_INDEX = (32 / (AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS * BITS_PER_VAL)) - 1;

    enum AxisMinMaxEnum
    {
        END_STOP_NOT_HIT = 0,
        END_STOP_HIT = 1,
        END_STOP_TOWARDS = 2,
        END_STOP_NONE = 3
    };

    AxisEndstopChecks()
    {
        _uint = 0;
        for (uint32_t axisIdx = 0; axisIdx < MAX_AXIS_INDEX; axisIdx++)
        {
            for (uint32_t endStopIdx = 0; endStopIdx < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; endStopIdx++)
            {
                uint32_t valIdx = (axisIdx * AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS + endStopIdx) * BITS_PER_VAL;
                _uint |= (END_STOP_NONE << valIdx);
            }
        }
    }
    AxisEndstopChecks(const AxisEndstopChecks &other)
    {
        _uint = other._uint;
    }
    AxisEndstopChecks &operator=(const AxisEndstopChecks &other)
    {
        _uint = other._uint;
        return *this;
    }
    bool operator==(const AxisEndstopChecks& other) const
    {
        return _uint == other._uint;
    }
    bool operator!=(const AxisEndstopChecks& other) const
    {
        return !(*this == other);
    }
    bool isValid() const
    {
        return _uint & (1 << MIN_MAX_VALID_BIT);
    }
    void set(uint32_t axisIdx, uint32_t endStopIdx, AxisMinMaxEnum checkType)
    {
        if ((axisIdx > MAX_AXIS_INDEX) || (endStopIdx >= AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS))
            return;
        uint32_t valIdx = (axisIdx * AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS + endStopIdx) * BITS_PER_VAL;
        uint32_t valMask = (BITS_PER_VAL_MASK << valIdx);
        _uint &= (valMask ^ 0xffffffff);
        _uint |= checkType << valIdx;
        _uint |= (1 << MIN_MAX_VALID_BIT);
    }
    void set(uint32_t axisIdx, uint32_t endStopIdx, const String& minMaxStr)
    {
        AxisMinMaxEnum setTo = END_STOP_NONE;
        for (uint32_t i = 0; i < sizeof(AxisEndstopMinMaxEnumStrs)/sizeof(AxisEndstopMinMaxEnumStrs[0]); i++)
        {
            if (minMaxStr.equalsIgnoreCase(AxisEndstopMinMaxEnumStrs[i]))
            {
                setTo = (AxisMinMaxEnum)i;
                break;
            }
        }
        set(axisIdx, endStopIdx, setTo);
    }
    AxisMinMaxEnum IRAM_ATTR get(uint32_t axisIdx, uint32_t endStopIdx) const
    {
        if ((axisIdx > MAX_AXIS_INDEX) || (endStopIdx >= AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS))
            return END_STOP_NONE;
        uint32_t valIdx = (axisIdx * AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS + endStopIdx) * BITS_PER_VAL;
        return (AxisMinMaxEnum)((_uint >> valIdx) & BITS_PER_VAL_MASK);
    }
    // Reverse endstop direction for endstops that are set
    void reverse()
    {
        for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        {
            for (uint32_t i = 0; i < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; i++)
            {
                AxisMinMaxEnum esEnum = get(axisIdx, i);
                if (esEnum == END_STOP_HIT)
                    esEnum = END_STOP_NOT_HIT;
                else if (esEnum == END_STOP_NOT_HIT)
                    esEnum = END_STOP_HIT;
                set(axisIdx, i, esEnum);
            }
        }
    }
    // Clear endstops on all axes
    void clear()
    {
        _uint = 0;
    }
    // Set endstop on all axes when moving towards
    void all()
    {
        uint32_t newUint = 0;
        for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        {
            newUint = newUint << (AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS * BITS_PER_VAL);
            // Stop when endstop is hit and axis is moving towards this endstop
            for (uint32_t valIdx = 0; valIdx < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; valIdx++)
                newUint |= END_STOP_TOWARDS << (valIdx * BITS_PER_VAL);
        }
        _uint = newUint |= (1 << MIN_MAX_VALID_BIT);
    }
    inline bool IRAM_ATTR any() const
    {
        return (_uint & (1 << MIN_MAX_VALID_BIT)) && (_uint & MIN_MAX_VALUES_MASK);
    }
    uint32_t debugGetRawValue() const
    {
        return _uint;
    }
    String getStr(AxisMinMaxEnum minMax) const
    {
        return AxisEndstopMinMaxEnumStrs[minMax];
    }
    void fromJSON(const JSONParams& jsonData, const char* elemName)
    {
        // Extract array
        std::vector<String> endpointList;
        jsonData.getArrayElems(elemName, endpointList);
        uint32_t axisIdx = 0;
        for (const JSONParams endpoint : endpointList)
        {
            if (axisIdx >= MAX_AXIS_INDEX)
                break;
            for (uint32_t endstopIdx = 0; endstopIdx < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; endstopIdx++)
            {
                char endstopIdxStr[20];
                snprintf(endstopIdxStr, sizeof(endstopIdxStr), "[%d]", endstopIdx);
                set(axisIdx, endstopIdx, endpoint.getString(endstopIdxStr, ""));
            }
            axisIdx++;
        }
    }
    String toJSON(const char* elemName) const
    {
        String jsonStr = "[";
        for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        {
            if (axisIdx != 0)
                jsonStr += ",";
            jsonStr += "[";
            for (uint32_t endstopIdx = 0; endstopIdx < AXIS_VALUES_MAX_ENDSTOPS_PER_AXIS; endstopIdx++)
            {
                if (endstopIdx != 0)
                    jsonStr += ",";
                jsonStr += "\"" + getStr(get(axisIdx, endstopIdx)) + "\"";
            }
            jsonStr += "]";
        }
        jsonStr += "]";
        return "\"" + String(elemName) + "\":" + jsonStr;
    }

private:
    uint32_t _uint;
};

class AxisInt32s
{
public:
    int32_t vals[AXIS_VALUES_MAX_AXES];

    AxisInt32s()
    {
        clear();
    }
    AxisInt32s(const AxisInt32s &u32s)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            vals[i] = u32s.vals[i];
    }
    AxisInt32s &operator=(const AxisInt32s &u32s)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            vals[i] = u32s.vals[i];
        return *this;
    }
    AxisInt32s(int32_t xVal, int32_t yVal, int32_t zVal)
    {
        vals[0] = xVal;
        vals[1] = yVal;
        vals[2] = zVal;
    }
    bool operator==(const AxisInt32s& other)
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            if (vals[i] != other.vals[i])
                return false;
        return true;
    }
    bool operator!=(const AxisInt32s& other)
    {
        return !(*this == other);
    }
    void clear()
    {
        for (uint32_t i = 0; i < AXIS_VALUES_MAX_AXES; i++)
            vals[i] = 0;
    }
    void set(int32_t val0, int32_t val1, int32_t val2 = 0)
    {
        vals[0] = val0;
        vals[1] = val1;
        vals[2] = val2;
    }
    int32_t X()
    {
        return vals[0];
    }
    int32_t Y()
    {
        return vals[1];
    }
    int32_t Z()
    {
        return vals[2];
    }
    int32_t getVal(uint32_t axisIdx)
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
            return vals[axisIdx];
        return 0;
    }
    void setVal(uint32_t axisIdx, int32_t val)
    {
        if (axisIdx < AXIS_VALUES_MAX_AXES)
            vals[axisIdx] = val;
    }
    String toJSON()
    {
        String jsonStr = "[";
        for (uint32_t axisIdx = 0; axisIdx < AXIS_VALUES_MAX_AXES; axisIdx++)
        {
            if (axisIdx != 0)
                jsonStr += ",";
            jsonStr += String(vals[axisIdx]);
        }
        jsonStr += "]";
        return jsonStr;
    }
};
