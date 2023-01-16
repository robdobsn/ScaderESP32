/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manager for SysMods (System Modules)
// All modules that are core to the system should be derived from SysModBase
// These modules are then serviced by this manager's service function
// They can be enabled/disabled and reconfigured in a consistent way
// Also modules can be referred to by name to allow more complex interaction
//
// Rob Dobson 2019
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once 

#include <ArduinoOrAlt.h>
#include <ExecTimer.h>
#include <SupervisorStats.h>
#include <SysModBase.h>
#include <RestAPIEndpointManager.h>
#include <CommsChannelManager.h>
#include <list>
#include <vector>

typedef String (*SysManager_statsCB)();

class ConfigBase;
class RestAPIEndpointManager;

class SysManager
{
public:
    // Constructor
    SysManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Setup
    void setup();

    // Service
    void service();

    // Manage machines
    void add(SysModBase* pSysMod);

    // Get system name
    String getSystemName()
    {
        return _systemName;
    }

    // Get system version
    String getSystemVersion()
    {
        return _systemVersion;
    }

    // Get friendly name
    String getFriendlyName();
    bool getFriendlyNameIsSet();
    bool setFriendlyName(const String& friendlyName, bool setHostname, String& respStr);

    // Set system unique string
    void setSystemUniqueString(const char* sysUniqueStr)
    {
        _systemUniqueString = sysUniqueStr;
    }

    // Set stats callback (for SysManager's own stats)
    void setStatsCB(SysManager_statsCB statsCB)
    {
        _statsCB = statsCB;
    }

    // Add status change callback on a SysMod
    void setStatusChangeCB(const char* sysModName, SysMod_statusChangeCB statusChangeCB);

    // Get status from SysMod
    String getStatusJSON(const char* sysModName);

    // Get debug from SysMod
    String getDebugJSON(const char* sysModName);

    // Send command to SysMod
    UtilsRetCode::RetCode sendCmdJSON(const char* sysModName, const char* cmdJSON);

    // Send message-generator callback to SysMod
    void sendMsgGenCB(const char* sysModName, const char* msgGenID, SysMod_publishMsgGenFn msgGenCB, SysMod_stateDetectCB stateDetectCB);

    // Get named value 
    double getNamedValue(const char* sysModName, const char* valueName, bool& isValid);

    // Request system restart
    void systemRestart()
    {
        // Actual restart occurs within service routine after a short delay
        _systemRestartPending = true;
        _systemRestartMs = millis();
    }

    // REST API Endpoints
    void setRestAPIEndpoints(RestAPIEndpointManager& restAPIEndpoints)
    {
        _pRestAPIEndpointManager = &restAPIEndpoints;
    }

    RestAPIEndpointManager* getRestAPIEndpointManager()
    {
        return _pRestAPIEndpointManager;
    }

    // CommsChannelManager
    void setCommsChannelManager(CommsChannelManager& commsChannelManager)
    {
        _pCommsChannelManager = &commsChannelManager;
    }

    CommsChannelManager* getCommsChannelManager()
    {
        return _pCommsChannelManager;
    }

    SupervisorStats* getStats()
    {
        return &_supervisorStats;
    }

    void informOfFileStreamActivity(bool isMainFWUpdate, bool isFileSystemActivity, bool isStreaming)
    {
        _isSystemMainFWUpdate = isMainFWUpdate;
        _isSystemFileTransferring = isFileSystemActivity;
        _isSystemStreaming = isStreaming;
    }

    // File/stream system activity - main FW update
    bool isSystemMainFWUpdate()
    {
        return _isSystemMainFWUpdate;
    }

    // File/stream system activity - streaming
    bool isSystemFileTransferring()
    {
        return _isSystemFileTransferring;
    }

    // File/stream system activity - streaming
    bool isSystemStreaming()
    {
        return _isSystemStreaming;
    }

private:

    // Name of this module
    String _moduleName;

    // Consts
    static const uint32_t RIC_SERIAL_NUMBER_BYTES = 16;
    static constexpr const char* RIC_SERIAL_SET_MAGIC_STR = "RoboticalMagic";

    // Service loop supervisor
    void supervisorSetup();
    bool _supervisorDirty = false;

    // Service loop
    std::vector<SysModBase*> _sysModServiceVector;
    uint32_t _serviceLoopCurModIdx = 0;

    // NOTE: _sysModuleList and _supervisorStats must be in synch
    //       when a module is added it must be added to both lists

    // List of modules
    std::list<SysModBase*> _sysModuleList;

    // Stress test loop delay
    uint32_t _stressTestLoopDelayMs = 0;
    uint32_t _stressTestLoopSkipCount = 0;
    uint32_t _stressTestCurSkipCount = 0;

    // Supervisor statistics
    SupervisorStats _supervisorStats;

    // Threshold of time for SysMod service considered too slow
    static const uint32_t SLOW_SYS_MOD_THRESHOLD_MS_DEFAULT = 50;
    uint32_t _slowSysModThresholdUs = SLOW_SYS_MOD_THRESHOLD_MS_DEFAULT * 1000;

    // Monitor timer and period
    unsigned long _monitorPeriodMs = 0;
    unsigned long _monitorTimerMs = 0;
    bool _monitorTimerStarted = false;
    std::vector<String> _monitorReportList;

    // Stats available callback
    SysManager_statsCB _statsCB = nullptr;

    // Stats
    void statsShow();

    // SysManager also handles system restarts
    bool _systemRestartPending = false;
    unsigned long _systemRestartMs = 0;
    static const int SYSTEM_RESTART_DELAY_MS = 1000;

    // System name and version
    String _systemName;
    String _systemVersion;

    // Module config
    ConfigBase _sysModManConfig;

    // Mutable config
    ConfigBase* _pMutableConfig = nullptr;
    String _defaultFriendlyName;
    bool _defaultFriendlyNameIsSet = false;
    String _friendlyNameStored;
    bool _friendlyNameIsSet = false;
    String _ricSerialNoStoredStr;

    // Unique string for this system
    String _systemUniqueString;

    // File/stream activity
    bool _isSystemMainFWUpdate = false;
    bool _isSystemFileTransferring = false;
    bool _isSystemStreaming = false;
    
    // Friendly name max len
    static const uint32_t MAX_FRIENDLY_NAME_LENGTH = 60;

    // Endpoints
    RestAPIEndpointManager* _pRestAPIEndpointManager = nullptr;

    // Comms channels
    CommsChannelManager* _pCommsChannelManager = nullptr;

    // API to reset system
    void apiReset(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Get system version
    void apiGetVersion(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Friendly name get/set
    void apiFriendlyName(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Serial no
    void apiSerialNumber(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // SysMod info and debug
    void apiGetSysModInfo(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);
    void apiGetSysModDebug(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Test function to set loop delay
    void apiTestSetLoopDelay(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Setup SysMan diagnostics
    void apiSysManSettings(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Clear status change callbacks
    void clearAllStatusChangeCBs();

    // Connection change on BLE
    void statusChangeBLEConnCB(const String& sysModName, bool changeToOnline);

    // Mutable config
    String getMutableConfigJson();

};
