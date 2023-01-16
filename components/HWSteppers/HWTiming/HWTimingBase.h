/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HWTiming Base-class
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <functional>

class ConfigBase;
class TimingBase;

typedef std::function<void(TimingBase& timingSource)> TimingTickCB;

class TimingBase
{
public:
    // Constructor
    TimingBase()
    {
    }
    virtual ~TimingBase()
    {
    }

    // Setup
    virtual bool setup(ConfigBase& config)
    {
        return false;
    }

    // Service
    virtual void service()
    {
    }

    // Get Name
    virtual String getName() const
    {
        return "";
    }

    // Request timing tick
    virtual bool requestTicks(void* pObject, TimingTickCB tickCB)
    {
        return false;
    }
};
