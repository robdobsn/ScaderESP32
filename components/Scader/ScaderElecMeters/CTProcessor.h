/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Current transformer processor
//
// Rob Dobson 2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include "ExpMovingAverage.h"
#include "PeakValueFollower.h"


// Debug vals
struct DebugCTProcessorVals
{
    float peakValPos;
    uint64_t peakTimePos = 0;
    float peakValNeg = 0;
    uint64_t peakTimeNeg = 0;
    float rmsCurrentAmps = 0;
    float rmsPowerW = 0;
    uint64_t lastZeroCrossingTimeUs = 0;
    float meanADCValue = 0;
    float prevACADCSample = 0;
    uint16_t sampleIntervalErrUsMean = 0;
    float sampleIntervalErrUsStdDev = 0;
    uint16_t curADCSample = 0;
    float totalKWh = 0;
};

template <typename ADC_DATA_TYPE>
class CTProcessor
{
public:
    CTProcessor()
    {
    }
    ~CTProcessor()
    {
    }

    // Setup
    void setup(float currentScalingFactor, uint32_t sampleRateHz, float signalFreqHz, float mainVoltageRMS, double totalKWh)
    {
        _currentScalingFactor = currentScalingFactor;
        _scaleADCToAmpsSquared = currentScalingFactor * currentScalingFactor;
        _sampleRateHz = sampleRateHz;
        _signalFreqHz = signalFreqHz;
        _numSamplesPerCycle = sampleRateHz / signalFreqHz;
        _numSamplesForMeanValid = _numSamplesPerCycle * 10;
        _sampleIntervalUs = 1000000 / sampleRateHz;
        _halfCycleTimeUs = 1000000 / (signalFreqHz * 2);
        _halfCycleSamples = _numSamplesPerCycle / 2;
        _mainsVoltageRMS = mainVoltageRMS;
        _totalKWh = totalKWh;
        _lastReportedTotalKWh = totalKWh;
        _totalKWhPersistanceReqd = false;

        // Setup the peak follower to use 10 cycles for 100% decay
        _peakValueFollower.setup(10 * 1000000 / signalFreqHz);
    }

    // Handle a new ADC reading
    void newADCReading(uint16_t sample, uint64_t sampleTimeUs)
    {
        // Store the sample
        _curSample = sample;

        // Update the exponential moving average
        _adcValueAverager.sample(sample);

        // Update the peak value follower
        _peakValueFollower.sample(sample, sampleTimeUs);

        // Update the sample smoother
        _adcData.sample(sample);

        // Update the total samples
        _totalSamples++;

        // Check if total samples is enough to calculate the mean
        if (_totalSamples < _numSamplesForMeanValid)
            return;

        // Smoothed sample
        ADC_DATA_TYPE smoothedSample = _adcData.getAverage();

        // Calculate sample AC value
        double sampleACADCValue = (float)smoothedSample - _adcValueAverager.getAverage();

        // Accumulate the square of the current values
        _sumAmpsSquared += sampleACADCValue * sampleACADCValue * _scaleADCToAmpsSquared;
        _curRMSSampleCount++;

        // Check for zero crossing (from negative to positive) and at least 1/2 cycle has passed
        if (_prevACADCSample < 0 && sampleACADCValue > 0 &&
            Raft::isTimeout(sampleTimeUs, _lastZeroCrossingTimeUs, _halfCycleTimeUs))
        {
            // Store the accumulated RMS value (unless this is the first zero crossing time)
            if (_lastZeroCrossingTimeUs != 0)
            {
                _rmsAmpsAverager.sample(sqrt(_sumAmpsSquared / _curRMSSampleCount));

                // Update total KWh
                _totalKWh += _rmsAmpsAverager.getAverage() * _mainsVoltageRMS * Raft::timeElapsed(sampleTimeUs, _lastZeroCrossingTimeUs) / 3600000000000.0;
            }

            // Reset the RMS value
            _sumAmpsSquared = 0;
            _curRMSSampleCount = 0;

            // Store the last zero crossing time
            _lastZeroCrossingTimeUs = sampleTimeUs;
        }

        // Check if the difference between lastReportedKWh and actual totalKWh is sufficient to trigger persistence
        if (fabs(_totalKWh - _lastReportedTotalKWh) > TOTAL_KWH_PERSISTENCE_THRESHOLD)
        {
            _totalKWhPersistanceReqd = true;
            _lastReportedTotalKWh = _totalKWh;
        }

        // Store the previous sample
        _prevACADCSample = sampleACADCValue;

        // Compute time interval stats for debugging
        if (_debugLastDataAcqSampleTimeUs != 0)
        {
            int64_t timeSinceLastSampleUs = sampleTimeUs - _debugLastDataAcqSampleTimeUs;
            _dataAcqSampleIntervals.sample(timeSinceLastSampleUs - _sampleIntervalUs);
        }
        _debugLastDataAcqSampleTimeUs = sampleTimeUs;
    }

    // Get JSON status
    String getStatusJSON() const
    {
        return String("{") + 
            "\"rmsCurrentA\":" + String(_rmsAmpsAverager.getAverage(), 1) + 
            ",\"rmsPowerW\":" + String(_rmsAmpsAverager.getAverage() * _mainsVoltageRMS, 1) +
            ",\"totalKWh\":" + String(_lastReportedTotalKWh, 1) +
            "}";
    }

    // Check if persistence is required
    bool isPersistenceReqd()
    {
        return _totalKWhPersistanceReqd;
    }
    void setPersistenceDone()
    {
        _totalKWhPersistanceReqd = false;
    }

    // Get total KWh
    float getTotalKWh() const
    {
        return _lastReportedTotalKWh;
    }

    // Set total KWh
    void setTotalKWh(float totalKWh)
    {
        _totalKWh = totalKWh;
        _lastReportedTotalKWh = totalKWh;
        _totalKWhPersistanceReqd = true;
    }

    // Debug
    void getDebugInfo(DebugCTProcessorVals& debugVals) const
    {
        debugVals.peakValPos =_peakValueFollower.getPositivePeakValue();
        debugVals.peakTimePos = _peakValueFollower.getPositivePeakTimeUs();
        debugVals.peakValNeg = _peakValueFollower.getNegativePeakValue();
        debugVals.peakTimeNeg = _peakValueFollower.getNegativePeakTimeUs();
        debugVals.rmsCurrentAmps = _rmsAmpsAverager.getAverage();
        debugVals.rmsPowerW = _rmsAmpsAverager.getAverage() * _mainsVoltageRMS;
        debugVals.lastZeroCrossingTimeUs = _lastZeroCrossingTimeUs;
        debugVals.meanADCValue = _adcValueAverager.getAverage();
        debugVals.prevACADCSample = _prevACADCSample;
        debugVals.sampleIntervalErrUsMean = _dataAcqSampleIntervals.getAverage();
        debugVals.sampleIntervalErrUsStdDev = _dataAcqSampleIntervals.getStandardDeviation();
        debugVals.curADCSample = _curSample;
        debugVals.totalKWh = _totalKWh;
    }

    void getStatusHash(std::vector<uint8_t>& stateHash)
    {
        // Hash changes on 200mA intervals
        uint8_t hashVal = 0;
        float rmsVal = _rmsAmpsAverager.getAverage();
        uint16_t rmsValInt = (uint16_t)(rmsVal*5);
        for (uint32_t i = 0; i < sizeof(rmsValInt); i++)
            hashVal ^= ((uint8_t*)&rmsValInt)[i];
        uint32_t totalKWhInt = (uint32_t)(_totalKWh*10);
        for (uint32_t i = 0; i < sizeof(totalKWhInt); i++)
            hashVal ^= ((uint8_t*)&totalKWhInt)[i];
        stateHash.push_back(hashVal);
    }

private:

    // Smoothing of ADC data
    SimpleMovingAverage<10, ADC_DATA_TYPE, uint32_t> _adcData;

    // Mean value
    ExpMovingAverage<4, ADC_DATA_TYPE> _adcValueAverager;

    // Last zero crossing time
    uint64_t _lastZeroCrossingTimeUs = 0;

    // Total samples (may wrap but only used to determine if mean is valid)
    uint32_t _totalSamples = 0;
    uint32_t _numSamplesForMeanValid = 0;

    // Samples
    ADC_DATA_TYPE _curSample = 0;
    float _prevACADCSample = 0;

    // RMS calculation
    double _sumAmpsSquared = 0;
    SimpleMovingAverage<25, float, float> _rmsAmpsAverager;
    uint32_t _curRMSSampleCount = 0;

    // total KWh
    double _totalKWh = 0;

    // total KWh reporting & persistence
    static constexpr float TOTAL_KWH_PERSISTENCE_THRESHOLD = 0.5;
    double _lastReportedTotalKWh = 0;
    bool _totalKWhPersistanceReqd = false;

    // Setup
    double _currentScalingFactor = 1.0;
    double _scaleADCToAmpsSquared = 1.0;
    uint32_t _sampleRateHz = 0;
    double _signalFreqHz = 0;
    uint32_t _numSamplesPerCycle = 0;
    float _mainsVoltageRMS = 230;
    static constexpr float MIN_AC_VALUE_ABOVE_NOISE_FOR_ZERO_CROSSING = 50;

    // Calculated timing
    uint64_t _sampleIntervalUs = 0;
    uint64_t _halfCycleTimeUs = 0;
    uint64_t _halfCycleSamples = 0;

    // Peak value follower
    PeakValueFollower<float, uint64_t> _peakValueFollower;

    // Debug timing
    SimpleMovingAverage<100, int16_t, int32_t> _dataAcqSampleIntervals;
    uint64_t _debugLastDataAcqSampleTimeUs = 0;
    uint32_t _debugSampleTimeUsCount = 0;
};
