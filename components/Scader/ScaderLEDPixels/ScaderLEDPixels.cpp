/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderLEDPixels
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ctime>
#include "driver/gpio.h"
#include "Logger.h"
#include "RaftArduino.h"
#include "ScaderLEDPixels.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"
#include "LEDPatternRainbowSnake.h"
#include "LEDPatternAutoID.h"

#define DEBUG_LED_PIXEL_SETUP

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderLEDPixels::ScaderLEDPixels(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::setup()
{    
    // Common
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Add patterns before setup so that initial pattern can be set during setup
    _ledPixels.addPattern("RainbowSnake", &LEDPatternRainbowSnake::create);
    _ledPixels.addPattern("autoid", &LEDPatternAutoID::create);

    // Setup LEDs using SimpleRMTLeds driver with LEDPixels wrapper
    // This will use the configuration from modConfig() and create SimpleRMTLeds strips
    bool rslt = _ledPixels.setup(modConfig());

    // Log
#ifdef DEBUG_LED_PIXEL_SETUP
    LOG_I(MODULE_PREFIX, "setup %s numPixels %d using SimpleRMTLeds driver", 
          rslt ? "OK" : "FAILED", _ledPixels.getNumPixels());
#endif

    // HW Now initialised
    _isInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loop (called frequently)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// int blinkyLed = 0;
// int addn = 1;
// static const int TAIL_LEN = 20;
// #define NUM_LEDS 100

void ScaderLEDPixels::loop()
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

    _ledPixels.loop();

#ifdef RUN_PATTERNS_IN_SYSMOD
    // Handle patterns
    switch(_pattern)
    {
        case PATTERN_NONE:
            break;
        case PATTERN_LOCATE:
            patternLocate_loop();
            break;
        case PATTERN_SNAKE:
            patternSnake_loop();
            break;
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("ledpix", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderLEDPixels::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "control LED pixels, ledpix/S/clear, ledpix/S/set/<N>/<RGBHex>, ledpix/S/setledsidx/<clear>/<data> or ledpix/S/run/<pattern-name> (S is the segment)");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader LEDPixels");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderLEDPixels::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Extract parameters
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    RaftJson nameValuesJson = RaftJson::getJSONFromNVPairs(nameValues, true);

    // Get element name or type
    String elemNameOrIdx = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    int32_t segmentIdx = _ledPixels.getSegmentIdx(elemNameOrIdx);
    if (segmentIdx < 0)
    {
        // Check for elemNameOrIdx being a segment index number - i.e. digits only
        bool isDigit = true;
        for (uint32_t i = 0; i < elemNameOrIdx.length(); i++)
        {
            if (!isdigit(elemNameOrIdx[i]))
            {
                isDigit = false;
                break;
            }
        }
        if (isDigit)
            segmentIdx = elemNameOrIdx.toInt();
    }
    if ((segmentIdx < 0) || (segmentIdx >= _ledPixels.getNumSegments()))
        return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "invalidElement");
    String cmd = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    cmd.trim();
    String data = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);

    // Debug
    // LOG_I(MODULE_PREFIX, "apiLEDs req %s numParams %d elemNameOrIdx %s segmentIdx %d cmd %s data %s args %s",
    //       reqStr.c_str(), params.size(), elemNameOrIdx.c_str(), segmentIdx, cmd.c_str(), data.c_str(), nameValuesJson.c_str());

    // Handle commands
    bool rslt = false;
    if (cmd.equalsIgnoreCase("setall") || cmd.equalsIgnoreCase("color") || cmd.equalsIgnoreCase("colour"))
    {
        // Stop any pattern
        _ledPixels.stopPattern(segmentIdx, false);

        // See if a start LED is specified
        int startLED = 0;
        String startLEDStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 4);
        if (startLEDStr.length() > 0)
            startLED = strtol(startLEDStr.c_str(), NULL, 10);

        // See if an end LED is specified
        int endLED = _ledPixels.getNumPixels(segmentIdx);
        String endLEDStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 5);
        if (endLEDStr.length() > 0)
            endLED = strtol(endLEDStr.c_str(), NULL, 10);

        // Set all LEDs to a single colour
        if (data.startsWith("#"))
            data = data.substring(1);
        auto rgb = Raft::getRGBFromHex(data);
        for (uint32_t i = startLED; i < endLED; i++)
        {
            _ledPixels.setRGB(segmentIdx, i, rgb.r, rgb.g, rgb.b, true);
        }

        // Show
        _ledPixels.show();
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("setleds"))
    {
        // Stop any pattern
        _ledPixels.stopPattern(segmentIdx, false);

        // Set LEDs to a series of specified colours
        for (uint32_t i = 0; i < _ledPixels.getNumPixels(segmentIdx); i++)
        {
            String subRGB = data.substring(i * 6, i * 6 + 6);
            if (subRGB.length() != 6)
                break;
            auto rgb = Raft::getRGBFromHex(subRGB);
            _ledPixels.setRGB(segmentIdx, i, rgb.r, rgb.g, rgb.b, true);
        }

        // Show
        _ledPixels.show();
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("setledsidx"))
    {
        // Stop any pattern
        _ledPixels.stopPattern(segmentIdx, false);

        // Check minimum length (at least clear flag)
        if (data.length() < 1)
        {
            LOG_W(MODULE_PREFIX, "setledsidx: data too short");
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "dataTooShort");
        }

        // Parse clear flag (first character: 0 or 1)
        bool clearFirst = (data.charAt(0) == '1');
        if (clearFirst)
        {
            _ledPixels.clear(false);
        }

        // Determine mode based on data length
        // RGB mode: 10 chars per LED (4 for index + 6 for RGB)
        // RGBW mode: 12 chars per LED (4 for index + 8 for RGBW)
        uint32_t dataLen = data.length() - 1; // Exclude clear flag
        uint32_t charsPerLED = 0;
        bool isRGBW = false;
        
        if (dataLen % 10 == 0)
        {
            charsPerLED = 10;
            isRGBW = false;
        }
        else if (dataLen % 12 == 0)
        {
            charsPerLED = 12;
            isRGBW = true;
        }
        else
        {
            LOG_W(MODULE_PREFIX, "setledsidx: invalid data length %d (should be multiple of 10 or 12)", dataLen);
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "invalidDataLength");
        }

        // Parse LED data
        uint32_t dataPos = 1; // Start after clear flag
        uint32_t numLEDsSet = 0;
        uint32_t numLEDsSkipped = 0;
        
        while (dataPos + charsPerLED <= data.length())
        {
            // Extract index (4 hex chars)
            String indexStr = data.substring(dataPos, dataPos + 4);
            uint32_t ledIdx = strtol(indexStr.c_str(), NULL, 16);
            
            // Validate index bounds
            if (ledIdx >= _ledPixels.getNumPixels(segmentIdx))
            {
                LOG_W(MODULE_PREFIX, "setledsidx: LED index %d out of bounds (max %d)", 
                      ledIdx, _ledPixels.getNumPixels(segmentIdx) - 1);
                numLEDsSkipped++;
                dataPos += charsPerLED;
                continue;
            }
            
            // Extract color data
            String colorStr = data.substring(dataPos + 4, dataPos + 4 + (isRGBW ? 8 : 6));
            uint8_t r = strtol(colorStr.substring(0, 2).c_str(), NULL, 16);
            uint8_t g = strtol(colorStr.substring(2, 4).c_str(), NULL, 16);
            uint8_t b = strtol(colorStr.substring(4, 6).c_str(), NULL, 16);
            // uint8_t w = isRGBW ? strtol(colorStr.substring(6, 8).c_str(), NULL, 16) : 0;
            
            // Set pixel (currently RGB only, W channel ignored for future RGBW support)
            _ledPixels.setRGB(segmentIdx, ledIdx, r, g, b, false);
            numLEDsSet++;
            
            dataPos += charsPerLED;
        }
        
        // Show once at end
        _ledPixels.show();
        
        // Log result
        LOG_I(MODULE_PREFIX, "setledsidx: mode=%s clear=%d set=%d skipped=%d",
              isRGBW ? "RGBW" : "RGB", clearFirst, numLEDsSet, numLEDsSkipped);
        
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("setled") || cmd.equalsIgnoreCase("set"))
    {
        // Stop pattern
        _ledPixels.stopPattern(segmentIdx, false);

        // Get LED and RGB for a single LED
        int ledID = strtol(data.c_str(), NULL, 10);
        String rgbStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 4);
        if (rgbStr.startsWith("#"))
            rgbStr = rgbStr.substring(1);
        auto rgb = Raft::getRGBFromHex(rgbStr);

        // Set pixel
        // LOG_I(MODULE_PREFIX, "setled %d %s r %d g %d b %d", ledID, rgbStr.c_str(), rgb.r, rgb.g, rgb.b);
        _ledPixels.setRGB(segmentIdx, ledID, rgb.r, rgb.g, rgb.b, true);
        _ledPixels.show();
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("off") || cmd.equalsIgnoreCase("clear"))
    {
        // Turn off all LEDs
        _ledPixels.stopPattern(segmentIdx, false);
        _ledPixels.clear(true);
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("pattern"))
    {
        // Set a named pattern
        _ledPixels.clear(false);
        _ledPixels.show();
        _ledPixels.setPattern(segmentIdx, data, nameValuesJson.c_str());
        rslt = true;
    }
    else if (cmd.equalsIgnoreCase("listpatterns"))
    {
        // Get list of patterns
        std::vector<String> patternNames;
        _ledPixels.getPatternNames(patternNames);
        String jsonResp;
        for (auto& name : patternNames)
        {
            if (jsonResp.length() > 0)
                jsonResp += ",";
            jsonResp += "\"" + name + "\"";
        }
        jsonResp = "\"patterns\":[" + jsonResp + "]";
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true, jsonResp.c_str());
    }

    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

String ScaderLEDPixels::getStatusJSON() const
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

#ifdef USE_FASTLED_LIBRARY
void ScaderLEDPixels::clearAllPixels()
{
    for (uint32_t i = 0; i < _ledPixels.size(); i++)
    {
        _ledPixels[i] = CRGB::Black;
    }
    FastLED.show();
}

void setPixelRGB(uint32_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    _ledPixels[idx] = CRGB(r, g, b);
}

void setPixelHSV(uint32_t idx, uint16_t h, uint8_t s, uint8_t v)
{
    _ledPixels[idx] = CHSV(h, s, v);
}

void show()
{
    FastLED.show();
}

uint32_t totalNumPixels()
{
    return _ledPixels.size();
}

#endif

#ifdef RUN_PATTERNS_IN_SYSMOD

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pattern Locate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixels::patternLocate_start()
{
    // Initialise pattern
    _pattern = PATTERN_LOCATE;

    // Clear first
    clearAllPixels();

    // Store pattern values
    _patternLEDIdx = 0;
    _patternSeqIdx = 0;

    // Pattern timing
    _patternLastTime = millis();
}

void ScaderLEDPixels::patternLocate_loop()
{
    // Check time
    if (Raft::isTimeout(millis(), _patternLastTime, PATTERN_LOCATE_STEP_MS))
    {
        // Update time
        _patternLastTime = millis();

        // Check LED index
        if (_patternLEDIdx >= totalNumPixels())
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
                for (uint32_t i = 0; i < totalNumPixels(); i++)
                {
                    setPixelRGB(i, 40, 40, 40);
                }
            }
            else
            {
                // Clear all LEDs
                clearAllPixels();
            }
            _patternSeqIdx++;
        }
        else
        {
            // Clear all LEDs
            clearAllPixels();

            // Set the next LED in sequence
            setPixelRGB(_patternLEDIdx, 255, 255, 255);
            _patternLEDIdx++;

            // Check if it's time for another sync
            if (_patternLEDIdx % PATTERN_LOCATE_LEDS_BETWEEN_SYNCS == 0)
            {
                // Reset sequence index
                _patternSeqIdx = 0;
            }
        }

        // Show LEDs
        show();
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
    clearAllPixels();

    // Store pattern values
    _patternLEDIdx = 0;
    _patternDirection = 0;
    _patternSnakeLen = snakeLen == 0 ? totalNumPixels() / 5 : snakeLen;
    _patternSnakeSpeed = snakeSpeed;

    _patternLen = totalNumPixels();

    // Pattern timing
    _patternLastTime = millis();
}

void ScaderLEDPixels::patternSnake_loop()
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
        clearAllPixels();

        // Set a rainbow sequence of colours into the next batch of pixels
        for (uint32_t i = 0; i < _patternSnakeLen; i++)
        {
            // Set colour
            setPixelHSV(_patternLEDIdx + i, i * 255 / _patternSnakeLen, 255, 255);
        }

        // Update LED index
        _patternLEDIdx += _patternDirection;

        // Show LEDs
        show();
    }
}

#endif // RUN_PATTERNS_IN_SYSMOD