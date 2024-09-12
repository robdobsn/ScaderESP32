////////////////////////////////////////////////////////////////////////////////
//
// SPIDimmer.h
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftUtils.h"
#include "SimpleMovingAverage.h"
#include <vector>
#include "esp_timer.h"

class SPIDimmer
{
public:
    SPIDimmer();
    virtual ~SPIDimmer();

    // Setup
    bool setup(int spiMOSI, int spiSCLK, std::vector<int>& spiCSPins, int mainsSyncPin);

    // Loop (called frequently)
    void loop();

    // Set channel value in percent
    void setChannelValue(uint32_t channelIdx, uint32_t valuePct);

    // Set timing
    void setTiming(uint32_t zeroCrossOffsetFromSyncUs, uint32_t valOffsetUs);

    // Get zero crossing period in us
    uint32_t getZeroCrossPeriodUs() { return _zeroCrossPeriodUsAvg.getAverage(); }

private:
    // Constants
    static constexpr int NUM_CHANNELS_PER_CHIP = 8;
    static constexpr int NUM_BITS_PER_CHANNEL = 2;
    static constexpr uint64_t CHANNEL_ON_BIT_SEQ = 0b10;
    static constexpr uint64_t CHANNEL_OFF_BIT_SEQ = 0b11;
    static constexpr uint64_t CHANNEL_MASK_BIT_SEQ = 0b11;

    // Flag indicating that sequences are valid
    bool _sequencesValid = false;
    bool _initialSetReqd = false;

    // SPI pins
    int _spiMOSI = -1;
    int _spiSCLK = -1;
    std::vector<int> _spiCSPins;

    // Mains sync
    bool _useMainsSync = false;
    int _mainsSyncPin = -1;

    // Dimmer bin timing values
    static constexpr uint32_t NUM_DIMMER_BINS = 10;
    float _dimmerBinTimingPct[NUM_DIMMER_BINS] = { 50.0, 47.5, 45.0, 40.0, 35.0, 32.5, 30.0, 25.0, 20.0, 0.1 };
    static constexpr uint32_t DIMMER_LEVEL_BIN_SIZE = 100 / NUM_DIMMER_BINS;

    // Timing of zero crossings
    static constexpr uint32_t ZERO_CROSS_PERIOD_US_DEFAULT = 10000;
    static constexpr uint32_t ZERO_CROSS_PERIOD_MIN_US = (ZERO_CROSS_PERIOD_US_DEFAULT * 0.9);
    static constexpr uint32_t ZERO_CROSS_PERIOD_MAX_US = (ZERO_CROSS_PERIOD_US_DEFAULT * 1.1);
    uint64_t _lastMainsSyncUs = 0;
    SimpleMovingAverage<50> _zeroCrossPeriodUsAvg;
    bool _mainsCyclePeriodSet = false;
    bool _mainsCyclePeriodValid = false;
    uint32_t _zeroCrossOffsetFromSyncUs = 3000;

    // Timer handles
    esp_timer_handle_t _zeroCrossTimerHandle = nullptr;
    esp_timer_handle_t _dimmingTimerHandle = nullptr;

    // Channel values in percent
    std::vector<uint8_t> _channelValuesPct;
    std::vector<bool> _anyChannelOnChipDimmed;

    // Steady-state data (non-dimmed channels are set on or off)
    // Dimmed channels are set to the off state
    uint64_t _steadyStateData = UINT64_MAX;

    // Timer sequence entry
    struct TimerSeqEntry
    {
        uint32_t us = 0;
        uint64_t data = 0;
    };

    // Timer sequence
    std::vector<TimerSeqEntry> _timerSeq;
    uint32_t _timerSeqTotal = 0;

    // Timer sequence index
    int _timerSeqIdx = 0;

    // Debug last loop timer
    uint32_t _debugLastLoopMs = 0;

    // Zero cross ISR
    static void mainsSyncISRStatic(void* pArg);
    void mainsSyncISR();

    // Zero cross timer ISR
    static void zeroCrossTimerCallbackStatic(void* pArg);
    void zeroCrossTimerCallback();

    // Dimming timer ISR
    static void dimmingTimerCallbackStatic(void* pArg);
    void dimmingTimerCallback();

    // Bit-bang SPI 16-bit transmit
    void bitBangSPI16Tx(uint16_t chipIdx, uint64_t data);

    // Send SPI data
    void sendSPIData(uint64_t spiData, bool forceSend);
    
    // Set values (if no mains sync) or recalculate timer sequence (if mains sync)
    void setValuesOrRecalculateTimerSequence();
    void recalculateTimerSequence();

    // Get dimmer us for specific bin
    uint32_t getDimmerUsForBin(uint32_t binNum)
    {
        if (binNum >= NUM_DIMMER_BINS)
            binNum = NUM_DIMMER_BINS - 1;
        return (uint32_t) (_dimmerBinTimingPct[binNum] * _zeroCrossPeriodUsAvg.getAverage() / 100.0);
    }

    // Get steady-state data for SPI command
    uint64_t getSteadyStateSPIData();

    // Debug
    static constexpr const char* MODULE_PREFIX = "SPIDimmer";
};
