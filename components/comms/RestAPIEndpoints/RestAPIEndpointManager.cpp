/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RestAPIEndpointManager
// Endpoints for REST API implementations
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RestAPIEndpointManager.h"
#include <Logger.h>
#include <RaftUtils.h>

// Warn
#define WARN_ON_NON_MATCHING_ENDPOINTS

// Debug
// #define DEBUG_REST_API_ENDPOINTS_ADD
// #define DEBUG_REST_API_ENDPOINTS_GET
// #define DEBUG_HANDLE_API_REQUEST_AND_RESPONSE
// #define DEBUG_NAME_VALUE_PAIR_EXTRACTION

#if defined(DEBUG_REST_API_ENDPOINTS_ADD) || \
    defined(DEBUG_REST_API_ENDPOINTS_GET) || \
    defined(DEBUG_HANDLE_API_REQUEST_AND_RESPONSE) || \
    defined(DEBUG_NAME_VALUE_PAIR_EXTRACTION) || \
    defined(WARN_ON_NON_MATCHING_ENDPOINTS)
static const char* MODULE_PREFIX = "RestAPIEndpointManager";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RestAPIEndpointManager::RestAPIEndpointManager()
{
}

RestAPIEndpointManager::~RestAPIEndpointManager()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get number of endpoints
int RestAPIEndpointManager::getNumEndpoints()
{
    return _endpointsList.size();
}

// Get nth endpoint
RestAPIEndpoint *RestAPIEndpointManager::getNthEndpoint(int n)
{
    if ((n >= 0) && (n < _endpointsList.size()))
    {
        return &_endpointsList[n];
    }
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add Endpoint
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add an endpoint
void RestAPIEndpointManager::addEndpoint(const char *pEndpointStr, 
                    RestAPIEndpoint::EndpointType endpointType,
                    RestAPIEndpoint::EndpointMethod endpointMethod,
                    RestAPIFunction callback,
                    const char *pDescription,
                    const char *pContentType,
                    const char *pContentEncoding,
                    RestAPIEndpoint::EndpointCache_t cacheControl,
                    const char *pExtraHeaders,
                    RestAPIFnBody callbackBody,
                    RestAPIFnChunk callbackChunk,
                    RestAPIFnIsReady callbackIsReady
                    )
{
    // Create new command definition and add
    _endpointsList.push_back(RestAPIEndpoint(pEndpointStr, endpointType,
                                endpointMethod, callback,
                                pDescription,
                                pContentType, pContentEncoding,
                                cacheControl, pExtraHeaders,
                                callbackBody, callbackChunk, callbackIsReady));
#ifdef DEBUG_REST_API_ENDPOINTS_ADD
    LOG_I(MODULE_PREFIX, "addEndpoint %s", pEndpointStr);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get the endpoint definition corresponding to a requested endpoint
RestAPIEndpoint* RestAPIEndpointManager::getEndpoint(const char *pEndpointStr)
{
    // Look for the command in the registered callbacks
    for (RestAPIEndpoint& endpoint : _endpointsList)
    {
        if (strcasecmp(endpoint._endpointStr.c_str(), pEndpointStr) == 0)
        {
            return &endpoint;
        }
    }
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get endpoint def matching REST API request - or NULL if none matches
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RestAPIEndpoint* RestAPIEndpointManager::getMatchingEndpoint(const char *requestStr,
                    RestAPIEndpoint::EndpointMethod endpointMethod, bool optionsMatchesAll)
{
    // Get req endpoint name
    String requestEndpoint = getNthArgStr(requestStr, 0);

#ifdef DEBUG_REST_API_ENDPOINTS_GET
    // Debug
    LOG_I(MODULE_PREFIX, "reqStr %s requestEndpoint %s, num endpoints %d", 
                requestStr, requestEndpoint.c_str(), _endpointsList.size());
#endif

    // Find endpoint
    for (RestAPIEndpoint& endpoint : _endpointsList)
    {
        if (endpoint._endpointType != RestAPIEndpoint::ENDPOINT_CALLBACK)
            continue;
        if ((endpoint._endpointMethod != endpointMethod) && 
                !((endpointMethod == RestAPIEndpoint::EndpointMethod::ENDPOINT_OPTIONS) && optionsMatchesAll))
            continue;
        if (requestEndpoint.equalsIgnoreCase(endpoint._endpointStr))
            return &endpoint;
    }
#ifdef WARN_ON_NON_MATCHING_ENDPOINTS
    LOG_W(MODULE_PREFIX, "getMatchingEndpoint %s method %s not found", 
                requestEndpoint.c_str(), getEndpointMethodStr(endpointMethod));
#endif
    return NULL;    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle simple REST API request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RestAPIEndpointManager::handleApiRequest(const char *requestStr, String &retStr, const APISourceInfo& sourceInfo)
{
    // Get matching def
    RestAPIEndpoint* pEndpoint = getMatchingEndpoint(requestStr);

    // Check valid
    if (!pEndpoint)
        return false;

    // Call endpoint
    String reqStr(requestStr);
    pEndpoint->callbackMain(reqStr, retStr, sourceInfo);
#ifdef DEBUG_HANDLE_API_REQUEST_AND_RESPONSE
    LOG_W(MODULE_PREFIX, "handleApiRequest %s resp %s channelID %d", requestStr, retStr.c_str(), sourceInfo.channelID);
#endif
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Form a string from a char buffer with a fixed length
void RestAPIEndpointManager::formStringFromCharBuf(String &outStr, const char *pStr, int len)
{
    outStr = "";
    outStr.reserve(len + 1);
    for (int i = 0; i < len; i++)
    {
        outStr.concat(*pStr);
        pStr++;
    }
}

// Remove first argument from string
String RestAPIEndpointManager::removeFirstArgStr(const char *argStr)
{
    // Get location of / (excluding first char if needed)
    String oStr = argStr;
    oStr = unencodeHTTPChars(oStr);
    int idxSlash = oStr.indexOf('/', 1);
    if (idxSlash == -1)
        return String("");
    return oStr.substring(idxSlash+1);
}

// Get Nth argument from a string
String RestAPIEndpointManager::getNthArgStr(const char *argStr, int argIdx, bool splitOnQuestionMark)
{
    int argLen = 0;
    String oStr;
    const char *pStr = getArgPtrAndLen(argStr, *argStr == '/' ? argIdx + 1 : argIdx, argLen, splitOnQuestionMark);

    if (pStr)
    {
        formStringFromCharBuf(oStr, pStr, argLen);
    }
    oStr = unencodeHTTPChars(oStr);
    return oStr;
}

// Get position and length of nth arg
const char* RestAPIEndpointManager::getArgPtrAndLen(const char *argStr, int argIdx, int &argLen, bool splitOnQuestionMark)
{
    int curArgIdx = 0;
    const char *pCh = argStr;
    const char *pArg = argStr;

    while (true)
    {
        if ((*pCh == '/') || (splitOnQuestionMark && (*pCh == '?')) || (*pCh == '\0'))
        {
            if (curArgIdx == argIdx)
            {
                argLen = pCh - pArg;
                return pArg;
            }
            if (*pCh == '\0')
            {
                return NULL;
            }
            pArg = pCh + 1;
            curArgIdx++;
        }
        pCh++;
    }
    return NULL;
}

// Num args from an argStr
int RestAPIEndpointManager::getNumArgs(const char *argStr)
{
    int numArgs = 0;
    int numChSinceSep = 0;
    const char *pCh = argStr;

    // Count args
    while (*pCh)
    {
        if (*pCh == '/')
        {
            numArgs++;
            numChSinceSep = 0;
        }
        pCh++;
        numChSinceSep++;
    }
    if (numChSinceSep > 0)
    {
        return numArgs + 1;
    }
    return numArgs;
}

// Convert encoded URL
String RestAPIEndpointManager::unencodeHTTPChars(String &inStr)
{
    inStr.replace("%20", " ");
    inStr.replace("%21", "!");
    inStr.replace("%22", "\"");
    inStr.replace("%23", "#");
    inStr.replace("%24", "$");
    inStr.replace("%25", "%");
    inStr.replace("%26", "&");
    inStr.replace("%27", "^");
    inStr.replace("%28", "(");
    inStr.replace("%29", ")");
    inStr.replace("%2A", "*");
    inStr.replace("%2B", "+");
    inStr.replace("%2C", ",");
    inStr.replace("%2D", "-");
    inStr.replace("%2E", ".");
    inStr.replace("%2F", "/");
    inStr.replace("%3A", ":");
    inStr.replace("%3B", ";");
    inStr.replace("%3C", "<");
    inStr.replace("%3D", "=");
    inStr.replace("%3E", ">");
    inStr.replace("%3F", "?");
    inStr.replace("%5B", "[");
    inStr.replace("%5C", "\\");
    inStr.replace("%5D", "]");
    inStr.replace("%5E", "^");
    inStr.replace("%5F", "_");
    inStr.replace("%60", "`");
    inStr.replace("%7B", "{");
    inStr.replace("%7C", "|");
    inStr.replace("%7D", "}");
    inStr.replace("%7E", "~");
    return inStr;
}

const char* RestAPIEndpointManager::getEndpointTypeStr(RestAPIEndpoint::EndpointType endpointType)
{
    if (endpointType == RestAPIEndpoint::ENDPOINT_CALLBACK)
        return "Callback";
    return "Unknown";
}

const char* RestAPIEndpointManager::getEndpointMethodStr(RestAPIEndpoint::EndpointMethod endpointMethod)
{
    switch(endpointMethod)
    {
        case RestAPIEndpoint::ENDPOINT_POST: return "POST";
        case RestAPIEndpoint::ENDPOINT_PUT: return "PUT";
        case RestAPIEndpoint::ENDPOINT_DELETE: return "DELETE";
        case RestAPIEndpoint::ENDPOINT_OPTIONS: return "OPTIONS";
        default: return "GET";
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extract positional parameters and name-value args
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RestAPIEndpointManager::getParamsAndNameValues(const char* reqStr, std::vector<String>& params, 
            std::vector<RdJson::NameValuePair>& nameValuePairs)
{
    // Params
    uint32_t numParams = getNumArgs(reqStr);
    params.resize(numParams);
    for (uint32_t i = 0; i < numParams; i++)
        params[i] = getNthArgStr(reqStr, i);

    // Check for existence of value pairs
    nameValuePairs.clear();
    char* pQMark = strstr(reqStr, "?");
    if (!pQMark)
        return true;

    // Count the pairs
    uint32_t pairCount = 0;
    char* pCurSep = pQMark;
    while(pCurSep)
    {
        pCurSep = strstr(pCurSep+1, "=");
        if (pCurSep)
            pairCount++;
    }
    // LOG_I(MODULE_PREFIX, "getParamsAndNamedValues found %d nameValues", pairCount);

    // Extract the pairs
    nameValuePairs.resize(pairCount);
    pCurSep = pQMark;
    bool sepTypeIsEqualsSign = true;
    uint32_t pairIdx = 0;
    String name, val;
    while(pCurSep)
    {
        // Each pair has the form "name=val;" (semicolon missing on last pair)
        char* pElemStart = pCurSep+1;
        if (sepTypeIsEqualsSign)
        {
            // Check for missing =
            pCurSep = strstr(pElemStart, "=");
            if (!pCurSep)
                break;
            Raft::strFromBuffer((uint8_t*)pElemStart, pCurSep-pElemStart, name);
        }
        else
        {
            // Handle two alternatives - sep or no sep
            pCurSep = strstr(pElemStart, ";");
            if (!pCurSep)
                pCurSep = strstr(pElemStart, "&");
            if (pCurSep)
                Raft::strFromBuffer((uint8_t*)pElemStart, pCurSep-pElemStart, val);
            else
                val = pElemStart;
        }

        // Next separator
        sepTypeIsEqualsSign = !sepTypeIsEqualsSign;
        if (!sepTypeIsEqualsSign)
            continue;

        // Store and move on
        if (pairIdx >= pairCount)
            break;
        name.trim();
        val.trim();
        nameValuePairs[pairIdx] = {name,val};
        pairIdx++;
    }

    // Debug
#ifdef DEBUG_NAME_VALUE_PAIR_EXTRACTION
    for (RdJson::NameValuePair& pair : nameValuePairs)
        LOG_I(MODULE_PREFIX, "getParamsAndNamedValues name %s val %s", pair.name.c_str(), pair.value.c_str());
#endif
    return true;

}