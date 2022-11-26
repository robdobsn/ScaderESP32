/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Comms Channel
// Channels for messages from interfaces (BLE, WiFi, etc)
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <ProtocolBase.h>
#include <ThreadSafeQueue.h>
#include <CommsChannelMsg.h>
#include <ProtocolRawMsg.h>
#include <CommsChannelSettings.h>

// Use a queue
#define COMMS_CHANNEL_USE_INBOUND_QUEUE

class CommsChannel
{
    friend class CommsChannelManager;
public:

    // xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
    CommsChannel(const char* pSourceProtocolName, 
                const char* interfaceName, 
                const char* channelName,
                CommsChannelMsgCB msgSendCallback, 
                ChannelReadyToSendCB outboundChannelReadyCB,
                const CommsChannelSettings* pSettings = nullptr);

private:
    String getInterfaceName()
    {
        return _interfaceName;
    }

    String getChannelName()
    {
        return _channelName;
    }

    String getSourceProtocolName()
    {
        return _channelProtocolName;
    }

    ProtocolBase* getProtocolCodec()
    {
        return _pProtocolCodec;
    }

    // Set protocol handler for channel
    void setProtocolCodec(ProtocolBase* pProtocolCodec);

    // Handle Rx data
    void handleRxData(const uint8_t* pMsg, uint32_t msgLen);

    // Inbound queue
    bool canAcceptInbound();

#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
    void addToInboundQueue(const uint8_t* pMsg, uint32_t msgLen);
#endif

#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
    bool getFromInboundQueue(ProtocolRawMsg& msg);
#endif

    uint32_t getInboundBlockLen()
    {
        return _settings.inboundBlockLen;
    }
    bool processInboundQueue();

    // Outbound queue
    void addToOutboundQueue(CommsChannelMsg& msg);
    bool getFromOutboundQueue(CommsChannelMsg& msg);
    uint32_t getOutboundBlockLen()
    {
        return _settings.outboundBlockLen;
    }

    // Call protocol handler with a message
    void addTxMsgToProtocolCodec(CommsChannelMsg& msg);

    // Check channel is ready
    bool canAcceptOutbound(uint32_t channelID, bool& noConn)
    {
        if (_channelReadyCB)
            return _channelReadyCB(channelID, noConn);
        return false;
    }

    // Send the message on the channel
    bool sendMsgOnChannel(CommsChannelMsg& msg)
    {
        if (_msgSendCallback)
            return _msgSendCallback(msg);
        return false;
    }

    // Get info JSON
    String getInfoJSON();

private:
    // Protocol supported
    String _channelProtocolName;

    // Callback to send message on channel
    CommsChannelMsgCB _msgSendCallback;

    // Name of interface and channel
    String _interfaceName;
    String _channelName;

    // Protocol codec
    ProtocolBase* _pProtocolCodec;

    // Channel ready callback
    ChannelReadyToSendCB _channelReadyCB;

    // Comms settings
    CommsChannelSettings _settings;

    // Inbound queue peak level
    uint16_t _inboundQPeak;

#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
    // Inbound message queue for raw messages
    ThreadSafeQueue<ProtocolRawMsg> _inboundQueue;
#endif

    // Outbount queue peak level
    uint16_t _outboundQPeak;

    // Outbound message queue for response messages
    ThreadSafeQueue<CommsChannelMsg> _outboundQueue;
};
