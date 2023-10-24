/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Latching Moving Average
//
// Rob Dobson 2023
//
// Modified from https://tttapa.github.io/Pages/Mathematics/Systems-and-Control-Theory/Digital-filters/Simple%20Moving%20Average/C++Implementation.html
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>

/*
 * Moving Average
 *
 * Calculate the average of an angle value over a moving window of N samples
 * 
 * Template N is the window size
 * Template maxVal is the maximum value of the input (which may wrap around)
 * Template input_t is the type of the input value
 * Template sum_t is the type of the sum
 * 
 */

template <uint16_t N, int32_t maxVal, class input_t = int32_t, class sum_t = int64_t>
class LatchingAngleAverage
{
public:
    LatchingAngleAverage()
    {
        clear();
    }
    void sample(input_t input)
    {
        // Unwrap the input
        if (input > lastInput + maxVal/2)
            fullRotations--;
        else if (input < lastInput - maxVal/2)
            fullRotations++;
        lastInput = input;
        input_t wrappedInput = input + fullRotations * maxVal;
        // Sum unwrapped values discarding the oldest
        sum -= previousInputs[index];
        sum += wrappedInput;
        previousInputs[index] = wrappedInput;
        // Bump index
        if (++index == N)
            index = 0;
        // Calculate result
        auto result = (sum + (N / 2)) / N;
        if (fabs(result - hysteresisResult) > hysteresis)
            hysteresisResult = result;
    }

    void setHysteresis(double hysteresisVal)
    {
        hysteresis = hysteresisVal;
    }

    input_t getAverage(bool withHysteresis, bool clamped)
    {
        if (withHysteresis)
            return clamped ? (hysteresisResult % maxVal + maxVal) % maxVal : hysteresisResult;
        return clamped ? (avgWithoutHysteresis() % maxVal + maxVal) % maxVal : avgWithoutHysteresis();
    }
    input_t avgWithoutHysteresis()
    {
        return (sum + (N / 2)) / N;
    }

    void clear()
    {
        index = 0;
        sum = 0;
        for (uint16_t i = 0; i < N; i++)
            previousInputs[i] = 0;
        hysteresisResult = 0;
    }

private:
    input_t lastInput = 0;
    uint16_t index = 0;
    input_t previousInputs[N] = {};
    sum_t sum = 0;
    double hysteresis = 0;
    input_t hysteresisResult = 0;
    int32_t fullRotations = 0;
};
