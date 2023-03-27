/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UI Module
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <OpenerStatus.h>

class UIModule
{
public:
    // Constructor/destructor
    UIModule();
    virtual ~UIModule();

    // Setup
    void setup(ConfigBase &config, OpenerStatus* pOpenerStatus);

    // Service - called frequently
    void service();

private:
    // Serial details
    int _uartNum = 0;
    int _baudRate = 912600;
    int _txPin = -1;
    int _rxPin = -1;
    uint32_t _rxBufSize = 1024;
    uint32_t _txBufSize = 1024;

    // Flag indicating begun
    bool _isInitialised = false;

    // Rx line
    String _rxLine;
    static const int MAX_RX_LINE_LEN = 500;

    // Opener parameters
    OpenerStatus* _pOpenerStatus;

    // Timing of status updates
    static const uint32_t STATUS_UPDATE_INTERVAL_MS = 1000;
    uint32_t _statusUpdateLastMs = 0;

    // Helpers
    void processStatus(const String& statusStr);
};
