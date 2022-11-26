/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommsChannelManager
// Manages channels for comms messages
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <ArduinoOrAlt.h>
#include <vector>
#include <ProtocolBase.h>
#include "ProtocolCodecFactoryHelper.h"
#include "CommsChannel.h"
#include "SysModBase.h"

class CommsChannelManager : public SysModBase
{
public:
    CommsChannelManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~CommsChannelManager();

    // Register as an external message channel
    // xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
    // Returns an ID used to identify this channel
    uint32_t registerChannel(const char* protocolName, 
                const char* interfaceName,
                const char* channelName, 
                CommsChannelMsgCB msgCB, 
                ChannelReadyToSendCB outboundChannelReadyCB,
                const CommsChannelSettings* pSettings = nullptr);

    // Register as an internal message sink
    uint32_t registerSink(CommsChannelMsgCB msgCB);

    // Add protocol handler
    void addProtocol(ProtocolCodecFactoryHelper& protocolDef);

    // Get channel IDs
    int32_t getChannelIDByName(const String& channelName, const String& protocolName);
    void getChannelIDsByInterface(const char* interfaceName, std::vector<uint32_t>& channelIDs);
    void getChannelIDs(std::vector<uint32_t>& channelIDs);

    // Check if we can accept inbound message
    bool canAcceptInbound(uint32_t channelID);
    
    // Handle channel message
    void handleInboundMessage(uint32_t channelID, const uint8_t* pMsg, uint32_t msgLen);

    // Check if we can accept outbound message
    bool canAcceptOutbound(uint32_t channelID, bool &noConn);
    
    // Handle outbound message
    void handleOutboundMessage(CommsChannelMsg& msg);

    // Get the optimal comms block size
    uint32_t getInboundBlockLen(uint32_t channelID, uint32_t defaultSize);

    // Special channelIDs
    static const uint32_t CHANNEL_ID_UNDEFINED = 0xffff;
    static const uint32_t CHANNEL_ID_REST_API = 0xfffe;

    // Get info
    String getInfoJSON();

protected:
    // Service - called frequently
    virtual void service() override final;

private:
    // Vector of channels - pointer must be deleted and vector
    // element set to nullptr is the channel is deleted
    std::vector<CommsChannel*> _commsChannelVec;

    // List of protocol translations
    std::list<ProtocolCodecFactoryHelper> _protocolCodecFactoryList;

    // Callbacks
    bool frameSendCB(CommsChannelMsg& msg);

    // Helpers
    void ensureProtocolCodecExists(uint32_t channelID);
    void handleOutboundMessageOnChannel(CommsChannelMsg& msg, uint32_t channelID);

    // Consts
    static const int MAX_INBOUND_MSGS_IN_LOOP = 1;
};
