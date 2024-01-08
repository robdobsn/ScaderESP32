/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Common code for Scaders
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SysModBase.h"
#include "SysManager.h"
#include "NetworkSystem.h"
#include "ESPUtils.h"

class ScaderCommon
{
public:
    ScaderCommon(SysModBase& base, const char* moduleName) : _base(base)
    {
        _moduleName = moduleName;
    }

    void setup()
    {
        // Get settings
        _isEnabled = _base.configGetLong("enable", false) != 0;

        // Name set in UI
        _scaderUIName = _base.configGetString("/ScaderCommon/name", "Scader");

        // Hostname set in UI
        _scaderHostname = _base.configGetString("/ScaderCommon/hostname", "Scader");

        // Debug
        LOG_I("ScaderCommon", "setup scaderUIName %s scaderHostname %s", 
                    _scaderUIName.c_str(), _scaderHostname.c_str());
    }

    String getStatusJSON()
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

    // Sysmodbase
    SysModBase& _base;

    // Module name
    String _moduleName;
};

