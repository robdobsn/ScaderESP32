/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderLEDPixels
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <ConfigBase.h>
#include <RaftUtils.h>
#include <FastLED.h>

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
    // Enabled and initalised flags
    bool _isEnabled;
    bool _isInitialised;

    // WS2812 strips
    std::vector<CRGB> _ledPixels;

    // Patterns
    enum LedStripPattern
    {
        PATTERN_NONE,
        PATTERN_LOCATE
    };
    LedStripPattern _pattern;

    // Pattern parameters
    uint32_t _patternLEDIdx;
    uint32_t _patternSeqIdx;
    uint32_t _patternLastTime;

    // Helper functions
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);
    void setAllPixels(const CRGB& colour);

    // Pattern locate
    static const uint32_t PATTERN_LOCATE_INITIAL_FLASHES = 3;
    static const uint32_t PATTERN_LOCATE_STEP_MS = 200;
    static const uint32_t PATTERN_LOCATE_LEDS_BETWEEN_SYNCS = 10;
    void patternLocate_start();
    void patternLocate_service();
};
