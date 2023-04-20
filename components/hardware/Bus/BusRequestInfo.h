/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bus Request Info
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <vector>
#include <HWElemReq.h>

class BusRequestResult;
typedef void (*BusRequestCallbackType) (void*, BusRequestResult&);

enum BusReqType
{
    BUS_REQ_TYPE_STD,
    BUS_REQ_TYPE_POLL,
    BUS_REQ_TYPE_FW_UPDATE,
    BUS_REQ_TYPE_SLOW_SCAN,
    BUS_REQ_TYPE_FAST_SCAN,
    BUS_REQ_TYPE_SEND_IF_PAUSED    
};

// Bus request info
class BusRequestInfo
{
public:
    friend class BusRequestRec;
    
    BusRequestInfo()
    {
        clear();
    }

    BusRequestInfo(const String& elemName, uint32_t address)
    {
        clear();
        _elemName = elemName;
        _address = address;
    }

    BusRequestInfo(const String& elemName, uint32_t address, const uint8_t* pData, uint32_t dataLen)
    {
        clear();
        _elemName = elemName;
        _address = address;
        _writeData.assign(pData, pData+dataLen);
    }

    void set(BusReqType reqType, const HWElemReq& hwElemReq, 
                double pollFreqHz=1.0,
                BusRequestCallbackType busReqCallback=NULL, void* pCallbackData=NULL)
    {
        _busReqType = reqType;
        _writeData = hwElemReq._writeData;
        _readReqLen = hwElemReq._readReqLen;
        _pollFreqHz = pollFreqHz;
        _busReqCallback = busReqCallback;
        _pCallbackData = pCallbackData;
        _cmdId = hwElemReq._cmdId;
        _barAccessForMsAfterSend = hwElemReq._barAccessAfterSendMs;
    }

    BusReqType getBusReqType()
    {
        return _busReqType;
    }

    BusRequestCallbackType getCallback()
    {
        return _busReqCallback;
    }

    void* getCallbackParam()
    {
        return _pCallbackData; 
    }

    bool isPolling()
    {
        return _busReqType == BUS_REQ_TYPE_POLL;
    }

    double getPollFreqHz()
    {
        return _pollFreqHz;
    }

    bool isFWUpdate()
    {
        return _busReqType == BUS_REQ_TYPE_FW_UPDATE;
    }

    bool isFastScan()
    {
        return _busReqType == BUS_REQ_TYPE_FAST_SCAN;
    }

    bool isSlowScan()
    {
        return _busReqType == BUS_REQ_TYPE_SLOW_SCAN;
    }

    uint8_t* getWriteData()
    {
        if (_writeData.size() == 0)
            return NULL;
        return _writeData.data();
    }

    uint32_t getWriteDataLen()
    {
        return _writeData.size();
    }

    uint32_t getReadReqLen()
    {
        return _readReqLen;
    }

    uint32_t getAddressUint32()
    {
        return _address;
    }

    uint32_t getCmdId()
    {
        return _cmdId;
    }

    void setBarAccessForMsAfterSend(uint32_t barMs)
    {
        _barAccessForMsAfterSend = barMs;
    }    

    uint32_t getBarAccessForMsAfterSend()
    {
        return _barAccessForMsAfterSend;
    }    

private:
    void clear()
    {
        _pollFreqHz = 1.0;
        _busReqType = BUS_REQ_TYPE_STD;
        _pCallbackData = nullptr;
        _busReqCallback = nullptr;
        _cmdId = 0;
        _readReqLen = 0;
        _barAccessForMsAfterSend = 0;
    }

    // Request type
    BusReqType _busReqType;

    // Address
    uint32_t _address;

    // CmdId
    uint32_t _cmdId;

    // Write data
    std::vector<uint8_t> _writeData;

    // Read data
    uint32_t _readReqLen;

    // Elem name
    String _elemName;

    // Data to include in callback
    void* _pCallbackData;

    // Callback
    BusRequestCallbackType _busReqCallback;

    // Polling
    float _pollFreqHz;

    // Bar access to element after request for a period
    uint32_t _barAccessForMsAfterSend;
};
