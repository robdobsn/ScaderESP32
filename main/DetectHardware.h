/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DetectHardware for Scader PCBs
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <ArduinoOrAlt.h>

enum HWRevision {
    HW_IS_GENERIC_BOARD = 1,
    HW_IS_RFID_BOARD = 2,
    HW_IS_LIGHT_SCADER_BOARD = 3,
    HW_IS_SCADER_SHADES_BOARD = 4,
    HW_IS_CONSV_OPENER_BOARD = 5,
    };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get hardware revision
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

class DetectHardware
{
public:
    static int _hardwareRevision;
    static int detectHardware();
    static int getHWRevision()
    {
        if (_hardwareRevision == -1)
            _hardwareRevision = detectHardware();
        return _hardwareRevision;
    }
    static const char* getHWRevisionStr(int hwRev)
    {
        switch (hwRev)
        {
            case HW_IS_GENERIC_BOARD:
                return "Generic";
            case HW_IS_RFID_BOARD:
                return "RFID";
            case HW_IS_LIGHT_SCADER_BOARD:
                return "LightScader";
            case HW_IS_SCADER_SHADES_BOARD:
                return "ScaderShades";
            case HW_IS_CONSV_OPENER_BOARD:
                return "ConsvOpener";
        }
        return "Unknown";
    }
};

