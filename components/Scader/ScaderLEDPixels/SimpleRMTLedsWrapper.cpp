// SimpleRMTLedsWrapper.cpp
// Wrapper to make SimpleRMTLeds compatible with RaftCore LEDPixels ESP32RMTLedStrip interface
// Rob Dobson 2024

#include "SimpleRMTLedsWrapper.h"
#include "Logger.h"

static const char* MODULE_PREFIX = "SimpleRMTWrap";

SimpleRMTLedsWrapper::SimpleRMTLedsWrapper()
{
}

SimpleRMTLedsWrapper::~SimpleRMTLedsWrapper()
{
}

bool SimpleRMTLedsWrapper::setup(const LEDStripConfig& ledStripConfig, uint32_t pixelIndexStartOffset)
{
    _pixelIdxStartOffset = pixelIndexStartOffset;
    _numPixels = ledStripConfig.numPixels;
    
    // Initialize SimpleRMTLeds driver
    bool rslt = _simpleRMT.init(ledStripConfig.ledDataPin, ledStripConfig.numPixels);
    
    if (rslt)
    {
        LOG_I(MODULE_PREFIX, "setup OK pin=%d pixels=%d offset=%d", 
              ledStripConfig.ledDataPin, ledStripConfig.numPixels, pixelIndexStartOffset);
        _isSetup = true;
    }
    else
    {
        LOG_E(MODULE_PREFIX, "setup FAILED pin=%d pixels=%d", 
              ledStripConfig.ledDataPin, ledStripConfig.numPixels);
    }
    
    return rslt;
}

void SimpleRMTLedsWrapper::loop()
{
    // SimpleRMTLeds doesn't need loop processing
}

void SimpleRMTLedsWrapper::showPixels(std::vector<LEDPixel>& pixels)
{
    if (!_isSetup)
        return;

    // Copy pixels from the global pixel buffer to our strip
    // LEDPixel uses c1,c2,c3 which are in GRB order (matching WS2812)
    // We need to convert back to RGB for SimpleRMTLeds (which handles GRB internally)
    for (uint32_t i = 0; i < _numPixels; i++)
    {
        uint32_t pixelIdx = _pixelIdxStartOffset + i;
        if (pixelIdx < pixels.size())
        {
            const LEDPixel& pix = pixels[pixelIdx];
            // LEDPixel is in GRB order: c1=G, c2=R, c3=B
            // SimpleRMTLeds expects RGB and converts to GRB internally
            _simpleRMT.setPixel(i, pix.c2, pix.c1, pix.c3);
        }
    }

    // Show the pixels
    _simpleRMT.show();
}

void SimpleRMTLedsWrapper::waitUntilShowComplete()
{
    // SimpleRMTLeds show() is blocking, so nothing to wait for
}
