/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommsChannelMsg
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <vector>
#include <RdJson.h>
#include <stdint.h>

// #define IMPLEMENT_COMMS_MSG_JSON

static const uint32_t COMMS_MSG_UNNUMBERED_NUM = UINT32_MAX;

enum CommsMsgProtocol 
{
    MSG_PROTOCOL_ROSSERIAL,
    MSG_PROTOCOL_RESERVED_1,
    MSG_PROTOCOL_RICREST,
    MSG_PROTOCOL_RAWCMDFRAME = 0x3e,
    MSG_PROTOCOL_NONE = 0x3f,
};

enum CommsMsgTypeCode
{
    MSG_TYPE_COMMAND,
    MSG_TYPE_RESPONSE,
    MSG_TYPE_PUBLISH,
    MSG_TYPE_REPORT
};

static const uint32_t MSG_CHANNEL_ID_ALL = 10000;

class CommsChannelMsg
{
public:
    CommsChannelMsg()
    {
        _channelID = 0;
        _msgProtocol = MSG_PROTOCOL_NONE;
        _msgNum = COMMS_MSG_UNNUMBERED_NUM;
        _msgTypeCode = MSG_TYPE_REPORT;
    }

    CommsChannelMsg(uint32_t channelID, CommsMsgProtocol msgProtocol, uint32_t msgNum, CommsMsgTypeCode msgTypeCode)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
    }

    void clear()
    {
        _cmdVector.clear();
        _cmdVector.shrink_to_fit();
    }

    void setFromBuffer(uint32_t channelID, CommsMsgProtocol msgProtocol, uint32_t msgNum, CommsMsgTypeCode msgTypeCode, const uint8_t* pBuf, uint32_t bufLen)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
        _cmdVector.assign(pBuf, pBuf+bufLen);

#ifdef IMPLEMENT_COMMS_MSG_JSON
        setJSON();
#endif
    }

    void setFromBuffer(const uint8_t* pBuf, uint32_t bufLen)
    {
        _cmdVector.assign(pBuf, pBuf+bufLen);
    }

    void setBufferSize(uint32_t bufSize)
    {
        _cmdVector.resize(bufSize);
    }

    void setPartBuffer(uint32_t startPos, const uint8_t* pBuf, uint32_t len)
    {
        uint32_t reqSize = startPos + len;
        if (_cmdVector.size() < reqSize)
            _cmdVector.resize(reqSize);
        memcpy(_cmdVector.data() + startPos, pBuf, len);
    }

    void setProtocol(CommsMsgProtocol protocol)
    {
        _msgProtocol = protocol;
    }

    void setMsgTypeCode(CommsMsgTypeCode msgTypeCode)
    {
        _msgTypeCode = msgTypeCode;
    }

    void setAsResponse(const CommsChannelMsg& reqMsg)
    {
        _channelID = reqMsg._channelID;
        _msgProtocol = reqMsg._msgProtocol;
        _msgNum = reqMsg._msgNum;
        _msgTypeCode = MSG_TYPE_RESPONSE;
    }

    void setAsResponse(uint32_t channelID, CommsMsgProtocol msgProtocol, uint32_t msgNum, CommsMsgTypeCode msgTypeCode)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
    }

    CommsMsgProtocol getProtocol() const
    {
        return _msgProtocol;
    }

    CommsMsgTypeCode getMsgTypeCode()
    {
        return _msgTypeCode;
    }

    void setMsgNumber(uint32_t num)
    {
        _msgNum = num;
    }

    uint32_t getMsgNumber() const
    {
        return _msgNum;
    }

    void setChannelID(uint32_t channelID)
    {
        _channelID = channelID;
    }

    uint32_t getChannelID() const
    {
        return _channelID;
    }

    static const char* getProtocolAsString(CommsMsgProtocol msgProtocol)
    {
        switch(msgProtocol)
        {
            case MSG_PROTOCOL_ROSSERIAL: return "ROSSerial";
            case MSG_PROTOCOL_RESERVED_1: return "Reserved1";
            case MSG_PROTOCOL_RICREST: return "RICREST";
            default: return "None";
        }
    }

    static const char* getMsgTypeAsString(CommsMsgTypeCode msgTypeCode)
    {
        switch(msgTypeCode)
        {
            case MSG_TYPE_COMMAND: return "CMD";
            case MSG_TYPE_RESPONSE: return "RSP";
            case MSG_TYPE_PUBLISH: return "PUB";
            case MSG_TYPE_REPORT: return "REP";
            default:
                return "OTH";
        }
    }

#ifdef IMPLEMENT_COMMS_MSG_JSON
    String getProtocolStr()
    {
        return getString("p", getProtocolAsString(MSG_PROTOCOL_NONE));
    }
    void setJSON()
    {
        _cmdJSON = String("{\"p\":\"") + getProtocolAsString(_msgProtocol) + String("\",") +
                    String("\"d\":\"") + getMsgTypeAsString(_msgTypeCode) + String("\",") +
                    String("\"n\":\"") + String(_msgNum) + String("\"}");
    }
    String getString(const char *dataPath, const char *defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue, _cmdJSON.c_str());
    }
    String getString(const char *dataPath, const String& defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue.c_str(), _cmdJSON.c_str());
    }

    long getLong(const char *dataPath, long defaultValue)
    {
        return RdJson::getLong(dataPath, defaultValue, _cmdJSON.c_str());
    }

    double getDouble(const char *dataPath, double defaultValue)
    {
        return RdJson::getDouble(dataPath, defaultValue, _cmdJSON.c_str());
    }
#endif

    // Access to command vector
    const uint8_t* getBuf()
    {
        return _cmdVector.data();
    }
    uint32_t getBufLen()
    {
        return _cmdVector.size();
    }
    std::vector<uint8_t>& getCmdVector()
    {
        return _cmdVector;
    }

private:
    uint32_t _channelID;
    CommsMsgProtocol _msgProtocol;
    uint32_t _msgNum;
    CommsMsgTypeCode _msgTypeCode;
    std::vector<uint8_t> _cmdVector;
#ifdef IMPLEMENT_COMMS_MSG_JSON
    String _cmdJSON;
#endif
};
