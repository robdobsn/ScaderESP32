/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolExchange
// Hub for handling protocol endpoint messages
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "ProtocolExchange.h"
#include "CommsChannelMsg.h"
#include <RestAPIEndpointManager.h>
#include <ProtocolRICSerial.h>
#include <ProtocolRICFrame.h>
#include <ProtocolRICJSON.h>
#include <RICRESTMsg.h>
#include <SysManager.h>
#include <JSONParams.h>

static const char* MODULE_PREFIX = "ProtExchg";

// Warn
// #define WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
#define WARN_ON_FILE_UPLOAD_FAILED

// Debug
// #define DEBUG_RICREST_MESSAGES
// #define DEBUG_RICREST_MESSAGES_RESPONSE
// #define DEBUG_ENDPOINT_MESSAGES
// #define DEBUG_ENDPOINT_MESSAGES_DETAIL
// #define DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
// #define DEBUG_FILE_STREAM_SESSIONS
// #define DEBUG_RAW_CMD_FRAME

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolExchange::ProtocolExchange(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Handlers
    _pFirmwareUpdater = nullptr;
    _nextStreamID = FileStreamBase::FILE_STREAM_ID_MIN;
    _sysManStateIndWasActive = false;
}

ProtocolExchange::~ProtocolExchange()
{
    // Remove sessions
    for (FileStreamSession* pSession : _sessions)
    {
        delete pSession;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolExchange::service()
{
    // Service active sessions

    bool isMainFWUpdate = false;
    bool isFileSystemActivity = false;
    bool isStreaming = false;
    for (FileStreamSession* pSession : _sessions)
    {
        // Service the session
        pSession->service();

        // Update status
        isMainFWUpdate = pSession->isMainFWUpdate();
        isFileSystemActivity = pSession->isFileSystemActivity();
        isStreaming = pSession->isStreaming();

        // Check if the session is inactive
        if (!pSession->isActive())
        {
#ifdef DEBUG_FILE_STREAM_SESSIONS
            LOG_I(MODULE_PREFIX, "service session inactive name %s channel %d streamID %d",
                        pSession->getFileStreamName().c_str(), pSession->getChannelID(), pSession->getStreamID());
#endif            
            // Tidy up inactive session
            _sessions.remove(pSession);
            delete pSession;

            // Must break here so list iteration isn't hampered
            break;
        }
    }

    // Inform SysManager of changes in file/stream and firmware update activity
    bool isActive = isMainFWUpdate || isFileSystemActivity || isStreaming;
    if (_sysManStateIndWasActive != isActive)
    {
        if (getSysManager())
            getSysManager()->informOfFileStreamActivity(isMainFWUpdate, isFileSystemActivity, isStreaming);
        _sysManStateIndWasActive = isActive;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get info JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ProtocolExchange::getDebugJSON()
{
    String jsonStr;
    for (FileStreamSession* pSession : _sessions)
    {
        if (jsonStr.length() != 0)
            jsonStr += ",";
        jsonStr += pSession->getDebugJSON();
    }
    return "[" + jsonStr + "]";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms Channels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolExchange::addCommsChannels(CommsCoreIF& commsCore)
{
    // Add support for RICSerial
    LOG_I(MODULE_PREFIX, "addCommsChannels - adding RICSerial");
    ProtocolCodecFactoryHelper ricSerialProtocolDef = { ProtocolRICSerial::getProtocolNameStatic(), 
                        ProtocolRICSerial::createInstance, 
                        configGetConfig(), "RICSerial",
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1),
                        std::bind(&ProtocolExchange::canProcessEndpointMsg, this) };
    commsCore.addProtocol(ricSerialProtocolDef);

    // Add support for RICFrame
    LOG_I(MODULE_PREFIX, "addCommsChannels - adding RICFrame");
    ProtocolCodecFactoryHelper ricFrameProtocolDef = { ProtocolRICFrame::getProtocolNameStatic(), 
                        ProtocolRICFrame::createInstance, 
                        configGetConfig(), "RICFrame",
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1),
                        std::bind(&ProtocolExchange::canProcessEndpointMsg, this) };
    commsCore.addProtocol(ricFrameProtocolDef);

    // Add support for RICJSON
    LOG_I(MODULE_PREFIX, "addCommsChannels - adding RICJSON");
    ProtocolCodecFactoryHelper ricJSONProtocolDef = { ProtocolRICJSON::getProtocolNameStatic(), 
                        ProtocolRICJSON::createInstance, 
                        configGetConfig(), "RICJSON",
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1),
                        std::bind(&ProtocolExchange::canProcessEndpointMsg, this) };
    commsCore.addProtocol(ricJSONProtocolDef);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if we can process endpoint message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::canProcessEndpointMsg()
{
    // Check if we can process the message 
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process endpoint message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processEndpointMsg(CommsChannelMsg &cmdMsg)
{
    // Handle the command message
    CommsMsgProtocol protocol = cmdMsg.getProtocol();

#ifdef WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
    uint32_t msgProcStartTimeMs = millis();
#endif

#ifdef DEBUG_ENDPOINT_MESSAGES
    // Debug
    LOG_I(MODULE_PREFIX, "processEndpointMsg protocol %s msgNum %s msgType %s len %d", 
    		CommsChannelMsg::getProtocolAsString(protocol), 
    		cmdMsg.getMsgNumber() == 0 ? "Unnumbered" : String(cmdMsg.getMsgNumber()).c_str(),
            CommsChannelMsg::getMsgTypeAsString(cmdMsg.getMsgTypeCode()),
    		cmdMsg.getBufLen());
#endif

    // Handle ROSSerial
    if (protocol == MSG_PROTOCOL_ROSSERIAL)
    {
        // Not implemented as ROSSERIAL is unused in this direction
    }
    else if (protocol == MSG_PROTOCOL_RICREST)
    {
        // Extract request msg
        RICRESTMsg ricRESTReqMsg;
        ricRESTReqMsg.decode(cmdMsg.getBuf(), cmdMsg.getBufLen());

#ifdef DEBUG_RICREST_MESSAGES
        LOG_I(MODULE_PREFIX, "processEndpointMsg RICREST elemCode %s", RICRESTMsg::getRICRESTElemCodeStr(ricRESTReqMsg.getElemCode()));
#endif
#ifdef DEBUG_ENDPOINT_MESSAGES_DETAIL
        // Debug
        String debugStr;
        if (ricRESTReqMsg.getElemCode() != RICRESTMsg::RICREST_ELEM_CODE_FILEBLOCK)
            Raft::strFromBuffer(cmdMsg.getBuf(), cmdMsg.getBufLen(), debugStr);
        else
            Raft::getHexStrFromBytes(cmdMsg.getBuf(), cmdMsg.getBufLen(), debugStr);
        LOG_I(MODULE_PREFIX, "processEndpointMsg payload %s", debugStr.c_str());
#endif

        // Check elemCode of message
        String respMsg;
        switch(ricRESTReqMsg.getElemCode())
        {
            case RICRESTMsg::RICREST_ELEM_CODE_URL:
            {
                processRICRESTURL(ricRESTReqMsg, respMsg, APISourceInfo(cmdMsg.getChannelID()));
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_BODY:
            {
                processRICRESTBody(ricRESTReqMsg, respMsg, APISourceInfo(cmdMsg.getChannelID()));
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON:
            {
                // This message type is reserved for responses
                LOG_W(MODULE_PREFIX, "processEndpointMsg RICREST JSON reserved for response");
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_COMMAND_FRAME:
            {
                processRICRESTCmdFrame(ricRESTReqMsg, respMsg, cmdMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_FILEBLOCK:
            {
                processRICRESTFileStreamBlock(ricRESTReqMsg, respMsg, cmdMsg);
                break;
            }
        }

        // Check for response
        if (respMsg.length() != 0)
        {
            // Send the response back
            CommsChannelMsg endpointMsg;
            RICRESTMsg::encode(respMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
            endpointMsg.setAsResponse(cmdMsg);

            // Send message on the appropriate channel
            if (getCommsCore())
                getCommsCore()->handleOutboundMessage(endpointMsg);

            // Debug
#ifdef DEBUG_RICREST_MESSAGES_RESPONSE
            LOG_I(MODULE_PREFIX, "processEndpointMsg restAPI msgLen %d response %s", 
                        ricRESTReqMsg.getBinLen(), endpointMsg.getBuf());
#endif
        }
    }
    else if (protocol == MSG_PROTOCOL_RAWCMDFRAME)
    {
        String cmdMsgStr;
        Raft::strFromBuffer(cmdMsg.getBuf(), cmdMsg.getBufLen(), cmdMsgStr);
        JSONParams cmdFrame = cmdMsgStr;
        String reqStr = cmdFrame.getString("cmdName", "");
        String queryStr = RdJson::getHTMLQueryFromJSON(cmdMsgStr);
        if (queryStr.length() > 0)
            reqStr += "?" + queryStr;

#ifdef DEBUG_RAW_CMD_FRAME
        LOG_I(MODULE_PREFIX, "processEndpointMsg rawCmdFrame %s", reqStr.c_str());
#endif

        // Handle via standard REST API
        String respMsg;
        if (getRestAPIEndpointManager())
            getRestAPIEndpointManager()->handleApiRequest(reqStr.c_str(), respMsg, APISourceInfo(cmdMsg.getChannelID()));
    }

#ifdef WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
    if (Raft::isTimeout(millis(), msgProcStartTimeMs, MSG_PROC_SLOW_PROC_THRESH_MS))
    {
#ifdef DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
        String msgHex;
        Raft::getHexStrFromBytes(cmdMsg.getBuf(), cmdMsg.getBufLen(), msgHex);
#endif
        LOG_W(MODULE_PREFIX, "processEndpointMsg SLOW took %ldms protocol %d len %d msg %s", 
                Raft::timeElapsed(millis(), msgProcStartTimeMs),
                protocol,
                cmdMsg.getBufLen(),
#ifdef DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
                msgHex.c_str()
#else
                ""
#endif
                );
    }
#endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo)
{
    // Handle via standard REST API
    if (getRestAPIEndpointManager())
        return getRestAPIEndpointManager()->handleApiRequest(ricRESTReqMsg.getReq().c_str(), respMsg, sourceInfo);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo)
{
    // NOTE - implements POST for RICREST - not currently needed

//     // Handle the body
//     String reqStr;
//     uint32_t bufferPos = ricRESTReqMsg.getBufferPos();
//     const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
//     uint32_t bufferLen = ricRESTReqMsg.getBinLen();
//     uint32_t totalBytes = ricRESTReqMsg.getTotalBytes();
//     bool rsltOk = false;
//     if (pBuffer && _pRestAPIEndpointManager)
//     {
//         _pRestAPIEndpointManager->handleApiRequestBody(reqStr, pBuffer, bufferLen, bufferPos, totalBytes, sourceInfo);
//         rsltOk = true;
//     }

//     // Response
//     Raft::setJsonBoolResult(pReqStr, respMsg, rsltOk);

//     // Debug
// // #ifdef DEBUG_RICREST_MESSAGES
//     LOG_I(MODULE_PREFIX, "addCommand restBody binBufLen %d bufferPos %d totalBytes %d", 
//                 bufferLen, bufferPos, totalBytes);
// // #endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode ProtocolExchange::processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg)
{
    // ChannelID
    uint32_t channelID = endpointMsg.getChannelID();

    // Check file/stream messages
    String fileStreamName;
    FileStreamBase::FileStreamContentType fileStreamContentType = FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE;
    String cmdName;
    String restAPIEndpointName;
    uint32_t streamID = FileStreamBase::FILE_STREAM_ID_ANY;
    uint32_t fileStreamLength = 0;
    FileStreamBase::FileStreamMsgType fileStreamMsgType = 
                    FileStreamBase::getFileStreamMsgInfo(ricRESTReqMsg, cmdName, fileStreamName, 
                            fileStreamContentType, streamID, restAPIEndpointName, fileStreamLength);

    // Handle non-file-stream messages
    if (fileStreamMsgType == FileStreamBase::FILE_STREAM_MSG_TYPE_NONE)
        return processRICRESTNonFileStream(cmdName, ricRESTReqMsg, respMsg, endpointMsg) ? UtilsRetCode::OK : UtilsRetCode::INVALID_OBJECT;

    // Handle file stream
    FileStreamSession* pSession = nullptr;
    switch (fileStreamMsgType)
    {
        case FileStreamBase::FILE_STREAM_MSG_TYPE_START:
            pSession = getFileStreamNewSession(fileStreamName.c_str(), channelID, fileStreamContentType, 
                                    restAPIEndpointName.c_str(), FileStreamBase::FILE_STREAM_FLOW_TYPE_COMMS_CHANNEL,
                                    fileStreamLength);
            break;
        case FileStreamBase::FILE_STREAM_MSG_TYPE_END:
            pSession = getFileStreamExistingSession(fileStreamName.c_str(), channelID, streamID);
            if (!pSession)
            {
                // Streams can end before ufEnd received so simply ack the message
                Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true);
                return UtilsRetCode::SESSION_NOT_FOUND;
            }
            break;
        default:
            LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame cmdName %s fileStreamMsgType %d", cmdName.c_str(), fileStreamMsgType);
            pSession = getFileStreamExistingSession(fileStreamName.c_str(), channelID, streamID);
            break;
    }

    // Check session is valid
    if (!pSession)
    {
        // Failure
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false);
        return UtilsRetCode::SESSION_NOT_FOUND;
    }

    // Session is valid so send message to it
    return pSession->handleCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg file/stream block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode ProtocolExchange::processRICRESTFileStreamBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg, CommsChannelMsg &cmdMsg)
{
    // Extract streamID
    uint32_t streamID = ricRESTReqMsg.getStreamID();

    // Find corresponding session
    FileStreamSession* pSession = findFileStreamSession(streamID, nullptr, cmdMsg.getChannelID());
    if (!pSession)
    {
        LOG_W(MODULE_PREFIX, "processRICRESTFileStreamBlock session not found for streamID %d", streamID);
        UtilsRetCode::RetCode rslt = UtilsRetCode::SESSION_NOT_FOUND;
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "\"streamID\":%d,\"reason\":\"%s\"", streamID, UtilsRetCode::getRetcStr(rslt));
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, errorMsg);
        return rslt;
    }

    // Handle message
    return pSession->handleDataFrame(ricRESTReqMsg, respMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame that are non-file-stream messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTNonFileStream(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg)
{
    // Convert to REST API query string - it won't necessarily be a valid query string for external use
    // but works fine for internal use
    String reqStr = cmdName;
    String queryStr = RdJson::getHTMLQueryFromJSON(ricRESTReqMsg.getPayloadJson());
    if (queryStr.length() > 0)
        reqStr += "?" + queryStr;

    // Handle via standard REST API
    if (getRestAPIEndpointManager())
        return getRestAPIEndpointManager()->handleApiRequest(reqStr.c_str(), respMsg, APISourceInfo(endpointMsg.getChannelID()));
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame that are non-file-stream messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::findFileStreamSession(uint32_t streamID, const char* fileStreamName, uint32_t channelID)
{
    // First check the case where we know the streamID
    if (streamID != FileStreamBase::FILE_STREAM_ID_ANY)
    {
        for (FileStreamSession* pSession : _sessions)
        {
            if (pSession->getStreamID() == streamID)
                return pSession;
        }
        return nullptr;
    }

    // Check for matching filename and channel
    for (FileStreamSession* pSession : _sessions)
    {
        if (pSession && 
                ((!fileStreamName || pSession->getFileStreamName().equals(fileStreamName))) &&
                (pSession->getChannelID() == channelID))
        {
            return pSession;
        }
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream start condition
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::getFileStreamNewSession(const char* fileStreamName, uint32_t channelID, 
                FileStreamBase::FileStreamContentType fileStreamContentType, const char* restAPIEndpointName,
                FileStreamBase::FileStreamFlowType flowType, uint32_t fileStreamLength)
{
    // Check existing sessions
    FileStreamSession* pSession = findFileStreamSession(FileStreamBase::FILE_STREAM_ID_ANY, fileStreamName, channelID);
    if (pSession)
    {
        // If we find one then ignore this as it is a re-start of an existing session
        LOG_W(MODULE_PREFIX, "getFileStreamNewSession restart existing - ignored name %s channelID %d",
                        fileStreamName, channelID);
        return pSession;
    }

    // Check number of sessions
    if (_sessions.size() > MAX_SIMULTANEOUS_FILE_STREAM_SESSIONS)
    {
        // Max sessions already active
        LOG_W(MODULE_PREFIX, "getFileStreamNewSession max active - ignored name %s channelID %d",
                        fileStreamName, channelID);
        return nullptr;
    }

    // Create new session
    pSession = new FileStreamSession(fileStreamName, channelID, 
                getCommsCore(), _pFirmwareUpdater, 
                fileStreamContentType, flowType, _nextStreamID, 
                restAPIEndpointName, getRestAPIEndpointManager(),
                fileStreamLength);
    if (!pSession)
    {
        LOG_W(MODULE_PREFIX, "getFileStreamNewSession failed to create session name %s channelID %d endpointName %s",
                        fileStreamName, channelID, restAPIEndpointName);
        return nullptr;
    }

    // Debug
#ifdef DEBUG_FILE_STREAM_SESSIONS
    LOG_I(MODULE_PREFIX, "getFileStreamNewSession name %s channelID %d streamID %d streamType %s endpointName %s flowType %s fileStreamLength %d", 
                        fileStreamName, channelID, _nextStreamID, 
                        FileStreamBase::getFileStreamContentTypeStr(fileStreamContentType), 
                        restAPIEndpointName, FileStreamBase::getFileStreamFlowTypeStr(flowType),
                        fileStreamLength);
#endif

    // Add to session list
    _sessions.push_back(pSession);

    // Bump the stream ID
    _nextStreamID++;
    if (_nextStreamID >= FileStreamBase::FILE_STREAM_ID_MAX)
        _nextStreamID = FileStreamBase::FILE_STREAM_ID_MIN;

    // Session is valid so send message to it
    return pSession;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::getFileStreamExistingSession(const char* fileStreamName, uint32_t channelID, uint32_t streamID)
{
    // Find existing session
    FileStreamSession* pSession = findFileStreamSession(streamID, fileStreamName, channelID);
    if (!pSession)
    {
    // Debug
#ifdef DEBUG_FILE_STREAM_SESSIONS
        LOG_I(MODULE_PREFIX, "getFileStreamExistingSession NOT FOUND name %s channelID %d streamID %d", 
                fileStreamName, channelID, streamID);
#endif
        return nullptr;
    }
#ifdef DEBUG_FILE_STREAM_SESSIONS
    LOG_I(MODULE_PREFIX, "getFileStreamExistingSession OK name %s channelID %d streamID %d", 
            fileStreamName, channelID, streamID);
#endif
    return pSession;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file upload data block
// This function is only called from the FileManager when the fileupload API is used via HTTP
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode ProtocolExchange::handleFileUploadBlock(const String& req, FileStreamBlock& fileStreamBlock, 
            const APISourceInfo& sourceInfo, FileStreamBase::FileStreamContentType fileStreamContentType,
            const char* restAPIEndpointName)
{
    // See if this is the first block
    if (fileStreamBlock.firstBlock)
    {
        // Get a new session
        if (!getFileStreamNewSession(fileStreamBlock.filename, 
                        sourceInfo.channelID, fileStreamContentType, restAPIEndpointName,
                        FileStreamBase::FILE_STREAM_FLOW_TYPE_HTTP_UPLOAD,
                        fileStreamBlock.fileLenValid ? fileStreamBlock.fileLen : fileStreamBlock.contentLen))
            return UtilsRetCode::INSUFFICIENT_RESOURCE;
    }

    // Get the session
    FileStreamSession* pSession = getFileStreamExistingSession(fileStreamBlock.filename, sourceInfo.channelID, 
                    FileStreamBase::FILE_STREAM_ID_ANY);
    if (!pSession)
        return UtilsRetCode::SESSION_NOT_FOUND;
    
    // Handle the block
    return pSession->fileStreamBlockWrite(fileStreamBlock);
}

