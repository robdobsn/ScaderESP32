/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Network Manager
// Handles state of WiFi system, retries, etc and Ethernet
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <NetworkManager.h>
#include <RaftUtils.h>
#include <ESPUtils.h>
#include <ConfigNVS.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>

// Log prefix
static const char *MODULE_PREFIX = "NetMan";

// Singleton network manager
NetworkManager* NetworkManager::_pNetworkManager = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NetworkManager::NetworkManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
            ConfigBase *pMutableConfig, const char* defaultHostname)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _prevConnState = NetworkSystem::CONN_STATE_NONE;
    _defaultHostname = defaultHostname;

    // Singleton
    _pNetworkManager = this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::setup()
{
    applySetup();
}

void NetworkManager::applySetup()
{
    // Extract info from config
    bool isWiFiEnabled = (configGetLong("WiFiEnabled", 0) != 0);
    bool isEthEnabled = (configGetLong("EthEnabled", 0) != 0);
    bool isWiFiAPModeEnabled = (configGetLong("WiFiAPModeEn", 0) != 0);
    bool isWiFiSTAModeEnabled = (configGetLong("WiFiSTAModeEn", 1) != 0);
    bool isEthWiFiBrided = (configGetLong("EthWiFiBridge", 0) != 0);
    String hostname = configGetString("defaultHostname", _defaultHostname.c_str());
    networkSystem.setup(isWiFiEnabled, isEthEnabled, hostname.c_str(), 
                isWiFiSTAModeEnabled, isWiFiAPModeEnabled, isEthWiFiBrided);

    // Get SSID and password
    String ssid = configGetString("WiFiSSID", "");
    String password = configGetString("WiFiPass", "");
    String apSSID = configGetString("WiFiAPSSID", "");
    String apPassword = configGetString("WiFiAPPass", "");
        // Set WiFi credentials
    networkSystem.configureWiFi(ssid, password, hostname, apSSID, apPassword);

    // Debug
    LOG_D(MODULE_PREFIX, "setup isEnabled %s hostname %s ", isWiFiEnabled ? "YES" : "NO", hostname.c_str(), 
            ssid.c_str(), password.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::service()
{
    // Service network system
    networkSystem.service();

    // Check for status change
    NetworkSystem::ConnStateCode connState = networkSystem.getConnState();
    if (_prevConnState != connState)
    {
        // Inform hooks of status change
        if (_pNetworkManager)
            _pNetworkManager->executeStatusChangeCBs(connState == NetworkSystem::ConnStateCode::CONN_STATE_WIFI_AND_IP);
        _prevConnState = connState;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String NetworkManager::getStatusJSON()
{

    NetworkSystem::ConnStateCode connState = networkSystem.getConnState();
    char statusStr[300];
    bool isValid = false;
    snprintf(statusStr, sizeof(statusStr), 
                R"({"rslt":"ok","isConn":%d,"isPaused":%d,"connState":"%s","SSID":"%s","IP":"%s","Hostname":"%s","WiFiMAC":"%s","rssi":%d,"ethConn":%d,"ethIP":"%s","v":"%s"})",
                connState != NetworkSystem::CONN_STATE_NONE,
                networkSystem.isPaused() ? 1 : 0, 
                networkSystem.getConnStateCodeStr(connState).c_str(),
                networkSystem.getSSID().c_str(),
                networkSystem.getWiFiIPV4AddrStr().c_str(),
                networkSystem.getHostname().c_str(),
                getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str(),
                networkSystem.getRSSI(isValid),
                networkSystem.isEthConnectedWithIP() ? 1 : 0,
                networkSystem.getEthIPV4AddrStr().c_str(),
                getSysManager() ? getSysManager()->getSystemVersion().c_str() : "0.0.0");
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String NetworkManager::getDebugJSON()
{
    String debugStr, debugEthStr;
    bool isValid = false;
    // WiFi
    if (networkSystem.isWiFiStaConnectedWithIP())
        debugStr += R"("s":"conn","SSID":")" + networkSystem.getSSID() + 
                    R"(","IP":")" + networkSystem.getWiFiIPV4AddrStr() + 
                    R"(","rssi":)" + networkSystem.getRSSI(isValid) + 
                    R"()";
    else if (networkSystem.isPaused())
        debugStr += R"("s":"paused")";
    else
        debugStr += R"("s":"none")";

    // Ethernet
    if(networkSystem.isEthConnectedWithIP())
        debugEthStr += R"("ethIP":")" + networkSystem.getEthIPV4AddrStr() + R"(")";
    return "{" + (debugEthStr.length() > 0 ? debugStr + "," + debugEthStr : debugStr) + "}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetNamedValue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double NetworkManager::getNamedValue(const char* valueName, bool& isValid)
{
    switch(valueName[0])
    {
        case 'R': { return networkSystem.getRSSI(isValid); }
        default: { isValid = false; return 0; }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    endpointManager.addEndpoint("w", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                          std::bind(&NetworkManager::apiWifiSet, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                          "Setup WiFi SSID/password/hostname");
    endpointManager.addEndpoint("wc", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                          std::bind(&NetworkManager::apiWifiClear, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                          "Clear WiFi settings");
    // endpointManager.addEndpoint("wax", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
    //                       std::bind(&NetworkManager::apiWifiExtAntenna, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    //                       "Set external WiFi Antenna");
    // endpointManager.addEndpoint("wai", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
    //                       std::bind(&NetworkManager::apiWifiIntAntenna, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    //                       "Set internal WiFi Antenna");
    endpointManager.addEndpoint("wifipause", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET, 
            std::bind(&NetworkManager::apiWiFiPause, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
            "WiFi pause, wifipause/pause, wifipause/resume");
    endpointManager.addEndpoint("wifiscan", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                          std::bind(&NetworkManager::apiWifiScan, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                          "Scan WiFi networks - wifiscan/start - wifiscan/results");
}

void NetworkManager::apiWifiSet(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // LOG_I(MODULE_PREFIX, "apiWifiSet incoming %s", reqStr.c_str());

    // Get SSID - note that ? is valid in SSIDs so don't split on ? character
    String ssid = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1, false);
    // Get pw - as above
    String pw = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2, false);
    // LOG_I(MODULE_PREFIX, "WiFi PW length %d PW %s", pw.length(), pw.c_str());
    // Get hostname - as above
    String hostname = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3, false);
    // Get AP SSID
    String apSSID = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 4, false);
    // Get AP pw
    String apPassword = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 5, false);


    // Debug
    if ((ssid.length() > 0) || (apSSID.length() > 0))
    {
        LOG_I(MODULE_PREFIX, "apiWifiSet SSID %s (len %d) hostname %s (len %d) AP SSID %s (len %d) ", 
                        ssid.c_str(), ssid.length(), hostname.c_str(), hostname.length(), apSSID.c_str(), apSSID.length());
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiWifiSet neither STA or AP SSID is set");
    }

    // Configure WiFi
    bool rslt = networkSystem.configureWiFi(ssid, pw, hostname, apSSID, apPassword);
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

void NetworkManager::apiWifiClear(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // See if system restart required
    String sysRestartStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    bool sysRestart = !sysRestartStr.equalsIgnoreCase("norestart");

    // Clear stored credentials back to default
    esp_err_t err = networkSystem.clearCredentials();

    // Debug
    LOG_I(MODULE_PREFIX, "apiWifiClear ResultOK %s", err == ESP_OK ? "Y" : "N");

    // Response
    if (err == ESP_OK)
    {
        Raft::setJsonResult(reqStr.c_str(), respStr, true, nullptr, sysRestart ? R"("norestart":1)" : R"("norestart":0)");

        // Request a system restart
        if (sysRestart && getSysManager())
            getSysManager()->systemRestart();
        return;
    }
    Raft::setJsonErrorResult(reqStr.c_str(), respStr, esp_err_to_name(err));
}

// void NetworkManager::apiWifiExtAntenna(String &reqStr, String &respStr)
// {
//     LOG_I(MODULE_PREFIX, "Set external antenna - not supported");
//     Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
// }

// void NetworkManager::apiWifiIntAntenna(String &reqStr, String &respStr)
// {
//     LOG_I(MODULE_PREFIX, "Set internal antenna - not supported");
//     Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control WiFi pause on BLE connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::apiWiFiPause(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Get pause arg
    String arg = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1, false);

    // Check
    if (arg.equalsIgnoreCase("pause"))
    {
        networkSystem.pauseWiFi(true);
    }
    else if (arg.equalsIgnoreCase("resume"))
    {
        networkSystem.pauseWiFi(false);
    }
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scan WiFi
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::apiWifiScan(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "apiWifiScan %s", reqStr.c_str());

    // Get arg
    String arg = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1, false);

    // Scan WiFi
    String jsonResult;
    bool rslt = networkSystem.wifiScan(arg.equalsIgnoreCase("start"), jsonResult);
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt, jsonResult.c_str());
}
