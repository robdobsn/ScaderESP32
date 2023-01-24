/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File/Stream Base (for protocol handlers)
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FileStreamBase.h>
#include <JSONParams.h>
#include <RICRESTMsg.h>

// Log prefix
// static const char *MODULE_PREFIX = "FileStreamBase";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamBase::FileStreamBase(FileStreamBlockCB fileRxBlockCB, 
        FileStreamCanceEndCB fileRxCancelEndCB,
        CommsCoreIF* pCommsCore,
        FileStreamBase::FileStreamContentType fileStreamContentType,
        FileStreamBase::FileStreamFlowType fileStreamFlowType,
        uint32_t streamID,
        uint32_t fileStreamLength,
        const char* fileStreamName)
{
    // Vars
    _fileStreamRxBlockCB = fileRxBlockCB;
    _fileStreamRxCancelEndCB = fileRxCancelEndCB;
    _pCommsCore = pCommsCore;
    _fileStreamContentType = fileStreamContentType;
    _fileStreamFlowType = fileStreamFlowType;
    _streamID = streamID;
    _fileStreamLength = fileStreamLength;
    _fileStreamName = fileStreamName;
}

FileStreamBase::~FileStreamBase()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get information from file stream start message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamBase::FileStreamMsgType FileStreamBase::getFileStreamMsgInfo(const RICRESTMsg& ricRESTReqMsg,
                String& cmdName, String& fileStreamName, 
                FileStreamContentType& fileStreamContentType, uint32_t& streamID,
                String& restAPIEndpointName, uint32_t& fileStreamLength)
{
    // Handle command frames
    JSONParams cmdFrame = ricRESTReqMsg.getPayloadJson();
    cmdName = cmdFrame.getString("cmdName", "");

    // Check msg type
    FileStreamMsgType fileStreamMsgType = FILE_STREAM_MSG_TYPE_NONE;
    if (cmdName.equalsIgnoreCase("ufStart"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_START;
    if (cmdName.equalsIgnoreCase("ufEnd"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_END;
    if (cmdName.equalsIgnoreCase("ufCancel"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_CANCEL;

    // Return none if not a file/stream protocol message
    if (fileStreamMsgType == FILE_STREAM_MSG_TYPE_NONE)
        return fileStreamMsgType;

    // Extract info
    fileStreamName = cmdFrame.getString("fileName", "");
    String fileStreamTypeStr = cmdFrame.getString("fileType", "");
    streamID = cmdFrame.getLong("streamID", FILE_STREAM_ID_ANY);
    fileStreamContentType = getFileStreamContentType(fileStreamTypeStr);
    restAPIEndpointName = cmdFrame.getString("endpoint", "");
    fileStreamLength = cmdFrame.getLong("fileLen", 0);
    return fileStreamMsgType;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get fileStreamContentType from string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamBase::FileStreamContentType FileStreamBase::getFileStreamContentType(const String& fileStreamTypeStr)
{
    if ((fileStreamTypeStr.length() == 0) || fileStreamTypeStr.equalsIgnoreCase("fs") || fileStreamTypeStr.equalsIgnoreCase("file"))
        return FILE_STREAM_CONTENT_TYPE_FILE;
    if (fileStreamTypeStr.equalsIgnoreCase("fw") || fileStreamTypeStr.equalsIgnoreCase("ricfw"))
        return FILE_STREAM_CONTENT_TYPE_FIRMWARE;
    if (fileStreamTypeStr.equalsIgnoreCase("rtstream"))
        return FILE_STREAM_CONTENT_TYPE_RT_STREAM;
    return FILE_STREAM_CONTENT_TYPE_FILE;
}
