/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolBase
// Base class for protocol translations
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <functional>

class CommsChannelMsg;
class ProtocolBase;

// Put byte callback function type
typedef std::function<void(uint8_t ch)> ProtocolBasePutByteCBType;
// Received frame callback function type
typedef std::function<void(const uint8_t *framebuffer, int framelength)> ProtocolBaseFrameCBType;
// Message callback function type
typedef std::function<bool(CommsChannelMsg& msg)> CommsChannelMsgCB;
// Ready to receive callback function type
typedef std::function<bool()> CommsChannelReadyToRxCB;
// Create protocol instance
typedef std::function<ProtocolBase* (uint32_t channelID, const char* pConfigJSON, CommsChannelMsgCB msgTxCB, 
                CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB)> ProtocolCreateFnType;
// Channel ready function type
typedef std::function<bool(uint32_t channelID, bool& noConn)> ChannelReadyToSendCB;

class ProtocolBase
{
public:
    ProtocolBase(uint32_t channelID, CommsChannelMsgCB msgTxCB, CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB)
            : _channelID(channelID), _msgTxCB(msgTxCB), _msgRxCB(msgRxCB), _readyToRxCB(readyToRxCB)
    {
    }
    virtual ~ProtocolBase()
    {
    }

    // Add received data to be decoded
    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) = 0;

    // Encode a message and send
    virtual void encodeTxMsgAndSend(CommsChannelMsg& msg) = 0;

    // Check if ready for received data
    virtual bool readyForRxData()
    {
        if (_readyToRxCB)
            return _readyToRxCB();
        return true;
    }

    // Protocol codec name
    virtual const char* getProtocolName()
    {
        return "BASE";
    }

    // Get channel ID
    uint32_t getChannelID()
    {
        return _channelID;
    }

protected:
    // The channelID for messages handled by this protocol handler
    uint32_t _channelID;

    // Callbacks
    CommsChannelMsgCB _msgTxCB;
    CommsChannelMsgCB _msgRxCB;
    CommsChannelReadyToRxCB _readyToRxCB;
};
