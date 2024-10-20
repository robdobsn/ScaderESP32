/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED Pattern Rainbow Snake
//
// Rob Dobson 2023-24
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"

class LEDPatternRainbowSnake : public LEDPatternBase
{
public:
    LEDPatternRainbowSnake(NamedValueProvider* pNamedValueProvider, LEDPixelIF& pixels) :
        LEDPatternBase(pNamedValueProvider, pixels)
    {
    }
    virtual ~LEDPatternRainbowSnake()
    {
    }

    // Create function for factory
    static LEDPatternBase* create(NamedValueProvider* pNamedValueProvider, LEDPixelIF& pixels)
    {
        return new LEDPatternRainbowSnake(pNamedValueProvider, pixels);
    }

    // Setup
    virtual void setup(const char* pParamsJson = nullptr) override final
    {
        if (pParamsJson)
        {
            // Update refresh rate if specified
            RaftJson paramsJson(pParamsJson, false);
            _refreshRateMs = paramsJson.getLong("rateMs", _refreshRateMs);

            // Get max brightness percent
            _maxBrightnessPC = paramsJson.getDouble("brightnessPC", 10);
        }
    }

    // Loop
    virtual void loop() override final
    {
        // Check update time
        if (!Raft::isTimeout(millis(), _lastLoopMs, _refreshRateMs))
            return;
        _lastLoopMs = millis();

        if (_curState)
        {
            uint32_t numPix = _pixels.getNumPixels();
            for (int pixIdx = _curIter; pixIdx < numPix; pixIdx += 3)
            {
                uint16_t hue = pixIdx * 360 / numPix + _curHue;
                _pixels.setHSV(pixIdx, hue, 100, _maxBrightnessPC);
            }
            // Show pixels
            _pixels.show();
        }
        else
        {
            _curIter = (_curIter + 2) % 3;
            if (_curIter == 0)
            {
                _curHue += 60;
            }
        }
        _curState = !_curState;
    }

private:
    // State
    uint32_t _lastLoopMs = 0;
    bool _curState = false;
    uint32_t _curIter = 0;
    uint32_t _curHue = 0;
    float _maxBrightnessPC = 10;

    // Debug
    static constexpr const char *MODULE_PREFIX = "LEDPatRS";
};
