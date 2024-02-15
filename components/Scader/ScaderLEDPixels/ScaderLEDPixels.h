/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderLEDPixels
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define USE_RAFT_PIXELS_LIBRARY
// #define USE_FASTLED_LIBRARY
#define RUN_PATTERNS_IN_SYSMOD

#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "RaftUtils.h"

#ifdef USE_RAFT_PIXELS_LIBRARY
#include "LEDPixels.h"
#endif
#ifdef USE_FASTLED_LIBRARY
#include "FastLED.h"
#endif

class APISourceInfo;

class ScaderLEDPixels : public RaftSysMod
{
  public:
    ScaderLEDPixels(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderLEDPixels(pModuleName, sysConfig);
    }
    
protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Tnitalised flag
    bool _isInitialised = false;

#ifdef USE_RAFT_PIXELS_LIBRARY
    // LED pixels
    LEDPixels _ledPixels;
    LEDPixels _ledPixels2;
#endif

#ifdef USE_FASTLED_LIBRARY
    // WS2812 strips
    std::vector<CRGB> _ledPixels;
#endif

#ifdef RUN_PATTERNS_IN_SYSMOD
    // Patterns
    enum LedStripPattern
    {
        PATTERN_NONE,
        PATTERN_LOCATE,
        PATTERN_SNAKE
    };
    LedStripPattern _pattern = PATTERN_NONE;

    // Pattern parameters
    uint32_t _patternLEDIdx = 0;
    uint32_t _patternSeqIdx = 0;
    uint32_t _patternSnakeLen = 0;
    uint32_t _patternSnakeSpeed = 0;
    int _patternDirection = 1;
    uint32_t _patternLastTime = 0;
    uint32_t _patternLen = 0;
#endif

    // Helper functions
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);
    void clearAllPixels();
    void setPixelRGB(uint32_t idx, uint8_t r, uint8_t g, uint8_t b);
    void setPixelHSV(uint32_t idx, uint16_t h, uint8_t s, uint8_t v);
    void show();
    uint32_t totalNumPixels();

#ifdef RUN_PATTERNS_IN_SYSMOD
    // Pattern locate
    static const uint32_t PATTERN_LOCATE_INITIAL_FLASHES = 3;
    static const uint32_t PATTERN_LOCATE_STEP_MS = 200;
    static const uint32_t PATTERN_LOCATE_LEDS_BETWEEN_SYNCS = 10;
    void patternLocate_start();
    void patternLocate_service();
    void patternSnake_start(uint32_t snakeLen, uint32_t snakeSpeed);
    void patternSnake_service();
#endif
};
