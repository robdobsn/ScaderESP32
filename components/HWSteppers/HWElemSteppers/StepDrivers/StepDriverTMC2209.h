/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StepDriverBase
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "StepDriverBase.h"

class HWElemBase;

class StepDriverTMC2209 : public StepDriverBase
{
public:
    StepDriverTMC2209();

    // Setup
    virtual bool setup(const String& stepperName, const StepDriverParams& stepperParams, bool usingISR) override final;
    
    // Service - called frequently
    virtual void service() override final;

    // Set microsteps
    virtual void setMicrosteps(uint32_t microsteps) override final;

    // Set direction
    virtual void IRAM_ATTR setDirection(bool dirn, bool forceSet = false) override final;

    // Start and end a single step
    virtual void IRAM_ATTR stepStart() override final;
    virtual bool IRAM_ATTR stepEnd() override final;

    virtual String getDriverType() override final
    {
        return "TMC2209";
    }

private:
    // Driver register codes
    // These must be in the same order as added to _driverRegisters
    // in the base class
    enum DriverRegisterCodes {
        DRIVER_REGISTER_CODE_GCONF,
        DRIVER_REGISTER_CODE_CHOPCONF,
        DRIVER_REGISTER_CODE_IHOLD_IRUN,
        DRIVER_REGISTER_CODE_PWMCONF,
        DRIVER_REGISTER_CODE_VACTUAL,
    };

    // Current direction and step values
    bool _dirnCurValue;
    bool _stepCurActive;

    // Driver register index
    uint32_t _driverRegisterIdx = 0;

    // Helpers
    void setPDNDisable(bool disable);
    uint32_t getMRESFieldValue(uint32_t microsteps);
    void convertRMSCurrentToRegs(double reqCurrentAmps, double holdFactor, 
            StepDriverParams::HoldModeEnum holdMode, bool& vsenseOut, uint32_t& irunOut, uint32_t& iholdOut);

    // TMC2209 Defs
    static const uint8_t TMC_2209_SYNC_BYTE = 5;
    static constexpr double TMC_2209_CLOCK_FREQ_HZ = 12000000.0;

    // PDN_UART_BIT should be 1 to enable UART operation
    // MSTEP_REG_SELECT_BIT should be 1 to indicate microstepping to be controlled by MSTEP register
    // and software pulse optimization (which is the default in any case)
    static const uint32_t TMC_2209_GCONF_EXT_VREF_BIT = 0;
    static const uint32_t TMC_2209_GCONF_EXT_SENSE_RES_BIT = 1;
    static const uint32_t TMC_2209_GCONF_INV_DIRN_BIT = 3;
    static const uint32_t TMC_2209_GCONF_PDN_UART_BIT = 6;
    static const uint32_t TMC_2209_GCONF_MSTEP_REG_SELECT_BIT = 7;
    static const uint32_t TMC_2209_GCONF_MULTISTEP_FILT_BIT = 8;

    // CHOPCONF register consts
    static const uint32_t TMC_2209_CHOPCONF_TOFF_BIT = 0;
    static const uint32_t TMC_2209_CHOPCONF_VSENSE_BIT = 17;
    static const uint32_t TMC_2209_CHOPCONF_MRES_BIT = 24;
    static const uint32_t TMC_2209_CHOPCONF_MRES_MASK = 0x0F000000;
    static const uint32_t TMC_2209_CHOPCONF_MRES_DEFAULT = 8;
    static const uint32_t TMC_2209_CHOPCONF_INTPOL_BIT = 28;

    // IHOLD_IRUN register consts
    static const uint32_t TMC_2209_IHOLD_BIT = 0;
    static const uint32_t TMC_2209_IRUN_BIT = 8;
    static const uint32_t TMC_2209_IHOLD_DELAY_BIT = 16;

    // PWMCONF register consts
    static const uint32_t TMC_2209_PWMCONF_PWM_OFS_BIT = 0;
    static const uint32_t TMC_2209_PWMCONF_PWM_GRAD_BIT = 8;
    static const uint32_t TMC_2209_PWMCONF_PWM_FREQ_BIT = 16;
    static const uint32_t TMC_2209_PWMCONF_AUTOSCALE_BIT = 18;
    static const uint32_t TMC_2209_PWMCONF_AUTOGRAD_BIT = 19;
    static const uint32_t TMC_2209_PWMCONF_FREEWHEEL_BIT = 20;
    static const uint32_t TMC_2209_PWMCONF_PWM_REG_BIT = 24;
    static const uint32_t TMC_2209_PWMCONF_PWM_LIM_BIT = 28;

    // PWMCONF DEFAULTS
    static const uint32_t TMC_2209_PWMCONF_PWM_OFS = 36;
    static const uint32_t TMC_2209_PWMCONF_PWM_GRAD = 0;
};
