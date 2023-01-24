/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stream Datagram Protocol
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FileStreamBase.h"

class StreamDatagramProtocol : public FileStreamBase
{
public:
    // Constructor
    StreamDatagramProtocol(FileStreamBlockCB fileRxBlockCB, 
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
    static const uint32_t MAX_DEBUG_BIN_HEX_LEN = 50;
    
    // Is active
    virtual bool isActive() override final;

private:
    // Stream position
    uint32_t _streamPos;

};
