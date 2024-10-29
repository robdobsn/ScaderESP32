/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderTest
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftUtils.h"

class APISourceInfo;

class ScaderTest : public RaftSysMod
{
public:
    ScaderTest(const char *pModuleName, RaftJsonIF& sysConfig)
            : RaftSysMod(pModuleName, sysConfig)
    {
    }

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderTest(pModuleName, sysConfig);
    }
    
protected:

    // Setup
    virtual void setup() override final
    {
        // Debug
        LOG_I(MODULE_PREFIX, "----------------------- setup enabled");
    }

    // Loop (called frequently)
    virtual void loop() override final
    {
    }

    // Get named value
    virtual double getNamedValue(const char* valueName, bool& isValid) override final
    {
        LOG_I(MODULE_PREFIX, "----------------- getNamedValue %s", valueName);
        isValid = true;
        if (String(valueName).equalsIgnoreCase("batteryPC"))
        {
            battPC += 1;
            if (battPC >= 100)
                battPC = 0;
            return battPC;
        }
        else if (String(valueName).equalsIgnoreCase("heartRate"))
        {
            if (heartRateUp)
            {
                heartRate += 5;
                if (heartRate >= 150)
                    heartRateUp = false;
            }
            else
            {
                heartRate -= 5;
                if (heartRate <= 60)
                    heartRateUp = true;
            }
            return heartRate;
        }
        isValid = false;
        return 0;
    }

    // Test value
    uint32_t battPC = 0;
    uint32_t heartRate = 60;
    bool heartRateUp = true;

    static constexpr const char *MODULE_PREFIX = "ScaderTest";
};
