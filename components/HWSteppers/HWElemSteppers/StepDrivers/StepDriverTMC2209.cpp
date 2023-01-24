/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StepDriverTMC2209
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "StepDriverTMC2209.h"
#include "ArduinoOrAlt.h"
#include "math.h"

static const char* MODULE_PREFIX = "StepDrv2209";

#define WARN_ON_DRIVER_BUSY

#define DEBUG_IHOLD_IRUN_CALCS
#define DEBUG_PWM_FREQ_CALCS
#define DEBUG_REGISTER_WRITE
#define DEBUG_IHOLD_IRUN
// #define DEBUG_REGISTER_READ_PROCESS
// #define DEBUG_STEPPING_ONLY_IF_NOT_ISR
// #define DEBUG_DIRECTION_ONLY_IF_NOT_ISR

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StepDriverTMC2209::StepDriverTMC2209()
{
    // Sync byte
    _tmcSyncByte = TMC_2209_SYNC_BYTE;

    // Add TMC registers
    // **** Do not rearrange this without changing the order of ***
    // DriverRegisterCodes enumeration as that is used for indexing

    // Add GCONF register
    _driverRegisters.push_back({"GCONF", 0, 0x000001C0});
    // Add CHOPCONF register with default value
    _driverRegisters.push_back({"CHOPCONF", 0x6c, 0x10000050});
    // Add IHOLD_IRUN register with default value
    _driverRegisters.push_back({"IHOLD_RUN", 0x10, 0x00001f00});
    // Add PWMCONF register with default value
    _driverRegisters.push_back({"PWMCONF", 0x70, 0xC10D0024});
    // Add VACTUAL register with default value
    _driverRegisters.push_back({"VACTUAL", 0x22, 0x00000000});

    // Vars
    _dirnCurValue = false;
    _stepCurActive = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StepDriverTMC2209::setup(const String& stepperName, const StepDriverParams& stepperParams, bool usingISR)
{
    // Configure base
    StepDriverBase::setup(stepperName, stepperParams, usingISR);
    _singleWireReadWrite = true;

    // Set main registers
    setMainRegs();

    // Calculate PWMCONF_PWM_FREQ
    double clockDiv = stepperParams.pwmFreqKHz * 1000.0 / TMC_2209_CLOCK_FREQ_HZ;
    uint32_t clockFactors[] = {1024,683,512,410};
    uint32_t pwmClockVal = 0;
    for (uint32_t i = 1; i < sizeof(clockFactors)/sizeof(clockFactors[0]); i++)
    {
        double midVal = (clockFactors[i-1] + clockFactors[i]) / 2;
#ifdef DEBUG_PWM_FREQ_CALCS
        LOG_I(MODULE_PREFIX, "pwmFreq %0.2f clockDiv %f pwmClockVal %d midVal %f 2/midVal %f", 
                stepperParams.pwmFreqKHz, clockDiv, pwmClockVal, midVal, 2/midVal);
#endif
        if (clockDiv > (2 / midVal))
            pwmClockVal++;
    }

    // Init the PWMCONF register
    _driverRegisters[DRIVER_REGISTER_CODE_PWMCONF].writeRequired = true;
    _driverRegisters[DRIVER_REGISTER_CODE_PWMCONF].regWriteVal = 
                (12 << TMC_2209_PWMCONF_PWM_LIM_BIT) |
                (1 << TMC_2209_PWMCONF_PWM_REG_BIT) |
                ((stepperParams.holdMode == StepDriverParams::HOLD_MODE_FREEWHEEL) ? 1 :
                    (stepperParams.holdMode == StepDriverParams::HOLD_MODE_PASSIVE_BREAKING) ? 2 : 0) << TMC_2209_PWMCONF_FREEWHEEL_BIT |
                (1 << TMC_2209_PWMCONF_AUTOGRAD_BIT) |
                (1 << TMC_2209_PWMCONF_AUTOSCALE_BIT) |
                (pwmClockVal << TMC_2209_PWMCONF_PWM_FREQ_BIT) |
                (TMC_2209_PWMCONF_PWM_GRAD << TMC_2209_PWMCONF_PWM_GRAD_BIT) |
                (TMC_2209_PWMCONF_PWM_OFS << TMC_2209_PWMCONF_PWM_OFS_BIT);

    // Init the VACTUAL register
    _driverRegisters[DRIVER_REGISTER_CODE_VACTUAL].writeRequired = true;
    _driverRegisters[DRIVER_REGISTER_CODE_VACTUAL].regWriteVal = 0;

    // The following code gets register initial contents
    // - this is only needed if one-time-programmable (OTP) functionality is used
    // on the TMC2209 to override the factory defaults
    // Read important registers initially
    // if (!writeOnly)
    // {
    // _driverRegisters[DRIVER_REGISTER_CODE_GCONF].readPending = true;
    // _driverRegisters[DRIVER_REGISTER_CODE_CHOPCONF].readPending = true;
    // }

    // Setup step pin
    if (stepperParams.stepPin >= 0)
    {
        // Setup the pin
        pinMode(stepperParams.stepPin, OUTPUT);
        digitalWrite(stepperParams.stepPin, false);
    }

    // Setup dirn pin
    if (stepperParams.dirnPin >= 0)
    {
        pinMode(stepperParams.dirnPin, OUTPUT);
    }
    
    // Hardware is not initialised
    _hwIsSetup = true;

    // Set initial direction arbitrarily
    setDirection(false, true);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverTMC2209::service()
{
    // Service base
    StepDriverBase::service();

    // Check if driver is ready
    if (driverBusy())
    {
#ifdef WARN_ON_DRIVER_BUSY
        if (!_warnOnDriverBusyDone)
        {
            if (_warnOnDriverBusyStartTimeMs == 0)
            {
                _warnOnDriverBusyStartTimeMs = millis();
            }
            else if (Raft::isTimeout(millis(), _warnOnDriverBusyStartTimeMs, WARN_ON_DRIVER_BUSY_AFTER_MS))
            {
                LOG_E(MODULE_PREFIX, "service driver busy for too long");
                _warnOnDriverBusyStartTimeMs = 0;
                _warnOnDriverBusyDone = true;
            }
        }
#endif
        return;
    }
    else
    {
        _warnOnDriverBusyStartTimeMs = 0;
        _warnOnDriverBusyDone = false;
    }

    // Handle register activity
    if (_driverRegisterIdx >= _driverRegisters.size())
        _driverRegisterIdx = 0;

    // Check readPending
    if (_driverRegisters[_driverRegisterIdx].readInProgress)
    {
#ifdef DEBUG_REGISTER_READ_PROCESS
        LOG_I(MODULE_PREFIX, "service readinprogress");
#endif
    }
    // Check for init required
    else if (_driverRegisters[_driverRegisterIdx].writeRequired)
    {
        writeTrinamicsRegister(_driverRegisters[_driverRegisterIdx].regName.c_str(), 
                                _driverRegisters[_driverRegisterIdx].regAddr, 
                                _driverRegisters[_driverRegisterIdx].regWriteVal);
        _driverRegisters[_driverRegisterIdx].regValCur = _driverRegisters[_driverRegisterIdx].regWriteVal;
        _driverRegisters[_driverRegisterIdx].writeRequired = false;
    }
    else if (_driverRegisters[_driverRegisterIdx].readPending)
    {
        // Start reading register
#ifdef DEBUG_REGISTER_READ_PROCESS
        LOG_I(MODULE_PREFIX, "service start read regCode %d", _driverRegisterIdx);
#endif
        startReadTrinamicsRegister(_driverRegisterIdx);
        _driverRegisters[_driverRegisterIdx].readPending = false;
    }
    // Check write pending
    else if (_driverRegisters[_driverRegisterIdx].writePending)
    {
        // Write
        uint32_t newRegValue = _driverRegisters[_driverRegisterIdx].regValCur;
        newRegValue &= ~_driverRegisters[_driverRegisterIdx].writeBitsMask;
        newRegValue |= _driverRegisters[_driverRegisterIdx].writeOrValue;
        writeTrinamicsRegister(_driverRegisters[_driverRegisterIdx].regName.c_str(), 
                    _driverRegisters[_driverRegisterIdx].regAddr, newRegValue);
        _driverRegisters[_driverRegisterIdx].writePending = false;
    }

    // Increment register index
    _driverRegisterIdx++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set microsteps
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverTMC2209::setMicrosteps(uint32_t microsteps)
{
    // Set mres value into CHOPCONF register
    uint32_t mres = getMRESFieldValue(microsteps);
    setRegBits(DRIVER_REGISTER_CODE_CHOPCONF, TMC_2209_CHOPCONF_MRES_MASK, 
                mres << TMC_2209_CHOPCONF_MRES_BIT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get MRES value from microsteps
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t StepDriverTMC2209::getMRESFieldValue(uint32_t microsteps)
{
    // Get MRES value
    uint8_t mres = TMC_2209_CHOPCONF_MRES_DEFAULT;
    switch(microsteps) 
    {
        case 256: mres = 0; break;
        case 128: mres = 1; break;
        case  64: mres = 2; break;
        case  32: mres = 3; break;
        case  16: mres = 4; break;
        case   8: mres = 5; break;
        case   4: mres = 6; break;
        case   2: mres = 7; break;
        case   0: mres = 8; break;
        default: mres = TMC_2209_CHOPCONF_MRES_DEFAULT; break;
    }
    return mres;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert RMS current setting to register values
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverTMC2209::convertRMSCurrentToRegs(double reqCurrentAmps, double holdFactor, 
            StepDriverParams::HoldModeEnum holdMode, bool& vsenseOut, uint32_t& irunOut, uint32_t& iholdOut)
{
    // Total resistance
    double totalResOhms = _stepperParams.extSenseOhms + 0.02;

    // IRUN value calculation
    double maxSenseVoltage = reqCurrentAmps * totalResOhms * 1.41;

    // VRef values
    double vRefLowSens = 0.325;
    double vRefHighSens = 0.180;

    // Calculate a vsense value
    vsenseOut = maxSenseVoltage < vRefHighSens;

    // Vfs
    double vFS = vsenseOut ? vRefHighSens : vRefLowSens;

    // Calculate IRUN
    double irunDouble = maxSenseVoltage / vFS;
    irunOut = uint32_t(ceil(irunDouble * 32)-1);
    if (irunOut < 8)
        irunOut = 8;
    else if (irunOut > 31)
        irunOut = 31;

    // IHOLD value
    iholdOut = 0;
    if (holdMode == StepDriverParams::HOLD_MODE_FACTOR)
    {
        uint32_t iholdVal = irunOut * holdFactor;
        if (iholdVal < 1)
            iholdOut = 1;
        else if (iholdVal > 31)
            iholdOut = 31;
        else
            iholdOut = iholdVal;
    }

#ifdef DEBUG_IHOLD_IRUN_CALCS
    LOG_I(MODULE_PREFIX, "convertRMSCurrentToRegs reqCurAmps %0.2f holdMode %d holdFactor %0.2f vsenseOut %d irunOut %d iholdOut %d maxSenseVoltage %.2f Vfs %.2f",
            reqCurrentAmps, holdMode, holdFactor, vsenseOut, irunOut, iholdOut, maxSenseVoltage, vFS);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set direction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR StepDriverTMC2209::setDirection(bool dirn, bool forceSet)
{
#ifdef DEBUG_DIRECTION_ONLY_IF_NOT_ISR
    if (!_usingISR)
    {
        LOG_I(MODULE_PREFIX, "setDirection %d logicalDirection %d forceSet %d", _stepperParams.dirnPin, dirn, forceSet);
    }
#endif        

    // Check if there is a hardware pin for this and use that if so
    if (_hwIsSetup && (_stepperParams.dirnPin >= 0) && ((dirn != _dirnCurValue) || forceSet))
    {
        // Check if direction reversal is done via the serial bus and reverse if applicable
        bool hwDirn = dirn;
        if (!_useBusForDirectionReversal && _stepperParams.invDirn)
            hwDirn = !dirn;
#ifdef DEBUG_DIRECTION_ONLY_IF_NOT_ISR
    if (!_usingISR)
    {
        LOG_I(MODULE_PREFIX, "setDirection %d logicalDirection %d hwDirn %d", _stepperParams.dirnPin, dirn, hwDirn);
    }
#endif        
        // Set the pin value
        digitalWrite(_stepperParams.dirnPin, hwDirn);
    }
    _dirnCurValue = dirn;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start a step
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR StepDriverTMC2209::stepStart()
{
    // Check hardware pin
    if (_hwIsSetup && (_stepperParams.stepPin >= 0))
    {
#ifdef DEBUG_STEPPING_ONLY_IF_NOT_ISR
        if (!_usingISR)
        {
            LOG_I(MODULE_PREFIX, "stepStart %d", _stepperParams.stepPin);
        }
#endif
        // Set the pin value
        digitalWrite(_stepperParams.stepPin, true);
        _stepCurActive = true;
    }
    else
    {
#ifdef DEBUG_STEPPING_ONLY_IF_NOT_ISR
        if (!_usingISR)
        {
            LOG_W(MODULE_PREFIX, "stepStart FAILED pin %d hwIsSetup %d", _stepperParams.stepPin, _hwIsSetup);
        }
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End a step
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool IRAM_ATTR StepDriverTMC2209::stepEnd()
{
    if (_stepCurActive && (_stepperParams.stepPin >= 0))
    {
        _stepCurActive = false;
        digitalWrite(_stepperParams.stepPin, false);
#ifdef DEBUG_STEPPING_ONLY_IF_NOT_ISR
        if (!_usingISR)
        {
            LOG_I(MODULE_PREFIX, "stepEnd %d", _stepperParams.stepPin);
        }
#endif
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the max motor current
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverTMC2209::setMaxMotorCurrentAmps(float maxMotorCurrentAmps)
{
    // Set the max motor current
    _stepperParams.rmsAmps = maxMotorCurrentAmps;

    // setMaxMotorCurrentAmps
    LOG_I(MODULE_PREFIX, "setMaxMotorCurrentAmps %0.2f", maxMotorCurrentAmps);

    // Set the current
    setMainRegs();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the motor current
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverTMC2209::setMainRegs()
{
    // Get CHOPCONF vsense value and IRUN, IHOLD values from required RMS current
    bool vsenseValue = false;
    uint32_t irunValue = 0;
    uint32_t iholdValue = 0;
    convertRMSCurrentToRegs(_stepperParams.rmsAmps, _stepperParams.holdFactor, _stepperParams.holdMode, 
                    vsenseValue, irunValue, iholdValue);

    // Init the GCONF register
    _driverRegisters[DRIVER_REGISTER_CODE_GCONF].writeRequired = true;
    _driverRegisters[DRIVER_REGISTER_CODE_GCONF].regWriteVal =
                (1 << TMC_2209_GCONF_MULTISTEP_FILT_BIT) |
                (1 << TMC_2209_GCONF_PDN_UART_BIT) |
                (_stepperParams.invDirn ? (1 << TMC_2209_GCONF_INV_DIRN_BIT) : 0) |
                ((_stepperParams.extSenseOhms < 0.01) ? (1 << TMC_2209_GCONF_EXT_SENSE_RES_BIT) : 0) |
                (_stepperParams.extVRef ? (1 << TMC_2209_GCONF_EXT_VREF_BIT) : 0) |
                (_stepperParams.extMStep ? 0 : (1 << TMC_2209_GCONF_MSTEP_REG_SELECT_BIT));

    // Init the CHOPCONF register
    _driverRegisters[DRIVER_REGISTER_CODE_CHOPCONF].writeRequired = true;
    _driverRegisters[DRIVER_REGISTER_CODE_CHOPCONF].regWriteVal =
                (getMRESFieldValue(_stepperParams.microsteps) << TMC_2209_CHOPCONF_MRES_BIT) |
                (StepDriverParams::TOFF_VALUE_DEFAULT << TMC_2209_CHOPCONF_TOFF_BIT) |
                (_stepperParams.intpol ? (1 << TMC_2209_CHOPCONF_INTPOL_BIT) : 0) |
                (vsenseValue ? (1 << TMC_2209_CHOPCONF_VSENSE_BIT) : 0);

    // Init the IHOLD_IRUN register
    _driverRegisters[DRIVER_REGISTER_CODE_IHOLD_IRUN].writeRequired = true;
    _driverRegisters[DRIVER_REGISTER_CODE_IHOLD_IRUN].regWriteVal = 
                (irunValue << TMC_2209_IRUN_BIT) |
                (iholdValue << TMC_2209_IHOLD_BIT) |
                (_stepperParams.holdDelay << TMC_2209_IHOLD_DELAY_BIT);
#ifdef DEBUG_IHOLD_IRUN
    LOG_I(MODULE_PREFIX, "setMainRegs irunValue %d iholdValue %d, reg %s(0x%02x), val %08x", 
                    irunValue, iholdValue, 
                    _driverRegisters[DRIVER_REGISTER_CODE_IHOLD_IRUN].regName,
                    DRIVER_REGISTER_CODE_IHOLD_IRUN,
                    _driverRegisters[DRIVER_REGISTER_CODE_IHOLD_IRUN].regWriteVal);
#endif

    // Set flags to indicate that registers should be read back to confirm
    _driverRegisters[DRIVER_REGISTER_CODE_GCONF].readPending = true;
    _driverRegisters[DRIVER_REGISTER_CODE_CHOPCONF].readPending = true;
    // Note that IHOLD_IRUN is not read back as it is read only
    _driverRegisters[DRIVER_REGISTER_CODE_PWMCONF].readPending = true;
}
