/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Common code for Scaders
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "PlatformUtils.h"
#include "NetworkSystem.h"
#include "esp_timer.h"

// #define DEBUG_SCADER_COMMON_SETUP

class ScaderCommon
{
public:
    ScaderCommon(RaftSysMod& base, RaftJsonIF& sysConfig, const char* moduleName) : 
                _base(base),
                _sysConfig(sysConfig)
    {
        _moduleName = moduleName;

        LOG_I("ScaderCommon", "ScaderCommon created moduleName %s", _moduleName.c_str());
    }

    void setup()
    {
        // Get settings
        _isEnabled = _base.configGetBool("enable", false);

        // Name set in UI
        _scaderUIName = _sysConfig.getString("ScaderCommon/name", "Scader");

        // Hostname set in UI
        _scaderHostname = _sysConfig.getString("ScaderCommon/hostname", "Scader");

        // If not blank set the UI name as the friendly name for the system
        if (_scaderUIName.length() > 0)
        {
            _base.getSysManager()->setNamedValue(nullptr, "AutoSetHostname", false);
            _base.getSysManager()->setNamedString(nullptr, "FriendlyName", _scaderUIName.c_str());
        }

        // Set the hostname if not blank
        if (_scaderHostname.length() > 0)
        {
            networkSystem.setHostname(_scaderHostname.c_str());
        }

        // Debug
#ifdef DEBUG_SCADER_COMMON_SETUP
        LOG_I("ScaderCommon", "setup scaderUIName %s scaderHostname %s", 
                    _scaderUIName.c_str(), _scaderHostname.c_str());
#endif
    }

    String getStatusJSON() const
    {
        // Get network information
        // NetworkSystem publishes nested objects:
        //   "eth":     {"conn":<0|1>, "IP":"<ip>", "MAC":"<mac>"}
        //   "wifiSTA": {"conn":<0|1>, "SSID":"...", "MAC":"<mac>", "IP":"<ip>" (only when connected)}
        RaftJson networkJson = _base.sysModGetStatusJSON("NetMan");

        // Extract hostname
        String hostname = networkJson.getString("hostname", "");

        // Check if ethernet or WiFi STA are connected (with IP)
        bool ethConnected = networkJson.getLong("eth/conn", 0) != 0;
        bool wifiConnected = networkJson.getLong("wifiSTA/conn", 0) != 0;

        // Extract MAC address and IP address (prefer Ethernet if both are up)
        String macAddress;
        String ipAddress;
        if (ethConnected)
        {
            macAddress = getSystemMACAddressStr(ESP_MAC_ETH, ":").c_str();
            ipAddress = networkJson.getString("eth/IP", "");
        }
        else if (wifiConnected)
        {
            macAddress = getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str();
            ipAddress = networkJson.getString("wifiSTA/IP", "");
        }
        else
        {
            // Not connected — still report the WiFi STA MAC so the address-resolver can
            // correlate the envelope with the device by MAC. IP remains blank.
            macAddress = getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str();
            ipAddress = "";
        }

        // Format base JSON
        bool isValid = false;
        String jsonStr =
                        R"("module":")" + _moduleName +
                        R"(","name":")" + _scaderUIName + 
                        R"(","version":")" + _base.getSysManager()->getNamedString(nullptr, "SystemVersion", isValid) + 
                        R"(","hostname":")" + _scaderHostname + 
                        R"(","IP":")" + ipAddress + 
                        R"(","MAC":")" + macAddress + 
                        R"(","upMs":)" + String(esp_timer_get_time() / 1000ULL);
        return jsonStr;
    }

    bool isEnabled() const
    {
        return _isEnabled;
    }

    String getModuleName()
    {
        return _moduleName;
    }

    String getUIName()
    {
        return _scaderUIName;
    }

    String getScaderHostname()
    {
        return _scaderHostname;
    }

private:
    // Name
    String _scaderUIName;

    // Hostname
    String _scaderHostname;

    // Enabled flag
    bool _isEnabled = false;

    // RaftSysMod
    RaftSysMod& _base;

    // Module name
    String _moduleName;

    // System config
    RaftJsonIF& _sysConfig;

    // Debug
    static constexpr const char* MODULE_PREFIX = "ScaderCommon";
};

