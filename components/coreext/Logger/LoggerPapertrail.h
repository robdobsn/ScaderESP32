/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Papertrail logger
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <LoggerBase.h>
#include <ArduinoOrAlt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

class ConfigBase;

class LoggerPapertrail : public LoggerBase
{
public:
    LoggerPapertrail(const ConfigBase& logDestConfig);
    virtual ~LoggerPapertrail();
    virtual void IRAM_ATTR log(esp_log_level_t level, const char *tag, const char* msg) override final;

private:
    String _host;
    String _port;
    String _sysName;
    struct addrinfo _hostAddrInfo;
    bool _dnsLookupDone = false;
    int _socketFd = -1;

    // Avoid swamping the network
    uint32_t _logWindowStartMs = 0;
    uint32_t _logWindowCount = 0;
    static const uint32_t LOG_WINDOW_SIZE_MS = 60000;
    static const uint32_t LOG_WINDOW_MAX_COUNT = 60;
    uint32_t _logWindowThrottleStartMs = 0;
    static const uint32_t LOG_WINDOW_THROTTLE_BACKOFF_MS = 30000;
};
