/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Common code for Scaders
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <SysManager.h>
#include <JSONParams.h>
#include <NetworkSystem.h>
#include <ESPUtils.h>

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
        _scaderFriendlyName = _base.configGetString("/ScaderCommon/name", "Scader");
    }

    String getStatusJSON()
    {
            // Get network information
        JSONParams networkJson = _base.sysModGetStatusJSON("NetMan");

        // Extract hostname
        String hostname = networkJson.getString("Hostname", "");

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
                        R"(","name":")" + _scaderFriendlyName + 
                        R"(","version":")" + _base.getSysManager()->getSystemVersion() + 
                        R"(","hostname":")" + hostname + 
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

    String getFriendlyName()
    {
        return _scaderFriendlyName;
    }

private:
    // Name
    String _scaderFriendlyName;

    // Enabled flag
    bool _isEnabled = false;

    // Sysmodbase
    SysModBase& _base;

    // Module name
    String _moduleName;
};

