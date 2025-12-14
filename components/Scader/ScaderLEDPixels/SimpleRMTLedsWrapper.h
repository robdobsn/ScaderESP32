// SimpleRMTLedsWrapper.h
// Wrapper to make SimpleRMTLeds compatible with RaftCore LEDPixels ESP32RMTLedStrip interface
// Rob Dobson 2024

#pragma once

#include "SimpleRMTLeds.h"
#include "LEDPixel.h"
#include "LEDStripConfig.h"
#include <vector>

class SimpleRMTLedsWrapper
{
public:
    SimpleRMTLedsWrapper();
    virtual ~SimpleRMTLedsWrapper();

    // Setup - matches ESP32RMTLedStrip interface
    bool setup(const LEDStripConfig& ledStripConfig, uint32_t pixelIndexStartOffset);

    // Loop
    void loop();

    // Show pixels
    void showPixels(std::vector<LEDPixel>& pixels);

    // Wait for show to complete
    void waitUntilShowComplete();

private:
    SimpleRMTLeds _simpleRMT;
    uint32_t _pixelIdxStartOffset = 0;
    uint32_t _numPixels = 0;
    bool _isSetup = false;
};
