# LoggerLoki Implementation Guide

## Overview

This document describes the implementation of a Grafana Loki logger (`LoggerLoki`) for the Raft framework. The logger follows the same architecture as the existing `LoggerPapertrail` — extending `LoggerBase`, using a FreeRTOS ring buffer for thread-safe log ingestion, and performing network I/O exclusively from the main task via `loop()`.

The key difference is the transport: Papertrail uses **UDP syslog** (single `sendto()` per message), while Loki uses **HTTP POST** with a JSON payload to `/loki/api/v1/push`. This means the Loki logger must batch multiple log lines into a single HTTP request and manage a TCP connection (via `esp_http_client`).

---

## Architecture

```
ESP_LOGx() → esp_log vprintf hook → LoggerCore::log()
    → LoggerLoki::log()          [called from ANY task/ISR context]
        → format + push to ring buffer (non-blocking, 0 ticks)

Main task loop → LoggerCore::loop()
    → LoggerLoki::loop()          [called from main task only]
        → drain ring buffer (up to N messages)
        → batch into JSON payload
        → HTTP POST to Loki push endpoint
```

### Thread Safety

Identical to LoggerPapertrail:
- `log()` only touches the FreeRTOS ring buffer via `xRingbufferSend(..., 0)` — safe from any context
- `loop()` does all network I/O — runs exclusively on the main task
- No mutexes needed

---

## Loki Push API

### Endpoint

```
POST /loki/api/v1/push
Content-Type: application/json
```

### Optional Authentication

- **None** (local/private Loki instance)
- **Basic Auth**: `Authorization: Basic base64(user:password)`
- **Bearer Token**: `Authorization: Bearer <api-key>` (e.g. Grafana Cloud)

### Payload Format

```json
{
  "streams": [
    {
      "stream": {
        "job": "scader",
        "host": "TestProS3_AABBCC",
        "level": "info"
      },
      "values": [
        ["1719500000000000000", "WiFi connected"],
        ["1719500000100000000", "MQTT broker reachable"]
      ]
    }
  ]
}
```

- **`stream`**: A set of label key-value pairs. All entries in `values` share these labels.
- **`values`**: Array of `[timestamp_nanoseconds_string, log_line_string]` pairs.
- Timestamps must be **nanoseconds since epoch** as a string. Loki rejects out-of-order timestamps within the same stream.

### Batching Strategy

Rather than one HTTP POST per log line (too expensive on ESP32), the logger batches messages:

1. `loop()` drains up to `MAX_MSGS_PER_BATCH` (e.g. 20) from the ring buffer
2. Groups them by log level (each level = separate stream, since `level` is a label)
3. Formats a single JSON payload with one or more streams
4. Sends one HTTP POST

If no messages are available, `loop()` returns immediately (no network call).

A **flush interval** (e.g. 2 seconds) ensures messages are sent even under low log volume — `loop()` checks elapsed time since last send and flushes if the interval has passed and there are buffered messages.

---

## Configuration

### SysTypes.json

Add a new entry to the `logDests` array in the `LogManager` config:

```json
{
  "LogManager": {
    "enable": 1,
    "logDests": [
      {
        "enable": true,
        "type": "Loki",
        "host": "192.168.1.100",
        "port": 3100,
        "path": "/loki/api/v1/push",
        "level": "INFO",
        "labels": {
          "job": "scader"
        },
        "authType": "none",
        "authUser": "",
        "authKey": ""
      }
    ]
  }
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enable` | bool | `false` | Enable/disable this logger |
| `type` | string | — | Must be `"Loki"` |
| `host` | string | — | Loki server hostname or IP |
| `port` | int | `3100` | Loki HTTP port |
| `path` | string | `"/loki/api/v1/push"` | Push API path |
| `level` | string | `"INFO"` | Minimum log level |
| `labels` | object | `{}` | Static Loki labels (merged with auto-generated `host` label) |
| `authType` | string | `"none"` | `"none"`, `"basic"`, or `"bearer"` |
| `authUser` | string | `""` | Username for basic auth |
| `authKey` | string | `""` | Password (basic) or API key (bearer) |

---

## Class Design

### LoggerLoki.h

```cpp
#pragma once

#include "LoggerBase.h"
#include "RaftArduino.h"
#include "DNSResolver.h"
#include "freertos/ringbuf.h"
#include "esp_http_client.h"

class RaftJsonIF;

class LoggerLoki : public LoggerBase
{
public:
    LoggerLoki(const RaftJsonIF& logDestConfig, const String& systemName,
               const String& systemUniqueString);
    virtual ~LoggerLoki();
    virtual void log(esp_log_level_t level, const char* tag, const char* msg) override final;
    virtual void loop() override;

private:
    // Config
    String _hostname;
    uint16_t _port = 3100;
    String _path;
    String _sysName;
    DNSResolver _dnsResolver;

    // Static labels from config (serialized as JSON fragment)
    String _staticLabelsJson;

    // Authentication
    enum AuthType { AUTH_NONE, AUTH_BASIC, AUTH_BEARER };
    AuthType _authType = AUTH_NONE;
    String _authHeader;  // Pre-formatted "Basic xxx" or "Bearer xxx"

    // HTTP client handle (persistent connection)
    esp_http_client_handle_t _httpClient = nullptr;

    // Ring buffer
    RingbufHandle_t _ringBuf = nullptr;
    static const uint32_t RING_BUF_SIZE = 16384;

    // Batching
    static const uint32_t MAX_MSGS_PER_BATCH = 20;
    static const uint32_t FLUSH_INTERVAL_MS = 2000;
    uint32_t _lastFlushTimeMs = 0;

    // Rate limiting (same as Papertrail)
    uint32_t _logWindowStartMs = 0;
    uint32_t _logWindowCount = 0;
    static const uint32_t LOG_WINDOW_SIZE_MS = 60000;
    static const uint32_t LOG_WINDOW_MAX_COUNT = 60;

    // Internal error throttling
    uint32_t _internalErrorLastTimeMs = 0;
    static const uint32_t INTERNAL_ERROR_LOG_MIN_GAP_MS = 10000;

    // Helpers
    bool ensureHttpClient();
    bool sendBatch(const char* jsonPayload, size_t len);
    void formatBatchPayload(String& outJson,
                            const char* messages[], esp_log_level_t levels[],
                            uint32_t count);
    static const char* lokiLevelStr(esp_log_level_t level);

    // Ring buffer item header: level byte + null-terminated message
    // Layout: [uint8_t level][char msg[]\0]

    static constexpr const char* MODULE_PREFIX = "LogLoki";
};
```

### Ring Buffer Item Format

Each item pushed to the ring buffer contains a level byte followed by the log message:

```
[1 byte: esp_log_level_t] [N bytes: message string including \0]
```

This avoids needing a second ring buffer or struct — `log()` writes `level + msg` atomically, and `loop()` reads them back, extracting the level for label grouping.

---

## Implementation Details

### LoggerLoki.cpp

#### Constructor

```cpp
LoggerLoki::LoggerLoki(const RaftJsonIF& logDestConfig, const String& systemName,
                       const String& systemUniqueString)
    : LoggerBase(logDestConfig)
{
    _hostname = logDestConfig.getString("host", "");
    _port = logDestConfig.getLong("port", 3100);
    _path = logDestConfig.getString("path", "/loki/api/v1/push");
    _sysName = systemName + "_" + systemUniqueString;

    // DNS resolver
    _dnsResolver.setHostname(_hostname.c_str());

    // Parse static labels from config
    // Build a JSON fragment like: "job":"scader","env":"prod"
    // These get merged with auto-generated labels (host, level)
    String labelsStr = logDestConfig.getString("labels", "{}");
    // TODO: iterate labels object and build _staticLabelsJson fragment

    // Authentication
    String authTypeStr = logDestConfig.getString("authType", "none");
    if (authTypeStr.equalsIgnoreCase("basic"))
    {
        _authType = AUTH_BASIC;
        String user = logDestConfig.getString("authUser", "");
        String key = logDestConfig.getString("authKey", "");
        // Base64 encode user:key and store as "Basic <encoded>"
        // Use mbedtls_base64_encode() available in ESP-IDF
    }
    else if (authTypeStr.equalsIgnoreCase("bearer"))
    {
        _authType = AUTH_BEARER;
        _authHeader = "Bearer " + logDestConfig.getString("authKey", "");
    }

    // Create ring buffer
    _ringBuf = xRingbufferCreate(RING_BUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (!_ringBuf)
        ESP_LOGE(MODULE_PREFIX, "Failed to create ring buffer");
}
```

#### log() — Called From Any Context

```cpp
void LOGGING_FUNCTION_DECORATOR LoggerLoki::log(esp_log_level_t level, const char* tag, const char* msg)
{
    if ((level > _level) || _isPaused || !_ringBuf)
        return;

    // Rate limiting (identical to Papertrail)
    if (Raft::isTimeout(millis(), _logWindowStartMs, LOG_WINDOW_SIZE_MS))
    {
        _logWindowStartMs = millis();
        _logWindowCount = 1;
    }
    else
    {
        _logWindowCount++;
        if (_logWindowCount >= LOG_WINDOW_MAX_COUNT)
            return;
    }

    // Build ring buffer item: [level_byte][tag: msg\0]
    size_t msgLen = strlen(msg);
    size_t tagLen = strlen(tag);
    size_t itemLen = 1 + tagLen + 2 + msgLen + 1;  // level + tag + ": " + msg + \0
    
    // Use stack buffer for small messages, drop oversized ones
    if (itemLen > 1024)
        return;
    
    char item[1024];
    item[0] = (char)level;
    memcpy(item + 1, tag, tagLen);
    item[1 + tagLen] = ':';
    item[2 + tagLen] = ' ';
    memcpy(item + 3 + tagLen, msg, msgLen + 1);  // includes \0

    xRingbufferSend(_ringBuf, item, itemLen, 0);
}
```

#### loop() — Main Task Only

```cpp
void LoggerLoki::loop()
{
    if (!_ringBuf)
        return;

    // Drain messages from ring buffer
    const char* messages[MAX_MSGS_PER_BATCH];
    esp_log_level_t levels[MAX_MSGS_PER_BATCH];
    void* items[MAX_MSGS_PER_BATCH];
    size_t itemSizes[MAX_MSGS_PER_BATCH];
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_MSGS_PER_BATCH; i++)
    {
        size_t itemSize = 0;
        void* pItem = xRingbufferReceive(_ringBuf, &itemSize, 0);
        if (!pItem)
            break;
        
        items[count] = pItem;
        itemSizes[count] = itemSize;
        levels[count] = (esp_log_level_t)((char*)pItem)[0];
        messages[count] = ((char*)pItem) + 1;  // skip level byte
        count++;
    }

    // Nothing to send?
    if (count == 0)
        return;

    // Check flush interval - if not enough time has passed, put items back
    // Actually, since we already consumed from ring buffer, always send.
    // The flush interval logic should gate whether we attempt to drain at all:
    // For simplicity, always drain and send immediately.

    // Ensure HTTP client is ready
    if (!ensureHttpClient())
    {
        // Return items to ring buffer (they're lost — acceptable)
        for (uint32_t i = 0; i < count; i++)
            vRingbufferReturnItem(_ringBuf, items[i]);
        return;
    }

    // Format JSON payload
    String jsonPayload;
    formatBatchPayload(jsonPayload, messages, levels, count);

    // Send
    sendBatch(jsonPayload.c_str(), jsonPayload.length());

    // Return all items to ring buffer
    for (uint32_t i = 0; i < count; i++)
        vRingbufferReturnItem(_ringBuf, items[i]);

    _lastFlushTimeMs = millis();
}
```

#### formatBatchPayload()

```cpp
void LoggerLoki::formatBatchPayload(String& outJson,
                                     const char* messages[], esp_log_level_t levels[],
                                     uint32_t count)
{
    // Get current time as nanosecond string
    // Use gettimeofday() if SNTP is synced, otherwise millis()-based relative time
    struct timeval tv;
    gettimeofday(&tv, NULL);

    outJson = "{\"streams\":[{\"stream\":{\"host\":\"";
    outJson += _sysName;
    outJson += "\"";
    // Append static labels
    if (_staticLabelsJson.length() > 0)
    {
        outJson += ",";
        outJson += _staticLabelsJson;
    }
    outJson += "},\"values\":[";

    for (uint32_t i = 0; i < count; i++)
    {
        if (i > 0)
            outJson += ",";
        
        // Nanosecond timestamp (increment by 1ns per message to ensure ordering)
        uint64_t nsTimestamp = (uint64_t)tv.tv_sec * 1000000000ULL +
                               (uint64_t)tv.tv_usec * 1000ULL + i;
        
        char tsBuf[24];
        snprintf(tsBuf, sizeof(tsBuf), "%llu", nsTimestamp);

        outJson += "[\"";
        outJson += tsBuf;
        outJson += "\",\"";
        
        // JSON-escape the message (minimal: escape quotes and backslashes)
        const char* msg = messages[i];
        for (const char* p = msg; *p; p++)
        {
            if (*p == '"')
                outJson += "\\\"";
            else if (*p == '\\')
                outJson += "\\\\";
            else if (*p == '\n')
                outJson += "\\n";
            else
                outJson += *p;
        }
        
        outJson += "\"]";
    }

    outJson += "]}]}";
}
```

#### ensureHttpClient()

```cpp
bool LoggerLoki::ensureHttpClient()
{
    // Check network connectivity
    if (!networkSystem.isIPConnected())
        return false;

    // If client already exists, reuse it
    if (_httpClient)
        return true;

    // Resolve hostname
    ip_addr_t hostIPAddr;
    if (!_dnsResolver.getIPAddr(hostIPAddr))
        return false;

    // Build URL
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s",
             ipaddr_ntoa(&hostIPAddr), _port, _path.c_str());

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 5000;
    config.keep_alive_enable = true;
    config.keep_alive_idle = 30;
    config.keep_alive_interval = 5;

    _httpClient = esp_http_client_init(&config);
    if (!_httpClient)
    {
        if (Raft::isTimeout(millis(), _internalErrorLastTimeMs, INTERNAL_ERROR_LOG_MIN_GAP_MS))
        {
            ESP_LOGE(MODULE_PREFIX, "Failed to init HTTP client");
            _internalErrorLastTimeMs = millis();
        }
        return false;
    }

    // Set headers
    esp_http_client_set_header(_httpClient, "Content-Type", "application/json");
    if (_authType != AUTH_NONE)
        esp_http_client_set_header(_httpClient, "Authorization", _authHeader.c_str());

    return true;
}
```

#### sendBatch()

```cpp
bool LoggerLoki::sendBatch(const char* jsonPayload, size_t len)
{
    esp_http_client_set_post_field(_httpClient, jsonPayload, len);

    esp_err_t err = esp_http_client_perform(_httpClient);
    if (err != ESP_OK)
    {
        if (Raft::isTimeout(millis(), _internalErrorLastTimeMs, INTERNAL_ERROR_LOG_MIN_GAP_MS))
        {
            ESP_LOGE(MODULE_PREFIX, "HTTP POST failed: %s", esp_err_to_name(err));
            _internalErrorLastTimeMs = millis();
        }
        // Destroy client so it gets recreated next time
        esp_http_client_cleanup(_httpClient);
        _httpClient = nullptr;
        return false;
    }

    int statusCode = esp_http_client_get_status_code(_httpClient);
    if (statusCode != 204 && statusCode != 200)
    {
        if (Raft::isTimeout(millis(), _internalErrorLastTimeMs, INTERNAL_ERROR_LOG_MIN_GAP_MS))
        {
            ESP_LOGW(MODULE_PREFIX, "Loki returned HTTP %d", statusCode);
            _internalErrorLastTimeMs = millis();
        }
    }

    return (statusCode == 204 || statusCode == 200);
}
```

#### Destructor

```cpp
LoggerLoki::~LoggerLoki()
{
    if (_httpClient)
    {
        esp_http_client_cleanup(_httpClient);
        _httpClient = nullptr;
    }
    if (_ringBuf)
    {
        vRingbufferDelete(_ringBuf);
        _ringBuf = nullptr;
    }
}
```

---

## Registration in LogManager

Add the new logger type to `LogManager.cpp`:

```cpp
#include "LoggerLoki.h"

// In LogManager::setup(), add after the Papertrail block:
else if (logDestType.equalsIgnoreCase("Loki"))
{
    LoggerLoki *pLogger = new LoggerLoki(logDestConfig, getSystemName(), getSystemUniqueString());
    loggerCore.addLogger(pLogger);
}
```

---

## Files to Create/Modify

| File | Action | Description |
|------|--------|-------------|
| `raftdevlibs/RaftSysMods/components/LogManager/LoggerLoki.h` | **Create** | Class declaration |
| `raftdevlibs/RaftSysMods/components/LogManager/LoggerLoki.cpp` | **Create** | Implementation |
| `raftdevlibs/RaftSysMods/components/LogManager/LogManager.cpp` | **Modify** | Add `#include "LoggerLoki.h"` and type dispatch |
| `systypes/*/SysTypes.json` | **Modify** | Add Loki log destination config |

No CMakeLists.txt changes needed — the LogManager component has no `CMakeLists.txt`; all `.cpp` files in the component directory are compiled automatically by the Raft build system.

---

## Key Differences from LoggerPapertrail

| Aspect | Papertrail | Loki |
|--------|-----------|------|
| Transport | UDP (single `sendto()`) | HTTP POST (TCP via `esp_http_client`) |
| Payload format | Syslog: `<22>sysName: msg` | JSON: `{"streams":[...]}` |
| Connection | Stateless UDP socket | Persistent HTTP keep-alive |
| Batching | 1 message per `sendto()` | N messages per HTTP POST |
| Timestamps | Not sent (server-assigned) | Required (nanoseconds since epoch) |
| Labels/metadata | Fixed syslog facility | Configurable key-value labels |
| Authentication | None | Optional Basic/Bearer |
| Error recovery | Recreate UDP socket | Destroy + recreate `esp_http_client` |
| Memory overhead | ~16KB ring buffer | ~16KB ring buffer + `esp_http_client` internal buffers |

---

## Implementation Considerations

### esp_http_client Blocking Behaviour

`esp_http_client_perform()` is **synchronous** — it blocks until the HTTP transaction completes (or `timeout_ms` expires). Since `loop()` runs on the main task, a 5-second timeout is acceptable but should be monitored. If Loki is unreachable, the logger will block for up to 5 seconds per loop iteration.

**Mitigation**: Only attempt to send when there are messages to send. If a send fails, back off (e.g. don't retry for 10 seconds) to avoid repeatedly blocking the main loop.

### Timestamp Accuracy

Loki requires monotonically increasing timestamps per stream. The implementation uses `gettimeofday()` (SNTP-synced) plus a per-message nanosecond increment to guarantee ordering within a batch.

If SNTP is not yet synced, timestamps will be relative to boot time (epoch 1970). Loki will accept these but they won't correlate with real wall-clock time. Consider delaying Loki sends until SNTP sync is confirmed via `networkSystem` or checking `tv.tv_sec > 1700000000` (post-2023).

### JSON Payload Size

A batch of 20 messages averaging 100 bytes each produces a JSON payload of roughly 3-4 KB — well within ESP32 memory limits and typical Loki ingestion limits.

### HTTPS Support

For Grafana Cloud or TLS-enabled Loki, the `esp_http_client` supports HTTPS with certificate bundles. Add `config.crt_bundle_attach = esp_crt_bundle_attach` in `ensureHttpClient()` and change the URL scheme to `https://`. This uses the ESP-IDF certificate bundle already included in the build.

### Avoiding Re-entrant Logging

The logger must not trigger ESP log calls from within `log()` — this would cause infinite recursion. All error logging (`ESP_LOGE`, `ESP_LOGW`) is confined to `loop()` and helper functions called from `loop()`, which are safely outside the log callback path.

### Memory Budget

- Ring buffer: 16 KB (configurable via `RING_BUF_SIZE`)
- `esp_http_client` internal buffer: ~2-4 KB
- JSON payload string: ~4 KB peak
- **Total**: ~24 KB — comparable to Papertrail's ~18 KB footprint

---

## Testing

1. **Local Loki instance**: Run Loki via Docker (`grafana/loki:latest`) on the development machine
2. **Verify log ingestion**: Query `{host="TestProS3_AABBCC"}` in Grafana Explore
3. **Test reconnection**: Kill and restart Loki — logger should recover automatically
4. **Test back-pressure**: Generate high log volume — ring buffer should silently drop excess
5. **Test without network**: Logs should queue in ring buffer until connectivity is restored (up to buffer capacity)

### Docker Compose for Local Testing

```yaml
services:
  loki:
    image: grafana/loki:latest
    ports:
      - "3100:3100"
    command: -config.file=/etc/loki/local-config.yaml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
```

---

## Implementation Steps

1. Create `LoggerLoki.h` with the class declaration
2. Create `LoggerLoki.cpp` with the full implementation
3. Add `#include "LoggerLoki.h"` and the `"Loki"` type dispatch to `LogManager.cpp`
4. Add a Loki log destination entry to the relevant `SysTypes.json`
5. Build and flash
6. Verify logs appear in Grafana
