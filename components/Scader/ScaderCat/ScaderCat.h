/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderCat
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <ConfigBase.h>
#include <ScaderCommon.h>
#include <RaftUtils.h>
#include <SysModBase.h>

class APISourceInfo;

class ScaderCat : public SysModBase
{
public:
    ScaderCat(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

    // Check if the shade is moving
    bool isBusy(int shadeIdx);

protected:

    // Setup
    virtual void setup() override final;

    // Service (called frequently)
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;

private:

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // Timed output control
    class TimedOutput
    {
    public:
        TimedOutput()
        {
            _timerActive = false;
            _isOn = false;
            _pin = -1;
            _onLevel = false;
            _startTimeMs = 0;
            _durationMs = 0;
        }
        TimedOutput(const char* name, int pin, bool onLevel)
        {
            _name = name;
            _timerActive = false;
            _isOn = false;
            _pin = pin;
            _onLevel = onLevel;
        }
        String _name;
        bool _timerActive;
        bool _isOn;
        int _pin;
        bool _onLevel;
        uint32_t _startTimeMs;
        uint32_t _durationMs;

        // Helpers
        void set(bool turnOn, int durationMs);
        void cmd(const String &ctrlStr, const String &durationStr);
        void service();
        void deinit();
        String getStatusStrJSON();
        uint8_t getStateHashByte();
    };

    // Timing
    std::vector<TimedOutput> _timedOutputs;

    // Get and check status changes
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Helper functions
    void deinit();
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
