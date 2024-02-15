/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DetectHardware
// Rob Dobson 2023-24
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include "RaftCoreApp.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get hardware type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class HWDetectPinDef
{
public:
    enum PinExpectation
    {
        PIN_EXPECTED_FLOATING,
        PIN_EXPECTED_HELD_HIGH,
        PIN_EXPECTED_HELD_LOW,
        PIN_EXPECTED_BETWEEN_BOUNDS_FLOATING,
        PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_UP,
        PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN,
        PIN_EXPECTED_NOT_STRONG_PULLUP,
        PIN_EXPECTED_NOT_STRONG_PULLDOWN
    };
    HWDetectPinDef(int pin, PinExpectation pinExpectation,
                int threshold1 = 0, int threshold2 = 0)
        :   _pin(pin),
            _pinExpectation(pinExpectation),
            _threshold1(threshold1),
            _threshold2(threshold2)
    {
    }
    int _pin;
    PinExpectation _pinExpectation;
    int _threshold1;
    int _threshold2;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Detection helper class
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class HWDetectConfig
{
public:
    HWDetectConfig(const std::vector<HWDetectPinDef> &pinDefs) 
        : _pinDefs(pinDefs)
    {
    }
    bool isThisHW(bool forceTestAll = false);
private:
    bool checkAnalogValues(int pin, int testPinMode, int threshold1, int threshold2);
    bool checkDigitalValues(int pin, int testPinMode, int expectedValue);
    std::vector<HWDetectPinDef> _pinDefs;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main detection function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace DetectHardware
{
    void detectHardware(RaftCoreApp& app);
};

