/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Papertrail logger
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LoggerPapertrail.h"
#include <ConfigBase.h>
#include <Logger.h>
#include <NetworkSystem.h>
#include <ESPUtils.h>
#include <RaftUtils.h>

// Log prefix
static const char *MODULE_PREFIX = "LogPapertrail";

LoggerPapertrail::LoggerPapertrail(const ConfigBase& logDestConfig)
    : LoggerBase(logDestConfig)
{
    // Get config
    _host = logDestConfig.getString("host", "");
    memset(&_hostAddrInfo, 0, sizeof(struct addrinfo));
    _dnsLookupDone = false;
    _port = logDestConfig.getLong("port", 0);
    _sysName = logDestConfig.getString("sysName", "");
    _sysName += "_" + getSystemMACAddressStr(ESP_MAC_WIFI_STA, "");
}

LoggerPapertrail::~LoggerPapertrail()
{
    if (_socketFd >= 0)
    {
        close(_socketFd);
    }
}

void IRAM_ATTR LoggerPapertrail::log(esp_log_level_t level, const char *tag, const char* msg)
{
    // Check level
    if (level > _level)
        return;

    // Check if we're connected
    if (!networkSystem.isIPConnected())
        return;

    // Check if DNS lookup done
    if (!_dnsLookupDone)
    {
        // Do DNS lookup
        struct addrinfo hints;
        memset(&hints,0,sizeof(hints));
        hints.ai_family=AF_INET;
        hints.ai_socktype=SOCK_DGRAM;
        hints.ai_flags=0;
        struct addrinfo *addrResult;
        if (getaddrinfo(_host.c_str(), _port.c_str(), &hints, &addrResult) != 0)
        {
            ESP_LOGE(MODULE_PREFIX, "log failed to resolve host %s", _host.c_str());
            return;
        }
        ESP_LOGI(MODULE_PREFIX, "log resolved host %s to %d.%d.%d.%d", _host.c_str(), 
                    addrResult->ai_addr->sa_data[0], addrResult->ai_addr->sa_data[1],
                    addrResult->ai_addr->sa_data[2], addrResult->ai_addr->sa_data[3]);
        _hostAddrInfo = *addrResult;
        _dnsLookupDone = true;

        // Create UDP socket
        ESP_LOGI(MODULE_PREFIX, "log create udp socket");
        _socketFd = socket(_hostAddrInfo.ai_family, _hostAddrInfo.ai_socktype, _hostAddrInfo.ai_protocol);
        if (_socketFd < 0)
        {
            ESP_LOGE(MODULE_PREFIX, "log create udp socket failed: %d errno: %d", _socketFd, errno);
        }
        else
        {
            // Debug
            ESP_LOGI(MODULE_PREFIX, "log hostIP %s port %s level %s sysName %s socketFd %d", 
                                _host.c_str(), _port.c_str(), getLevelStr(), _sysName.c_str(), _socketFd);
        }

    }
    
    // Start of log window?
    if (Raft::isTimeout(millis(), _logWindowStartMs, LOG_WINDOW_SIZE_MS))
    {
        _logWindowStartMs = millis();
        _logWindowCount = 1;
    }
    else
    {
        // Count log messages
        _logWindowCount++;
        if (_logWindowCount >= LOG_WINDOW_MAX_COUNT)
        {
            // Discard
            return;
        }
    }

    // Format message
    String logMsg = "<22>" + _sysName + ": " + msg;

    // Send to papertrail using UDP socket
    int ret = sendto(_socketFd, logMsg.c_str(), logMsg.length(), 0, _hostAddrInfo.ai_addr, _hostAddrInfo.ai_addrlen);
    if (ret < 0)
    {
        ESP_LOGE(MODULE_PREFIX, "log failed: %d errno %d", ret, errno);
        return;
    }
}
