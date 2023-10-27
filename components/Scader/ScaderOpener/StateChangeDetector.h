/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// State change detector
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <RaftArduino.h>

typedef std::function<void(bool isActive, uint32_t msSinceLastChange)> StateChangeDetectorCallback;

class StateChangeDetector
{
public:
    StateChangeDetector(StateChangeDetectorCallback cb) :
        _callback(cb)
    {
    }

    void setup(bool activeLevel)
    {
        _activeLevel = activeLevel;
        _lastLevel = !activeLevel;
    }

    bool service(bool currentLevel)
    {
        // Check for change
        if (currentLevel != _lastLevel)
        {
            // Get time
            uint32_t timeNowMs = millis();

            // Get time since last change
            uint32_t timeSinceLastChangeMs = timeNowMs - _lastSampleTimeMs;
            _lastSampleTimeMs = timeNowMs;

            // Update state
            _lastLevel = currentLevel;

            // Call callback
            if (_callback)
                _callback(currentLevel == _activeLevel, timeSinceLastChangeMs);

            // Return true to indicate change
            return true;
        }

        // Return false to indicate no change
        return false;
    }

    bool getState()
    {
        return _lastLevel == _activeLevel;
    }

private:
    bool _lastLevel = false;
    bool _activeLevel = false;
    uint32_t _lastSampleTimeMs = 0;
    StateChangeDetectorCallback _callback = nullptr;
};
