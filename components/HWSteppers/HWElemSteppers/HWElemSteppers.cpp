/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Element Steppers
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "HWElemSteppers.h"
#include <Logger.h>

static const char *MODULE_PREFIX = "HWElemSteppers";

#define DEBUG_STEPPER_CMD_JSON

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWElemSteppers::HWElemSteppers()
{
}

HWElemSteppers::~HWElemSteppers()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemSteppers::setup(ConfigBase &config, ConfigBase* pDefaults, const char* pConfigPrefix)
{
    // Base setup
    HWElemBase::setup(config, pDefaults, pConfigPrefix);

    // Setup motion controller
    _motionController.setup(config, pConfigPrefix);

    // Debug
    LOG_I(MODULE_PREFIX, "setup prefix %s name %s type %s bus %s pollRateHz %f",
            pConfigPrefix, _name.c_str(), _type.c_str(), _busName.c_str(), _pollRateHz);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Post-Setup - called after any buses have been connected
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemSteppers::postSetup()
{
    // If HWElem is configured with a bus then use soft commands for direction reversal
    _motionController.setupSerialBus(getBus(), getBus() != nullptr); 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemSteppers::service()
{
    // Service motion controller
    _motionController.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Has capability
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWElemSteppers::hasCapability(const char* pCapabilityStr)
{
    switch(pCapabilityStr[0])
    {
        // Streaming outbound
        case 's': return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Data JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String HWElemSteppers::getDataJSON(HWElemStatusLevel_t level)
{
    // Get data
    return _motionController.getDataJSON(level);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a named value
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double HWElemSteppers::getNamedValue(const char* param, bool& isFresh)
{
    switch(tolower(param[0]))
    {
        case 'x':
        case 'y':
        case 'z':
        {
            // Get axis position
            isFresh = true;
            AxesPosValues pos = _motionController.getLastPos();
            switch(tolower(param[0]))
            {
                case 'x': return pos.getVal(0);
                case 'y': return pos.getVal(1);
                case 'z': return pos.getVal(2);
            }
            isFresh = false;
            return 0;
        }
        case 'b':
        {
            // Check for busy
            isFresh = true;
            return _motionController.isBusy();
            break;
        }
        default: { isFresh = false; return 0; }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get values binary = format specific to hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t HWElemSteppers::getValsBinary(uint32_t formatCode, uint8_t* pBuf, uint32_t bufMaxLen)
{
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send encoded command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode HWElemSteppers::sendCmdBinary(uint32_t formatCode, const uint8_t* pData, uint32_t dataLen)
{
    // Check format code
    if (formatCode == MULTISTEPPER_CMD_BINARY_FORMAT_1)
    {
        // Check length ok
        if (dataLen < MULTISTEPPER_OPCODE_POS + 1)
            return UtilsRetCode::INVALID_DATA;

        // Check op-code
        switch(pData[MULTISTEPPER_OPCODE_POS])
        {
            case MULTISTEPPER_MOVETO_OPCODE:
            {
                handleCmdBinary_MoveTo(pData + MULTISTEPPER_OPCODE_POS + 1, dataLen - MULTISTEPPER_OPCODE_POS - 1);
                break;
            }
        }
    }
    return UtilsRetCode::OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle JSON command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode HWElemSteppers::sendCmdJSON(const char* cmdJSON)
{
    // Extract command from JSON
    JSONParams jsonInfo(cmdJSON);
    String cmd = jsonInfo.getString("cmd", "");
    if (cmd.equalsIgnoreCase("motion"))
    {
    MotionArgs motionArgs;
    motionArgs.fromJSON(cmdJSON);
#ifdef DEBUG_STEPPER_CMD_JSON
    String cmdStr = motionArgs.toJSON();
    LOG_I(MODULE_PREFIX, "sendCmdJSON %s", cmdStr.c_str());
#endif
    _motionController.moveTo(motionArgs);
    } 


    // LOG_I(MODULE_PREFIX, "sendCmdJSON %s", cmdJSON);
    
    // // Extract command from JSON
    // ConfigBase jsonInfo(cmdJSON);
    // String cmd = jsonInfo.getString("cmd", "");

    // // Check for raw command
    // if (cmd.equals("test"))
    // {
    //     pinMode(12, OUTPUT);
    //     digitalWrite(12, 0);
    //     pinMode(15, OUTPUT);
    //     pinMode(27, OUTPUT);
    //     digitalWrite(27, 0);
    //     delay(1);
    //     for (int i = 0; i < 10000; i++)
    //     {
    //         // if (_pStepperDriver)
    //         //     _pStepperDriver->stepStart();
    //         // delayMicroseconds(100);
    //         // if (_pStepperDriver)
    //         //     _pStepperDriver->stepEnd();
    //         // delayMicroseconds(100);
    //         digitalWrite(15,0);
    //         delay(1);
    //         digitalWrite(15,1);
    //         delay(1);
    //     }
    // }    
    // Check command
    // Extract command from JSON
//     ConfigBase jsonInfo(cmdJSON);
//     String cmd = jsonInfo.getString("cmd", "");
//     if (cmd.equalsIgnoreCase("gcode"))
//     {
//         // Extract gcode from JSON if present
//         String gCode = jsonInfo.getString("g", "");
//         gCode.trim();

// #ifdef DEBUG_GCODE_CMD
//     LOG_I(MODULE_PREFIX, "sendCmdJSON gcode %s", gCode.c_str());
// #endif

//         // Check for raw command
//         if (gCode.length() != 0)
//         {
//             // Parse gcode
//             _evaluatorGCode.interpretGcode(gCode);
//         }
//     }
    return UtilsRetCode::OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle MoveTo
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemSteppers::handleCmdBinary_MoveTo(const uint8_t* pData, uint32_t dataLen)
{
    // Check length ok
    if (dataLen < MULTISTEPPER_MOVETO_BINARY_FORMAT_POS + 1)
        return;
    
    // Check version of args
    if (pData[MULTISTEPPER_MOVETO_BINARY_FORMAT_POS] != MULTISTEPPER_MOTION_ARGS_BINARY_FORMAT_1)
        return;

    // Check args length
    if (dataLen < sizeof(MotionArgs))
        return;

    // Send the request for interpretation
    _motionController.moveTo((const MotionArgs&)*pData);
}

