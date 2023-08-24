/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DetectHardware for Scader PCBs
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DetectHardware.h"
#include "esp_log.h"
#include <SimpleMovingAverage.h>

// Module
static const char *MODULE_PREFIX = "DetectHardware";

// Initial HW revision
int DetectHardware::_hardwareRevision = -1;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check digital values
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWDetectConfig::checkDigitalValues(int pin, int testPinMode, int expectedValue)
{
    // Set pin mode
    pinMode(pin, testPinMode);
    delay(1);

    // Check pin value
    bool pinVal = digitalRead(pin) != LOW;
    bool inRange = pinVal == expectedValue;

    // Debug
    ESP_LOGI(MODULE_PREFIX, "checkDigitalValues pin %d mode %s val %d asExpected %s",
             pin,
            testPinMode == INPUT ? "INPUT" : (testPinMode == INPUT_PULLDOWN) ? "INPUT_PULLDOWN"
                    : (testPinMode == INPUT_PULLUP) ? "INPUT_PULLUP" : "UNKNOWN",
            pinVal, 
            inRange ? "YES" : "NO");

    // Result
    return inRange;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check analog values
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWDetectConfig::checkAnalogValues(int pin, int testPinMode, int threshold1, int threshold2)
{
    // Check pin voltage is between bounds
    pinMode(pin, testPinMode);
    SimpleMovingAverage<100> ma;
    for (int i = 0; i < 100; i++)
    {
        int pinVal = analogRead(pin);
        ma.sample(pinVal);
    }

    // Check pin value
    bool inRange = ma.getAverage() >= threshold1 && ma.getAverage() <= threshold2;

    // Debug
    ESP_LOGI(MODULE_PREFIX, "checkAnalogValues pin %d mode %s th1 %d th2 %d val %d inRange %s",
             pin,
             testPinMode == INPUT ? "INPUT" : (testPinMode == INPUT_PULLDOWN) ? "INPUT_PULLDOWN"
                                                                              : "INPUT_PULLUP",
             threshold1, threshold2,
             ma.getAverage(),
             inRange ? "Y" : "N");

    // Return result
    return inRange;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Detect
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWDetectConfig::isThisHW(bool forceTestAll)
{
    // Check all pins are as expected
    bool overallResult = true;
    for (auto pinDef : _pinDefs)
    {
        // Check pin value
        bool rslt = false;
        switch (pinDef._pinExpectation)
        {
            case HWDetectPinDef::PIN_EXPECTED_FLOATING:
            {
                bool check1Valid = checkDigitalValues(pinDef._pin, INPUT_PULLUP, HIGH);
                bool check2Valid = (forceTestAll || check1Valid) && checkDigitalValues(pinDef._pin, INPUT_PULLDOWN, LOW);
                rslt = check1Valid && check2Valid;
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_HELD_HIGH:
            {
                rslt = checkDigitalValues(pinDef._pin, INPUT_PULLDOWN, HIGH);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_HELD_LOW:
            {
                rslt = checkDigitalValues(pinDef._pin, INPUT_PULLUP, LOW);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_FLOATING:
            {
                rslt = checkAnalogValues(pinDef._pin, INPUT, pinDef._threshold1, pinDef._threshold2);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_UP:
            {
                rslt = checkAnalogValues(pinDef._pin, INPUT_PULLUP, pinDef._threshold1, pinDef._threshold2);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN:
            {
                rslt = checkAnalogValues(pinDef._pin, INPUT_PULLDOWN, pinDef._threshold1, pinDef._threshold2);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_NOT_STRONG_PULLUP:
            {
                rslt = checkDigitalValues(pinDef._pin, INPUT_PULLDOWN, LOW);
                break;
            }
            case HWDetectPinDef::PIN_EXPECTED_NOT_STRONG_PULLDOWN:
            {
                rslt = checkDigitalValues(pinDef._pin, INPUT_PULLUP, HIGH);
                break;
            }
        }

        // Restore pin
        pinMode(pinDef._pin, INPUT);

        // Overall result
        overallResult = overallResult && rslt;

        // If not as expected then exit
        if (!forceTestAll && !overallResult)
            break;
    }
    return overallResult;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main detection function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DetectHardware::detectHardware()
{
    // Default to generic
    _hardwareRevision = HW_IS_GENERIC_BOARD;

    // Turn off Ethernet PHY power
    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
    ESP_LOGI(MODULE_PREFIX, "detectHardware turned off Ethernet PHY power");
    delay(500);

    // Check for OLIMEX ESP32-POE which has a voltage enable circuit on pin 12
    // Initially pins 18,25,26,27 should be low as power is off
    bool isOlimexESP32PoE = false;
    if (HWDetectConfig(
        {
            HWDetectPinDef(25, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 0, 2500),
            HWDetectPinDef(26, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 0, 2500),
            HWDetectPinDef(27, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 0, 2500),
        }).isThisHW())
    {
        // Turn on Ethernet PHY power
        pinMode(12, OUTPUT);
        digitalWrite(12, HIGH);
        ESP_LOGI(MODULE_PREFIX, "detectHardware turned on Ethernet PHY power");
        delay(500);


        // Check pins 18,25,26,27 are now high
        if (HWDetectConfig(
            {
                HWDetectPinDef(18, HWDetectPinDef::PIN_EXPECTED_HELD_HIGH),
                HWDetectPinDef(25, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 2600, 4096),
                HWDetectPinDef(26, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 2600, 4096),
                HWDetectPinDef(27, HWDetectPinDef::PIN_EXPECTED_BETWEEN_BOUNDS_IF_PULLED_DOWN, 2600, 4096),
            }).isThisHW())
        {
            isOlimexESP32PoE = true;
        }
    }
    // Turn off Ethernet PHY power
    digitalWrite(12, LOW);
    pinMode(12, INPUT);
    ESP_LOGI(MODULE_PREFIX, "detectHardware turned off Ethernet PHY power");
    delay(500);

    // Debug
    ESP_LOGI(MODULE_PREFIX, "isOlimexESP32PoE %s", isOlimexESP32PoE ? "YES" : "NO");

    // Detect boards based on Olimex ESP32-PoE
    if (isOlimexESP32PoE)
    {
        // Default to light scader
        _hardwareRevision = HW_IS_LIGHT_SCADER_BOARD;

        // Check for scader shades
        // Pins 4, 13, 14 and 16 are pulled low on scader shades
        if (HWDetectConfig(
            {
                HWDetectPinDef(4, HWDetectPinDef::PIN_EXPECTED_HELD_LOW),
                HWDetectPinDef(13, HWDetectPinDef::PIN_EXPECTED_HELD_HIGH),
                HWDetectPinDef(14, HWDetectPinDef::PIN_EXPECTED_HELD_LOW),
                HWDetectPinDef(16, HWDetectPinDef::PIN_EXPECTED_HELD_HIGH)
            }).isThisHW())
        {
            _hardwareRevision = HW_IS_SCADER_SHADES_BOARD;
        }
    }
    else
    {
        // Check for RFID PCB hardware
        // Pins 13, 14, 32 are pulled high on that hardware so try to pull them low with the weak ESP32 internal
        // pull-down and see if they remain high
        if (HWDetectConfig(
            {
                HWDetectPinDef(13, HWDetectPinDef::PIN_EXPECTED_HELD_LOW),
                HWDetectPinDef(14, HWDetectPinDef::PIN_EXPECTED_HELD_LOW),
                HWDetectPinDef(32, HWDetectPinDef::PIN_EXPECTED_HELD_LOW)
            }).isThisHW(true))
        {
            _hardwareRevision = HW_IS_RFID_BOARD;
        }

        // Check for Conservatory opener hardware
        if (HWDetectConfig(
            {
                HWDetectPinDef(4, HWDetectPinDef::PIN_EXPECTED_HELD_LOW),
                HWDetectPinDef(5, HWDetectPinDef::PIN_EXPECTED_HELD_LOW)
            }).isThisHW(true))
        {
            _hardwareRevision = HW_IS_CONSV_OPENER_BOARD;
        }
    }

    ESP_LOGI(MODULE_PREFIX, "detectHardware() returning %s (%d)", 
                getHWRevisionStr(_hardwareRevision), _hardwareRevision);
    return _hardwareRevision;
}
