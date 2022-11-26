/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pump Control
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <driver/ledc.h>
#include <stdint.h>

class PumpControl
{
public:
    PumpControl();
    virtual ~PumpControl();
    void setup(ConfigBase& config);
    void service();
    bool isBusy();
    void deinit();
    void setFlow(uint32_t pumpIdx, float flowRate, float durationSecs);

private:

    // PWM output control
    class PWMOutput
    {
    public:
        PWMOutput()
        {
        }
        PWMOutput(const char* name, int pinNum, uint32_t ledcChanNum, uint32_t maxDuty)
        {
            _name = name;
            _ledcChanNum = ledcChanNum;
            _maxDuty = maxDuty;
            _pinNum = pinNum;
            _isSetup = true;
        }
        bool _isSetup = false;
        String _name;
        bool _isOn = false;
        bool _timerActive = false;
        int _pinNum = -1;
        uint32_t _startTimeMs = 0;
        uint32_t _durationMs = 0;
        uint32_t _ledcChanNum = 0;
        uint32_t _maxDuty = 0;

        // Helpers
        void set(float pwmRatio, uint32_t durationMs);
        void service();
        void deinit();
        String getStatusStrJSON();
        uint8_t getStateHashByte();
    };

    // PWM outputs
    std::vector<PWMOutput> _pwmOutputs;

    // Get and check status changes
    String getStatusStrJSON(bool includeBraces);
    void getStatusHash(std::vector<uint8_t>& stateHash);

    // Debug
    uint32_t _debugLastDisplayMs = 0;
};
