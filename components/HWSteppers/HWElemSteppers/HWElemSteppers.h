/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Element Steppers
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <UtilsRetCode.h>
#include <HWElemBase.h>
#include <Controller/MotionController.h>
// #include "RampGenerator/RampGenTimer.h"
// #include "EvaluatorGCode.h"

class ConfigBase;

class HWElemSteppers : public HWElemBase
{
public:
    // Constructor/Destructor
    HWElemSteppers();
    virtual ~HWElemSteppers();

    // Setup
    virtual void setup(ConfigBase& config, ConfigBase* pDefaults) override final;

    // Post-Setup - called after any buses have been connected
    virtual void postSetup() override final;

    // Service
    virtual void service() override final;
    
    // Get values binary = format specific to hardware
    virtual uint32_t getValsBinary(uint32_t formatCode, uint8_t* pBuf, uint32_t bufMaxLen) override final;
    virtual double getNamedValue(const char* param, bool& isFresh) override final;

    // Send binary command
    virtual UtilsRetCode::RetCode sendCmdBinary(uint32_t formatCode, const uint8_t* pData, uint32_t dataLen) override final;

    // Send JSON command
    virtual UtilsRetCode::RetCode sendCmdJSON(const char* jsonCmd) override final;

    // Has capability
    virtual bool hasCapability(const char* pCapabilityStr) override final;
 
     // Creator fn
    static HWElemBase* createFn()
    {
        return new HWElemSteppers();
    }

    // Get data
    virtual String getDataJSON(HWElemStatusLevel_t level = ELEM_STATUS_LEVEL_MIN) override final;

    // Set motor on time after move
    void setMotorOnTimeAfterMoveSecs(float motorOnTimeAfterMoveSecs)
    {
        _motionController.setMotorOnTimeAfterMoveSecs(motorOnTimeAfterMoveSecs);
    }

    // Set max motor current (amps)
    void setMaxMotorCurrentAmps(uint32_t axisIdx, float maxMotorCurrent)
    {
        _motionController.setMaxMotorCurrentAmps(axisIdx, maxMotorCurrent);
    }

private:
    // Motion controller
    MotionController _motionController;

    // Command handlers
    void handleCmdBinary_MoveTo(const uint8_t* pData, uint32_t dataLen);

};
