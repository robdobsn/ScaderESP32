/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RICRESTMsg
// Message encapsulation for REST message
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ArduinoOrAlt.h>

static const uint32_t RICREST_ELEM_CODE_POS = 0;
static const uint32_t RICREST_HEADER_PAYLOAD_POS = 1;
static const uint32_t RICREST_HEADER_MIN_MSG_LEN = 4;
static const uint32_t RICREST_BODY_BUFFER_POS = 1;
static const uint32_t RICREST_BODY_TOTAL_POS = 5;
static const uint32_t RICREST_BODY_PAYLOAD_POS = 9;
static const uint32_t RICREST_COMMAND_FRAME_PAYLOAD_POS = 1;
static const uint32_t RICREST_FILEBLOCK_FILE_POS = 1;
static const uint32_t RICREST_FILEBLOCK_PAYLOAD_POS = 5;

// TODO - ensure this is long enough for all needs
static const uint32_t RICREST_MAX_PAYLOAD_LEN = 5000;

class CommsChannelMsg;

class RICRESTMsg
{
public:
    enum RICRESTElemCode
    {
        RICREST_ELEM_CODE_URL,
        RICREST_ELEM_CODE_CMDRESPJSON,
        RICREST_ELEM_CODE_BODY,
        RICREST_ELEM_CODE_COMMAND_FRAME,
        RICREST_ELEM_CODE_FILEBLOCK
    };

    static const char* getRICRESTElemCodeStr(RICRESTElemCode elemCode)
    {
        switch (elemCode)
        {
            case RICREST_ELEM_CODE_URL: return "URL";
            case RICREST_ELEM_CODE_CMDRESPJSON: return "CMDRESPJSON";
            case RICREST_ELEM_CODE_BODY: return "BODY";
            case RICREST_ELEM_CODE_COMMAND_FRAME: return "COMMAND_FRAME";
            case RICREST_ELEM_CODE_FILEBLOCK: return "FILEBLOCK";
            default: return "UNKNOWN";
        }
    }
    static const uint32_t MAX_REST_BODY_SIZE = 5000;
    RICRESTMsg()
    {
        _RICRESTElemCode = RICREST_ELEM_CODE_URL;
        _bufferPos = 0;
        _streamID = 0;
        _binaryLen = 0;
        _pBinaryData = NULL;
        _totalBytes = 0;
    }
    bool decode(const uint8_t* pBuf, uint32_t len);
    static void encode(const String& payload, CommsChannelMsg& endpointMsg, RICRESTElemCode elemCode);
    static void encode(const uint8_t* pBuf, uint32_t len, CommsChannelMsg& endpointMsg, RICRESTElemCode elemCode);

    const String& getReq() const
    {
        return _req;
    }
    const String& getPayloadJson() const
    {
        return _payloadJson;
    }
    const uint8_t* getBinBuf() const
    {
        return _pBinaryData;
    }
    uint32_t getBinLen() const
    {
        return _binaryLen;
    }
    uint32_t getBufferPos() const
    {
        return _bufferPos;
    }
    uint32_t getStreamID() const
    {
        return _streamID;
    }
    uint32_t getTotalBytes() const
    {
        return _totalBytes;
    }
    RICRESTElemCode getElemCode() const
    {
        return _RICRESTElemCode;
    }
    void setElemCode(RICRESTElemCode elemCode)
    {
        _RICRESTElemCode = elemCode;
    }

private:
    // Get string
    void getStringFromBuf(String& destStr, const uint8_t* pBuf, uint32_t offset, uint32_t len, uint32_t maxLen)
    {    
        // Ensure length is ok
        uint32_t lenToCopy = len-offset+1;
        if (lenToCopy > maxLen)
            lenToCopy = maxLen;
        destStr = "";
        if (lenToCopy < 1)
            return;
        // Set into string without allocating additional buffer space
        destStr.concat((const char*)(pBuf+offset), lenToCopy-1);
    }

    // RICRESTElemCode
    RICRESTElemCode _RICRESTElemCode;

    // Parameters
    String _req;
    String _payloadJson;
    uint32_t _bufferPos;
    uint32_t _binaryLen;
    uint32_t _totalBytes;
    uint32_t _streamID;
    const uint8_t* _pBinaryData;
};
