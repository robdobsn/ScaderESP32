/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Elem Base
// Base for hardwware elements - like a smart-servo - that can accept and receive messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "HWElemBase.h"
#include <BusRequestInfo.h>
#include <BusRequestResult.h>
#include <RICRESTMsg.h>
#include <CommsChannelManager.h>
#include <ConfigBase.h>
#include <RaftUtils.h>
#include "ArduinoOrAlt.h"

static const char *MODULE_PREFIX = "HWElemBase";

// #define DEBUG_CMD_JSON

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWElemBase::HWElemBase()
{
    _pBus = nullptr;
    setPollRateAndTimeout(10.0);
    _versionStr = "0.0.0";
    _IDNo = 0;
    _address = 0;
    _addressIsSet = false;
    _whoAmITypeCode = WHOAMI_TYPE_CODE_NONE;
    _addOnFamily = ADD_ON_FAMILY_ORDINARY_I2C;
    _pCommsChannelManager = nullptr;
}

HWElemBase::~HWElemBase()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemBase::setup(ConfigBase& config, ConfigBase* pDefaults)
{
    // Get settings
    _name = config.getString("name", _name.c_str());
    _type = config.getString("type", _type.c_str());
    _busName = getStringWithDefault("bus", "", config, pDefaults);
    if (!_addressIsSet)
    {
        uint32_t address = strtoul(config.getString("addr", "0xffffffff").c_str(), nullptr, 0);
        if (address != 0xffffffff)
        {
            _address = address;
            _addressIsSet = true;
        }
    }
    _IDNo = config.getLong("IDNo", -1);

    // Polling details
    _pollFor = getStringWithDefault("poll", "", config, pDefaults);
    setPollRateAndTimeout(getDoubleWithDefault("pollHz", 10, config, pDefaults));

    // Queued bus requests
    _queuedBusReqsActive = false;
    _queuedBusReqLastStartMs = 0;
    _queuedBusReqHoldOffMs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemBase::service()
{
    // Handle queued requests
    if (_queuedBusReqsActive && (_queuedBusReqs.size() > 0))
    {
        // Delay slightly longer than requested to ensure the barring 
        // that is done at a lower level is released
        if (Raft::isTimeout(millis(), _queuedBusReqLastStartMs, _queuedBusReqHoldOffMs + 20))
        {
            // Get request
            HWElemReq& req = _queuedBusReqs.front();

            // Make request
            makeBusRequest(req, nullptr, nullptr);

            // Handle hold-off if required
            _queuedBusReqHoldOffMs = req._barAccessAfterSendMs;
            _queuedBusReqLastStartMs = millis();

            // Remove from queue
            _queuedBusReqs.pop_front();
            if (_queuedBusReqs.size() == 0)
                _queuedBusReqsActive = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Make bus request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWElemBase::makeBusRequest(const HWElemReq& hwElemReq, BusRequestCallbackType callback, 
                void* callbackParam, BusReqType reqType)
{
    if (!_pBus)
        return false;
    BusRequestInfo busReqInfo(_name, _address);
    busReqInfo.set(reqType, hwElemReq, _pollRateHz, callback, callbackParam);
    return _pBus->addRequest(busReqInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Queue bus requests for sending
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemBase::queuedBusReqClear()
{
    _queuedBusReqsActive = false;
    _queuedBusReqs.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add bus request to queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWElemBase::queuedBusReqAdd(const HWElemReq& hwElemReq, BusReqType reqType)
{
    if (_queuedBusReqs.size() >= MAX_QUEUED_BUS_REQS)
        return false;
    _queuedBusReqs.push_back(hwElemReq);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start queued bus request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HWElemBase::queuedBusReqStart(BusRequestCallbackType callback, void* callbackParam)
{
    // Start the queue processing
    _queuedBusReqsActive = true;
    _queuedBusReqLastStartMs = millis();
    _queuedBusReqHoldOffMs = 0;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle JSON command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode HWElemBase::sendCmdJSON(const char* cmdJSON)
{
    // Extract command from JSON
    ConfigBase jsonInfo(cmdJSON);
    String cmd = jsonInfo.getString("cmd", "");

#ifdef DEBUG_CMD_JSON
    LOG_I(MODULE_PREFIX, "sendCmdJSON cmd %s, cmdJSON %s", cmd.c_str(), cmdJSON);
#endif

    // Check for raw command
    if (cmd.equals("raw"))
    {
        // Get args
        String hexWriteData = jsonInfo.getString("hexWr", "");
        int numBytesToRead = jsonInfo.getLong("numToRd", 0);
        String msgKey = jsonInfo.getString("msgKey", "");

        // Get bytes to write
        uint8_t writeBytes[MAX_RAW_WRITE_BYTES];
        uint32_t writeBytesLen = Raft::getBytesFromHexStr(hexWriteData.c_str(), writeBytes, MAX_RAW_WRITE_BYTES);

        // Make request
        std::vector<uint8_t> writeVec(writeBytes, writeBytes+writeBytesLen);
        HWElemReq hwElemReq = {writeVec, numBytesToRead, SEND_CMD_JSON_REQ, "SendCmdJson", 0};
        if (!makeBusRequest(hwElemReq, cmdResultCallbackStatic, this))
        {
            LOG_W(MODULE_PREFIX, "sendCmdJSON failed send raw command");

            // TODO 2022 should we return UtilsRetCode::BUSY here?
        }

        // Store the msg key for response
        _cmdResponseMsgKey = msgKey;

        // Debug
#ifdef DEBUG_CMD_JSON
        LOG_I(MODULE_PREFIX, "sendCmdJson hexWriteData %s numToRead %d", hexWriteData.c_str(), numBytesToRead);
#endif
        return UtilsRetCode::OK;
    }
    else if (cmd.equals("bin"))
    {
        // Get args
        String hexWriteData = jsonInfo.getString("hexWr", "");
        int formatCode = jsonInfo.getLong("formatCode", 0);

        // Get bytes to write
        uint8_t writeBytes[MAX_RAW_WRITE_BYTES];
        uint32_t writeBytesLen = Raft::getBytesFromHexStr(hexWriteData.c_str(), writeBytes, MAX_RAW_WRITE_BYTES);

        // Make request
        UtilsRetCode::RetCode retc = sendCmdBinary(formatCode, writeBytes, writeBytesLen);

        // Debug
#ifdef DEBUG_CMD_JSON
        LOG_I(MODULE_PREFIX, "sendCmdJson bin hexWriteData %s retc %s", hexWriteData.c_str(),
                HWElemBase::RetcToString(retc));
#endif
        return retc;
    }
    return UtilsRetCode::INVALID_OPERATION;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cmd result callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HWElemBase::cmdResultCallbackStatic(void *pCallbackData, BusRequestResult &reqResult)
{
    if (pCallbackData)
        ((HWElemBase *)pCallbackData)->cmdResultCallback(reqResult);
}

// Cmd result callback
void HWElemBase::cmdResultCallback(BusRequestResult &reqResult)
{
#ifdef DEBUG_CMD_JSON
    LOG_I(MODULE_PREFIX, "cmdResultCallback addr 0x%02x len %d", _address, reqResult.getReadDataLen());
    Raft::logHexBuf(reqResult.getReadData(), reqResult.getReadDataLen(), MODULE_PREFIX, "cmdResultCallback");
#endif

    // Generate hex response
    String hexResp;
    Raft::getHexStrFromBytes(reqResult.getReadData(), reqResult.getReadDataLen(), hexResp);

    // Report
    RICRESTMsg ricRESTMsg;
    ricRESTMsg.setElemCode(RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
    CommsChannelMsg endpointMsg(MSG_CHANNEL_ID_ALL, MSG_PROTOCOL_RICREST, 0, MSG_TYPE_REPORT);
    char msgBuf[200];
    snprintf(msgBuf, sizeof(msgBuf), R"({"msgType":"raw","hexRd":"%s","elemName":"%s","IDNo":%d,"msgKey":"%s","addr":"0x%02x"})", 
                hexResp.c_str(), _name.c_str(), _IDNo, _cmdResponseMsgKey.c_str(), _address);
    ricRESTMsg.encode(msgBuf, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);

    // Send message on the appropriate channel
    if (_pCommsChannelManager)
        _pCommsChannelManager->handleOutboundMessage(endpointMsg);
}

