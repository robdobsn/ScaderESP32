/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RICRESTMsg
// Message encapsulation for REST message
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RICRESTMsg.h"
#include <RaftUtils.h>
#include <CommsChannelMsg.h>
#include <stdio.h>

// #define DEBUG_RICREST_MSG

// Logging
#ifdef DEBUG_RICREST_MSG
static const char* MODULE_PREFIX = "RICRESTMsg";
#endif

bool RICRESTMsg::decode(const uint8_t* pBuf, uint32_t len)
{
    // Debug
#ifdef DEBUG_RICREST_MSG
    String decodeMsgHex;
    Raft::getHexStrFromBytes(pBuf, len, decodeMsgHex);
    LOG_I(MODULE_PREFIX, "decode payloadLen %d payload %s", len, decodeMsgHex.c_str());
#endif

    // Check there is a RESTElementCode
    if (len <= RICREST_ELEM_CODE_POS)
        return false;

    // Extract RESTElementCode
    _RICRESTElemCode = (RICRESTElemCode)pBuf[RICREST_ELEM_CODE_POS];

#ifdef DEBUG_RICREST_MSG
    LOG_I(MODULE_PREFIX, "decode elemCode %d", _RICRESTElemCode);
#endif

    switch(_RICRESTElemCode)
    {
        case RICREST_ELEM_CODE_URL:
        {
            // Set request
            getStringFromBuf(_req, pBuf, RICREST_HEADER_PAYLOAD_POS, len, RICREST_MAX_PAYLOAD_LEN);

            // Debug
#ifdef DEBUG_RICREST_MSG
            LOG_I(MODULE_PREFIX, "decode URL req %s", _req.c_str());
#endif
            break;
        }
        case RICREST_ELEM_CODE_CMDRESPJSON:
        {
            getStringFromBuf(_payloadJson, pBuf, RICREST_HEADER_PAYLOAD_POS, len, RICREST_MAX_PAYLOAD_LEN);
#ifdef DEBUG_RICREST_MSG
            String debugStr;
            Raft::getHexStrFromBytes(pBuf, len, debugStr);
            LOG_I(MODULE_PREFIX, "decode CMDRESPJSON data %s len %d string form %s", debugStr.c_str(), len, _payloadJson.c_str());
#endif
            _req = "resp";
            break;
        }
        case RICREST_ELEM_CODE_BODY:
        {
            // Extract params
            const uint8_t* pData = pBuf + RICREST_BODY_BUFFER_POS;
            const uint8_t* pEndStop = pBuf + len;
            _bufferPos = Raft::getBEUint32AndInc(pData, pEndStop);
            _totalBytes = Raft::getBEUint32AndInc(pData, pEndStop);
            if (_totalBytes > MAX_REST_BODY_SIZE)
                _totalBytes = MAX_REST_BODY_SIZE;
            if (_bufferPos > _totalBytes)
                _bufferPos = 0;
            if (pData < pEndStop)
            {
                _pBinaryData = pData;
                _binaryLen = pEndStop-pData;
            }
            _req = "elemBody";
            break;
        }
        case RICREST_ELEM_CODE_COMMAND_FRAME:
        {
            // Find any null-terminator within len
            int terminatorFoundIdx = -1;
            for (uint32_t i = RICREST_COMMAND_FRAME_PAYLOAD_POS; i < len; i++)
            {
                if (pBuf[i] == 0)
                {
                    terminatorFoundIdx = i;
                    break;
                }
            }

            // Extract json
            getStringFromBuf(_payloadJson, pBuf, RICREST_COMMAND_FRAME_PAYLOAD_POS, 
                    terminatorFoundIdx < 0 ? len : terminatorFoundIdx, 
                    RICREST_MAX_PAYLOAD_LEN);

            // Check for any binary element
            if (terminatorFoundIdx >= 0)
            {
                if (len > terminatorFoundIdx)
                {
                    _pBinaryData = pBuf + terminatorFoundIdx + 1;
                    _binaryLen = len - terminatorFoundIdx - 1;
                }
            }

#ifdef DEBUG_RICREST_MSG
            LOG_I(MODULE_PREFIX, "RICREST_CMD_FRAME json %s binaryLen %d", _payloadJson.c_str(), _binaryLen);
#endif
            _req = RdJson::getString("cmdName", "unknown", _payloadJson.c_str());          
            break;
        }
        case RICREST_ELEM_CODE_FILEBLOCK:
        {
            // Extract params
            const uint8_t* pData = pBuf + RICREST_FILEBLOCK_FILE_POS;
            const uint8_t* pEndStop = pBuf + len;
            uint32_t streamIDAndBufferPos = Raft::getBEUint32AndInc(pData, pEndStop);
            _bufferPos = streamIDAndBufferPos & 0xffffff;
            _streamID = streamIDAndBufferPos >> 24;
            if (pData < pEndStop)
            {
                _pBinaryData = pData;
                _binaryLen = pEndStop-pData;
            }
            _req = "ufBlock";
            break;
        }
        default:
            return false;
    }
    return true;
}

void RICRESTMsg::encode(const String& payload, CommsChannelMsg& endpointMsg, RICRESTElemCode elemCode)
{
    // Setup buffer for the RESTElementCode
    uint8_t msgPrefixBuf[RICREST_HEADER_PAYLOAD_POS];
    msgPrefixBuf[RICREST_ELEM_CODE_POS] = elemCode;

    // Set the response ensuring to include the string terminator
    endpointMsg.setBufferSize(RICREST_HEADER_PAYLOAD_POS + payload.length() + 1);
    endpointMsg.setPartBuffer(RICREST_ELEM_CODE_POS, msgPrefixBuf, sizeof(msgPrefixBuf));
    endpointMsg.setPartBuffer(RICREST_HEADER_PAYLOAD_POS, (uint8_t*)payload.c_str(), payload.length() + 1);
}

void RICRESTMsg::encode(const uint8_t* pBuf, uint32_t len, CommsChannelMsg& endpointMsg, RICRESTElemCode elemCode)
{
    // Setup buffer for the RESTElementCode
    uint8_t msgPrefixBuf[RICREST_HEADER_PAYLOAD_POS];
    msgPrefixBuf[RICREST_ELEM_CODE_POS] = elemCode;

    // Set the response
    endpointMsg.setBufferSize(RICREST_HEADER_PAYLOAD_POS + len);
    endpointMsg.setPartBuffer(RICREST_ELEM_CODE_POS, msgPrefixBuf, sizeof(msgPrefixBuf));
    endpointMsg.setPartBuffer(RICREST_HEADER_PAYLOAD_POS, pBuf, len);
}
