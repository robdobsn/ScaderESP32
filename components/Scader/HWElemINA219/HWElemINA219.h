/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Element Steppers
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "HWElemBase.h"

class HWElemINA219 : public HWElemBase
{
public:
    // Constructor/Destructor
    HWElemINA219();
    virtual ~HWElemINA219();

    // Setup
    virtual void setup(const RaftJsonIF& config, const RaftJsonIF* pDefaults) override final;

    // Post-Setup - called after any buses have been connected
    virtual void postSetup() override final;

    // Service
    virtual void service() override final;
    
    // Get values binary = format specific to hardware
    virtual uint32_t getValsBinary(uint32_t formatCode, uint8_t* pBuf, uint32_t bufMaxLen) override final;
    virtual double getNamedValue(const char* param, bool& isFresh) override final;

    // Send binary command
    virtual RaftRetCode sendCmdBinary(uint32_t formatCode, const uint8_t* pData, uint32_t dataLen) override final;

    // Send JSON command
    virtual RaftRetCode sendCmdJSON(const char* jsonCmd) override final;

    // Has capability
    virtual bool hasCapability(const char* pCapabilityStr) override final;
 
     // Creator fn
    static HWElemBase* createFn()
    {
        return new HWElemINA219();
    }

protected:
    // Get data
    virtual String getDataJSON(ElemStatusLevel_t level = ELEM_STATUS_LEVEL_MIN) override final;

private:
    bool _pollReqSent;
    static const std::vector<HWElemReq> _initCmds;
    static const std::vector<HWElemReq> _statusCmds;
    static const uint32_t BYTES_TO_READ_FOR_INA219_STATUS = 2;

    // Helpers
    static void pollResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult);
    void pollResultCallback(BusRequestResult& reqResult);
    static void initResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult);
    void initResultCallback(BusRequestResult& reqResult);
};
