/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stream Datagram Protocol
// Used for streaming audio, etc
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "StreamDatagramProtocol.h"
#include <Logger.h>
#include <RaftUtils.h>
#include <RICRESTMsg.h>
#include <FileStreamBlock.h>

// Debug
// #define DEBUG_STREAM_DATAGRAM_PROTOCOL
// #define DEBUG_STREAM_DATAGRAM_PROTOCOL_DETAIL

// Log prefix
#if defined(DEBUG_STREAM_DATAGRAM_PROTOCOL) || defined(DEBUG_STREAM_DATAGRAM_PROTOCOL_DETAIL)
static const char *MODULE_PREFIX = "StreamDatagram";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StreamDatagramProtocol::StreamDatagramProtocol(FileStreamBlockCB fileRxBlockCB, 
            FileStreamCanceEndCB fileRxCancelEndCB,
            CommsCoreIF* pCommsCore,
            FileStreamBase::FileStreamContentType fileStreamContentType, 
            FileStreamBase::FileStreamFlowType fileStreamFlowType,
            uint32_t streamID,
            uint32_t fileStreamLength,
            const char* fileStreamName) :
    FileStreamBase(fileRxBlockCB, fileRxCancelEndCB, pCommsCore, fileStreamContentType, 
            fileStreamFlowType, streamID, fileStreamLength, fileStreamName)
{
    _streamPos = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StreamDatagramProtocol::service()
{
    // Nothing to do
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode StreamDatagramProtocol::handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg)
{
    // Response
    char extraJson[100];
    snprintf(extraJson, sizeof(extraJson), R"("streamID":%d)", _streamID);
    Raft::setJsonResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, nullptr, extraJson);

    // Debug
#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL
    LOG_I(MODULE_PREFIX, "handleCmdFrame %s resp %s", cmdName.c_str(), respMsg.c_str());
#endif
    return UtilsRetCode::OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle data frame (file/stream block)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode StreamDatagramProtocol::handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    // Check valid CB
    if (!_fileStreamRxBlockCB)
        return UtilsRetCode::INVALID_OBJECT;

    // Handle the upload block
    uint32_t filePos = ricRESTReqMsg.getBufferPos();
    const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
    uint32_t bufferLen = ricRESTReqMsg.getBinLen();
    uint32_t streamID = ricRESTReqMsg.getStreamID();

#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL
    LOG_I(MODULE_PREFIX, "handleDataFrame %s dataLen %d msgPos %d expectedPos %d streamID %d", 
            _streamPos == filePos ? "OK" : "BAD STREAM POS",
            bufferLen, filePos, _streamPos, streamID);
#endif
#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL_DETAIL
    String debugStr;
    Raft::getHexStrFromBytes(pBuffer, 
                bufferLen  < MAX_DEBUG_BIN_HEX_LEN ? bufferLen : MAX_DEBUG_BIN_HEX_LEN, debugStr);
    LOG_I(MODULE_PREFIX, "handleDataFrame %s%s", debugStr.c_str(),
                bufferLen < MAX_DEBUG_BIN_HEX_LEN ? "" : "...");

#endif

    // Process the frame
    UtilsRetCode::RetCode rslt = UtilsRetCode::POS_MISMATCH;
    bool isFinalBlock = (_fileStreamLength != 0) && (filePos + bufferLen >= _fileStreamLength);

    // Check position
    if (_streamPos == filePos)
    {
        FileStreamBlock fileStreamBlock(_fileStreamName.c_str(), 
                            _fileStreamLength, 
                            filePos, 
                            pBuffer, 
                            bufferLen, 
                            isFinalBlock,
                            0,
                            false,
                            _fileStreamLength,
                            _fileStreamLength != 0,
                            filePos == 0
                            );

        // Call the callback
        rslt = _fileStreamRxBlockCB(fileStreamBlock);
    }

    // Check ok
    if (rslt == UtilsRetCode::OK)
    {
        // Update stream position
        _streamPos = filePos + bufferLen;

        // Check end of stream
        if (isFinalBlock)
        {
            // Send a SOKTO to indicate the end received
            char ackJson[100];
            snprintf(ackJson, sizeof(ackJson), "\"streamID\":%d,\"sokto\":%d", streamID, _streamPos);
            Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);
#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL
            LOG_I(MODULE_PREFIX, "handleDataFrame: ENDRX streamID %d, streamPos %d, sokto %d", streamID, _streamPos, _streamPos);
#endif
        }
    }
    else if ((rslt == UtilsRetCode::BUSY) || (rslt == UtilsRetCode::POS_MISMATCH))
    {
        // Send a SOKTO which indicates where the stream was received up to (so we can resend)
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"streamID\":%d,\"sokto\":%d,\"reason\":\"%s\"", streamID, _streamPos,
                                UtilsRetCode::getRetcStr(rslt));
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);
#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL
        LOG_I(MODULE_PREFIX, "handleDataFrame: %s streamID %d streamPos %d sokto %d retc %s", 
                    rslt == UtilsRetCode::BUSY ? "BUSY" : "POS_MISMATCH", streamID, _streamPos, _streamPos,
                    UtilsRetCode::getRetcStr(rslt));
#endif
    }
    else
    {
        // Failure of the stream
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "\"streamID\":%d,\"reason\":\"%s\"", streamID, UtilsRetCode::getRetcStr(rslt));
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, errorMsg);
#ifdef DEBUG_STREAM_DATAGRAM_PROTOCOL
        LOG_I(MODULE_PREFIX, "handleDataFrame: FAIL streamID %d streamPos %d sokto %d retc %s", streamID, _streamPos, _streamPos,
                                UtilsRetCode::getRetcStr(rslt));
#endif
    }

    // Result
    return rslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get debug str
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String StreamDatagramProtocol::getDebugJSON(bool includeBraces)
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Is active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StreamDatagramProtocol::isActive()
{
    return true;
}
