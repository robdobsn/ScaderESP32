/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BLEManager
// Handles BLE connectivity and data
//
// RIC Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <SysModBase.h>
#include <sdkconfig.h>
#ifdef CONFIG_BT_ENABLED
#include "host/ble_uuid.h"
#endif
#include <ThreadSafeQueue.h>
#include <ProtocolRawMsg.h>
#include "BLEManStats.h"

#define USE_TIMED_ADVERTISING_CHECK 1

class CommsChannelMsg;
class APISourceInfo;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BLEManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BLEManager : public SysModBase
{
public:
    BLEManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
                ConfigBase *pMutableConfig, const char* defaultAdvName);
    virtual ~BLEManager();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

    // Add comms channel
    virtual void addCommsChannels(CommsCoreIF& commsCoreIF) override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

    // Get debug string
    virtual String getDebugJSON() override final;

    // Get named value
    virtual double getNamedValue(const char* valueName, bool& isValid) override final;

private:
    // Singleton BLEManager
    static BLEManager* _pBLEManager;

    // BLE enabled
    bool _enableBLE;

    // BLE device initialised
    bool _BLEDeviceInitialised;

    // Default advertising name
    String _defaultAdvName;

    // Configured advertising name
    String _configuredAdvertisingName;

    // Addr type
    uint8_t own_addr_type = 0;

    // Preferred connection params
    static const uint32_t PREFERRED_MTU_VALUE = 512;
    static const uint32_t LL_PACKET_TIME = 2120;
    static const uint32_t LL_PACKET_LENGTH = 251;

    // BLE advertising service UUIDs
#ifdef CONFIG_BT_ENABLED
    static ble_uuid128_t BLE_RICV2_ADVERTISING_UUID;
#endif

    // ChannelID used to identify this message channel to the CommsChannelManager object
    uint32_t _commsChannelID;

    // Status
    bool _isConnected = false;
    uint16_t _bleGapConnHandle = 0;
    int8_t _rssi = 0;
    uint32_t _rssiLastMs = 0;

    // RSSI check interval
    static const uint32_t RSSI_CHECK_MS = 2000;

    // Max packet length - seems to be OS dependent (iOS seems to truncate at 182?)
    static const uint32_t MAX_BLE_PACKET_LEN_DEFAULT = 450;
    uint32_t _maxPacketLength;
    
    // Outbound queue for fragments of messages
    static const int DEFAULT_OUTBOUND_MSG_QUEUE_SIZE = 30;
    uint32_t _outboundQueueSize = DEFAULT_OUTBOUND_MSG_QUEUE_SIZE;
    ThreadSafeQueue<ProtocolRawMsg> _bleFragmentQueue;

    // Min time between adjacent outbound messages
    static const uint32_t BLE_MIN_TIME_BETWEEN_OUTBOUND_MSGS_MS = 25;
    uint32_t _lastOutboundMsgMs;

    // Task that runs the outbound queue - if selected by #define in cpp file
    volatile TaskHandle_t _outboundMsgTaskHandle;
    static const int DEFAULT_TASK_CORE = 0;
    static const int DEFAULT_TASK_PRIORITY = 1;
    static const int DEFAULT_TASK_SIZE_BYTES = 3000;

    // Outbound message in flight
    volatile bool _outboundMsgInFlight = false;
    uint32_t _outbountMsgInFlightStartMs = 0;
    static const uint32_t BLE_OUTBOUND_MSG_IN_FLIGHT_TIMEOUT_MS = 1000;

    // Stats
    BLEManStats _bleStats;

    // BLE performance testing
    static const uint32_t TEST_THROUGHPUT_MAX_PAYLOAD = 500;
    uint32_t _testPerfPrbsState = 1;
    uint32_t _lastTestMsgCount = 0;

    // BLE restart state
    enum BLERestartState
    {
        BLERestartState_Idle,
        BLERestartState_StopRequired,
        BLERestartState_StartRequired
    };
    BLERestartState _bleRestartState = BLERestartState_Idle;
    static const uint32_t BLE_RESTART_BEFORE_STOP_MS = 200;
    static const uint32_t BLE_RESTART_BEFORE_START_MS = 200;
    uint32_t _bleRestartLastMs = 0;

#ifdef USE_TIMED_ADVERTISING_CHECK
    // Advertising check timeout
    bool _advertisingCheckRequired = false;
    uint32_t _advertisingCheckMs = 0;
    static const uint32_t ADVERTISING_CHECK_MS = 3000;
#endif

    // Helpers
    void applySetup();
    void onSync();
    bool startAdvertising();
    void stopAdvertising();
    static int nimbleGapEventStatic(struct ble_gap_event *event, void *arg);
    int nimbleGapEvent(struct ble_gap_event *event, void *arg);
    static void bleHostTask(void *param);
    static void logConnectionInfo(struct ble_gap_conn_desc *desc);
    static void print_addr(const uint8_t *addr);
    static void gattAccessCallbackStatic(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength);
    void gattAccessCallback(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength);
    bool sendBLEMsg(CommsChannelMsg& msg);
    void setIsConnected(bool isConnected, uint16_t connHandle = 0);
    String getAdvertisingName();
    void handleSendFromOutboundQueue();
    static void outboundMsgTaskStatic(void* pvParameters);
    void outboundMsgTask();
    bool nimbleStart();
    bool nimbleStop();
    void apiBLERestart(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    uint32_t parkmiller_next(uint32_t seed) const;
};
