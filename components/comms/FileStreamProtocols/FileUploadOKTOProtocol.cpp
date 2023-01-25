/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload OKTO Protocol
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <JSONParams.h>
#include <FileUploadOKTOProtocol.h>
#include <RICRESTMsg.h>
#include <FileSystem.h>
#include <CommsCoreIF.h>
#include <CommsChannelMsg.h>
#include <CommsCoreIF.h>

// Log prefix
static const char *MODULE_PREFIX = "FileUpldOKTO";

// #define DEBUG_SHOW_FILE_UPLOAD_PROGRESS
// #define DEBUG_RICREST_FILEUPLOAD
// #define DEBUG_RICREST_FILEUPLOAD_FIRST_AND_LAST_BLOCK
// #define DEBUG_FILE_STREAM_BLOCK_DETAIL
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
// #define DEBUG_RICREST_HANDLE_CMD_FRAME

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileUploadOKTOProtocol::FileUploadOKTOProtocol(FileStreamBlockCB fileRxBlockCB, 
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
    // File params
    _fileSize = 0;
    _commsChannelID = 0;
    _expCRC16Valid = false;
    _expCRC16 = 0;

    // Status
    _isUploading = false;

    // Timing
    _startMs = 0;
    _lastMsgMs = 0;

    // Stats
    _blockCount = 0;
    _bytesCount = 0;
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
    _fileUploadStartMs = 0;

    // Debug
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;

    // Batch and block size
    _batchAckSize = BATCH_ACK_SIZE_DEFAULT;
    _blockSize = FILE_BLOCK_SIZE_DEFAULT;

    // Batch handling
    _expectedFilePos = 0;
    _batchBlockCount = 0;
    _batchBlockAckRetry = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileUploadOKTOProtocol::handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg)
{
#ifdef DEBUG_RICREST_HANDLE_CMD_FRAME
    LOG_I(MODULE_PREFIX, "handleCmdFrame cmdName %s req %s", cmdName.c_str(), ricRESTReqMsg.getReq().c_str());
#endif

    // Check file upload related
    JSONParams cmdFrame = ricRESTReqMsg.getPayloadJson();
    if (cmdName.equalsIgnoreCase("ufStart"))
    {
        handleUploadStartMsg(ricRESTReqMsg.getReq(), respMsg, endpointMsg.getChannelID(), cmdFrame);
        return UtilsRetCode::OK;
    }
    else if (cmdName.equalsIgnoreCase("ufEnd"))
    {
        handleUploadEndMsg(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
        return UtilsRetCode::OK;
    } 
    else if (cmdName.equalsIgnoreCase("ufCancel"))
    {
        handleUploadCancelMsg(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
        return UtilsRetCode::OK;
    }
    return UtilsRetCode::INVALID_OPERATION;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle data frame (file/stream block)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileUploadOKTOProtocol::handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
#ifdef DEBUG_FILE_STREAM_BLOCK_DETAIL
    LOG_I(MODULE_PREFIX, "handleDataFrame isUploading %d msgLen %d expectedPos %d", 
            _isUploading, ricRESTReqMsg.getBinLen(), _expectedFilePos);
#endif

    // Check if upload has been cancelled
    if (!_isUploading)
    {
        LOG_W(MODULE_PREFIX, "handleFileBlock called when not uploading");
        uploadCancel("failBlockUnexpected");
        return UtilsRetCode::NOT_UPLOADING;
    }

    // Handle the upload block
    uint32_t filePos = ricRESTReqMsg.getBufferPos();
    const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
    uint32_t bufferLen = ricRESTReqMsg.getBinLen();

    // Validate received block and update state machine
    bool blockValid = false;
    bool isFinalBlock = false;
    bool isFirstBlock = false;
    bool genAck = false;
    validateRxBlock(filePos, bufferLen, blockValid, isFirstBlock, isFinalBlock, genAck);

    // Debug
    if (isFinalBlock)
    {
        LOG_I(MODULE_PREFIX, "handleFileBlock isFinal %d", isFinalBlock);
    }

#ifdef DEBUG_RICREST_FILEUPLOAD_FIRST_AND_LAST_BLOCK
    if (isFinalBlock || (_expectedFilePos == 0))
    {
        String hexStr;
        Raft::getHexStrFromBytes(ricRESTReqMsg.getBinBuf(), ricRESTReqMsg.getBinLen(), hexStr);
        LOG_I(MODULE_PREFIX, "handleFileBlock %s", hexStr.c_str());
    }
#endif

    // Check if time to generate an ACK
    if (genAck)
    {
        // block returns true when an acknowledgement is required - so send that ack
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", getOkTo());
        Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "handleFileBlock BatchOK Sending OkTo %d rxBlockFilePos %d len %d batchCount %d resp %s", 
                getOkTo(), filePos, bufferLen, _batchBlockCount, respMsg.c_str());
#endif
    }
    else
    {
        // Just another block - don't ack yet
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
        LOG_I(MODULE_PREFIX, "handleFileBlock filePos %d len %d batchCount %d resp %s heapSpace %d", 
                filePos, bufferLen, _batchBlockCount, respMsg.c_str(),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#endif
    }

    // Check valid
    UtilsRetCode::RetCode rslt = UtilsRetCode::OK;
    if (blockValid && _fileStreamRxBlockCB)
    {
        FileStreamBlock fileStreamBlock(_fileName.c_str(), 
                            _fileSize, 
                            filePos, 
                            pBuffer, 
                            bufferLen, 
                            isFinalBlock,
                            _expCRC16,
                            _expCRC16Valid,
                            _fileSize,
                            true,
                            isFirstBlock
                            );            

        // If this is the first block of a firmware update then there will be a long delay
        rslt = _fileStreamRxBlockCB(fileStreamBlock);

        // Check result
        if (rslt != UtilsRetCode::OK)
        {
            if (_fileStreamContentType == FileUploadOKTOProtocol::FILE_STREAM_CONTENT_TYPE_FIRMWARE)
            {
                Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"OTAWriteFailed")");
                if (isFirstBlock)
                    uploadCancel("failOTAStart");
                else
                    uploadCancel("failOTAWrite");
            }
            else
            {
                Raft::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"FileWriteFailed")");
                uploadCancel("failFileWrite");
            }
        }
    }
    return rslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::service()
{
#ifdef DEBUG_SHOW_FILE_UPLOAD_PROGRESS
    // Stats display
    if (debugStatsReady())
    {
        LOG_I(MODULE_PREFIX, "fileUploadStats %s", debugStatsStr().c_str());
    }
#endif
    // Check uploading
    if (!_isUploading)
        return;
    
    // Handle upload activity
    bool genBatchAck = false;
    uploadService(genBatchAck);
    if (genBatchAck)
    {
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", getOkTo());
        String respMsg;
        Raft::setJsonBoolResult("ufBlock", respMsg, true, ackJson);

        // Send the response back
        RICRESTMsg ricRESTRespMsg;
        CommsChannelMsg endpointMsg;
        ricRESTRespMsg.encode(respMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
        endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_TYPE_RESPONSE);

        // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
        LOG_I(MODULE_PREFIX, "service Sending OkTo %d batchCount %d", 
                getOkTo(), _batchBlockCount);
#endif

        // Send message on the appropriate channel
        if (_pCommsCore)
            _pCommsCore->handleOutboundMessage(endpointMsg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get debug str
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileUploadOKTOProtocol::getDebugJSON(bool includeBraces)
{
    if (includeBraces)
        return "{" + debugStatsStr() + "}";
    return debugStatsStr();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::handleUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, const JSONParams& cmdFrame)
{
    // Get params
    String ufStartReq = cmdFrame.getString("reqStr", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    String crc16Str = cmdFrame.getString("CRC16", "");
    uint32_t blockSizeFromHost = cmdFrame.getLong("batchMsgSize", -1);
    uint32_t batchAckSizeFromHost = cmdFrame.getLong("batchAckSize", -1);
    // CRC handling
    uint32_t crc16 = 0;
    bool crc16Valid = false;
    if (crc16Str.length() > 0)
    {
        crc16Valid = true;
        crc16 = strtoul(crc16Str.c_str(), nullptr, 0);
    }

    // Validate the start message
    String errorMsg;
    bool startOk = validateFileStreamStart(ufStartReq, fileName, fileLen,  
                channelID, errorMsg, crc16, crc16Valid);

    // Check ok
    if (startOk)
    {
        // If we are sent batchMsgSize and/or batchAckSize then we use these values
        if ((blockSizeFromHost != -1) && (blockSizeFromHost > 0))
            _blockSize = blockSizeFromHost;
        else
            _blockSize = FILE_BLOCK_SIZE_DEFAULT;
        if ((batchAckSizeFromHost != -1) && (batchAckSizeFromHost > 0))
            _batchAckSize = batchAckSizeFromHost;
        else
            _batchAckSize = BATCH_ACK_SIZE_DEFAULT;

        // Check block sizes against channel maximum
        uint32_t chanBlockMax = 0;
        if (_pCommsCore)
        {
            chanBlockMax = _pCommsCore->getInboundBlockLen(channelID, FILE_BLOCK_SIZE_DEFAULT);
            if ((_blockSize > chanBlockMax) && (chanBlockMax > 0))
            {
                // Apply factor to block size
                _blockSize = chanBlockMax * 3 / 2;
            }
        }

        // Check maximum total bytes in batch
        uint32_t totalBytesInBatch = _blockSize * _batchAckSize;
        if (totalBytesInBatch > MAX_TOTAL_BYTES_IN_BATCH)
        {
            _batchAckSize = MAX_TOTAL_BYTES_IN_BATCH / _blockSize;
            if (_batchAckSize == 0)
                _batchAckSize = 1;
        }

#ifdef DEBUG_RICREST_HANDLE_CMD_FRAME
        LOG_I(MODULE_PREFIX, "handleUploadStartMsg reqStr %s filename %s fileLen %d fileType %s streamID %d errorMsg %s", 
                    _reqStr.c_str(),
                    _fileName.c_str(), 
                    _fileSize,
                    fileType.c_str(),
                    _streamID,
                    errorMsg.c_str());
        LOG_I(MODULE_PREFIX, "handleUploadStartMsg blockSize %d chanBlockMax %d defaultBlockSize %d batchAckSize %d defaultBatchAckSize %d crc16 %s crc16Valid %d",
                    _blockSize,
                    chanBlockMax,
                    FILE_BLOCK_SIZE_DEFAULT,
                    _batchAckSize,
                    BATCH_ACK_SIZE_DEFAULT,
                    crc16Str.c_str(),
                    crc16Valid);
#endif
    }
    else
    {
        LOG_W(MODULE_PREFIX, "handleUploadStartMsg FAIL reqStr %s streamID %d errorMsg %s", 
                    _reqStr.c_str(),
                    _streamID,
                    errorMsg.c_str());
    }
    // Response
    char extraJson[100];
    snprintf(extraJson, sizeof(extraJson), R"("batchMsgSize":%d,"batchAckSize":%d,"streamID":%d)", 
                _blockSize, _batchAckSize, _streamID);
    Raft::setJsonResult(reqStr.c_str(), respMsg, startOk, errorMsg.c_str(), extraJson);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload ended normally
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::handleUploadEndMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame)
{
    // Handle file end
#ifdef DEBUG_RICREST_FILEUPLOAD
    uint32_t blocksSent = cmdFrame.getLong("blockCount", 0);
#endif

    // Callback to indicate end of activity
    if (_fileStreamRxCancelEndCB)
        _fileStreamRxCancelEndCB(true);

    // Response
    Raft::setJsonBoolResult(reqStr.c_str(), respMsg, true);

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    LOG_I(MODULE_PREFIX, "handleUploadEndMsg reqStr %s fileName %s fileType %s fileLen %d blocksSent %d", 
                _reqStr.c_str(),
                fileName.c_str(), 
                fileType.c_str(), 
                fileLen, 
                blocksSent);
#endif

    // End upload
    uploadEnd();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cancel file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::handleUploadCancelMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame)
{
    // Handle file cancel
    String fileName = cmdFrame.getString("fileName", "");
    String reason = cmdFrame.getString("reason", "");

    // Cancel upload
    uploadCancel(reason.c_str());

    // Response
    Raft::setJsonBoolResult(reqStr.c_str(), respMsg, true);

    // Debug
    LOG_I(MODULE_PREFIX, "handleUploadCancelMsg fileName %s", fileName.c_str());

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    LOG_I(MODULE_PREFIX, "handleUploadCancelMsg reqStr %s fileName %s", 
                _reqStr.c_str(),
                fileName.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileUploadOKTOProtocol::validateFileStreamStart(String& reqStr, String& fileName, uint32_t fileSize, 
        uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid)
{
    // Check if already in progress
    if (_isUploading && (_expectedFilePos > 0))
    {
        respInfo = "uploadInProgress";
        return false;
    }
    

    // File params
    _reqStr = reqStr;
    _fileName = fileName;
    _fileSize = fileSize;
    _commsChannelID = channelID;
    _expCRC16 = crc16;
    _expCRC16Valid = crc16Valid;

    // Status
    _isUploading = true;
    
    // Timing
    _startMs = millis();
    _lastMsgMs = millis();

    // Stats
    _blockCount = 0;
    _bytesCount = 0;
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
    _fileUploadStartMs = millis();

    // Debug
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;

    // Batch handling
    _expectedFilePos = 0;
    _batchBlockCount = 0;
    _batchBlockAckRetry = 0;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::uploadService(bool& genAck)
{
    unsigned long nowMillis = millis();
    genAck = false;
    // Check valid
    if (!_isUploading)
        return;
    
    // At the start of ESP firmware update there is a long delay (3s or so)
    // This occurs after reception of the first block
    // So need to ensure that there is a long enough timeout
    if (Raft::isTimeout(nowMillis, _lastMsgMs, _blockCount < 2 ? FIRST_MSG_TIMEOUT : BLOCK_MSGS_TIMEOUT))
    {
        _batchBlockAckRetry++;
        if (_batchBlockAckRetry < MAX_BATCH_BLOCK_ACK_RETRIES)
        {
            LOG_W(MODULE_PREFIX, "uploadService blockMsgs timeOut - okto ack needed bytesRx %d lastOkTo %d lastMsgMs %d curMs %ld blkCount %d blkSize %d batchSize %d retryCount %d",
                        _bytesCount, getOkTo(), _lastMsgMs, nowMillis, _blockCount, _blockSize, _batchAckSize, _batchBlockAckRetry);
            _lastMsgMs = nowMillis;
            genAck = true;
            return;
        }
        else
        {
            LOG_W(MODULE_PREFIX, "uploadService blockMsgs ack failed after retries");
            uploadCancel("failRetries");
        }
    }

    // Check for overall time-out
    if (Raft::isTimeout(nowMillis, _startMs, UPLOAD_FAIL_TIMEOUT_MS))
    {
        LOG_W(MODULE_PREFIX, "uploadService overall time-out startMs %d nowMs %ld maxMs %d",
                    _startMs, nowMillis, UPLOAD_FAIL_TIMEOUT_MS);
        uploadCancel("failTimeout");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Validate received block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::validateRxBlock(uint32_t filePos, uint32_t blockLen, bool& blockValid, 
            bool& isFirstBlock, bool& isFinalBlock, bool& genAck)
{
    // Check uploading
    if (!_isUploading)
        return;

    // Returned vals
    blockValid = false;
    isFinalBlock = false;
    isFirstBlock = false;
    genAck = false;

    // Add to batch
    _batchBlockCount++;
    _lastMsgMs = millis();

    // Check
    if (filePos != _expectedFilePos)
    {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_W(MODULE_PREFIX, "validateRxBlock unexpected %d != %d, blockCount %d batchBlockCount %d, batchAckSize %d", 
                    filePos, _expectedFilePos, _blockCount, _batchBlockCount, _batchAckSize);
#endif
    }
    else
    {
        // Valid block so bump values
        blockValid = true;
        _expectedFilePos += blockLen;
        _blockCount++;
        _bytesCount += blockLen;
        _blocksInWindow++;
        _bytesInWindow += blockLen;
        isFirstBlock = filePos == 0;

        // Check if this is the final block
        isFinalBlock = checkFinalBlock(filePos, blockLen);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "validateRxBlock ok blockCount %d batchBlockCount %d, batchAckSize %d",
                    _blockCount, _batchBlockCount, _batchAckSize);
#endif
    }

    // Generate an ack on the first block received and on completion of each batch
    bool batchComplete = (_batchBlockCount == _batchAckSize) || (_blockCount == 1) || isFinalBlock;
    if (batchComplete)
        _batchBlockCount = 0;
    _batchBlockAckRetry = 0;
    genAck = batchComplete;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine upload cancel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::uploadCancel(const char* reasonStr)
{
    // End upload state machine
    uploadEnd();

    // Callback to indicate cancellation
    if (_fileStreamRxCancelEndCB)
        _fileStreamRxCancelEndCB(false);

    // Check if we need to send back a reason
    if (reasonStr != nullptr)
    {
        // Form cancel message
        String cancelMsg;
        char tmpStr[50];
        snprintf(tmpStr, sizeof(tmpStr), R"("cmdName":"ufCancel","reason":"%s")", reasonStr);
        Raft::setJsonBoolResult("", cancelMsg, true, tmpStr);

        // Send status message
        RICRESTMsg ricRESTRespMsg;
        CommsChannelMsg endpointMsg;
        ricRESTRespMsg.encode(cancelMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
        endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_TYPE_RESPONSE);

        // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
        LOG_W(MODULE_PREFIX, "uploadCancel ufCancel reason %s", reasonStr);
#endif

        // Send message on the appropriate channel
        if (_pCommsCore)
            _pCommsCore->handleOutboundMessage(endpointMsg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadOKTOProtocol::uploadEnd()
{
    _isUploading = false;
    _debugFinalMsgToSend = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get OkTo
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileUploadOKTOProtocol::getOkTo()
{
    return _expectedFilePos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get block info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double FileUploadOKTOProtocol::getBlockRate()
{
    uint32_t elapsedMs = millis() - _startMs;
    if (elapsedMs > 0)
        return 1000.0*_blockCount/elapsedMs;
    return 0;
}
bool FileUploadOKTOProtocol::checkFinalBlock(uint32_t filePos, uint32_t blockLen)
{
    return filePos + blockLen >= _fileSize;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug and Stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileUploadOKTOProtocol::debugStatsReady()
{
    return _debugFinalMsgToSend || (_isUploading && Raft::isTimeout(millis(), _debugLastStatsMs, DEBUG_STATS_MS));
}

String FileUploadOKTOProtocol::debugStatsStr()
{
    char outStr[200];
    snprintf(outStr, sizeof(outStr), 
            R"("actv":%d,"msgRate":%.1f,"dataBps":%.1f,"bytes":%d,"blks":%d,"blkSize":%d,"strmID":%d,"name":"%s")",
            _isUploading,
            statsFinalMsgRate(), 
            statsFinalDataRate(), 
            _bytesCount,
            _blockCount, 
            _blockSize,
            _streamID,
            _fileName.c_str());
    statsEndWindow();
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;
    return outStr;
}

double FileUploadOKTOProtocol::statsMsgRate()
{
    uint32_t winMs = millis() - _statsWindowStartMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _blocksInWindow / winMs;
}

double FileUploadOKTOProtocol::statsDataRate()
{
    uint32_t winMs = millis() - _statsWindowStartMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _bytesInWindow / winMs;
}

double FileUploadOKTOProtocol::statsFinalMsgRate()
{
    uint32_t winMs = _lastMsgMs - _startMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _blockCount / winMs;
}

double FileUploadOKTOProtocol::statsFinalDataRate()
{
    uint32_t winMs = _lastMsgMs - _startMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _bytesCount / winMs;
}

void FileUploadOKTOProtocol::statsEndWindow()
{
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
}

