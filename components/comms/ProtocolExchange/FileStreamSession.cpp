/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Stream Session
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileStreamSession.h"
#include "SysModBase.h"
#include "FileUploadHTTPProtocol.h"
#include "FileUploadOKTOProtocol.h"
#include "StreamDatagramProtocol.h"
#include "RestAPIEndpointManager.h"
#include "RICRESTMsg.h"

static const char* MODULE_PREFIX = "FSSess";

class CommsChannelManager;

// Warn
#define WARN_ON_FW_UPDATE_FAILED
#define WARN_ON_STREAM_FAILED

// Debug
// #define DEBUG_FILE_STREAM_START_END
// #define DEBUG_FILE_STREAM_BLOCK

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession::FileStreamSession(const String& filename, uint32_t channelID,
                CommsChannelManager* pCommsChannelManager, SysModBase* pFirmwareUpdater,
                FileStreamBase::FileStreamContentType fileStreamContentType, 
                FileStreamBase::FileStreamFlowType fileStreamFlowType,
                uint32_t streamID, const char* restAPIEndpointName,
                RestAPIEndpointManager* pRestAPIEndpointManager,
                uint32_t fileStreamLength) :
                _streamSourceInfo(channelID)
{
    _pFileStreamProtocolHandler = nullptr;
    _isActive = true;
    _sessionLastActiveMs = millis();
    _fileStreamName = filename;
    _channelID = channelID;
    _fileStreamContentType = fileStreamContentType;
    _fileStreamFlowType = fileStreamFlowType;
    _startTimeMs = millis();
    _totalWriteTimeUs = 0;
    _totalBytes = 0;
    _totalChunks = 0;
    _pFileChunker = nullptr;
    _pFirmwareUpdater = pFirmwareUpdater;
    _restAPIEndpointName = restAPIEndpointName;
    _pRestAPIEndpointManager = pRestAPIEndpointManager;
    _pStreamChunkCB = nullptr;
    _pStreamIsReadyCB = nullptr;

#ifdef DEBUG_FILE_STREAM_START_END
    LOG_I(MODULE_PREFIX, "constructor filename %s channelID %d streamID %d endpointName %s", 
                _fileStreamName.c_str(), _channelID, streamID, restAPIEndpointName);
#endif

    // For file handling use a FileSystemChunker to access the file
    if (_fileStreamContentType == FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE)
    {
        _pFileChunker = new FileSystemChunker();
        if (_pFileChunker)
            _pFileChunker->start(filename, 0, false, true, true);
    }

    // Construct file/stream handling protocol
    switch(_fileStreamContentType)
    {
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE:
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_FIRMWARE:
        {
            if (_fileStreamFlowType == FileStreamBase::FILE_STREAM_FLOW_TYPE_HTTP_UPLOAD)
            {
                _pFileStreamProtocolHandler = new FileUploadHTTPProtocol(
                            std::bind(&FileStreamSession::fileStreamBlockWrite, this, std::placeholders::_1),
                            std::bind(&FileStreamSession::fileStreamCancelEnd, this, std::placeholders::_1),
                            pCommsChannelManager,
                            fileStreamContentType, 
                            fileStreamFlowType,
                            streamID,
                            fileStreamLength,
                            filename.c_str());
            }
            else
            {
                _pFileStreamProtocolHandler = new FileUploadOKTOProtocol(
                            std::bind(&FileStreamSession::fileStreamBlockWrite, this, std::placeholders::_1),
                            std::bind(&FileStreamSession::fileStreamCancelEnd, this, std::placeholders::_1),
                            pCommsChannelManager,
                            fileStreamContentType, 
                            fileStreamFlowType,
                            streamID,
                            fileStreamLength,
                            filename.c_str());
            }
            break;
        }
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_RT_STREAM:
        {
            // Create protocol handler
            _pFileStreamProtocolHandler = new StreamDatagramProtocol(
                        std::bind(&FileStreamSession::fileStreamBlockWrite, this, std::placeholders::_1),
                        std::bind(&FileStreamSession::fileStreamCancelEnd, this, std::placeholders::_1),
                        pCommsChannelManager,
                        fileStreamContentType, 
                        fileStreamFlowType,
                        streamID,
                        fileStreamLength,
                        filename.c_str());

            // Find endpoint
            RestAPIEndpoint* pRestAPIEndpoint = _pRestAPIEndpointManager->getEndpoint(_restAPIEndpointName.c_str());
            if (pRestAPIEndpoint && pRestAPIEndpoint->_callbackChunk)
            {
#ifdef DEBUG_FILE_STREAM_START_END
                LOG_I(MODULE_PREFIX, "constructor API %s filename %s channelID %d streamID %d endpointName %s", 
                            pRestAPIEndpoint->getEndpointName(), _fileStreamName.c_str(), _channelID, streamID, restAPIEndpointName);
#endif
                // Set endpoint callbacks
                _pStreamChunkCB = pRestAPIEndpoint->_callbackChunk;
                _pStreamIsReadyCB = pRestAPIEndpoint->_callbackIsReady;
            }
            else
            {
                // Not found
                _isActive = false;
            }
            break;
        }
        default:
        {
            // Not supported
            _isActive = false;
            break;
        }
    }
}

FileStreamSession::~FileStreamSession()
{
    // Tidy up
    delete _pFileChunker;
    delete _pFileStreamProtocolHandler;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileStreamSession::service()
{
    // Service file/stream protocol
    if (_pFileStreamProtocolHandler)
        _pFileStreamProtocolHandler->service();

    // Check for timeouts
    if (_isActive && Raft::isTimeout(millis(), _sessionLastActiveMs, MAX_SESSION_IDLE_TIME_MS))
    {
        _isActive = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
            const CommsChannelMsg &endpointMsg)
{
    if (!_pFileStreamProtocolHandler)
        return UtilsRetCode::INVALID_OBJECT;
    UtilsRetCode::RetCode rslt = _pFileStreamProtocolHandler->handleCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg);
    if (!_pFileStreamProtocolHandler->isActive())
        _isActive = false;
    return rslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream block message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    if (!_pFileStreamProtocolHandler)
    {
        UtilsRetCode::RetCode rslt = UtilsRetCode::INVALID_OBJECT;
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "\"reason\":\"%s\"", UtilsRetCode::getRetcStr(rslt));
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, errorMsg);
        return rslt;
    }
    return _pFileStreamProtocolHandler->handleDataFrame(ricRESTReqMsg, respMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getDebugJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileStreamSession::getDebugJSON()
{
    // Build JSON
    if (_pFileStreamProtocolHandler)
        return _pFileStreamProtocolHandler->getDebugJSON(true);
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File/stream block write - callback from FileStreamBase
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::fileStreamBlockWrite(FileStreamBlock& fileStreamBlock)
{
#ifdef DEBUG_FILE_STREAM_BLOCK
    LOG_I(MODULE_PREFIX, "fileStreamBlockWrite pos %d len %d%s fileStreamContentType %d isFirst %d isFinal %d name %s",
                fileStreamBlock.filePos, fileStreamBlock.blockLen, fileStreamBlock.fileLenValid ? (" of " + String(fileStreamBlock.fileLen) + " ").c_str() : "",
                _fileStreamContentType, fileStreamBlock.firstBlock, fileStreamBlock.finalBlock, fileStreamBlock.filename);
#endif

    // Keep session alive while we're receiving
    _sessionLastActiveMs = millis();

    // Handle file/stream types
    UtilsRetCode::RetCode handledOk = UtilsRetCode::INVALID_DATA;
    switch(_fileStreamContentType)
    {
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_FIRMWARE:
            handledOk = writeFirmwareBlock(fileStreamBlock);
            break;
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE:
            handledOk = writeFileBlock(fileStreamBlock);
            break;
        case FileStreamBase::FILE_STREAM_CONTENT_TYPE_RT_STREAM:
            handledOk = writeRealTimeStreamBlock(fileStreamBlock);
            break;
        default:
            _isActive = false;
            return UtilsRetCode::INVALID_DATA;
    }

    // Check handled ok
    if (handledOk == UtilsRetCode::OK)
    {
        // Check for first block
        if (fileStreamBlock.firstBlock)
            _startTimeMs = millis();

        // Check for final block
        if (fileStreamBlock.finalBlock)
            _isActive = false;

        // Update stats
        _totalChunks++;
    }
    else if (handledOk != UtilsRetCode::BUSY)
    {
        // Not handled ok
        _isActive = false;
    }
    return handledOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write firmware block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::writeFirmwareBlock(FileStreamBlock& fileStreamBlock)
{
    // Firmware updater valid?
    if (!_pFirmwareUpdater)
        return UtilsRetCode::INVALID_OPERATION;

    // Check if this is the first block
    if (fileStreamBlock.firstBlock)
    {
        // Start OTA update
        // For ESP32 there will be a long delay at this point - around 4 seconds so
        // the block will not get acknowledged until after that
        if (!_pFirmwareUpdater->fileStreamStart(fileStreamBlock.filename, fileStreamBlock.fileLen))
        {
#ifdef WARN_ON_FW_UPDATE_FAILED
            LOG_W(MODULE_PREFIX, "writeFirmwareBlock start FAILED name %s len %d",
                            fileStreamBlock.filename, fileStreamBlock.fileLen);
#endif
            return UtilsRetCode::CANNOT_START;
        }
    }
    uint64_t startUs = micros();
    UtilsRetCode::RetCode fwRslt = _pFirmwareUpdater->fileStreamDataBlock(fileStreamBlock);
    _totalBytes += fileStreamBlock.blockLen;
    _totalWriteTimeUs += micros() - startUs;
    return fwRslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write file block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::writeFileBlock(FileStreamBlock& fileStreamBlock)
{
    // Write using the chunker
    if (!_pFileChunker)
        return UtilsRetCode::INVALID_OPERATION;

    uint32_t bytesWritten = 0; 
    uint64_t startUs = micros();
    bool chunkerRslt = _pFileChunker->nextWrite(fileStreamBlock.pBlock, fileStreamBlock.blockLen, 
                    bytesWritten, fileStreamBlock.finalBlock);
    _totalBytes += bytesWritten;
    _totalWriteTimeUs += micros() - startUs;
    return chunkerRslt ? UtilsRetCode::OK : UtilsRetCode::OTHER_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write file block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileStreamSession::writeRealTimeStreamBlock(FileStreamBlock& fileStreamBlock)
{
    // Check valid
    if (!_pStreamChunkCB)
        return UtilsRetCode::INVALID_OPERATION;

    // Write to stream
    return _pStreamChunkCB(_streamRequestStr, fileStreamBlock, _streamSourceInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// file/stream cancel - callback from FileStreamBase
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileStreamSession::fileStreamCancelEnd(bool isNormalEnd)
{
    // Session no longer active
    _isActive = false;

    // Check if we should cancel a firmware update
    if (_pFirmwareUpdater && (_fileStreamContentType == FileStreamBase::FILE_STREAM_CONTENT_TYPE_FIRMWARE))
    {
        _pFirmwareUpdater->fileStreamCancelEnd(isNormalEnd);
        return;
    }
}
