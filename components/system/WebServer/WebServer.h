/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WebServer
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include <RdWebInterface.h>
#include <CommsChannelSettings.h>

class WebServerResource;
class CommsChannelMsg;

#include "RaftWebServer.h"

class WebServer : public SysModBase
{
public:
    // Constructor/destructor
    WebServer(const char* pModuleName, ConfigBase& defaultConfig, 
            ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);
    virtual ~WebServer();

    // Begin
    void beginServer();

    // Add resources to the web server
    void addStaticResources(const WebServerResource *pResources, int numResources);
    void serveStaticFiles(const char* baseUrl, const char* baseFolder, const char* cacheControl = NULL);
    
    // Server-side event handler (one-way text to browser)
    void enableServerSideEvents(const String& eventsURL);
    void sendServerSideEvent(const char* eventContent, const char* eventGroup);

protected:
    // Setup
    virtual void setup();

    // Service - called frequently
    virtual void service();

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager);

    // Add comms channels
    virtual void addCommsChannels(CommsChannelManager& commsChannelManager);
    
private:
    // Helpers
    void addStaticResource(const WebServerResource *pResource, const char *pAliasPath = nullptr);
    void configChanged();
    void applySetup();
    void setupEndpoints();
    bool restAPIMatchEndpoint(const char* url, RdWebServerMethod method,
                    RdWebServerRestEndpoint& endpoint);
    void webSocketSetup();

    // Server config
    bool _accessControlAllowOriginAll;
    bool _webServerEnabled;
    uint32_t _port;
    String _restAPIPrefix;

    // Web server setup
    bool _isWebServerSetup;

    // Server
    RaftWebServer _rdWebServer;

    // Singleton
    static WebServer* _pThisWebServer;

    // Websockets
    std::vector<String> _webSocketConfigs;

    // Mapping from web-server method to RESTAPI method enums
    RestAPIEndpoint::EndpointMethod convWebToRESTAPIMethod(RdWebServerMethod method)
    {
        switch(method)
        {
            case WEB_METHOD_POST: return RestAPIEndpoint::ENDPOINT_POST;
            case WEB_METHOD_PUT: return RestAPIEndpoint::ENDPOINT_PUT;
            case WEB_METHOD_DELETE: return RestAPIEndpoint::ENDPOINT_DELETE;
            default: return RestAPIEndpoint::ENDPOINT_GET;
        }
    }
};
