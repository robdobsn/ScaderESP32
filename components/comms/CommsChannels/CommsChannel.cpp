/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Comms Channel
// Channels for messages from interfaces (BLE, WiFi, etc)
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <CommsChannel.h>

// Warn
#define WARN_ON_INBOUND_QUEUE_FULL

// Debug
// #define DEBUG_COMMS_CHANNEL
// #define DEBUG_COMMS_CHANNEL_CREATE_DELETE
// #define DEBUG_OUTBOUND_QUEUE
// #define DEBUG_INBOUND_QUEUE

#if defined(DEBUG_COMMS_CHANNEL) || defined(DEBUG_COMMS_CHANNEL_CREATE_DELETE) || defined(DEBUG_OUTBOUND_QUEUE) || defined(DEBUG_INBOUND_QUEUE) || defined(WARN_ON_INBOUND_QUEUE_FULL)
static const char* MODULE_PREFIX = "CommsChan";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
// xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommsChannel::CommsChannel(const char* pSourceProtocolName, 
            const char* interfaceName, 
            const char* channelName,
            CommsChannelMsgCB msgSendCallback, 
            ChannelReadyToSendCB outboundChannelReadyCB,
            const CommsChannelSettings* pSettings)
            :
            _settings(pSettings ? *pSettings : CommsChannelSettings()),
#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
            _inboundQueue(_settings.inboundBlockLen),
#endif
            _outboundQueue(_settings.outboundQueueMaxLen)
{
    _channelProtocolName = pSourceProtocolName;
    _msgSendCallback = msgSendCallback;
    _interfaceName = interfaceName;
    _channelName = channelName;
    _channelReadyCB = outboundChannelReadyCB;
    _pProtocolCodec = NULL;
    _outboundQPeak = 0;
    _inboundQPeak = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setProtocolCodec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannel::setProtocolCodec(ProtocolBase* pProtocolCodec)
{
#ifdef DEBUG_COMMS_CHANNEL_CREATE_DELETE
    LOG_I(MODULE_PREFIX, "setProtocolCodec channelID %d", 
                            pProtocolCodec->getChannelID());
#endif
    _pProtocolCodec = pProtocolCodec;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Inbound queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsChannel::canAcceptInbound()
{
#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
    return _inboundQueue.canAcceptData();
#else
    return true;
#endif
}

void CommsChannel::handleRxData(const uint8_t* pMsg, uint32_t msgLen)
{
    // Debug
#ifdef DEBUG_COMMS_CHANNEL
    LOG_I(MODULE_PREFIX, "handleRxData protocolName %s interfaceName %s channelName %s len %d handlerPtrOk %s", 
        _channelProtocolName.c_str(), 
        _interfaceName.c_str(), 
        _channelName.c_str(),
        msgLen, 
        (_pProtocolCodec ? "YES" : "NO"));
#endif
#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
    addToInboundQueue(pMsg, msgLen);
#else
if (_pProtocolCodec)
    _pProtocolCodec->addRxData(pMsg, msgLen);
#endif
}

#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
void CommsChannel::addToInboundQueue(const uint8_t* pMsg, uint32_t msgLen)
{
    ProtocolRawMsg msg(pMsg, msgLen);
#if defined(DEBUG_COMMS_CHANNEL) || defined(WARN_ON_INBOUND_QUEUE_FULL)
    bool addedOk = 
#endif
    _inboundQueue.put(msg);
    if (_inboundQPeak < _inboundQueue.count())
        _inboundQPeak = _inboundQueue.count();
#ifdef DEBUG_COMMS_CHANNEL
    LOG_I(MODULE_PREFIX, "addToInboundQueue %slen %d peak %d", addedOk ? "" : "FAILED QUEUE IS FULL ", msgLen, _inboundQPeak);
#endif
#ifdef WARN_ON_INBOUND_QUEUE_FULL
    if (!addedOk)
    {
        LOG_W(MODULE_PREFIX, "addToInboundQueue QUEUE IS FULL peak %d", _inboundQPeak);
    }
#endif
}
#endif

#ifdef COMMS_CHANNEL_USE_INBOUND_QUEUE
bool CommsChannel::getFromInboundQueue(ProtocolRawMsg& msg)
{
    return _inboundQueue.get(msg);
}
#endif

bool CommsChannel::processInboundQueue()
{
#ifndef COMMS_CHANNEL_USE_INBOUND_QUEUE
    return false;
#else
    // Peek queue
    ProtocolRawMsg msg;
    bool msgAvailable = _inboundQueue.peek(msg);
    if (!msgAvailable || !_pProtocolCodec)
        return false;

    // Check if protocol codec can handle more data
    if (!_pProtocolCodec->readyForRxData())
        return false;

    // Add data to codec
    _pProtocolCodec->addRxData(msg.getBuf(), msg.getBufLen());

#ifdef DEBUG_INBOUND_QUEUE
    LOG_I(MODULE_PREFIX, "processInboundQueue msgLen %d channelID %d protocolName %s", 
                msg.getBufLen(), 
                _pProtocolCodec ? _pProtocolCodec->getChannelID() : -1, 
                _pProtocolCodec ? _pProtocolCodec->getProtocolName() : "NULL");
#endif

    // Remove message from queue
    _inboundQueue.get(msg);
    return true;
#endif
}

void CommsChannel::addTxMsgToProtocolCodec(CommsChannelMsg& msg)
{
    if (_pProtocolCodec)
        _pProtocolCodec->encodeTxMsgAndSend(msg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Outbound queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannel::addToOutboundQueue(CommsChannelMsg& msg)
{
    _outboundQueue.put(msg);
    if (_outboundQPeak < _outboundQueue.count())
        _outboundQPeak = _outboundQueue.count();

}

bool CommsChannel::getFromOutboundQueue(CommsChannelMsg& msg)
{
    bool hasGot = _outboundQueue.get(msg);
#ifdef DEBUG_OUTBOUND_QUEUE
    if (hasGot)
    {
        LOG_I(MODULE_PREFIX, "Got from outboundQueue msgLen %d", msg.getBufLen());
    }
#endif
    return hasGot;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get info JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String CommsChannel::getInfoJSON()
{
    char jsonInfoStr[200];
    snprintf(jsonInfoStr, sizeof(jsonInfoStr),
            R"("name":"%s","if":"%s","ch":%s,"hdlr":%d,"chanID":%d,"inMax":%d,"inPk":%d,"inBlk":%d,"outMax":%d,"outPk":%d,"outBlk":%d)",
            _channelProtocolName.c_str(), _interfaceName.c_str(), _channelName.c_str(),
            _pProtocolCodec ? 1 : 0,
            _pProtocolCodec ? _pProtocolCodec->getChannelID() : -1,
            _inboundQueue.maxLen(), _inboundQPeak, _settings.inboundBlockLen,
            _outboundQueue.maxLen(), _outboundQPeak, _settings.outboundBlockLen);
    return "{" + String(jsonInfoStr) + "}";
}
