
#include <stdint.h>
#include "ExpMovingAverage.h"
#include "PeakValueFollower.h"

// Debug vals
struct CTProcessorDebugVals
{
    float peakValPos;
    uint64_t peakTimePos = 0;
    float peakValNeg = 0;
    uint64_t peakTimeNeg = 0;
    float rmsVal = 0;
    uint64_t lastZeroCrossingTimeUs = 0;
    float meanADCValue = 0;
    float prevACADCSample = 0;
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
    void setup(float currentScalingFactor, uint32_t sampleRateHz, float signalFreqHz)
    {
        _currentScalingFactor = currentScalingFactor;
        _currentScalingFactorSquared = currentScalingFactor * currentScalingFactor;
        _sampleRateHz = sampleRateHz;
        _signalFreqHz = signalFreqHz;
        _numSamplesPerCycle = sampleRateHz / signalFreqHz;
        _numSamplesForMeanValid = _numSamplesPerCycle * 10;
        _sampleIntervalUs = 1000000 / sampleRateHz;
        _halfCycleTimeUs = 1000000 / (signalFreqHz * 2);
        _halfCycleSamples = _numSamplesPerCycle / 2;

        // Setup the peak follower to use 10 cycles for 100% decay
        _peakValueFollower.setup(10 * 1000000 / signalFreqHz);
    }
    void newADCReading(uint16_t sample, uint64_t sampleTimeUs)
    {
        // Update the exponential moving average
        _expMovingAverage.sample(sample);

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
        double sampleACADCValue = (float)smoothedSample - _expMovingAverage.getAverage();

        // Accumulate the RMS value
        _curSquareACSamples += sampleACADCValue * sampleACADCValue * _currentScalingFactorSquared;
        _curRMSSampleCount++;

        // Check for zero crossing (from negative to positive) and at least 1/2 cycle has passed
        if (_prevACADCSample < 0 && sampleACADCValue > 0 &&
            Raft::isTimeout(sampleTimeUs, _lastZeroCrossingTimeUs, _halfCycleTimeUs))
        {
            // Store the accumulated RMS value (unless this is the first zero crossing time)
            if (_lastZeroCrossingTimeUs != 0)
                _curRMSACValue = sqrt(_curSquareACSamples / _curRMSSampleCount);

            // Reset the RMS value
            _curSquareACSamples = 0;
            _curRMSSampleCount = 0;

            // Store the last zero crossing time
            _lastZeroCrossingTimeUs = sampleTimeUs;
        }

        // Store the previous sample
        _prevACADCSample = sampleACADCValue;




        //     // Calculate the time since the last zero crossing
        //     uint64_t timeSinceLastZeroCrossingUs = timestamp - _prevSampleTimeUs;

        //     // Calculate the RMS value
        //     float rmsValue = _expMovingAverage.getAverage() * _currentScalingFactor;

        //     // Calculate the peak value
        //     float peakValue = _peakValue * _currentScalingFactor;

        //     // Check for a new peak value
        //     if (rmsValue > peakValue)
        //     {
        //         _peakValue = rmsValue;
        //         _peakValueTimeUs = timestamp;
        //     }

        //     // Debug
        //     // LOG_I("CTProcessor", "RMS: %f, Peak: %f, PeakTime: %u, TimeSinceLastZeroCrossing: %llu", rmsValue, peakValue, _peakValueTimeUs, timeSinceLastZeroCrossingUs);

        //     // Reset the exponential moving average
        //     _expMovingAverage.sample(0);

        //     // Reset the total samples
        //     _totalSamples = 0;

        //     // Update the last zero crossing time
        //     _lastZeroCrossingTimeUs = timestamp;
        // }

        // // Update the previous sample
        // _prevSample = sample;
        // _prevSampleTimeUs = timestamp;
    }

    // Get the current RMS value
    float getCurrentRMS() const;

    // Get the current peak value
    float getCurrentPeak() const;

    // Get the current peak time
    uint32_t getCurrentPeakTime() const;

    // Debug
    void getDebugInfo(CTProcessorDebugVals& debugVals) const
    {
        debugVals.peakValPos =_peakValueFollower.getPositivePeakValue();
        debugVals.peakTimePos = _peakValueFollower.getPositivePeakTimeUs();
        debugVals.peakValNeg = _peakValueFollower.getNegativePeakValue();
        debugVals.peakTimeNeg = _peakValueFollower.getNegativePeakTimeUs();
        debugVals.rmsVal = _curRMSACValue;
        debugVals.lastZeroCrossingTimeUs = _lastZeroCrossingTimeUs;
        debugVals.meanADCValue = _expMovingAverage.getAverage();
        debugVals.prevACADCSample = _prevACADCSample;
    }

private:

    // ADC data
    SimpleMovingAverage<5, ADC_DATA_TYPE, uint32_t> _adcData;

    // Mean value
    ExpMovingAverage<3, ADC_DATA_TYPE> _expMovingAverage;

    // Last zero crossing time
    uint64_t _lastZeroCrossingTimeUs = 0;

    // Total samples (may wrap but only used to determine if mean is valid)
    uint32_t _totalSamples = 0;
    uint32_t _numSamplesForMeanValid = 0;

    // Previous sample
    float _prevACADCSample = 0;

    // RMS calculation
    double _curSquareACSamples = 0;
    double _curRMSACValue = 0;
    uint32_t _curRMSSampleCount = 0;

    // Setup
    double _currentScalingFactor = 1.0;
    double _currentScalingFactorSquared = 1.0;
    uint32_t _sampleRateHz = 0;
    double _signalFreqHz = 0;
    uint32_t _numSamplesPerCycle = 0;
    static constexpr float MIN_AC_VALUE_ABOVE_NOISE_FOR_ZERO_CROSSING = 50;

    // Calculated timing
    uint64_t _sampleIntervalUs = 0;
    uint64_t _halfCycleTimeUs = 0;
    uint64_t _halfCycleSamples = 0;

    // Peak value follower
    PeakValueFollower<float, uint64_t> _peakValueFollower;
};
