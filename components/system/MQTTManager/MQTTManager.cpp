/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MQTT Manager
// Handles state of MQTT connections
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MQTTManager.h"
#include <Logger.h>
#include <RaftUtils.h>
#include <JSONParams.h>
#include <ESPUtils.h>
#include <CommsCoreIF.h>
#include <CommsChannelMsg.h>
#include <CommsChannelSettings.h>
#include <RestAPIEndpointManager.h>
#include <CommsCoreIF.h>
#include <SysManager.h>

// Log prefix
static const char *MODULE_PREFIX = "MQTTMan";

// #define DEBUG_MQTT_SEND

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MQTTManager::MQTTManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
            ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // ChannelID
    _commsChannelID = CommsCoreIF::CHANNEL_ID_UNDEFINED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTManager::setup()
{
    // Extract info from config
    bool isMQTTEnabled = configGetBool("enable", false);
    String brokerHostname = configGetString("brokerHostname", "");
    uint32_t brokerPort = configGetLong("brokerPort", RdMQTTClient::DEFAULT_MQTT_PORT);
    String mqttClientID = configGetString("clientID", "");

    // Form unique client ID
    mqttClientID += getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":");

    // Setup client
    _mqttClient.setup(isMQTTEnabled, brokerHostname.c_str(), brokerPort, mqttClientID.c_str());

    // Handle topics
    std::vector<String> mqttTopics;
    configGetConfig().getArrayElems("topics", mqttTopics);
    LOG_I(MODULE_PREFIX, "setup topics %d", mqttTopics.size());
    for (uint32_t i = 0; i < mqttTopics.size(); i++)
    {
        // Extract topic details
        JSONParams topicJSON = mqttTopics[i];

        // Check direction
        String defaultName = "topic" + String(i+1);
        String topicName = topicJSON.getString("name", defaultName);
        bool isInbound = topicJSON.getBool("inbound", true);
        String topicPath = topicJSON.getString("path", "");
        uint8_t qos = topicJSON.getLong("qos", 0);

        // Handle topic
        _mqttClient.addTopic(topicName.c_str(), isInbound, topicPath.c_str(), qos);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTManager::service()
{
    // Service client
    _mqttClient.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String MQTTManager::getStatusJSON()
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String MQTTManager::getDebugJSON()
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTManager::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

void MQTTManager::addCommsChannels(CommsCoreIF& commsCoreIF)
{
    // Get a list of outbound topic names
    std::vector<String> topicNames;
    _mqttClient.getTopicNames(topicNames, false, true);
    LOG_I(MODULE_PREFIX, "addCommsChannels numOutTopics %d", topicNames.size());

    // Comms channel
    static const CommsChannelSettings commsChannelSettings;

    // Register an endpoint for each
    for (String& topicName : topicNames)
    {
        LOG_I(MODULE_PREFIX, "addCommsChannels %s", topicName.c_str());

        // Register as a channel
        _commsChannelID = commsCoreIF.registerChannel("RICJSON", 
                "MQTT",
                topicName.c_str(),
                [this, topicName](CommsChannelMsg& msg) { return sendMQTTMsg(topicName, msg); },
                std::bind(&MQTTManager::readyToSend, this, std::placeholders::_1),
                &commsChannelSettings);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over MQTT
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MQTTManager::sendMQTTMsg(const String& topicName, CommsChannelMsg& msg)
{
    String msgStr;
    Raft::strFromBuffer(msg.getBuf(), msg.getBufLen(), msgStr);
#ifdef DEBUG_MQTT_SEND
    LOG_I(MODULE_PREFIX, "sendMQTTMsg topicName %s msg %s", topicName.c_str(), msgStr.c_str());
#endif

    // Publish using client
    return _mqttClient.publishToTopic(topicName, msgStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if ready to message over MQTT
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MQTTManager::readyToSend(uint32_t channelID)
{
    return true;
}
