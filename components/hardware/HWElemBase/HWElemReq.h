/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Elem Request
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <vector>

class HWElemReq
{
public:
    // cmdId to use for unnumbered requests
    static const int UNNUM = 0;

    HWElemReq(const std::vector<uint8_t> writeData, int readLen, int cmdId, 
                const char* pReqName, uint32_t barAccessAfterSendMs)
    {
        // LOG_D(MODULE_PREFIX, "HWElemReq construct writeLen %d readLen %d", writeData.size(), readLen);
        clear();
        _writeData = writeData;
        _readReqLen = readLen;
        _pReqName = pReqName;
        _cmdId = cmdId;
        _barAccessAfterSendMs = barAccessAfterSendMs;
    }

    void clear()
    {
        _readReqLen = 0;
        _writeData.clear();
    }

    // Write data
    std::vector<uint8_t> _writeData;

    // Read data len
    uint32_t _readReqLen;

    // ID and Name of request
    int _cmdId;
    const char* _pReqName;

    // Bar access to element for ms after sending
    uint32_t _barAccessAfterSendMs;
};
