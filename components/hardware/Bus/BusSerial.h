/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Serial Bus Handler
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "BusBase.h"
#include <stdint.h>
#include <ArduinoOrAlt.h>
#include <list>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

class BusSerial : public BusBase
{
public:
    // Constructor
    BusSerial(BusElemStatusCB busElemStatusCB, BusOperationStatusCB busOperationStatusCB);
    virtual ~BusSerial();

    // Setup
    virtual bool setup(ConfigBase& config, const char* pConfigPrefix) override final;

    // Service
    virtual void service() override final;

    // Clear
    virtual void clear(bool incPolling) override final;

    // Pause
    virtual void pause(bool pause) override final
    {
    }

    // IsPaused
    virtual bool isPaused() override final
    {
        return false;
    }

    // Get bus name
    virtual String getBusName() const override final
    {
        return _busName;
    }

    // isReady (for new requests)
    virtual bool isReady() override final;

    // Request bus action
    virtual bool addRequest(BusRequestInfo& busReqInfo) override final;

    // Clear receive buffer
    virtual void rxDataClear() override final;

    // Received data bytes available
    virtual uint32_t rxDataBytesAvailable() override final;

    // Get rx data - returns number of bytes placed in pData buffer
    virtual uint32_t rxDataGet(uint8_t* pData, uint32_t maxLen) override final;

    // Creator fn
    static BusBase* createFn(BusElemStatusCB busElemStatusCB, BusOperationStatusCB busOperationStatusCB)
    {
        return new BusSerial(busElemStatusCB, busOperationStatusCB);
    }

private:
    // Settings
    int _uartNum;
    int _rxPin;
    int _txPin;
    int _baudRate;
    String _busName;
    uint32_t _rxBufSize;
    uint32_t _txBufSize;
    uint32_t _minTimeBetweenSendsMs;
    uint32_t _lastSendTimeMs;

    // isInitialised
    bool _isInitialised;

    // Defaults
    static const uint32_t BAUD_RATE_DEFAULT = 115200;
    static const uint32_t RX_BUF_SIZE_DEFAULT = 256;
    static const uint32_t TX_BUF_SIZE_DEFAULT = 256;

    // Helpers
    bool serialInit();
};
