/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// EndStops
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdint.h"
#include "RaftUtils.h"
#include <ArduinoOrAlt.h>

class EndStops
{
public:
    EndStops();
    virtual ~EndStops();

    // Clear
    void clear();

    // Add endstop
    void add(bool isMax, const char* name, int endStopPin, bool actvLevel, int inputType);

    // Service - called frequently
    virtual void service();

    // Check if at end stop
    bool IRAM_ATTR isAtEndStop(bool max);

    // Check if valid
    bool IRAM_ATTR isValid(bool max);

    // Get pin and level
    bool IRAM_ATTR getPinAndLevel(bool max, int& pin, bool& actvLevel);

private:
    String _maxName;
    int _maxEndStopPin;
    bool _maxActLevel;
    int _maxInputType;

    String _minName;
    int _minEndStopPin;
    int _minActLevel;
    int _minInputType;
};
