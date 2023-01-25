/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Comms Channel Manager
// Manages channels for comms messages
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CommsChannelManager.h"
#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <CommsChannelMsg.h>

static const char* MODULE_PREFIX = "CommsMan";

// #define WARN_ON_NO_CHANNEL_MATCH
// #define DEBUG_OUTBOUND_NON_PUBLISH
// #define DEBUG_OUTBOUND_PUBLISH
// #define DEBUG_INBOUND_MESSAGE
// #define DEBUG_COMMS_MANAGER_SERVICE
// #define DEBUG_CHANNEL_ID
// #define DEBUG_PROTOCOL_CODEC
// #define DEBUG_FRAME_SEND
// #define DEBUG_REGISTER_CHANNEL
// #define DEBUG_OUTBOUND_MSG_ALL_CHANNELS
// #define DEBUG_INBOUND_BLOCK_MAX

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommsChannelManager::CommsChannelManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
}

CommsChannelManager::~CommsChannelManager()
{
    // Clean up 
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is used
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;
        ProtocolBase* pCodec = pChannel->getProtocolCodec();
        if (pCodec)
            delete pCodec;
        // Delete the channel
        delete pChannel;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::service()
{
    // Pump comms queues
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is used
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;

        // Outbound messages - check if interface is ready
        bool noConn = false;
        bool canAccept = pChannel->canAcceptOutbound(channelID, noConn);

        // When either canAccept or no-connection get any message to be sent
        // In the case of noConn this is so that the queue doesn't block
        // when the connection is not present
        if (canAccept || noConn)
        {
            // Check for outbound message
            CommsChannelMsg msg;
            if (pChannel->getFromOutboundQueue(msg))
            {
                // Check if we can send
                if (canAccept)
                {
                    // Ensure protocol handler exists
                    ensureProtocolCodecExists(channelID);

                // Debug
#ifdef DEBUG_COMMS_MANAGER_SERVICE
                    LOG_I(MODULE_PREFIX, "service, got msg channelID %d, msgType %s msgNum %d, len %d",
                        msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen());
#endif
                    // Handle the message
                    pChannel->addTxMsgToProtocolCodec(msg);
#ifdef DEBUG_COMMS_MANAGER_SERVICE
                    LOG_I(MODULE_PREFIX, "service, msg sent channelID %d, msgType %s msgNum %d, len %d",
                        msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen());
#endif
                }
            }
        }

        // Inbound messages - possibly multiple messages
        for (int msgIdx = 0; msgIdx < MAX_INBOUND_MSGS_IN_LOOP; msgIdx++)
        {
            if (!pChannel->processInboundQueue())
                break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Register a channel
// The blockMax and queueMaxLen values can be left at 0 for default values to be applied
// Returns channel ID
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t CommsChannelManager::registerChannel(const char* protocolName, 
                    const char* interfaceName, const char* channelName,
                    CommsChannelMsgCB msgCB, 
                    ChannelReadyToSendCB outboundChannelReadyCB,
                    const CommsChannelSettings* pSettings)
{
    // Create new command definition and add
    CommsChannel* pCommsChannel = new CommsChannel(protocolName, interfaceName, channelName, msgCB, outboundChannelReadyCB, pSettings);
    if (pCommsChannel)
    {
        // Add to vector
        _commsChannelVec.push_back(pCommsChannel);

        // Channel ID is position in vector
        uint32_t channelID = _commsChannelVec.size() - 1;

#ifdef DEBUG_REGISTER_CHANNEL
        LOG_I(MODULE_PREFIX, "registerChannel protocolName %s interfaceName %s channelID %d", 
                    protocolName, interfaceName, channelID);
#endif

        // Return channelID
        return channelID;
    }

    // Failed to create channel
    LOG_W(MODULE_PREFIX, "registerChannel FAILED protocolName %s interfaceName %s", 
                protocolName, interfaceName);
    return CommsCoreIF::CHANNEL_ID_UNDEFINED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add protocol handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::addProtocol(ProtocolCodecFactoryHelper& codecFactoryHelper)
{
    LOG_D(MODULE_PREFIX, "Adding protocol for %s", codecFactoryHelper.protocolName.c_str());
    _protocolCodecFactoryList.push_back(codecFactoryHelper);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get channel ID by name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t CommsChannelManager::getChannelIDByName(const String& channelName, const String& protocolName)
{
    // Iterate the channels list to find a match
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is used
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;

#ifdef DEBUG_CHANNEL_ID
        LOG_I(MODULE_PREFIX, "Testing chName %s with %s protocol %s with %s", 
                    pChannel->getChannelName().c_str(), channelName.c_str(),
                    pChannel->getSourceProtocolName().c_str(), protocolName.c_str());
#endif
        if (pChannel->getChannelName().equalsIgnoreCase(channelName) &&
                        (pChannel->getSourceProtocolName().equalsIgnoreCase(protocolName)))
            return channelID;
    }
#ifdef WARN_ON_NO_CHANNEL_MATCH
    LOG_W(MODULE_PREFIX, "getChannelID noMatch chName %s protocol %s", channelName.c_str(), protocolName.c_str());
#endif
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get channel IDs by interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::getChannelIDsByInterface(const char* interfaceName, std::vector<uint32_t>& channelIDs)
{
    // Iterate the endpoints list to find matches
    channelIDs.clear();
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is used
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;

#ifdef DEBUG_CHANNEL_ID
        LOG_I(MODULE_PREFIX, "Testing interface %s with %s", 
                    pChannel->getInterfaceName().c_str(), interfaceName);
#endif
        if (pChannel->getInterfaceName().equalsIgnoreCase(interfaceName))
            channelIDs.push_back(channelID);
    }
#ifdef WARN_ON_NO_CHANNEL_MATCH
    LOG_W(MODULE_PREFIX, "getChannelID interface %s returning %d IDs", interfaceName, channelIDs.size());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get channel IDs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::getChannelIDs(std::vector<uint32_t>& channelIDs)
{
    // Iterate and check
    channelIDs.clear();
    channelIDs.reserve(_commsChannelVec.size());
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is valid
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;
        channelIDs.push_back(channelID);
    }
    channelIDs.shrink_to_fit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Check if we can accept inbound message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsChannelManager::canAcceptInbound(uint32_t channelID)
{
    // Check the channel
    if (channelID >= _commsChannelVec.size())
        return false;

    // Check if channel is used
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if (!pChannel)
        return false;

    // Ensure we have a handler
    ensureProtocolCodecExists(channelID);

    // Check validity
    return pChannel->canAcceptInbound();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle channel message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::handleInboundMessage(uint32_t channelID, const uint8_t* pMsg, uint32_t msgLen)
{
    // Check the channel
    if (channelID >= _commsChannelVec.size())
    {
        LOG_W(MODULE_PREFIX, "handleInboundMessage, channelID channelId %d is INVALID msglen %d", channelID, msgLen);
        return;
    }

    // Check if channel is used
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if (!pChannel)
    {
        LOG_W(MODULE_PREFIX, "handleInboundMessage, channelID channelId %d is NULL msglen %d", channelID, msgLen);
        return;
    }

    // Debug
#ifdef DEBUG_INBOUND_MESSAGE
    LOG_I(MODULE_PREFIX, "handleInboundMessage, channel Id %d channel name %s, msglen %d", channelID, 
                pChannel->getInterfaceName().c_str(), msgLen);
#endif

    // Ensure we have a handler for this msg
    ensureProtocolCodecExists(channelID);

    // Handle the message
    pChannel->handleRxData(pMsg, msgLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Check if we can accept outbound message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsChannelManager::canAcceptOutbound(uint32_t channelID, bool& noConn)
{
    // Check the channel
    if (channelID >= _commsChannelVec.size())
        return false;

    // Check if channel is used
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if (!pChannel)
        return false;

    // Ensure we have a handler
    ensureProtocolCodecExists(channelID);

    // Check validity
    return pChannel->canAcceptOutbound(channelID, noConn);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle outbound message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::handleOutboundMessage(CommsChannelMsg& msg)
{
    // Get the channel
    uint32_t channelID = msg.getChannelID();
    if (channelID < _commsChannelVec.size())
    {
        // Send to one channel
        handleOutboundMessageOnChannel(msg, channelID);
    }
    else if (channelID == MSG_CHANNEL_ID_ALL)
    {
        // Send on all open channels with an appropriate protocol
        std::vector<uint32_t> channelIDs;
        getChannelIDs(channelIDs);
        for (uint32_t specificChannelID : channelIDs)
        {
            msg.setChannelID(specificChannelID);
            handleOutboundMessageOnChannel(msg, specificChannelID);
#ifdef DEBUG_OUTBOUND_MSG_ALL_CHANNELS
            CommsChannel* pChannel = _commsChannelVec[specificChannelID];
            LOG_W(MODULE_PREFIX, "handleOutboundMessage, all chanId %u channelName %s msglen %d", 
                            specificChannelID, pChannel ? pChannel->getInterfaceName().c_str() : "UNKNONW", msg.getBufLen());
#endif            
        }
    }
    else
    {
        LOG_W(MODULE_PREFIX, "handleOutboundMessage, channelID INVALID channel Id %u msglen %d", channelID, msg.getBufLen());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the inbound comms block size
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t CommsChannelManager::getInboundBlockLen(uint32_t channelID, uint32_t defaultSize)
{
    // Get the optimal block size
    if (channelID >= _commsChannelVec.size())
        return defaultSize;

    // Ensure we have a handler
    ensureProtocolCodecExists(channelID);

    // Check validity
    uint32_t blockMax = _commsChannelVec[channelID]->getInboundBlockLen();
#ifdef DEBUG_INBOUND_BLOCK_MAX
    LOG_I(MODULE_PREFIX, "getInboundBlockLen channelID %d %d", channelID, blockMax);
#endif
    return blockMax;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle outbound message on a specific channel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::handleOutboundMessageOnChannel(CommsChannelMsg& msg, uint32_t channelID)
{
    // Check valid
    if (channelID >= _commsChannelVec.size())
        return;

    // Check if channel is used
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if (!pChannel)
        return;

    // Check for message type code, this controls whether a publish message or response
    // response messages are placed in a regular queue
    // publish messages are handled such that only the latest one of any address is sent
    if (msg.getMsgTypeCode() != MSG_TYPE_PUBLISH)
    {
#ifdef DEBUG_OUTBOUND_NON_PUBLISH
        // Debug
        LOG_I(MODULE_PREFIX, "handleOutboundMessage queued channel Id %d channel name %s, msglen %d, msgType %s msgNum %d", channelID, 
                    pChannel->getInterfaceName().c_str(), msg.getBufLen(), msg.getMsgTypeAsString(msg.getMsgTypeCode()),
                    msg.getMsgNumber());
#endif
        pChannel->addToOutboundQueue(msg);
    }
    else
    {
            // TODO - maybe on callback thread here so make sure this is ok!!!!
            // TODO - probably have a single-element buffer for each publish type???
            //      - then service it in the service loop

            // Ensure protocol handler exists
            ensureProtocolCodecExists(channelID);

#ifdef DEBUG_OUTBOUND_PUBLISH
            // Debug
            LOG_I(MODULE_PREFIX, "handleOutboundMessage msg channelID %d, msgType %s msgNum %d, len %d",
                msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen());
#endif

            // Check if channel can accept an outbound message and send if so
            bool noConn = false;
            if (pChannel->canAcceptOutbound(channelID, noConn))
                pChannel->addTxMsgToProtocolCodec(msg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ensure protocol codec exists - lazy creation of codec
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsChannelManager::ensureProtocolCodecExists(uint32_t channelID)
{
    // Check valid
    if (channelID >= _commsChannelVec.size())
        return;

    // Check if we already have a handler for this msg
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if ((!pChannel) || (pChannel->getProtocolCodec() != nullptr))
        return;

    // If not we need to get one
    String channelProtocol = pChannel->getSourceProtocolName();

    // Debug
#ifdef DEBUG_PROTOCOL_CODEC
    LOG_I(MODULE_PREFIX, "No handler specified so creating one for channelID %d protocol %s",
            channelID, channelProtocol.c_str());
#endif

    // Find the protocol in the factory
    for (ProtocolCodecFactoryHelper& codecFactoryHelper : _protocolCodecFactoryList)
    {
        if (codecFactoryHelper.protocolName == channelProtocol)
        {
            // Debug
#ifdef DEBUG_PROTOCOL_CODEC
            LOG_I(MODULE_PREFIX, "ensureProtocolCodecExists channelID %d protocol %s configPrefix %s",
                    channelID, channelProtocol.c_str(), 
                    codecFactoryHelper.pConfigPrefix ? codecFactoryHelper.pConfigPrefix : "NULL");
#endif

            // Create a protocol object
            ProtocolBase* pProtocolCodec = codecFactoryHelper.createFn(channelID, 
                        codecFactoryHelper.config,
                        codecFactoryHelper.pConfigPrefix,
                        std::bind(&CommsChannelManager::frameSendCB, this, std::placeholders::_1), 
                        codecFactoryHelper.frameRxCB,
                        codecFactoryHelper.readyToRxCB);
            pChannel->setProtocolCodec(pProtocolCodec);
            return;
        }
    }

    // Debug
    LOG_W(MODULE_PREFIX, "No suitable codec found for protocol %s map entries %d", channelProtocol.c_str(), _protocolCodecFactoryList.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsChannelManager::frameSendCB(CommsChannelMsg& msg)
{
#ifdef DEBUG_FRAME_SEND
    LOG_I(MODULE_PREFIX, "frameSendCB (response/publish) frame type %s len %d", msg.getProtocolAsString(msg.getProtocol()), msg.getBufLen());
#endif

    uint32_t channelID = msg.getChannelID();
    if (channelID >= _commsChannelVec.size())
    {
        LOG_W(MODULE_PREFIX, "frameSendCB, channelID INVALID channel Id %d msglen %d", channelID, msg.getBufLen());
        return false;
    }

    // Check if channel is used
    CommsChannel* pChannel = _commsChannelVec[channelID];
    if (!pChannel)
        return false;

#ifdef DEBUG_FRAME_SEND
    // Debug
    LOG_I(MODULE_PREFIX, "frameSendCB, channel Id %d channel name %s, msglen %d", channelID, 
                pChannel->getInterfaceName().c_str(), msg.getBufLen());
#endif

    // Send back to the channel
    return pChannel->sendMsgOnChannel(msg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get info JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String CommsChannelManager::getInfoJSON()
{
    String outStr;
    // Channels
    for (uint32_t channelID = 0; channelID < _commsChannelVec.size(); channelID++)
    {
        // Check if channel is used
        CommsChannel* pChannel = _commsChannelVec[channelID];
        if (!pChannel)
            continue;
        // Get info
        if (outStr.length() > 0)
            outStr += ",";
        outStr += pChannel->getInfoJSON();
    }
    return "[" + outStr + "]";
}
