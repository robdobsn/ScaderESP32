/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderLEDPixels
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// #define USE_FASTLED_LIBRARY

#include <SysModBase.h>
#include <ConfigBase.h>
#include <ScaderCommon.h>
#include <RaftUtils.h>
#ifdef USE_FASTLED_LIBRARY
#include <FastLED.h>
#endif

class APISourceInfo;

class ScaderLEDPixels : public SysModBase
{
  public:
    ScaderLEDPixels(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

protected:

    // Setup
    virtual void setup() override final;

    // Service (called frequently)
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Tnitalised flag
    bool _isInitialised = false;

#ifdef USE_FASTLED_LIBRARY
    // WS2812 strips
    std::vector<CRGB> _ledPixels;
#endif

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

    // Helper functions
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);
#ifdef USE_FASTLED_LIBRARY
    void setAllPixels(const CRGB& colour);
#endif

    // Pattern locate
    static const uint32_t PATTERN_LOCATE_INITIAL_FLASHES = 3;
    static const uint32_t PATTERN_LOCATE_STEP_MS = 200;
    static const uint32_t PATTERN_LOCATE_LEDS_BETWEEN_SYNCS = 10;
    void patternLocate_start();
    void patternLocate_service();
    void patternSnake_start(uint32_t snakeLen, uint32_t snakeSpeed);
    void patternSnake_service();
};
