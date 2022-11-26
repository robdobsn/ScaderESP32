/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICJSON
// Protocol packet contains JSON with no additional overhead
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>
#include <vector>

class ProtocolRICJSON : public ProtocolBase
{
public:
    ProtocolRICJSON(uint32_t channelID, const char* configJSON, CommsChannelMsgCB msgTxCB, 
                    CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB);
    virtual ~ProtocolRICJSON();

    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, const char* configJSON, CommsChannelMsgCB msgTxCB, 
                    CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB)
    {
        return new ProtocolRICJSON(channelID, configJSON, msgTxCB, msgRxCB, readyToRxCB);
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
        return "RICJSON";
    }

private:
    // Consts
    static const int DEFAULT_RIC_FRAME_RX_MAX = 1000;
    static const int DEFAULT_RIC_FRAME_TX_MAX = 1000;
};
