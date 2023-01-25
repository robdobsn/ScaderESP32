/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Protocol
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FileStreamBase.h"

class JSONParams;

class FileUploadOKTOProtocol : public FileStreamBase
{
public:
    // Consts
    static const uint32_t FIRST_MSG_TIMEOUT = 5000;
    static const uint32_t BLOCK_MSGS_TIMEOUT = 1000;
    static const uint32_t MAX_BATCH_BLOCK_ACK_RETRIES = 5;
    static const uint32_t FILE_BLOCK_SIZE_DEFAULT = 5000;
    static const uint32_t BATCH_ACK_SIZE_DEFAULT = 40;
    static const uint32_t MAX_TOTAL_BYTES_IN_BATCH = 50000;
    // The overall timeout needs to be very big as BLE uploads can take over 30 minutes
    static const uint32_t UPLOAD_FAIL_TIMEOUT_MS = 2 * 3600 * 1000;

    // Constructor
    FileUploadOKTOProtocol(FileStreamBlockCB fileRxBlockCB, 
            FileStreamCanceEndCB fileRxCancelCB,
            CommsCoreIF* pCommsCore,
            FileStreamBase::FileStreamContentType fileStreamContentType, 
            FileStreamBase::FileStreamFlowType fileStreamFlowType,
            uint32_t streamID, 
            uint32_t fileStreamLength,
            const char* fileStreamName);

    // Service file upload
    void service();

    // Handle command frame
    virtual UtilsRetCode::RetCode handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg);

    // Handle received file/stream block
    virtual UtilsRetCode::RetCode handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg);

    // Get debug str
    virtual String getDebugJSON(bool includeBraces) override final;

    // Is active
    virtual bool isActive() override final
    {
        return _isUploading;
    }

private:
    // Message helpers
    void handleUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, const JSONParams& cmdFrame);
    void handleUploadEndMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame);
    void handleUploadCancelMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame);

    // Upload state-machine helpers
    void uploadService(bool& genAck);
    bool validateFileStreamStart(String& reqStr, String& fileName, uint32_t fileSize, 
            uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid);
    void validateRxBlock(uint32_t filePos, uint32_t blockLen, 
                bool& isFirstBlock, bool& blockValid, bool& isFinalBlock, bool& genAck);
    void uploadCancel(const char* reasonStr = nullptr);
    void uploadEnd();

    // Helpers
    uint32_t getOkTo();
    double getBlockRate();
    bool checkFinalBlock(uint32_t filePos, uint32_t blockLen);
    bool debugStatsReady();
    String debugStatsStr();
    double statsMsgRate();
    double statsDataRate();
    double statsFinalMsgRate();
    double statsFinalDataRate();
    void statsEndWindow();

    // File info
    String _reqStr;
    uint32_t _fileSize;
    String _fileName;
    uint32_t _expCRC16;
    bool _expCRC16Valid;

    // Upload state
    bool _isUploading;
    uint32_t _startMs;
    uint32_t _lastMsgMs;
    uint32_t _commsChannelID;

    // Size of batch and block
    uint32_t _batchAckSize;
    uint32_t _blockSize;

    // Stats
    uint32_t _blockCount;
    uint32_t _bytesCount;
    uint32_t _blocksInWindow;
    uint32_t _bytesInWindow;
    uint32_t _statsWindowStartMs;
    uint32_t _fileUploadStartMs;

    // Batch handling
    uint32_t _expectedFilePos;
    uint32_t _batchBlockCount;
    uint32_t _batchBlockAckRetry;

    // Debug
    uint32_t _debugLastStatsMs;
    static const uint32_t DEBUG_STATS_MS = 10000;
    bool _debugFinalMsgToSend;
};