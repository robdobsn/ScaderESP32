/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderLEDPixels
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include "ScaderLEDPixels.h"
#include <ConfigPinMap.h>
#include <RaftUtils.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>
#include <JSONParams.h>
#include "time.h"
#include "driver/gpio.h"

// #include <FastLED.h>

// #define NUM_STRIPS 2
// #define NUM_LEDS_PER_STRIP 100
// #define DATA_PIN_1   32
// #define DATA_PIN_2   33
// //#define CLK_PIN   4
// #define LED_TYPE    WS2811
// #define COLOR_ORDER GRB
// #define NUM_LEDS    NUM_LEDS_PER_STRIP * NUM_STRIPS
// CRGB leds[NUM_LEDS];

// #define BRIGHTNESS          96
// #define FRAMES_PER_SECOND  120

// #define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

static const char *MODULE_PREFIX = "ScaderLEDPixels";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderLEDPixels::ScaderLEDPixels(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig),
          _scaderCommon(*this, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::setup()
{
    // Common
    _scaderCommon.setup();

    // Get settings
    uint8_t defaultBrightness = configGetLong("brightness", 96);
 
    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Clear the strips
    _ledPixels.clear();

    // Get string definition arrays
    std::vector<String> stripInfos;
    uint32_t totalNumPix = 0;
    if (configGetArrayElems("strips", stripInfos))
    {
        // Create LED info (one for all strips)
        for (const JSONParams& stripInfo : stripInfos)
        {
            totalNumPix += stripInfo.getLong("numPix", 0);
        }
        _ledPixels.resize(totalNumPix);

        // Loop over strips
        uint32_t curPixPos = 0;
        for (size_t i = 0; i < stripInfos.size(); i++)
        {
            // Get strip info
            JSONParams stripInfo = stripInfos[i];

            // Extract pin and number of pixels
            uint8_t pixPin = stripInfo.getLong("pixPin", 0);
            uint32_t numPixels = stripInfo.getLong("numPix", 0);

            // Add to FastLED library
            if (pixPin == 32)
                FastLED.addLeds<WS2811, 32, GRB>(_ledPixels.data(), curPixPos, numPixels).setCorrection(TypicalPixelString);
            else if (pixPin == 33)
                FastLED.addLeds<WS2811, 33, GRB>(_ledPixels.data(), curPixPos, numPixels).setCorrection(TypicalPixelString);
            curPixPos += numPixels;
        }

        // set master brightness control
        FastLED.setBrightness(defaultBrightness);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled name %s with %d strips total LEDs %d brightness %d", 
                _scaderCommon.getFriendlyName().c_str(),
                stripInfos.size(), totalNumPix, defaultBrightness);

    // HW Now initialised
    _isInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// int blinkyLed = 0;
// int addn = 1;
// static const int TAIL_LEN = 20;
// #define NUM_LEDS 100

void ScaderLEDPixels::service()
{
//   for (int i = 0; i < TAIL_LEN+1; i++)
//   {
//     int nextLED = blinkyLed + addn*-1*i;
//     if ((nextLED < 0) || (nextLED >= NUM_LEDS))
//       continue;
//     _ledPixels[nextLED] = CHSV(100, 200, 255 * (TAIL_LEN-i) * 1.0 / TAIL_LEN);
//   }
//   FastLED.show();
//   delay(100);

//   // Move the blinkyLed around the strip
//   blinkyLed += addn;
//   if (blinkyLed == NUM_LEDS-1) {
//     addn = -1;
//   }
//   if (blinkyLed == 0) {
//     addn = 1;
//   }

    // Check enabled
    if (!_isInitialised)
        return;

    // Handle patterns
    switch(_pattern)
    {
        case PATTERN_NONE:
            break;
        case PATTERN_LOCATE:
            patternLocate_service();
            break;
        case PATTERN_SNAKE:
            patternSnake_service();
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("ledpix", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderLEDPixels::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "control LED pixels, ledpix/clear, ledpix/set/<N>/<RGBHex> or ledpix/run/<pattern-name>");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader LEDPixels");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check init
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    bool rslt = true;
    String reasonStr = "";
    
    // Get command
    String cmd = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);

    // Check command
    if (cmd.equalsIgnoreCase("clear"))
    {
        // Stop pattern
        _pattern = PATTERN_NONE;

        // Clear the pixels
        setAllPixels(CRGB::Black);
    }
    else if (cmd.equalsIgnoreCase("set"))
    {
        // Pixel idx
        uint32_t pixIdx = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2).toInt();

        // RGB hex
        String rgbHex = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);

        // Convert to rgb
        Raft::RGBValue rgb = Raft::getRGBFromHex(rgbHex);

        if (pixIdx < _ledPixels.size())
        {
            _ledPixels[pixIdx] = CRGB(rgb.r, rgb.g, rgb.b);
            FastLED.show();
            LOG_I(MODULE_PREFIX, "apiControl set pixel %d to %s R%02x G%02x B%02x", pixIdx, rgbHex.c_str(), rgb.r, rgb.g, rgb.b);
        }
        else
        {
            rslt = false;
            reasonStr = "\"reason\":\"invalidPixIdx\"";
        }

        // // Set first 50 pixels to red
        // for (uint32_t i = 0; i < _ledPixels.size(); i++)
        // {
        //     for (uint32_t j = 0; j < 50; j++)
        //     {
        //         _ledPixels[i].setPixelColor(j, 0xFF, 0x00, 0x00);
        //     }
        //     _ledPixels[i].show();
        // }
    } else if (cmd.equalsIgnoreCase("show"))
    {
        FastLED.show();
    } else if (cmd.equalsIgnoreCase("run"))
    {
        // Get pattern name
        String patternName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);

        // Check pattern name
        if (patternName.equalsIgnoreCase("locate"))
        {
            patternLocate_start();
        }
        else if (patternName.equalsIgnoreCase("snake"))
        {
            // Snake length
            uint32_t patternLen = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3).toInt();

            // Speed
            uint32_t speed = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 4).toInt();

            patternSnake_start(patternLen, speed);
        }
        else
        {
            rslt = false;
            reasonStr = "\"reason\":\"invalidPatternName\"";
        }
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt, reasonStr.c_str());
}

String ScaderLEDPixels::getStatusJSON()
{
    return "{" + _scaderCommon.getStatusJSON() + "}"; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear all pixels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::setAllPixels(const CRGB& colour)
{
    for (uint32_t i = 0; i < _ledPixels.size(); i++)
    {
        _ledPixels[i] = colour;
    }
    FastLED.show();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pattern Locate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::patternLocate_start()
{
    // Initialise pattern
    _pattern = PATTERN_LOCATE;

    // Clear first
    setAllPixels(CRGB::Black);

    // Store pattern values
    _patternLEDIdx = 0;
    _patternSeqIdx = 0;

    // Pattern timing
    _patternLastTime = millis();

}

void ScaderLEDPixels::patternLocate_service()
{
    // Check time
    if (Raft::isTimeout(millis(), _patternLastTime, PATTERN_LOCATE_STEP_MS))
    {
        // Update time
        _patternLastTime = millis();

        // Check LED index
        if (_patternLEDIdx >= _ledPixels.size())
        {
            // Restarting pattern
            LOG_I(MODULE_PREFIX, "patternLocate_service RESTART LEDIdx", _patternLEDIdx);

            // Repeat pattern
            patternLocate_start();
            return;
        }

        // Check sequence
        uint32_t initialFlashes = (_patternLEDIdx == 0) ? PATTERN_LOCATE_INITIAL_FLASHES : 1;
        if (_patternSeqIdx < initialFlashes * 2)
        {
            // Debug
            if (_patternSeqIdx == 0)
            {
                LOG_I(MODULE_PREFIX, "patternLocate_service SYNC ledIdx = %d", _patternLEDIdx);
            }
            
            // Check for sync flash on or off
            if (_patternSeqIdx % 2 == 0)
            {
                // Flash all LEDs on as sync indicator
                for (uint32_t i = 0; i < _ledPixels.size(); i++)
                {
                    _ledPixels[i] = CRGB(40,40,40);
                }
            }
            else
            {
                // Clear all LEDs
                setAllPixels(CRGB::Black);
            }
            _patternSeqIdx++;
        }
        else
        {
            // Clear all LEDs
            setAllPixels(CRGB::Black);

            // Set the next LED in sequence
            _ledPixels[_patternLEDIdx] = CRGB::White;
            _patternLEDIdx++;

            // Check if it's time for another sync
            if (_patternLEDIdx % PATTERN_LOCATE_LEDS_BETWEEN_SYNCS == 0)
            {
                // Reset sequence index
                _patternSeqIdx = 0;
            }
        }

        // Show LEDs
        FastLED.show();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pattern Snake
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::patternSnake_start(uint32_t snakeLen, uint32_t snakeSpeed)
{
    // Initialise pattern
    _pattern = PATTERN_SNAKE;

    // Clear first
    setAllPixels(CRGB::Black);

    // Store pattern values
    _patternLEDIdx = 0;
    _patternDirection = 0;
    _patternSnakeLen = snakeLen == 0 ? 40 : snakeLen;
    _patternSnakeSpeed = snakeSpeed;

    _patternLen = _ledPixels.size();

    // Pattern timing
    _patternLastTime = millis();

}

void ScaderLEDPixels::patternSnake_service()
{
    // Check time
    if (Raft::isTimeout(millis(), _patternLastTime, _patternSnakeSpeed))
    {
        // Update time
        _patternLastTime = millis();

        // Check LED index
        if (_patternLEDIdx >= _patternLen - _patternSnakeLen)
        {
            // Change direction
            _patternDirection = -1;

            // Debug
            LOG_I(MODULE_PREFIX, "patternSnake_service CHANGE DIRECTION LEDIdx", _patternLEDIdx);
        }
        else if (_patternLEDIdx == 0)
        {
            // Change direction
            _patternDirection = 1;

            // Debug
            LOG_I(MODULE_PREFIX, "patternSnake_service CHANGE DIRECTION LEDIdx", _patternLEDIdx);
        }

        // Clear all pixels
        for (uint32_t i = 0; i < _ledPixels.size(); i++)
        {
            _ledPixels[i] = CRGB::Black;
        }

        // Set a rainbow sequence of colours into the next batch of pixels
        for (uint32_t i = 0; i < _patternSnakeLen; i++)
        {
            // Set colour
            _ledPixels[_patternLEDIdx + i] = CHSV(i * 255 / _patternSnakeLen, 255, 255);
        }

        // Update LED index
        _patternLEDIdx += _patternDirection;

        // Show LEDs
        FastLED.show();
    }
}

