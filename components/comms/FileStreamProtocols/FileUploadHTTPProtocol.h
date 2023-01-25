/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Over HTTP Protocol
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FileStreamBase.h"

class FileUploadHTTPProtocol : public FileStreamBase
{
public:
    // Constructor
    FileUploadHTTPProtocol(FileStreamBlockCB fileRxBlockCB, 
            FileStreamCanceEndCB fileRxCancelCB,
            CommsCoreIF* pCommsCore,
            FileStreamBase::FileStreamContentType fileStreamContentType,
            FileStreamBase::FileStreamFlowType fileStreamFlowType,
            uint32_t streamID, 
            uint32_t fileStreamLength,
            const char* fileStreamName);

    // Service
    void service() override final;

    // Handle command frame
    virtual UtilsRetCode::RetCode handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg) override final;

    // Handle received file/stream block
    virtual UtilsRetCode::RetCode handleDataFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg) override final;

    // Get debug str
    virtual String getDebugJSON(bool includeBraces) override final;

    // Is active
    virtual bool isActive() override final;
};
