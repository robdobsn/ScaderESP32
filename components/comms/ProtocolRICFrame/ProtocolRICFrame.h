/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICFrame
// Protocol wrapper implementing RICFrame
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>
#include <vector>

class ProtocolRICFrame : public ProtocolBase
{
public:
    ProtocolRICFrame(uint32_t channelID, ConfigBase& config, const char* pConfigPrefix, CommsChannelMsgCB msgTxCB, 
                            CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB);
    virtual ~ProtocolRICFrame();
    
    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, ConfigBase& config, const char* pConfigPrefix, 
                CommsChannelMsgCB msgTxCB, CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB)
    {
        return new ProtocolRICFrame(channelID, config, pConfigPrefix, msgTxCB, msgRxCB, readyToRxCB);
    }

    // // Set message complete callback
    // virtual void setMsgCompleteCB(CommsChannelMsgCB msgCB)
    // {
    //     _msgCB = msgCB;
    // }

    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) override final;
    static bool decodeParts(const uint8_t* pData, uint32_t dataLen, uint32_t& msgNumber, 
                    uint32_t& msgProtocolCode, uint32_t& msgTypeCode, uint32_t& payloadStartPos);

    virtual void encodeTxMsgAndSend(CommsChannelMsg& msg) override final;
    static void encode(CommsChannelMsg& msg, std::vector<uint8_t>& outMsg);

    virtual const char* getProtocolName() override final
    {
        return getProtocolNameStatic();
    }

    static const char* getProtocolNameStatic()
    {
        return "RICFrame";
    }
};
