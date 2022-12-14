/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StatePublisher
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "StatePublisher.h"
#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <CommsChannelManager.h>
#include <RestAPIEndpointManager.h>
#include <ConfigBase.h>
#include <JSONParams.h>

// Debug
// #define DEBUG_PUBLISHING_HANDLE
// #define DEBUG_PUBLISHING_MESSAGE
// #define DEBUG_ONLY_THIS_MSG_ID "ScaderWaterer"
// #define DEBUG_API_SUBSCRIPTION
// #define DEBUG_STATE_PUBLISHER_SETUP
// #define DEBUG_REDUCED_PUBLISHING_RATE_WHEN_BUSY

// Logging
static const char* MODULE_PREFIX = "StatePub";

// Debug
#ifdef DEBUG_ONLY_THIS_ROSTOPIC
#include <algorithm>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StatePublisher::StatePublisher(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig)
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _pCommsChannelManager = NULL;
#ifdef DEBUG_STATEPUB_OUTPUT_PUBLISH_STATS
    _worstTimeSetMs = 0;
    _recentWorstTimeUs = 0;
#endif
}

StatePublisher::~StatePublisher()
{
    // Clean up
    cleanUp();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::setup()
{
    // Clear down
    cleanUp();

    // Publications info
    std::vector<String> pubList;
    if (!configGetArrayElems("pubList", pubList))
    {
        LOG_I(MODULE_PREFIX, "setup - no pubList found");
        return;
    }

    // Iterate over pubList
    for (int pubIdx = 0; pubIdx < pubList.size(); pubIdx++)
    {
        // Get the publication info
        JSONParams pubInfo = pubList[pubIdx];

        // Check pub type
        String pubType = pubInfo.getString("type", "");
#ifdef DEBUG_STATE_PUBLISHER_SETUP
        LOG_I(MODULE_PREFIX, "setup pubInfo %s type %s", pubInfo.c_str(), pubType.c_str());
#endif

        if (pubType.equalsIgnoreCase("HW"))
        {
            // Create pubrec
            PubRec pubRec;

            // Get settings
            pubRec._name = pubInfo.getString("name", "");
            pubRec._trigger = TRIGGER_NONE;
            String triggerStr = pubInfo.getString("trigger", "");
            triggerStr.toLowerCase();
            if (triggerStr.indexOf("change") >= 0)
            {
                pubRec._trigger = TRIGGER_ON_STATE_CHANGE;
                if (triggerStr.indexOf("time") >= 0)
                    pubRec._trigger = TRIGGER_ON_TIME_OR_CHANGE;
            }
            if (pubRec._trigger == TRIGGER_NONE)
            {
                pubRec._trigger = TRIGGER_ON_TIME_INTERVALS;
            }
            String ratesJSON = pubInfo.getString("rates", "");
            pubRec._msgIDStr = pubInfo.getString("msgID", "");

            // Check for interfaces
            int numRatesAndInterfaces = 0;
            if (RdJson::getType(numRatesAndInterfaces, ratesJSON.c_str()) == RD_JSMN_ARRAY)
            {
                // Iterate rates and interfaces
                for (int rateIdx = 0; rateIdx < numRatesAndInterfaces; rateIdx++)
                {
                    // Get the rate and interface info
                    ConfigBase rateAndInterfaceInfo = RdJson::getString(("["+String(rateIdx)+"]").c_str(), "{}", ratesJSON.c_str());
                    String interface = rateAndInterfaceInfo.getString("if", "");
                    String protocol = rateAndInterfaceInfo.getString("protocol", "");
                    double rateHz = rateAndInterfaceInfo.getDouble("rateHz", 1.0);

                    // Add to list
                    InterfaceRateRec ifRateRec;
                    ifRateRec._interface = interface;
                    ifRateRec._protocol = protocol;
                    ifRateRec.setRateHz(rateHz);
                    ifRateRec._lastPublishMs = millis();
                    pubRec._interfaceRates.push_back(ifRateRec);

                    // Debug
#ifdef DEBUG_STATE_PUBLISHER_SETUP
                    LOG_I(MODULE_PREFIX, "setup publishIF %s rateHz %.1f msBetween %d name %s protocol %s msgID %s", interface.c_str(),
                                    rateHz, ifRateRec._betweenPubsMs, pubRec._name.c_str(), ifRateRec._protocol.c_str(), 
                                    pubRec._msgIDStr.c_str());
#endif
                }

                // Add to the list of publication records
                _publicationRecs.push_back(pubRec);
            }
        }
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup num publication recs %d", _publicationRecs.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::service()
{
    // Check valid
    if (!_pCommsChannelManager)
        return;

    // Check if publishing rate is to be throttled back
    bool reducePublishingRate = isSystemMainFWUpdate() || isSystemFileTransferring();
    
    // Check through publishers
    for (PubRec& pubRec : _publicationRecs)
    {
        // Check for state change detection callback
        bool forceMsgGeneration = false;
        if (pubRec._stateDetectFn)
        {
            // Check for the mimimum time between publications
            if (Raft::isTimeout(millis(), pubRec._lastPublishMs, MIN_MS_BETWEEN_STATE_CHANGE_PUBLISHES))
            {
                // Callback function generates a hash in the form of a std::vector<uint8_t>
                // If this is not identical to previously returned hash then force message generation
                std::vector<uint8_t> newStateHash;
                pubRec._stateDetectFn(pubRec._msgIDStr.c_str(), newStateHash);
                if(pubRec._stateHash != newStateHash)
                {
                    forceMsgGeneration = true;
                    pubRec._stateHash = newStateHash;
                    LOG_I(MODULE_PREFIX, "Force generation on state change for %s", pubRec._name.c_str());
                }
            }
        }

        // And each interface
        for (InterfaceRateRec& rateRec : pubRec._interfaceRates)
        {
            // Check for time to publish
            if (forceMsgGeneration || ((rateRec._rateHz != 0) && (Raft::isTimeout(millis(), rateRec._lastPublishMs, 
                        reducePublishingRate ? REDUCED_PUB_RATE_WHEN_BUSY_MS : rateRec._betweenPubsMs))))
            {
                rateRec._lastPublishMs = pubRec._lastPublishMs = millis();

                // Check if channelID is defined
                if (rateRec._channelID == PUBLISHING_HANDLE_UNDEFINED)
                {
                    // Get a match of interface and protocol
                    rateRec._channelID = _pCommsChannelManager->getChannelIDByName(rateRec._interface, rateRec._protocol);

#ifdef DEBUG_PUBLISHING_HANDLE
                    // Debug
                    LOG_I(MODULE_PREFIX, "Got channelID %d for name %s IF %s protocol %s", rateRec._channelID, pubRec._name.c_str(),
                            rateRec._interface.c_str(), rateRec._protocol.c_str());
#endif

                    // Still undefined?
                    if (rateRec._channelID == PUBLISHING_HANDLE_UNDEFINED)
                        continue;
                }

                // Check if interface can accept messages
                bool noConn = false;
                if (!_pCommsChannelManager->canAcceptOutbound(rateRec._channelID, noConn))
                    continue;

#ifdef DEBUG_REDUCED_PUBLISHING_RATE_WHEN_BUSY
                if (reducePublishingRate)
                {
                    LOG_I(MODULE_PREFIX, "service publishing rate reduced for channel %d", rateRec._channelID);
                }
#endif

#ifdef DEBUG_STATEPUB_OUTPUT_PUBLISH_STATS
                uint64_t startUs = micros();
#endif

                publishData(pubRec, rateRec);

#ifdef DEBUG_STATEPUB_OUTPUT_PUBLISH_STATS
                uint64_t elapUs = micros() - startUs;
                if (_recentWorstTimeUs < elapUs)
                    _recentWorstTimeUs = elapUs;
                if (Raft::isTimeout(millis(), _worstTimeSetMs, 1000))
                {
                    LOG_I(MODULE_PREFIX, "PubSlowest %lld", _recentWorstTimeUs);
                    _recentWorstTimeUs = 0;
                    _worstTimeSetMs = millis();
                }
#endif
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    // Subscription to published messages
    endpointManager.addEndpoint("subscription", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                std::bind(&StatePublisher::apiSubscription, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "Subscription to published messages, see docs for details");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms channels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::addCommsChannels(CommsChannelManager &commsChannelManager)
{
    _pCommsChannelManager = &commsChannelManager;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String StatePublisher::getDebugJSON()
{
    // Debug string
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Receive msg generator callback function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::receiveMsgGenCB(const char* msgGenID, SysMod_publishMsgGenFn msgGenCB, SysMod_stateDetectCB stateDetectCB)
{
    // Search for publication records using this msgGenID
    for (PubRec& pubRec : _publicationRecs)
    {
        // Check ID
        if (pubRec._msgIDStr.equals(msgGenID))
        {
            LOG_I(MODULE_PREFIX, "receiveMsgGenCB registered msgGenFn for msgID %s", msgGenID);
            pubRec._msgGenFn = msgGenCB;
            pubRec._stateDetectFn = stateDetectCB;
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Publish data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StatePublisher::publishData(StatePublisher::PubRec& pubRec, InterfaceRateRec& rateRec)
{
    // Endpoint message we're going to send
    CommsChannelMsg endpointMsg(rateRec._channelID, MSG_PROTOCOL_ROSSERIAL, 0, MSG_TYPE_PUBLISH);

    // Generate message
    bool msgOk = false;
    if (pubRec._msgGenFn)
        msgOk = pubRec._msgGenFn(pubRec._msgIDStr.c_str(), endpointMsg);

#ifdef DEBUG_PUBLISHING_MESSAGE
#ifdef DEBUG_ONLY_THIS_MSG_ID
    if (pubRec._msgIDStr.equals(DEBUG_ONLY_THIS_MSG_ID))
#endif
    {
        LOG_I(MODULE_PREFIX, "MsgGen len %d msgID %s rslt %d", endpointMsg.getBufLen(), pubRec._msgIDStr.c_str(), msgOk);
    }
#endif
    if (!msgOk)
        return false;

#ifdef DEBUG_PUBLISHING_MESSAGE
#ifdef DEBUG_ONLY_THIS_MSG_ID
    if (pubRec._msgIDStr.equals(DEBUG_ONLY_THIS_MSG_ID))
#endif
    {
        // Debug
        String outStr;
        Raft::getHexStrFromBytes(endpointMsg.getBuf(), endpointMsg.getBufLen(), outStr);
        LOG_I(MODULE_PREFIX, "sendPublishMsg payloadLen %d payload %s", endpointMsg.getBufLen(), outStr.c_str());
    }
#endif

    // Send message
    _pCommsChannelManager->handleOutboundMessage(endpointMsg);

#ifdef DEBUG_PUBLISHING_MESSAGE
#ifdef DEBUG_ONLY_THIS_MSG_ID
    if (pubRec._msgIDStr.equals(DEBUG_ONLY_THIS_MSG_ID))
#endif
    {
        // Debug
        LOG_I(MODULE_PREFIX, "sendPublishMsg channelID %d payloadLen %d", rateRec._channelID, endpointMsg.getBufLen());
    }
#endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Subscription
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::apiSubscription(String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
#ifdef DEBUG_API_SUBSCRIPTION
    LOG_I(MODULE_PREFIX, "apiSubscription reqStr %s", reqStr.c_str());
#endif

    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);

    // Can't use the full request as the reqStr in the response as it won't be valid json
    String cmdName = reqStr;
    if (params.size() > 0)
        cmdName = params[0];

    // JSON params and channelID
    JSONParams jsonParams = RdJson::getJSONFromNVPairs(nameValues, true); 
    uint32_t channelID = sourceInfo.channelID;

    // Debug
#ifdef DEBUG_API_SUBSCRIPTION
    for (RdJson::NameValuePair& nvp : nameValues)
    {
        LOG_I(MODULE_PREFIX, "apiSubscription %s = %s", nvp.name.c_str(), nvp.value.c_str());
    }
    if (params.size() > 0)
    {
        LOG_I(MODULE_PREFIX, "apiSubscription params[0] %s", params[0].c_str());
    }
    LOG_I(MODULE_PREFIX, "subscription jsonFromNVPairs %s", jsonParams.c_str());
#endif

    // Handle subscription commands
    String actionStr = jsonParams.getString("action", "");

#ifdef DEBUG_API_SUBSCRIPTION
    LOG_I(MODULE_PREFIX, "apiSubscription %s", actionStr.c_str());
#endif

    // Check for record update
    if (actionStr.equalsIgnoreCase("update"))
    {
        // Get the details of the publish records to alter - initially try to get
        // these from array of values
        std::vector<String> pubRecsToMod;
        if (!jsonParams.getArrayElems("pubRecs", pubRecsToMod))
        {
            // That failed so try to see if a single value is present and create an array with
            // a single element if so
            String pubRecName = jsonParams.getString("name", "");
            double pubRateHz = jsonParams.getDouble("rateHz", 1.0);
            String pubRec = R"({"name":")" + pubRecName + R"(","rateHz":)" + String(pubRateHz) + R"(})";
            pubRecsToMod.push_back(pubRec);
        }

        // Iterate pub record names
        for (String& pubRecToMod : pubRecsToMod)
        {
            // Get details of changes
            ConfigBase pubRecConf = pubRecToMod;
            String pubRecName = pubRecConf.getString("name", "");
            double pubRateHz = pubRecConf.getDouble("rateHz", 1.0);

            // Find the existing publication record to update
#ifdef DEBUG_API_SUBSCRIPTION
            LOG_I(MODULE_PREFIX, "apiSubscription pubRec info %s", pubRecToMod.c_str());
#endif
            for (PubRec& pubRec : _publicationRecs)
            {
                // Check name
                if (!pubRec._name.equals(pubRecName))
                    continue;

                // Update interface-rate record (if there is one)
                bool interfaceRateFound = false;
                for (InterfaceRateRec& rateRec : pubRec._interfaceRates)
                {
                    if (rateRec._channelID == channelID)
                    {
                        interfaceRateFound = true;
                        rateRec.setRateHz(pubRateHz);
#ifdef DEBUG_API_SUBSCRIPTION
                        LOG_I(MODULE_PREFIX, "apiSubscription updated rateRec channelID %d rateHz %.2f", channelID, pubRateHz);
#endif
                    }
                }

                // Check we found an existing record
                if (!interfaceRateFound)
                {
                    // No record found so create one
                    InterfaceRateRec ifRateRec;
                    ifRateRec._channelID = channelID;
                    ifRateRec.setRateHz(pubRateHz);
                    ifRateRec._lastPublishMs = millis();
                    pubRec._interfaceRates.push_back(ifRateRec);
#ifdef DEBUG_API_SUBSCRIPTION
                    LOG_I(MODULE_PREFIX, "apiSubscription created rateRec channelID %d rateHz %.2f", channelID, pubRateHz);
#endif
                }
                break;
            }
        }
    }
    Raft::setJsonBoolResult(cmdName.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StatePublisher::cleanUp()
{
}
