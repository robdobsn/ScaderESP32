/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Element INA219
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "HWElemINA219.h"
#include <Logger.h>
#include <Utils.h>
#include "BusRequestResult.h"

static const char *MODULE_PREFIX = "HWElemINA219";

// Init command(s)
const std::vector<HWElemReq> HWElemINA219::_initCmds = {
    // {{0x00,0x39,0x9f}, 0, 0, "Init", 0},
    {{0x05,0x40,0x00}, 0, 0, "Cal", 0},
    };

// Status command(s)
const std::vector<HWElemReq> HWElemINA219::_statusCmds = 
            {{{0x04}, BYTES_TO_READ_FOR_INA219_STATUS, HWElemReq::UNNUM, "Status", 0}};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWElemINA219::HWElemINA219()
{
    _pollReqSent = false;
}

HWElemINA219::~HWElemINA219()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemINA219::setup(ConfigBase &config, ConfigBase* pDefaults)
{
    // Base setup
    HWElemBase::setup(config, pDefaults);

    // Debug
    LOG_I(MODULE_PREFIX, "name %s type %s bus %s pollRateHz %f",
              _name.c_str(), _type.c_str(), _busName.c_str(), _pollRateHz);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Post-Setup - called after any buses have been connected
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemINA219::postSetup()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemINA219::service()
{
    if (isElemResponding() && !_pollReqSent)
    {
        // Send init commands
        for (const HWElemReq& hwElemReq : _initCmds)
        {
            if (!makeBusRequest(hwElemReq, initResultCallbackStatic, this)) {
                LOG_W(MODULE_PREFIX, "sendInitCommands failed addr %02x", _address);
            }
        }


        // Send polling requests
        for (const HWElemReq &hwElemReq : _statusCmds)
        {
            LOG_I(MODULE_PREFIX, "setupPollingRequest readLen %d", hwElemReq._readReqLen);
            if (!makeBusRequest(hwElemReq, pollResultCallbackStatic, this, BUS_REQ_TYPE_POLL)) 
            {
                LOG_E(MODULE_PREFIX, "setupPollingRequest failed");
            }
        }
        _pollReqSent = true;
    }
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Has capability
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWElemINA219::hasCapability(const char* pCapabilityStr)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Data JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String HWElemINA219::getDataJSON(HWElemBase::ElemStatusLevel_t level)
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a named value
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double HWElemINA219::getNamedValue(const char* param, bool& isFresh)
{
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get values binary = format specific to hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t HWElemINA219::getValsBinary(uint32_t formatCode, uint8_t* pBuf, uint32_t bufMaxLen)
{
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send encoded command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode HWElemINA219::sendCmdBinary(uint32_t formatCode, const uint8_t* pData, uint32_t dataLen)
{
    return UtilsRetCode::OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle JSON command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode HWElemINA219::sendCmdJSON(const char* cmdJSON)
{
    return UtilsRetCode::OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemINA219::pollResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult)
{
    if (pCallbackData)
        ((HWElemINA219 *)pCallbackData)->pollResultCallback(reqResult);
}

void HWElemINA219::pollResultCallback(BusRequestResult& reqResult)
{
    // Check for failure
    if (!reqResult.isResultOk())
    {
        LOG_W(MODULE_PREFIX, "polling failed - reinit required");
        return;
    }

    // Access the read data
    const uint8_t* pData = reqResult.getReadData();
    if (!pData)
        return;

// #ifdef DEBUG_SMART_SERVO_RX_DATA
    // Debug
    int sz = reqResult.getReadDataLen();
    uint8_t* pVals = reqResult.getReadData();
    String debugStr;
    Raft::getHexStrFromBytes(pVals, sz, debugStr);
    LOG_I(MODULE_PREFIX, "polling data %s", debugStr.c_str());
// #endif

    uint32_t readDataLen = reqResult.getReadDataLen();
    if (readDataLen != BYTES_TO_READ_FOR_INA219_STATUS)
    {
        LOG_E(MODULE_PREFIX, "polling data len %d != %d", readDataLen, BYTES_TO_READ_FOR_INA219_STATUS);
        return;
    }
    else
    {
        // Decode the status data
        double ina219Current = (pData[0] << 8) | pData[1];
        double convCurrent = ina219Current / 0x4000;
        LOG_I(MODULE_PREFIX, "ina219Current %.0f convCurrent %.0fmA", ina219Current, convCurrent * 1000);

//         const uint8_t* pEndStop = pData + readDataLen;
//         double pos = Raft::getBEInt16AndInc(pData, pEndStop) / 10.0;
//         int8_t current = Raft::getInt8AndInc(pData, pEndStop);
//         uint8_t state = Raft::getUint8AndInc(pData, pEndStop);
//         int16_t velocity = Raft::getBEInt16AndInc(pData, pEndStop);

//         // Set data
//         bool isCurrLimInstant = HWDataServo::isCurrLimInstant(state);
//         bool isCurrLimSustained = HWDataServo::isCurrLimSustained(state);
//         _hwData.setFromHw(pos, current, state, velocity);

//         // Check for current over-range
//         if (_overCurrentSafetyEnabled && (isCurrLimInstant || isCurrLimSustained))
//         {
//             if (!_isOverCurrent)
//             {
// #ifdef DEBUG_SERVO_OVERCURRENT_IN_POLL_CB_ON_BUS_THREAD
//                 LOG_I(MODULE_PREFIX, "Servo overcurrent IDNo %d", _IDNo);
// #endif
//                 // Set safe mode on this servo
//                 _safeMode = true;
//                 _overCurrentStartMs = millis();
//                 if (_handleSafetiesCallback)
//                     _handleSafetiesCallback(true, HWELEM_SAFETY_OVER_CURRENT_MASK, _IDNo);
//             }
//             _isOverCurrent = true;
//         } else {
//             _isOverCurrent = false;
//         }

//         // Reset timeout to stay in the polling state
//         _deviceConnTimerMs = millis();

//         // LOG_I(MODULE_PREFIX, "Servo #%d Poll Pos %.1f Current %d State %02x Velocity %d", 
//         //             _IDNo, pos, current, state, velocity);
    }
}

// Init result callback
void HWElemINA219::initResultCallbackStatic(void *pCallbackData, BusRequestResult &reqResult)
{
    if (pCallbackData)
        ((HWElemINA219 *)pCallbackData)->initResultCallback(reqResult);
}

// Init result callback
void HWElemINA219::initResultCallback(BusRequestResult &reqResult)
{
    if (!reqResult.isResultOk())
    {
        LOG_W(MODULE_PREFIX, "initCB addr %02x FAILED", _address);
        return;
    }
}

