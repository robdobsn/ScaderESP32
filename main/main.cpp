/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main entry point
//
// Scader ESP32 V5 Firmware
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Name and Version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SYSTEM_VERSION "5.16.1"

#define MACRO_STRINGIFY(x) #x
#define MACRO_TOSTRING(x) MACRO_STRINGIFY(x)
#define SYSTEM_NAME MACRO_TOSTRING(HW_SYSTEM_NAME)
#define DEFAULT_FRIENDLY_NAME MACRO_TOSTRING(HW_DEFAULT_FRIENDLY_NAME)
#define DEFAULT_HOSTNAME MACRO_TOSTRING(HW_DEFAULT_HOSTNAME)
#define DEFAULT_ADVNAME MACRO_TOSTRING(HW_DEFAULT_ADVNAME)
#define DEFAULT_SERIAL_SET_MAGIC_STR MACRO_TOSTRING(HW_SERIAL_SET_MAGIC_STR)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default system config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE:
// In VSCode C++ raw strings can be removed - to reveal JSON with regex search and replace:
//      regex:    R"\((.*)\)"
//      replace:  $1
// And then reinserted with:
//      regex:    (\s*)(.*)
//      replace:  $1R"($2)"

static const char *defaultConfigJSON =
    R"({)"
        R"("SystemName":")" SYSTEM_NAME R"(",)"
        R"("SystemVersion":")" SYSTEM_VERSION R"(",)"
        R"("IDFVersion":")" IDF_VER R"(",)"
        R"("DefaultName":")" DEFAULT_FRIENDLY_NAME R"(",)"
        R"("SysManager":{)"
            R"("monitorPeriodMs":10000,)"
            R"("reportList":["NetMan","BLEMan","SysMan","StatsCB"],)"
            R"("pauseWiFiforBLE":1,)"
            R"("RICSerial":{)"
                R"("FrameBound":"0xE7",)"
                R"("CtrlEscape":"0xD7")"
            R"(})"
        R"(})"
    R"(})"
;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Parameters
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Main task parameters
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const int MAIN_TASK_PRIORITY = 20;
static const int PRO_TASK_PROCESSOR_CORE = 0;
static const int MAIN_TASK_PROCESSOR_CORE = 1;
static const int MAIN_TASK_STACK_SIZE = 16384;
static TaskHandle_t mainTaskHandle = nullptr;
extern void mainTask(void *pvParameters);

// Debug
static const char* MODULE_NAME = "MainTask";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #define TEST_MINIMAL_FLASH_HEAP_BASE_CASE
// #define DEBUG_SHOW_TASK_INFO
// #define DEBUG_SHOW_RUNTIME_STATS
// #define DEBUG_HEAP_ALLOCATION
// #define DEBUG_TIMING_OF_STARTUP
#define DEBUG_SHOW_NVS_INFO
#define DEBUG_SHOW_NVS_CONTENTS

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#ifdef DEBUG_SHOW_TASK_INFO
#define DEBUG_SHOW_TASK_MS 10000
int tasks_info();
uint32_t lasttaskdumpms = 0;
#endif
#ifdef DEBUG_SHOW_RUNTIME_STATS
#define DEBUG_SHOW_RUNTIME_STATS_MS 10000
int runtime_stats();
uint32_t laststatsdumpms = 0;
#endif
#endif

#ifdef DEBUG_HEAP_ALLOCATION
#include "esp_heap_trace.h"
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Includes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// C, ESP32 and RTOS
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

// App
#include "DetectHardware.h"
#include "CommsChannelManager.h"
#include "SysTypeManager.h"
#include "SysManager.h"
#include "SerialConsole.h"
#include "FileManager.h"
#ifdef CONFIG_BT_ENABLED
#include "BLEManager.h"
#endif
#ifdef FEATURE_NETWORK_FUNCTIONALITY
#include "NetworkManager.h"
#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
#include "WebServer.h"
#endif
#endif
#include "CommandSerial.h"
#include "CommandSocket.h"
#include "CommandFile.h"
#include "StatePublisher.h"
#include "LogManager.h"

#include "FileSystem.h"
#include "ESPOTAUpdate.h"
#include "ProtocolExchange.h"

// #include "BusSerial.h"

#ifdef FEATURE_MQTT_MANAGER
#include "MQTTManager.h"
#endif

// #ifdef FEATURE_HWELEM_STEPPERS
// #include "HWElemSteppers.h"
// #endif

// #ifdef FEATURE_POWER_UP_LED_ASAP
// #include "PowerUpLEDSet.h"
// #endif

// #ifdef FEATURE_EMBED_MICROPYTHON
// #include "MicropythonRICIF.h"
// #endif

// System type consts
#include "SysTypeStatics.h"

#ifdef FEATURE_INCLUDE_SCADER
// Scader components
#include "ScaderRelays.h"
#include "ScaderShades.h"
#include "ScaderDoors.h"
#include "ScaderOpener.h"
// #include "ScaderCat.h"
#include "ScaderLEDPixels.h"
#include "ScaderRFID.h"
// #include "ScaderWaterer.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Standard Entry Point
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef TEST_MINIMAL_FLASH_HEAP_BASE_CASE

// Entry point
extern "C" void app_main(void)
{
    // Initialize flash
    esp_err_t flashInitResult = nvs_flash_init();
    if (flashInitResult != ESP_OK)
    {
        // Error message
        ESP_LOGE(MODULE_NAME, "nvs_flash_init() failed with error %s (%d)", esp_err_to_name(flashInitResult), flashInitResult);

        // Clear flash if possible
        if ((flashInitResult == ESP_ERR_NVS_NO_FREE_PAGES) || (flashInitResult == ESP_ERR_NVS_NEW_VERSION_FOUND))
        {
            esp_err_t flashEraseResult = nvs_flash_erase();
            if (flashEraseResult != ESP_OK)
            {
                ESP_LOGE(MODULE_NAME, "nvs_flash_erase() failed with error %s (%d)", 
                                esp_err_to_name(flashEraseResult), flashEraseResult);
            }
            flashInitResult = nvs_flash_init();
            if (flashInitResult != ESP_OK)
            {
                // Error message
                ESP_LOGW(MODULE_NAME, "nvs_flash_init() failed a second time with error %s (%d)", 
                                esp_err_to_name(flashInitResult), flashInitResult);
            }
        }
    }

    // Start the mainTask
    xTaskCreatePinnedToCore(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, nullptr, MAIN_TASK_PRIORITY, 
                        &mainTaskHandle, MAIN_TASK_PROCESSOR_CORE);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test Entry point for Minimal Flash/Heap
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TEST_MINIMAL_FLASH_HEAP_BASE_CASE
extern "C" void app_main(void)
{
    int i = 0;
    while (1) {
        printf("[%d] Hello world! heap %d\n", i, heap_caps_get_free_size(MALLOC_CAP_8BIT));
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main execution task - started from main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mainTask(void *pvParameters)
{
#ifdef DEBUG_SHOW_NVS_INFO
#ifdef DEBUG_SHOW_NVS_CONTENTS
    RaftJsonNVS::debugShowNVSInfo(true);
#else
    RaftJsonNVS::debugShowNVSInfo(false);
#endif // DEBUG_SHOW_NVS_CONTENTS
#endif // DEBUG_SHOW_NVS_INFO

    // Trace heap allocation - see RdWebConnManager.cpp and RdWebConnection.h/cpp
#ifdef DEBUG_HEAP_ALLOCATION
    heap_trace_init_standalone(trace_record, NUM_RECORDS);
#endif
    // Set hardware revision - ensure this runs early as some methods for determining
    // hardware revision may get disabled later on (e.g. GPIO pins later used for output)
    int hwRevision = DetectHardware::getHWRevision();

    // System configuration
    RaftJsonNVS systemConfig("sys");

    // SysType configuration
    RaftJson sysTypeConfig;

    // Chain the default config to the SysType so that it is used as a fallback
    // if no SysTypes are specified
    RaftJson defaultConfig(defaultConfigJSON, false);
    sysTypeConfig.setChainedRaftJson(&defaultConfig);

    // SysType manager
    SysTypeManager _sysTypeManager(systemConfig, sysTypeConfig);
    _sysTypeManager.setBaseSysTypes(&sysTypeStatics);
    _sysTypeManager.setHardwareRevision(hwRevision);

    // System Module Manager
    SysManager _sysManager("SysManager", systemConfig,
                    DEFAULT_FRIENDLY_NAME, SYSTEM_NAME, 
                    HW_SERIAL_NUMBER_BYTES, DEFAULT_SERIAL_SET_MAGIC_STR,
                    "system");
    _sysManager.setHwRevision(hwRevision);

    // Add the system restart callback to the SysTypeManager
    _sysTypeManager.setSystemRestartCallback([&_sysManager] { (&_sysManager)->systemRestart(); });

// #ifdef FEATURE_INCLUDE_ROBOT_CONTROLLER
//     // Robot Controller
//     RobotController _robotController("RobotCtrl", defaultSystemConfig, &_sysTypeConfig, &_robotMutableConfig,
//                 "Robot", &_addOnMutableConfig);
//     // Register serial bus
//     _robotController.busRegister("serial", BusSerial::createFn);
//     // Register HWElemTypes
//     _robotController.hwElemRegister("I2SOut", HWElemAudioOut::createFn);
//     _robotController.hwElemRegister("GPIO", HWElemGPIO::createFn);
// #ifdef FEATURE_HWELEM_STEPPERS
//     _robotController.hwElemRegister("Steppers", HWElemSteppers::createFn);
// #endif
// #endif

    // API Endpoints
    RestAPIEndpointManager _restAPIEndpointManager;
    _sysManager.setRestAPIEndpoints(_restAPIEndpointManager);

    // Comms Channel Manager
    CommsChannelManager _commsChannelManager("CommsMan", systemConfig);
    _sysManager.setCommsCore(&_commsChannelManager);

    // SerialConsole
    SerialConsole _serialConsole("SerialConsole", systemConfig);

    // FileManager
    FileManager _fileManager("FileManager", systemConfig);

#ifdef FEATURE_NETWORK_FUNCTIONALITY
    // NetworkManager
    NetworkManager _networkManager("NetMan", systemConfig);
#endif

    // ESP OTA Update
    ESPOTAUpdate _espotaUpdate("ESPOTAUpdate", systemConfig);

    // ProtocolExchange
    ProtocolExchange _protocolExchange("ProtExchg", systemConfig);
    _protocolExchange.setHandlers(&_espotaUpdate);
    _fileManager.setProtocolExchange(_protocolExchange);

#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
    // WebServer
    WebServer _webServer("WebServer", systemConfig);
#endif

#ifdef CONFIG_BT_ENABLED
    // BLEManager
    BLEManager _bleManager("BLEMan", systemConfig, DEFAULT_ADVNAME);
#endif

    // Command Serial
    CommandSerial _commandSerial("CommandSerial", systemConfig);

    // Command Socket
    CommandSocket _commandSocket("CommandSocket", systemConfig);

    // Command File
    CommandFile _commandFile("CommandFile", systemConfig);

#ifdef FEATURE_MQTT_MANAGER
    // MQTT
    MQTTManager _mqttManager("MQTTMan", systemConfig);
#endif

    // State Publisher
    StatePublisher _statePublisher("Publish", systemConfig);

    // Log manager
    LogManager _LogManager("LogManager", systemConfig);
    
// #ifdef FEATURE_EMBED_MICROPYTHON
//     // Micropython
//     MicropythonRICIF _micropythonRICIF("uPy", defaultSystemConfig, &_sysTypeConfig, nullptr);
// #endif

#ifdef FEATURE_INCLUDE_SCADER
    // Scader components
    ScaderRelays _scaderRelays("ScaderRelays", systemConfig);
    ScaderShades _scaderShades("ScaderShades", systemConfig);
    ScaderDoors _scaderDoors("ScaderDoors", systemConfig);
    ScaderOpener _scaderOpener("ScaderOpener", systemConfig);
    // // ScaderOpener _scaderOpener("ScaderOpener", cfg, nullptr, &_scaderMutableConfig);
    // // _scaderOpener.setup();
    // // ScaderCat _scaderCat("ScaderCat", defaultSystemConfig, &_sysTypeConfig, &_scaderMutableConfig);
    ScaderLEDPixels _scaderLEDPixels("ScaderLEDPix", systemConfig);
    ScaderRFID _scaderRFID("ScaderRFID", systemConfig);
    // ScaderWaterer _scaderWaterer("ScaderWaterer", defaultSystemConfig, &_sysTypeConfig, &_scaderMutableConfig);
#endif

    // Log out system info
    ESP_LOGI(MODULE_NAME, SYSTEM_NAME " " SYSTEM_VERSION " (built " __DATE__ " " __TIME__ ") Heap %d", 
                        heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // SysTypeManager endpoints
    _sysTypeManager.addRestAPIEndpoints(_restAPIEndpointManager);

    // Initialise the system module manager here so that API endpoints are registered
    // before file system ones
    _sysManager.setup();

#ifdef FEATURE_WEB_SERVER_STATIC_FILES
    // Web server add files
    String servePaths = String("/=/") + fileSystem.getDefaultFSRoot();
    servePaths += ",/files/local=/local";
    servePaths += ",/files/sd=/sd";
    _webServer.serveStaticFiles(servePaths.c_str());
#endif

#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
    // Start webserver
    _webServer.beginServer();
#endif

    // Loop forever
    while (1)
    {
        // Yield for 1 tick
        vTaskDelay(1);

        // Service all the system modules
        _sysManager.service();

        // Test and Monitoring
#ifdef DEBUG_SHOW_TASK_INFO
        if (Raft::isTimeout(millis(), lasttaskdumpms, DEBUG_SHOW_TASK_MS))
        {
            tasks_info();
            lasttaskdumpms = millis();
        }
#endif
#ifdef DEBUG_SHOW_RUNTIME_STATS
        if (Raft::isTimeout(millis(), laststatsdumpms, DEBUG_SHOW_RUNTIME_STATS_MS))
        {
            runtime_stats();
            laststatsdumpms = millis();
        }
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#include "freertos/task.h"

#ifdef DEBUG_SHOW_TASK_INFO
int tasks_info()
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = (char*)malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == nullptr) {
        ESP_LOGE("Main", "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}
#endif

#ifdef DEBUG_SHOW_RUNTIME_STATS
int runtime_stats()
{
    const size_t bytes_per_task = 50; /* see vTaskGetRunTimeStats description */
    char *task_stats_buffer = (char*)malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_stats_buffer == nullptr) {
        ESP_LOGE("Main", "failed to allocate buffer for vTaskGetRunTimeStats output");
        return 1;
    }
    fputs("Task Name\tTime(uS)\t%", stdout);
    fputs("\n", stdout);
    vTaskGetRunTimeStats(task_stats_buffer);
    fputs(task_stats_buffer, stdout);
    free(task_stats_buffer);
    return 0;
}
#endif
#endif
