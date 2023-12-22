/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED Pattern Rainbow Snake
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LEDPatternBase.h"

class LEDPatternRainbowSnake : public LEDPatternBase
{
public:
    LEDPatternRainbowSnake(NamedValueProvider* pNamedValueProvider, LEDPixels& pixels) :
        LEDPatternBase(pNamedValueProvider, pixels)
    {
    }
    virtual ~LEDPatternRainbowSnake()
    {
    }

    // Build function for factory
    static LEDPatternBase* build(NamedValueProvider* pNamedValueProvider, LEDPixels& pixels)
    {
        return new LEDPatternRainbowSnake(pNamedValueProvider, pixels);
    }

    // Setup
    virtual void setup(const char* pParamsJson = nullptr) override final
    {
    }

    // Service
    virtual void service() override final
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
                _pixels.setHSV(pixIdx, hue, 100, 10);
            }
            // Show pixels
            _pixels.show();
        }
        else
        {
            _pixels.clear();
            _pixels.show();
            _curIter = (_curIter + 1) % 3;
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
};
