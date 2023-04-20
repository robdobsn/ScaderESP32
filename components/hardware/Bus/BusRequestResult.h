/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bus Request Result
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <vector>
#include <BusRequestInfo.h>

class BusRequestResult
{
public:
    BusRequestResult()
    {
        clear();
    }

    BusRequestResult(uint32_t address, uint32_t cmdId, const uint8_t *pBuf, uint32_t len, bool ok, 
                    BusRequestCallbackType callback, void *callbackParam)
    {
        _address = address;
        uint32_t bytesToCopy = (len <= RESPONSE_BUFFER_MAX_BYTES) ? len : RESPONSE_BUFFER_MAX_BYTES;
        if (bytesToCopy > 0)
            _respBuf.assign(pBuf, pBuf+bytesToCopy);
        _result = ok ? REQ_RESULT_OK : REQ_RESULT_FAIL;
        _callback = callback;
        _callbackParam = callbackParam;
        _cmdId = cmdId;
    }

    void clear()
    {
        _address = 0;
        _result = REQ_RESULT_NONE;
        _callbackParam = nullptr;
        _callback = nullptr;
        _respBuf.clear();
        _cmdId = 0;
    }

    uint8_t *getReadData()
    {
        return _respBuf.data();
    }

    uint32_t getReadDataLen()
    {
        return _respBuf.size();
    }

    enum ReqResultType
    {
        REQ_RESULT_NONE,
        REQ_RESULT_FAIL,
        REQ_RESULT_OK
    };

    ReqResultType getResult()
    {
        return _result;
    }

    uint32_t getAddress()
    {
        return _address;
    }

    bool isResultOk()
    {
        return _result == REQ_RESULT_OK;
    }

    BusRequestCallbackType getCallback()
    {
        return _callback;
    }

    void* getCallbackParam()
    {
        return _callbackParam;
    }

    uint32_t getCmdId()
    {
        return _cmdId;
    }

private:
    // Read data
    static const int RESPONSE_BUFFER_MAX_BYTES = 120;
    std::vector<uint8_t> _respBuf;

    // Result
    ReqResultType _result;

    // Callback
    BusRequestCallbackType _callback;
    void *_callbackParam;

    // Address
    uint32_t _address;

    // Command ID (used to identify command that requested)
    uint32_t _cmdId;
};
