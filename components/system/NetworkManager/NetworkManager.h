/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Network Manager
// Handles state of WiFi system, retries, etc and Ethernet
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include "NetworkSystem.h"

class ConfigBase;
class RestAPIEndpointManager;
class APISourceInfo;

class NetworkManager : public SysModBase
{
public:
    NetworkManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, 
                ConfigBase* pMutableConfig, const char* defaultHostname);

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

    // Get debug string
    virtual String getDebugJSON() override final;

    // Get named value
    virtual double getNamedValue(const char* valueName, bool& isValid);

private:
    // Singleton NetworkManager
    static NetworkManager* _pNetworkManager;

    // Default hostname
    String _defaultHostname;
    
    // Last connection status
    NetworkSystem::ConnStateCode _prevConnState;
    
    // Helpers
    void applySetup();
    void apiWifiSet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiWifiClear(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiWiFiPause(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);
    void apiWifiScan(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
