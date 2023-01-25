/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICSerial
// Protocol wrapper implementing RICSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>

class MiniHDLC;

class ProtocolRICSerial : public ProtocolBase
{
public:
    ProtocolRICSerial(uint32_t channelID, ConfigBase& config, const char* pConfigPrefix, CommsChannelMsgCB msgTxCB, 
                        CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB);
    virtual ~ProtocolRICSerial();
    
    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, ConfigBase& config, const char* pConfigPrefix, 
                        CommsChannelMsgCB msgTxCB, CommsChannelMsgCB msgRxCB, CommsChannelReadyToRxCB readyToRxCB)
    {
        return new ProtocolRICSerial(channelID, config, pConfigPrefix, msgTxCB, msgRxCB, readyToRxCB);
    }

    // // Set message complete callback
    // virtual void setMsgCompleteCB(CommsChannelMsgCB msgCB)
    // {
    //     _msgCB = msgCB;
    // }

    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) override final;
    virtual void encodeTxMsgAndSend(CommsChannelMsg& msg) override final;

    virtual const char* getProtocolName() override final
    {
        return getProtocolNameStatic();
    }

    static const char* getProtocolNameStatic()
    {
        return "RICSerial";
    }

private:
    // HDLC
    MiniHDLC* _pHDLC;

    // Consts
    static const int DEFAULT_RIC_SERIAL_RX_MAX = 5000;
    static const int DEFAULT_RIC_SERIAL_TX_MAX = 5000;

    // Max msg lengths
    uint32_t _maxTxMsgLen;
    uint32_t _maxRxMsgLen;

    // Debug
    uint32_t _debugLastInReportMs = 0;
    uint32_t _debugNumBytesRx = 0;

    // Helpers
    void hdlcFrameRxCB(const uint8_t* pFrame, int frameLen);
};
