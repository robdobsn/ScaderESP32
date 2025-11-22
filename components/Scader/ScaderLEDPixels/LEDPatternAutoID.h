/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED Pattern AutoID
// Pattern for automatic identification of LED positions using sync flashes and sequential LED lighting
//
// Sequence:
// (a) N initial sync flashes (all LEDs on then off) to mark start of sequence
// (b) Light M LEDs one by one for a fixed duration
// (c) 1 sync flash (all LEDs on then off)
// (d) Repeat (b) and (c) until all LEDs have been flashed
// (e) Return to start
//
// Rob Dobson 2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"

// #define DEBUG_LEDPATTERN_AUTOID

class LEDPatternAutoID : public LEDPatternBase
{
public:
    LEDPatternAutoID(NamedValueProvider* pNamedValueProvider, LEDPixelIF& pixels) :
        LEDPatternBase(pNamedValueProvider, pixels)
    {
    }
    virtual ~LEDPatternAutoID()
    {
    }

    // Create function for factory
    static LEDPatternBase* create(NamedValueProvider* pNamedValueProvider, LEDPixelIF& pixels)
    {
        return new LEDPatternAutoID(pNamedValueProvider, pixels);
    }

    // Setup
    virtual void setup(const char* pParamsJson = nullptr) override final
    {
        if (pParamsJson)
        {
            RaftJson paramsJson(pParamsJson, false);

            // Number of initial sync flashes at start of entire sequence
            _initialSyncFlashes = paramsJson.getLong("initFlashes", DEFAULT_INITIAL_SYNC_FLASHES);

            // Time in ms for each sync flash (on phase and off phase)
            _syncFlashTimeMs = paramsJson.getLong("syncFlashMs", DEFAULT_SYNC_FLASH_TIME_MS);

            // Time in ms each individual LED is lit
            _ledOnTimeMs = paramsJson.getLong("ledOnMs", DEFAULT_LED_ON_TIME_MS);

            // Number of LEDs to light between sync flashes
            _ledsBetweenSyncs = paramsJson.getLong("ledsBetweenSyncs", DEFAULT_LEDS_BETWEEN_SYNCS);

            // Brightness for sync flashes (0-255)
            _syncBrightness = paramsJson.getLong("syncBrightness", DEFAULT_SYNC_BRIGHTNESS);

            // Brightness for individual LEDs (0-255)
            _ledBrightness = paramsJson.getLong("ledBrightness", DEFAULT_LED_BRIGHTNESS);

            // Starting LED index
            _startLedIdx = paramsJson.getLong("startIdx", 0);

            // Ending LED index (0 = use all LEDs)
            _endLedIdx = paramsJson.getLong("endIdx", 0);
        }

        // Initialize state
        _curLedIdx = _startLedIdx;
        _syncPhase = 0;
        _state = STATE_INITIAL_SYNC;
        _lastUpdateMs = millis();

        // Determine actual end index
        _actualEndIdx = (_endLedIdx == 0 || _endLedIdx > _pixels.getNumPixels())
                        ? _pixels.getNumPixels()
                        : _endLedIdx;

#ifdef DEBUG_LEDPATTERN_AUTOID
        LOG_I(MODULE_PREFIX, "Setup initFlashes %d syncFlashMs %d ledOnMs %d ledsBetweenSyncs %d startIdx %d endIdx %d numPix %d",
              _initialSyncFlashes, _syncFlashTimeMs, _ledOnTimeMs, _ledsBetweenSyncs,
              _startLedIdx, _actualEndIdx, _pixels.getNumPixels());
#endif
    }

    // Loop
    virtual void loop() override final
    {
        uint32_t now = millis();

        switch (_state)
        {
            case STATE_INITIAL_SYNC:
                handleInitialSync(now);
                break;

            case STATE_LED_ON:
                handleLedOn(now);
                break;

            case STATE_INTER_SYNC:
                handleInterSync(now);
                break;
        }
    }

private:
    // State machine states
    enum State
    {
        STATE_INITIAL_SYNC,    // Initial sync flashes at start of sequence
        STATE_LED_ON,          // Individual LED lit
        STATE_INTER_SYNC       // Sync flash between LED groups
    };

    // Default configuration values
    static const uint32_t DEFAULT_INITIAL_SYNC_FLASHES = 3;
    static const uint32_t DEFAULT_SYNC_FLASH_TIME_MS = 200;
    static const uint32_t DEFAULT_LED_ON_TIME_MS = 200;
    static const uint32_t DEFAULT_LEDS_BETWEEN_SYNCS = 10;
    static const uint32_t DEFAULT_SYNC_BRIGHTNESS = 40;
    static const uint32_t DEFAULT_LED_BRIGHTNESS = 255;

    // Configuration
    uint32_t _initialSyncFlashes = DEFAULT_INITIAL_SYNC_FLASHES;
    uint32_t _syncFlashTimeMs = DEFAULT_SYNC_FLASH_TIME_MS;
    uint32_t _ledOnTimeMs = DEFAULT_LED_ON_TIME_MS;
    uint32_t _ledsBetweenSyncs = DEFAULT_LEDS_BETWEEN_SYNCS;
    uint32_t _syncBrightness = DEFAULT_SYNC_BRIGHTNESS;
    uint32_t _ledBrightness = DEFAULT_LED_BRIGHTNESS;
    uint32_t _startLedIdx = 0;
    uint32_t _endLedIdx = 0;
    uint32_t _actualEndIdx = 0;

    // State
    State _state = STATE_INITIAL_SYNC;
    uint32_t _curLedIdx = 0;
    uint32_t _syncPhase = 0;        // Counts on/off phases during sync
    uint32_t _ledsLitSinceSync = 0; // Count LEDs lit since last sync
    uint32_t _lastUpdateMs = 0;

    // Debug
    static constexpr const char* MODULE_PREFIX = "LEDPatAID";

    void handleInitialSync(uint32_t now)
    {
        uint32_t totalSyncPhases = _initialSyncFlashes * 2; // Each flash has on and off phase

        if (!Raft::isTimeout(now, _lastUpdateMs, _syncFlashTimeMs))
            return;

        _lastUpdateMs = now;

        if (_syncPhase < totalSyncPhases)
        {
            if (_syncPhase % 2 == 0)
            {
                // Flash all LEDs on
                setAllLeds(_syncBrightness, _syncBrightness, _syncBrightness);
#ifdef DEBUG_LEDPATTERN_AUTOID
                if (_syncPhase == 0)
                    LOG_I(MODULE_PREFIX, "Initial sync start");
#endif
            }
            else
            {
                // All LEDs off
                _pixels.clear();
            }
            _pixels.show();
            _syncPhase++;
        }
        else
        {
            // Done with initial sync, move to first LED
            _syncPhase = 0;
            _ledsLitSinceSync = 0;
            _state = STATE_LED_ON;
            showCurrentLed();
        }
    }

    void handleLedOn(uint32_t now)
    {
        if (!Raft::isTimeout(now, _lastUpdateMs, _ledOnTimeMs))
            return;

        _lastUpdateMs = now;

        // Move to next LED
        _curLedIdx++;
        _ledsLitSinceSync++;

        // Check if we've completed all LEDs
        if (_curLedIdx >= _actualEndIdx)
        {
#ifdef DEBUG_LEDPATTERN_AUTOID
            LOG_I(MODULE_PREFIX, "Sequence complete, restarting");
#endif
            // Restart the entire sequence
            _curLedIdx = _startLedIdx;
            _syncPhase = 0;
            _ledsLitSinceSync = 0;
            _state = STATE_INITIAL_SYNC;
            _pixels.clear();
            _pixels.show();
            return;
        }

        // Check if it's time for an inter-sync flash
        if (_ledsLitSinceSync >= _ledsBetweenSyncs)
        {
            _syncPhase = 0;
            _ledsLitSinceSync = 0;
            _state = STATE_INTER_SYNC;
            // Start first phase of inter-sync immediately
            setAllLeds(_syncBrightness, _syncBrightness, _syncBrightness);
            _pixels.show();
            _syncPhase++;
#ifdef DEBUG_LEDPATTERN_AUTOID
            LOG_I(MODULE_PREFIX, "Inter-sync at LED %d", _curLedIdx);
#endif
        }
        else
        {
            // Show next LED
            showCurrentLed();
        }
    }

    void handleInterSync(uint32_t now)
    {
        if (!Raft::isTimeout(now, _lastUpdateMs, _syncFlashTimeMs))
            return;

        _lastUpdateMs = now;

        // Inter-sync is just one flash (on then off = 2 phases)
        if (_syncPhase < 2)
        {
            if (_syncPhase % 2 == 0)
            {
                // Flash all LEDs on (already done when entering this state)
                setAllLeds(_syncBrightness, _syncBrightness, _syncBrightness);
            }
            else
            {
                // All LEDs off
                _pixels.clear();
            }
            _pixels.show();
            _syncPhase++;
        }
        else
        {
            // Done with inter-sync, continue with LEDs
            _state = STATE_LED_ON;
            showCurrentLed();
        }
    }

    void showCurrentLed()
    {
        _pixels.clear();
        if (_curLedIdx < _actualEndIdx)
        {
            _pixels.setRGB(_curLedIdx, _ledBrightness, _ledBrightness, _ledBrightness, false);
#ifdef DEBUG_LEDPATTERN_AUTOID
            LOG_I(MODULE_PREFIX, "LED %d", _curLedIdx);
#endif
        }
        _pixels.show();
    }

    void setAllLeds(uint32_t r, uint32_t g, uint32_t b)
    {
        for (uint32_t i = _startLedIdx; i < _actualEndIdx; i++)
        {
            _pixels.setRGB(i, r, g, b, false);
        }
    }
};
