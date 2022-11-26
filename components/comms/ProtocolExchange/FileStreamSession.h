/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Stream Session
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include "FileStreamBlock.h"
#include "FileSystemChunker.h"
#include "FileStreamBase.h"
#include "RestAPIEndpoint.h"

class RestAPIEndpointManager;

class FileStreamSession
{
public:
    // Constructor / destructor
    FileStreamSession(const String& filename, uint32_t channelID,
                CommsChannelManager* pCommsChannelManager, SysModBase* pFirmwareUpdater,
                FileStreamBase::FileStreamContentType fileStreamContentType, 
                FileStreamBase::FileStreamFlowType fileStreamFlowType,
                uint32_t streamID, const char* restAPIEndpointName,
                RestAPIEndpointManager* pRestAPIEndpointManager,
                uint32_t fileStreamLength);
    virtual ~FileStreamSession();

    // Info
    bool isActive()
    {
        return _isActive;
    }
    const String& getFileStreamName()
    {
        return _fileStreamName;
    }
    uint32_t getChannelID()
    {
        return _channelID;
    }
    uint32_t getStreamID()
    {
        if (_pFileStreamProtocolHandler)
            return _pFileStreamProtocolHandler->getStreamID();
        return FileStreamBase::FILE_STREAM_ID_ANY;
    }
    bool isMainFWUpdate()
    {
        return _fileStreamContentType == FileStreamBase::FILE_STREAM_CONTENT_TYPE_FIRMWARE;
    }
    bool isFileSystemActivity()
    {
        return _fileStreamContentType == FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE;
    }
    bool isStreaming()
    {
        return _fileStreamContentType == FileStreamBase::FILE_STREAM_CONTENT_TYPE_RT_STREAM;
    }
    bool isUpload()
    {
        return true;
    }
    void service();

    // Handle command frame
    UtilsRetCode::RetCode handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg);

    // Handle file/stream block message
    UtilsRetCode::RetCode handleDataFrame(RICRESTMsg& RICRESTReqMsg, String& respMsg);

    // Handle file upload block and cancel
    UtilsRetCode::RetCode fileStreamBlockWrite(FileStreamBlock& fileStreamBlock);
    void fileStreamCancelEnd(bool isNormalEnd);

    // Debug
    String getDebugJSON();
    
private:
    // Is active
    bool _isActive = false;

    // File/stream name and content type
    String _fileStreamName;
    FileStreamBase::FileStreamContentType _fileStreamContentType;

    // File/stream flow type
    FileStreamBase::FileStreamFlowType _fileStreamFlowType;

    // Endpoint name
    String _restAPIEndpointName;
    
    // REST API endpoint manager
    RestAPIEndpointManager* _pRestAPIEndpointManager;

    // Callbacks to stream endpoint
    RestAPIFnChunk _pStreamChunkCB;
    RestAPIFnIsReady _pStreamIsReadyCB;

    // Stream info
    String _streamRequestStr;
    APISourceInfo _streamSourceInfo;

    // Channel ID
    uint32_t _channelID;

    // Protocol handler
    FileStreamBase* _pFileStreamProtocolHandler = nullptr;

    // Chunker
    FileSystemChunker* _pFileChunker;

    // Handlers
    SysModBase* _pFirmwareUpdater;

    // Session idle handler
    uint32_t _sessionLastActiveMs;
    static const uint32_t MAX_SESSION_IDLE_TIME_MS = 10000;

    // Stats
    uint32_t _startTimeMs;
    uint64_t _totalWriteTimeUs;
    uint32_t _totalBytes;
    uint32_t _totalChunks;

    // Helpers
    UtilsRetCode::RetCode writeFirmwareBlock(FileStreamBlock& fileStreamBlock);
    UtilsRetCode::RetCode writeFileBlock(FileStreamBlock& fileStreamBlock);
    UtilsRetCode::RetCode writeRealTimeStreamBlock(FileStreamBlock& fileStreamBlock);
};
