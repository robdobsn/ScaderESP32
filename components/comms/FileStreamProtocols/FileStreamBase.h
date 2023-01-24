/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File/Stream Protocol Base
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ArduinoOrAlt.h>
#include <UtilsRetCode.h>

class RICRESTMsg;
class CommsChannelMsg;
class CommsCoreIF;
class SysModBase;
class FileStreamBlock;

// File/stream callback function types
typedef std::function<UtilsRetCode::RetCode(FileStreamBlock& fileBlock)> FileStreamBlockCB;
typedef std::function<void(bool isNormalEnd)> FileStreamCanceEndCB;

class FileStreamBase
{
public:

    // File/Stream Content Type
    enum FileStreamContentType
    {
        FILE_STREAM_CONTENT_TYPE_FILE,
        FILE_STREAM_CONTENT_TYPE_FIRMWARE,
        FILE_STREAM_CONTENT_TYPE_RT_STREAM
    };

    // Get file/stream message type
    enum FileStreamMsgType
    {
        FILE_STREAM_MSG_TYPE_NONE,
        FILE_STREAM_MSG_TYPE_START,
        FILE_STREAM_MSG_TYPE_END,
        FILE_STREAM_MSG_TYPE_CANCEL
    };

    // File/Stream Flow Type
    enum FileStreamFlowType
    {
        FILE_STREAM_FLOW_TYPE_HTTP_UPLOAD,
        FILE_STREAM_FLOW_TYPE_COMMS_CHANNEL
    };

    // Constructor
    FileStreamBase(FileStreamBlockCB fileRxBlockCB, 
            FileStreamCanceEndCB fileRxCancelEndCB,
            CommsCoreIF* pCommsCoreIF,
            FileStreamBase::FileStreamContentType fileStreamContentType,
            FileStreamBase::FileStreamFlowType fileStreamFlowType,
            uint32_t streamID,
            uint32_t fileStreamLength,
            const char* fileStreamName);

    // Destructor
    virtual ~FileStreamBase();

    // Service file/stream
    virtual void service() = 0;

    // Handle command frame
    virtual UtilsRetCode::RetCode handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg) = 0;

    // Handle data frame (file/stream block)
    virtual UtilsRetCode::RetCode handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg) = 0;

    // Get debug str
    virtual String getDebugJSON(bool includeBraces) = 0;

    // Get file/stream message information from header
    static FileStreamMsgType getFileStreamMsgInfo(const RICRESTMsg& ricRESTReqMsg,
                String& cmdName, String& fileStreamName, 
                FileStreamContentType& fileStreamContentType, uint32_t& streamID,
                String& restAPIEndpointName, uint32_t& fileStreamLength);

    // File/stream message type string
    static const char* getFileStreamMsgTypeStr(FileStreamMsgType msgType)
    {
        switch (msgType)
        {
            case FILE_STREAM_MSG_TYPE_START: return "ufStart";
            case FILE_STREAM_MSG_TYPE_END: return "ufEnd";
            case FILE_STREAM_MSG_TYPE_CANCEL: return "ufCancel";
            default: return "unknown";
        }
    }

    // Content type string
    static const char* getFileStreamContentTypeStr(FileStreamContentType fileStreamContentType)
    {
        switch (fileStreamContentType)
        {
        case FILE_STREAM_CONTENT_TYPE_FILE: return "file";
        case FILE_STREAM_CONTENT_TYPE_FIRMWARE: return "firmware";
        case FILE_STREAM_CONTENT_TYPE_RT_STREAM: return "realTimeStream";
        default: return "unknown";
        }
    }

    // Get fileStreamContentType from string
    static FileStreamContentType getFileStreamContentType(const String& fileStreamTypeStr);

    // Get fileStreamFlowType string
    static const char* getFileStreamFlowTypeStr(FileStreamFlowType fileStreamFlowType)
    {
        switch (fileStreamFlowType)
        {
        case FILE_STREAM_FLOW_TYPE_HTTP_UPLOAD: return "httpUpload";
        case FILE_STREAM_FLOW_TYPE_COMMS_CHANNEL: return "commsChannel";
        default: return "unknown";
        }
    }

    // StreamID values
    static const uint32_t FILE_STREAM_ID_ANY = 0;
    static const uint32_t FILE_STREAM_ID_MIN = 1;
    static const uint32_t FILE_STREAM_ID_MAX = 255;

    // Get streamID
    virtual uint32_t getStreamID()
    {
        return _streamID;
    }

    // Is active
    virtual bool isActive() = 0;

protected:
    // Callbacks
    FileStreamBlockCB _fileStreamRxBlockCB;
    FileStreamCanceEndCB _fileStreamRxCancelEndCB;

    // Comms core
    CommsCoreIF* _pCommsCore;

    // File/stream content type
    FileStreamContentType _fileStreamContentType;

    // File/stream flow type
    FileStreamFlowType _fileStreamFlowType;

    // StreamID
    uint32_t _streamID;

    // File/stream length
    uint32_t _fileStreamLength;

    // Name of file/stream
    String _fileStreamName;
};