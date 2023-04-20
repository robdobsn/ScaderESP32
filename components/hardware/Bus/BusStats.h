/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bus Stats
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <stdio.h>
#include "BusStats.h"

class BusStats
{
public:
    
public:
    // Constructor
    BusStats()
    {
        _busInteractionCount = 0;
        _respBufferFulls = 0;
        _reqBufferFulls = 0;
        _pollCompletes = 0;
        _cmdCompletes = 0;
        _respQueueCount = 0;
        _respQueuePeak = 0;
        _reqQueueCount = 0;
        _reqQueuePeak = 0;
        _respLengthError = 0;
    }
    virtual ~BusStats()
    {
    }

    // Get stats
    String getStatsJSON(const String& busName)
    {
        char outStr[150];
            snprintf(outStr, sizeof(outStr), R"("%s":{"cnt":%d,"reqF":%d,"reqQ":%d,"reqQPk":%d,"rspF":%d,"rspQ":%d,"rspQPk":%d,"rspE":%d,"poll":%d,"cmds":%d})",
                busName.c_str(),
                (unsigned int)_busInteractionCount, 
                (unsigned int)_reqBufferFulls, (unsigned int)_reqQueueCount, (unsigned int)_reqQueuePeak,
                (unsigned int)_respBufferFulls, (unsigned int)_respQueueCount, (unsigned int)_respQueuePeak, 
                (unsigned int)_respLengthError, (unsigned int)_pollCompletes, (unsigned int)_cmdCompletes);
        return outStr;
    }
    
    // Record activity
    void activity()
    {
        _busInteractionCount++;
    }

    void respBufferFull()
    {
        _respBufferFulls++;
    }

    void reqBufferFull()
    {
        _reqBufferFulls++;
    }

    void pollComplete()
    {
        _pollCompletes++;
    }

    void cmdComplete()
    {
        _cmdCompletes++;
    }

    void respLengthError()
    {
        _respLengthError++;
    }

    void respQueueCount(uint32_t count)
    {
        // LOG_I("BUSSTATS", "respQueueCount %d pk %d", count, _respQueuePeak);
        _respQueueCount = count;
        if (_respQueuePeak < _respQueueCount)
            _respQueuePeak = _respQueueCount;
    }

    void reqQueueCount(uint32_t count)
    {
        // LOG_I("BUSSTATS", "reqQueueCount %d pk %d", count, _reqQueuePeak);
        _reqQueueCount = count;
        if (_reqQueuePeak < _reqQueueCount)
            _reqQueuePeak = _reqQueueCount;
    }

private:
    // Count of calls
    uint32_t _busInteractionCount;

    // Request buffer fulls
    uint32_t _reqBufferFulls;

    // Response buffer fulls
    uint32_t _respBufferFulls;

    // Poll complete
    uint32_t _pollCompletes;

    // Command complete
    uint32_t _cmdCompletes;

    // Length error
    uint32_t _respLengthError;

    // Request queue instantaneous and peak levels
    uint32_t _reqQueueCount;
    uint32_t _reqQueuePeak;

    // Response queue instantaneous and peak levels
    uint32_t _respQueueCount;
    uint32_t _respQueuePeak;

};
