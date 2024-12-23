/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Common code for Scaders
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftSysMod.h"
#include "SysManager.h"
#include "NetworkSystem.h"
#include "PlatformUtils.h"

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
            String respStr;
            _base.getSysManager()->setFriendlyName(_scaderUIName.c_str(), false, respStr);
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
        RaftJson networkJson = _base.sysModGetStatusJSON("NetMan");

        // Extract hostname
        String hostname = networkJson.getString("hostname", "");

        // Check if ethernet connected
        bool ethConnected = networkJson.getLong("ethConn", false) != 0;

        // Extract MAC address
        String macAddress;
        String ipAddress;
        if (ethConnected)
        {
            macAddress = getSystemMACAddressStr(ESP_MAC_ETH, ":").c_str(),
            ipAddress = networkJson.getString("ethIP", "");
        }
        else
        {
            macAddress = getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str(),
            ipAddress = networkJson.getString("IP", "");
        }

        // Format base JSON
        String jsonStr =
                        R"("module":")" + _moduleName +
                        R"(","name":")" + _scaderUIName + 
                        R"(","version":")" + _base.getSysManager()->getSystemVersion() + 
                        R"(","hostname":")" + _scaderHostname + 
                        R"(","IP":")" + ipAddress + 
                        R"(","MAC":")" + macAddress + 
                        R"(","upMs":)" + String(esp_timer_get_time() / 1000ULL);
        return jsonStr;
    }

    bool isEnabled()
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

