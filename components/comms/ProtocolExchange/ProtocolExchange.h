/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolExchange
// Hub for handling protocol endpoint messages
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <ArduinoOrAlt.h>
#include <vector>
#include <ProtocolBase.h>
#include "ProtocolCodecFactoryHelper.h"
#include "CommsChannel.h"
#include "SysModBase.h"
#include "FileStreamBase.h"
#include "FileStreamSession.h"

class APISourceInfo;
class commsCoreIF;

class ProtocolExchange : public SysModBase
{
public:
    ProtocolExchange(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~ProtocolExchange();

    // Set handlers
    void setHandlers(SysModBase* pFirmwareUpdater)
    {
        _pFirmwareUpdater = pFirmwareUpdater;
    }

    // File Upload Block - only used for HTTP file uploads
    UtilsRetCode::RetCode handleFileUploadBlock(const String& req, FileStreamBlock& fileStreamBlock, 
            const APISourceInfo& sourceInfo, FileStreamBase::FileStreamContentType fileStreamContentType,
            const char* restAPIEndpointName);

protected:
    // Service - called frequently
    virtual void service() override final;

    // Add comms channels
    virtual void addCommsChannels(CommsCoreIF& commsCore) override final;

    // Get debug info
    virtual String getDebugJSON();

private:
    // Handlers
    SysModBase* _pFirmwareUpdater;

    // Next streamID to allocate to a stream session
    uint32_t _nextStreamID; 

    // Transfer sessions
    static const uint32_t MAX_SIMULTANEOUS_FILE_STREAM_SESSIONS = 3;
    std::list<FileStreamSession*> _sessions;

    // Previous activity indicator to keep SysManager up-to-date
    bool _sysManStateIndWasActive;

    // Threshold for determining if message processing is slow
    static const uint32_t MSG_PROC_SLOW_PROC_THRESH_MS = 50;

    // Process endpoint message
    bool canProcessEndpointMsg();
    bool processEndpointMsg(CommsChannelMsg& msg);
    bool processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo);
    bool processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo);
    UtilsRetCode::RetCode processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                        const CommsChannelMsg &endpointMsg);
    UtilsRetCode::RetCode processRICRESTFileStreamBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg, CommsChannelMsg &cmdMsg);
    bool processRICRESTNonFileStream(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const CommsChannelMsg &endpointMsg);

    // File/stream session handling
    FileStreamSession* findFileStreamSession(uint32_t streamID, const char* fileStreamName, uint32_t channelID);
    FileStreamSession* getFileStreamNewSession(const char* fileStreamName, uint32_t channelID, 
                    FileStreamBase::FileStreamContentType fileStreamContentType, const char* restAPIEndpointName,
                    FileStreamBase::FileStreamFlowType flowType, uint32_t fileStreamLength);
    FileStreamSession* getFileStreamExistingSession(const char* fileStreamName, uint32_t channelID, uint32_t streamID);
};
