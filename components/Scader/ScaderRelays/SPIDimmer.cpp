////////////////////////////////////////////////////////////////////////////////
//
// SPIDimmer.cpp
//
////////////////////////////////////////////////////////////////////////////////

#include "SPIDimmer.h"
#include "driver/gpio.h"

// #define DEBUG_MAINS_FREQ

// TODO - investigate ESP_TIMER_ISR flag
// #define USE_ISR_FOR_TIMER_SEQUENCE

// Use an extra timer to start the dimming sequence at a specific phase offset from zero cross
// #define USE_ZERO_CROSS_OFFSET_TIMER

// Adjust timing by monitoring difference from zero cross period start
// #define ADJUST_TIMING_BASED_ON_ZERO_CROSS_PERIOD

// #define TEST_ISR_USING_GPIO 5
// #define TEST_DIMMING_TIMER_USING_GPIO 5
// #define DEBUG_TIMING_SEQUENCE

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor
SPIDimmer::SPIDimmer()
{
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destructor
SPIDimmer::~SPIDimmer()
{
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Setup
/// @param spiMOSI SPI MOSI pin
/// @param spiSCLK SPI SCLK pin
/// @param spiCSPins SPI CS pins
/// @param mainsSyncPin Mains sync pin
bool SPIDimmer::setup(int spiMOSI, int spiSCLK, std::vector<int>& spiCSPins, int mainsSyncPin)
{
    // Check number of CS pins is less than or equal to 4
    if (spiCSPins.size() > 4)
    {
        LOG_E(MODULE_PREFIX, "Number of CS pins must be less than or equal to 4");
        return false;
    }

    // Check if already setup
    if (_timerSeq.size() > 0)
    {
        LOG_E(MODULE_PREFIX, "Already setup");
        return false;
    }

    // Setup channel values
    _channelValuesPct.resize(spiCSPins.size() * NUM_CHANNELS_PER_CHIP, 0);

    // Setup flag for any channel dimmed
    _anyChannelOnChipDimmed.resize(spiCSPins.size(), false);    

    // Save SPI pins
    _spiMOSI = spiMOSI;
    _spiSCLK = spiSCLK;
    _spiCSPins = spiCSPins;
    _mainsSyncPin = mainsSyncPin;
    _useMainsSync = mainsSyncPin >= 0;

    // Setup SPI pins
    if ((_spiMOSI >= 0) && (_spiSCLK >= 0))
    {
        pinMode(_spiMOSI, OUTPUT);
        digitalWrite(_spiMOSI, LOW);
        pinMode(_spiSCLK, OUTPUT);
        digitalWrite(_spiSCLK, LOW);
    }

    // Setup SPI CS pins
    for (int i = 0; i < _spiCSPins.size(); i++)
    {
        if (_spiCSPins[i] >= 0)
        {
            pinMode(_spiCSPins[i], OUTPUT);
            digitalWrite(_spiCSPins[i], HIGH);
        }
    }

    // Check if mains sync is used - if so, setup timers and interrupt
    if (_useMainsSync)
    {
        // Setup sequence entries
        _timerSeq.resize(spiCSPins.size() * NUM_CHANNELS_PER_CHIP + 1);

        // Setup timer for zero crossing offset mo mains sync edge time
    #ifdef USE_ZERO_CROSS_OFFSET_TIMER
        const esp_timer_create_args_t zeroCrossTimerArgs = {
            .callback = &zeroCrossTimerCallbackStatic,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "dimming_offset_timer",
            .skip_unhandled_events = 0
        };
        esp_timer_create(&zeroCrossTimerArgs, &_zeroCrossTimerHandle);
    #endif

        // Setup timer for dimming
        const esp_timer_create_args_t dimmingTimerArgs = {
            .callback = &dimmingTimerCallbackStatic,
            .arg = this,
    #ifdef USE_ISR_FOR_TIMER_SEQUENCE
            .dispatch_method = ESP_TIMER_ISR,
    #else
            .dispatch_method = ESP_TIMER_TASK,
    #endif
            .name = "dimming_timer",
            .skip_unhandled_events = 0
        };
        esp_timer_create(&dimmingTimerArgs, &_dimmingTimerHandle);

        // Setup mains sync interrupt
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << _mainsSyncPin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_NEGEDGE
        };
        gpio_config(&io_conf);
        gpio_install_isr_service(0);
        gpio_isr_handler_add((gpio_num_t) _mainsSyncPin, mainsSyncISRStatic, this);
    }

    // Initialize mains cycle period average
    _zeroCrossPeriodUsAvg.sample(ZERO_CROSS_PERIOD_US_DEFAULT);

    // Debugging
#ifdef TEST_ISR_USING_GPIO
    pinMode(TEST_ISR_USING_GPIO, OUTPUT);
    digitalWrite(TEST_ISR_USING_GPIO, LOW);
#endif
#ifdef TEST_DIMMING_TIMER_USING_GPIO
    pinMode(TEST_DIMMING_TIMER_USING_GPIO, OUTPUT);
    digitalWrite(TEST_DIMMING_TIMER_USING_GPIO, LOW);
#endif

    // Set flag indicating sequences are valid
    _sequencesValid = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Loop (called frequently)
void SPIDimmer::loop()
{
    // Check if mains cycle period is valid for the first time
    if (!_mainsCyclePeriodSet && (_mainsCyclePeriodValid || !_useMainsSync))
    {
        // Set flag indicating mains cycle period is set
        _mainsCyclePeriodSet = true;

        // Set values (if no mains sync) or recalculate timer sequence
        setValuesOrRecalculateTimerSequence();
    }

#ifdef DEBUG_MAINS_FREQ
    if (Raft::isTimeout(millis(), _debugLastLoopMs, 1000))
    {
        LOG_I(MODULE_PREFIX, "Mains freq: %f Hz _mainsCyclePeriodValid %d _mainsCyclePeriodSet %d _useMainsSync %d", 
                getMainsHz(), _mainsCyclePeriodValid, _mainsCyclePeriodSet, _useMainsSync);
        _debugLastLoopMs = millis();
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set channel value in percent
/// @param channelNum Channel index (0 based)
/// @param valuePct Value in percent
void SPIDimmer::setChannelValue(uint32_t channelIdx, uint32_t valuePct)
{
    // Validate channel number
    if (channelIdx >= _channelValuesPct.size())
        return;

    // Validate value
    if ((valuePct > 100) || (!_useMainsSync && (valuePct != 0)))
        valuePct = 100;

    // Save the channel value
    _channelValuesPct[channelIdx] = valuePct;

    // Set values (if no mains sync) or recalculate timer sequence
    setValuesOrRecalculateTimerSequence();
}

////////////////////////////////////////////////////////////////////////////////
// Set timing
void SPIDimmer::setTiming(uint32_t zeroCrossOffsetFromSyncUs, uint32_t valOffsetUs)
{
    // Set phase offset from zero cross
    if (zeroCrossOffsetFromSyncUs != UINT32_MAX)
        _zeroCrossOffsetFromSyncUs = zeroCrossOffsetFromSyncUs;

    // TODO - remove
    if (valOffsetUs != UINT32_MAX)
        _timerSeq[0].us = valOffsetUs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Mains sync ISR static
/// @param pArg Pointer to this object
/// @note This fires at a consistent point in the rectified mains cycle - so 
///       twice per cycle - but not necessarily at the zero crossing point or peak
void IRAM_ATTR SPIDimmer::mainsSyncISRStatic(void* pArg) 
{
    SPIDimmer* pThis = (SPIDimmer*)pArg;
    if (pThis)
        pThis->mainsSyncISR();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Mains sync ISR
/// @note This fires at a consistent point in the rectified mains cycle - so 
///       twice per cycle - but not necessarily at the zero crossing point or peak
void IRAM_ATTR SPIDimmer::mainsSyncISR()
{
    // Check if sequences are valid
    if (!_sequencesValid)
        return;

    // Calculate the zero cross period
    uint64_t nowUs = esp_timer_get_time();
#ifdef ADJUST_TIMING_BASED_ON_ZERO_CROSS_PERIOD
    uint32_t timerCorrectionUs = 0;
#endif
    if (_lastMainsSyncUs > 0)
    {
        uint32_t zeroCrossingPeriod = nowUs - _lastMainsSyncUs;
        _mainsCyclePeriodValid = zeroCrossingPeriod >= ZERO_CROSS_PERIOD_MIN_US && zeroCrossingPeriod <= ZERO_CROSS_PERIOD_MAX_US;
        if (_mainsCyclePeriodValid)
        {
#ifdef ADJUST_TIMING_BASED_ON_ZERO_CROSS_PERIOD
            timerCorrectionUs = _zeroCrossPeriodUsAvg.getAverage() - zeroCrossingPeriod;
#endif
            _zeroCrossPeriodUsAvg.sample(zeroCrossingPeriod);
        }
    }

    // Save the mains sync time
    _lastMainsSyncUs = esp_timer_get_time();

#ifdef USE_ZERO_CROSS_OFFSET_TIMER
    // Start the zero crossing offset timer
    esp_timer_stop(_zeroCrossTimerHandle);
    esp_timer_start_once(_zeroCrossTimerHandle, _zeroCrossOffsetFromSyncUs + timerCorrectionUs);
#else
    // Use the zero cross timer callback directly
    zeroCrossTimerCallback();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Zero cross timer callback static
/// @param pArg Pointer to this object
void SPIDimmer::zeroCrossTimerCallbackStatic(void* pArg)
{
    SPIDimmer* pThis = (SPIDimmer*)pArg;
    if (pThis)
        pThis->zeroCrossTimerCallback();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Zero cross timer callback
void SPIDimmer::zeroCrossTimerCallback()
{
    // Debug
#ifdef TEST_ISR_USING_GPIO
    digitalWrite(TEST_ISR_USING_GPIO, HIGH);
#endif

    // Check if initial set required
    if (_initialSetReqd)
    {
        // Send the SPI data for steady state
        sendSPIData(_steadyStateData, true);

        // Set flag indicating initial set not required
        _initialSetReqd = false;
    }

    // Start sequence
    _timerSeqIdx = 0;

    // Start the timer for the next sequence entry
    if (_timerSeqIdx < _timerSeqTotal)
    {
        esp_timer_stop(_dimmingTimerHandle);
        int32_t timeSinceSyncUs = esp_timer_get_time() - _lastMainsSyncUs;
        int32_t zeroCrossingFromNowUs = int32_t(_zeroCrossOffsetFromSyncUs) - timeSinceSyncUs;
        uint32_t timeToFirstEventUs = int32_t(_timerSeq[_timerSeqIdx].us) + zeroCrossingFromNowUs;
        esp_timer_start_once(_dimmingTimerHandle, timeToFirstEventUs);
    }

    // Debug
#ifdef TEST_ISR_USING_GPIO
    digitalWrite(TEST_ISR_USING_GPIO, LOW);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Timer callback static (may be ISR if USE_ISR_FOR_TIMER_SEQUENCE is defined)
/// @param pArg Pointer to this object
void SPIDimmer::dimmingTimerCallbackStatic(void* pArg)
{
    SPIDimmer* pThis = (SPIDimmer*)pArg;
    if (pThis)
        pThis->dimmingTimerCallback();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Timer ISR
void SPIDimmer::dimmingTimerCallback()
{
    // Check if sequences are valid
    if (!_sequencesValid)
        return;

    // Debug
#ifdef TEST_DIMMING_TIMER_USING_GPIO
    digitalWrite(TEST_DIMMING_TIMER_USING_GPIO, HIGH);
#endif

    // Send the SPI data
    sendSPIData(_timerSeq[_timerSeqIdx].data, false);

    // Restore channels to steady state
    sendSPIData(_steadyStateData, false);

    // Check if at end of sequence
    _timerSeqIdx++;
    if (_timerSeqIdx < _timerSeqTotal)
    {
        // Time since sequence started
        uint32_t timeSinceSyncUs = esp_timer_get_time() - _lastMainsSyncUs;
        int32_t timeSinceZeroCrossUs = timeSinceSyncUs - int32_t(_zeroCrossOffsetFromSyncUs);

        // Calculate the time until the next sequence event
        uint32_t nextEventUs = _timerSeq[_timerSeqIdx].us <= timeSinceZeroCrossUs ? 1 : _timerSeq[_timerSeqIdx].us - timeSinceZeroCrossUs;

        // Start the timer for the next sequence entry
        esp_timer_stop(_dimmingTimerHandle);
        esp_timer_start_once(_dimmingTimerHandle, nextEventUs);
    }

    // Debug
#ifdef TEST_DIMMING_TIMER_USING_GPIO
    digitalWrite(TEST_DIMMING_TIMER_USING_GPIO, LOW);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Send SPI data
void IRAM_ATTR SPIDimmer::sendSPIData(uint64_t spiData, bool forceSend)
{
    // Send the SPI data for each chip
    for (int chipIdx = 0; chipIdx < _spiCSPins.size(); chipIdx++)
    {
        // Check if any channel is dimmed on the chip
        if (forceSend || _anyChannelOnChipDimmed[chipIdx])
        {
            // Write the sequence entry data to the chip
            bitBangSPI16Tx(chipIdx, spiData);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Bit-bang SPI 16-bit transmit
/// @param chipIdx Chip index
/// @param data Data to transmit
void IRAM_ATTR SPIDimmer::bitBangSPI16Tx(uint16_t chipIdx, uint64_t data)
{
    // Select the chip
    digitalWrite(_spiCSPins[chipIdx], LOW);
    
    // Shift the data for the specific chip
    data = data >> (chipIdx * NUM_CHANNELS_PER_CHIP * NUM_BITS_PER_CHANNEL);
    
    // Transmit the data
    for (int i = 0; i < NUM_CHANNELS_PER_CHIP * NUM_BITS_PER_CHANNEL; i++)
    {
        digitalWrite(_spiMOSI, (data & 0x8000) ? HIGH : LOW);
        digitalWrite(_spiSCLK, HIGH);
        delayMicroseconds(1);
        digitalWrite(_spiSCLK, LOW);
        delayMicroseconds(1);
        data <<= 1;
    }

    // Deselect the chip
    digitalWrite(_spiCSPins[chipIdx], HIGH);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set values (if no mains sync) or recalculate timer sequence (if mains sync)
void SPIDimmer::setValuesOrRecalculateTimerSequence()
{
    // Check if mains sync is used
    if (!_useMainsSync || !_mainsCyclePeriodValid)
    {
        // Set values
        sendSPIData(getSteadyStateSPIData(), true);
    }
    else
    {
        // Recalculate timer sequence
        recalculateTimerSequence();
    }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get steady-state data for SPI command
/// @return Steady-state data
uint64_t SPIDimmer::getSteadyStateSPIData()
{
    // Calculate initial timer sequence entry which turns on all channels
    // that are at 100% brightness (and others off)
    uint64_t steadyStateData = UINT64_MAX;
    for (int i = 0; i < _channelValuesPct.size(); i++)
    {
        // Calculate dimmer level bin
        uint32_t dimmerLevelBin = _channelValuesPct[i] / DIMMER_LEVEL_BIN_SIZE;
        bool isMaxDimmerLevel = (dimmerLevelBin >= NUM_DIMMER_BINS-1);

        // Calculate initial bit sequence for all channels
        uint64_t bitField = isMaxDimmerLevel ? CHANNEL_ON_BIT_SEQ : CHANNEL_OFF_BIT_SEQ;
        steadyStateData &= ~(CHANNEL_MASK_BIT_SEQ << (i * NUM_BITS_PER_CHANNEL));
        steadyStateData |= (bitField << (i * NUM_BITS_PER_CHANNEL));
    }
    return steadyStateData;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Recalculate the timer sequence
void SPIDimmer::recalculateTimerSequence()
{
    // Sequence working variables
    class DimmerLevelEntry
    {
    public:
        uint32_t dimmerLevelBin = 0;
        std::vector<uint32_t> channelIdxs;
    };
    std::vector<DimmerLevelEntry> dimmerLevels;

    // Clear any channel dimmed flag for each chip
    for (int i = 0; i < _spiCSPins.size(); i++)
        _anyChannelOnChipDimmed[i] = false;

    // Calculate initial timer sequence entry which turns on all channels
    // that are at 100% brightness (and others off)
    for (int i = 0; i < _channelValuesPct.size(); i++)
    {
        // Calculate dimmer level bin
        uint32_t dimmerLevelBin = _channelValuesPct[i] / DIMMER_LEVEL_BIN_SIZE;
        bool isMaxDimmerLevel = (dimmerLevelBin >= NUM_DIMMER_BINS-1);

        // Set any channel dimmed flag for each chip
        if ((_channelValuesPct[i] != 0) && (!isMaxDimmerLevel) && _mainsCyclePeriodValid)
        {
            // Set flag
            _anyChannelOnChipDimmed[i / NUM_CHANNELS_PER_CHIP] = true;

            // Insert the channel into the dimmer level entry table, keeping it in order
            // from highest to lowest dimmer level bin
            bool found = false;
            for (int j = 0; j < dimmerLevels.size(); j++)
            {
                if (dimmerLevels[j].dimmerLevelBin == dimmerLevelBin)
                {
                    dimmerLevels[j].channelIdxs.push_back(i);
                    found = true;
                    break;
                }
                else if (dimmerLevels[j].dimmerLevelBin < dimmerLevelBin)
                {
                    DimmerLevelEntry newEntry;
                    newEntry.dimmerLevelBin = dimmerLevelBin;
                    newEntry.channelIdxs.push_back(i);
                    dimmerLevels.insert(dimmerLevels.begin() + j, newEntry);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                DimmerLevelEntry newEntry;
                newEntry.dimmerLevelBin = dimmerLevelBin;
                newEntry.channelIdxs.push_back(i);
                dimmerLevels.push_back(newEntry);
            }
        }
    }

    // Calculate the timer sequence entries for each dimmer level
    uint32_t timingSeqTotal = 0;
    std::vector<TimerSeqEntry> tempTimerSeq;
    tempTimerSeq.reserve(dimmerLevels.size());
    for (int i = 0; i < dimmerLevels.size(); i++)
    {
        // Calculate the dimmer level data
        uint64_t dimmerLevelData = UINT64_MAX;
        for (int j = 0; j < dimmerLevels[i].channelIdxs.size(); j++)
        {
            // Calculate the bit field for the channel
            uint32_t channelIdx = dimmerLevels[i].channelIdxs[j];
            dimmerLevelData &= ~(CHANNEL_MASK_BIT_SEQ << (channelIdx * NUM_BITS_PER_CHANNEL));
            dimmerLevelData |= (CHANNEL_ON_BIT_SEQ << (channelIdx * NUM_BITS_PER_CHANNEL));
        }

        // Save the dimmer level data
        tempTimerSeq[timingSeqTotal].data = dimmerLevelData;

        // Calculate the time for the dimmer level
        tempTimerSeq[timingSeqTotal].us = getDimmerUsForBin(dimmerLevels[i].dimmerLevelBin);

        // Increment the total timer sequence entries
        timingSeqTotal++;
    }

    // Set flag indicating sequences are not valid
    _sequencesValid = false;
    
    // Save steady state data
    _steadyStateData = getSteadyStateSPIData();

    // Restart the timing sequence
    _timerSeqIdx = 0;

    // Copy over temporary values for timing sequences
    _timerSeqTotal = timingSeqTotal;
    for (int i = 0; i < timingSeqTotal; i++)
    {
        _timerSeq[i] = tempTimerSeq[i];
    }

    // Set flag indicating sequences are valid
    _sequencesValid = true;
    _initialSetReqd = true;

    // Debug
#ifdef DEBUG_TIMING_SEQUENCE
    String anyChannelDimmedStr;
    for (int i = 0; i < _spiCSPins.size(); i++)
    {
        anyChannelDimmedStr += " [" + String(i) + (_anyChannelOnChipDimmed[i] ? "]=Y" : "]=N");
    }
    LOG_I(MODULE_PREFIX, "Initial %s data: 0x%016llx anyDimmed:%s",
            dimmerLevels.size() == _timerSeqTotal ? "OK" : "INVALID",
            initialData, anyChannelDimmedStr.c_str());
    for (int i = 0; i < dimmerLevels.size(); i++)
    {
        String channelIdxsStr;
        for (int j = 0; j < dimmerLevels[i].channelIdxs.size(); j++)
        {
            channelIdxsStr += " " + String(dimmerLevels[i].channelIdxs[j]);
        }
        LOG_I(MODULE_PREFIX, "Dimmer bin %u data: 0x%016llx us: %u channels:%s", dimmerLevels[i].dimmerLevelBin, _timerSeq[i].data, _timerSeq[i].us, channelIdxsStr.c_str());
    }
#endif
}
