/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RestAPIEndpointManager
// Endpoints for REST API implementations
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <functional>
#include <vector>
#include <ArduinoOrAlt.h>
#include <RdJson.h>
#include "RestAPIEndpoint.h"

// Collection of endpoints
class RestAPIEndpointManager
{
public:
    RestAPIEndpointManager();

    virtual ~RestAPIEndpointManager();

    // Get number of endpoints
    int getNumEndpoints();

    // Get nth endpoint
    RestAPIEndpoint *getNthEndpoint(int n);

    // Add an endpoint
    void addEndpoint(const char *pEndpointStr, 
                    RestAPIEndpoint::EndpointType endpointType,
                    RestAPIEndpoint::EndpointMethod endpointMethod,
                    RestAPIFunction callbackMain,
                    const char *pDescription,
                    const char *pContentType = nullptr,
                    const char *pContentEncoding = nullptr,
                    RestAPIEndpoint::EndpointCache_t pCache = RestAPIEndpoint::ENDPOINT_CACHE_NEVER,
                    const char *pExtraHeaders = nullptr,
                    RestAPIFnBody callbackBody = nullptr,
                    RestAPIFnChunk callbackChunk = nullptr,
                    RestAPIFnIsReady callbackIsReady = nullptr);

    // Get the endpoint definition corresponding to a requested endpoint
    RestAPIEndpoint *getEndpoint(const char *pEndpointStr);

    // Handle an API request
    bool handleApiRequest(const char *requestStr, String &retStr, const APISourceInfo& sourceInfo);

    // Get matching endpoint def
    RestAPIEndpoint* getMatchingEndpoint(const char *requestStr,
                    RestAPIEndpoint::EndpointMethod endpointMethod = RestAPIEndpoint::ENDPOINT_GET,
                    bool optionsMatchesAll = false);

    // Form a string from a char buffer with a fixed length
    static void formStringFromCharBuf(String &outStr, const char *pStr, int len);

    // Remove first argument from string
    static String removeFirstArgStr(const char *argStr);

    // Get Nth argument from a string
    static String getNthArgStr(const char *argStr, int argIdx, bool splitOnQuestionMark = true);

    // Get position and length of nth arg
    static const char *getArgPtrAndLen(const char *argStr, int argIdx, int &argLen, bool splitOnQuestionMark = true);

    // Num args from an argStr
    static int getNumArgs(const char *argStr);

    // Convert encoded URL
    static String unencodeHTTPChars(String &inStr);

    static const char *getEndpointTypeStr(RestAPIEndpoint::EndpointType endpointType);

    static const char *getEndpointMethodStr(RestAPIEndpoint::EndpointMethod endpointMethod);

    // URL Parser
    static bool getParamsAndNameValues(const char* reqStr, std::vector<String>& params, std::vector<RdJson::NameValuePair>& nameValuePairs);

    // Special channel IDs
    static const uint32_t CHANNEL_ID_EVENT_DETECTOR = 20000;
    static const uint32_t CHANNEL_ID_ROBOT_CONTROLLER = 20001;
    static const uint32_t CHANNEL_ID_COMMAND_FILE = 20002;
    static const uint32_t CHANNEL_ID_SERIAL_CONSOLE = 20003;
    static const uint32_t CHANNEL_ID_COMMAND_SCHEDULER = 20004;
    static const uint32_t CHANNEL_ID_MQTT_COMMS = 20005;

private:
    // Vector of endpoints
    std::vector<RestAPIEndpoint> _endpointsList;
};
