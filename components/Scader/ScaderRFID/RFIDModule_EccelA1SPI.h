/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RFID Module Eccel A1 SPI
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once 

#include "RaftArduino.h"
#include "RFIDModuleBase.h"
#include "RaftUtils.h"
#include "driver/spi_master.h"

class RFIDModule_EccelA1SPI : public RFIDModuleBase
{
public:
    RFIDModule_EccelA1SPI(int rfidSPIMOSIPin, int rfidSPIMISOPin, 
                int rfidSPIClkPin, int rfidSPICS0Pin,
                int rfidSPIHostID, int nBusyPin,
                int resetPin, bool resetActiveLevel);
    virtual void requestReset() override final;
    virtual bool isModulePresent() override final
    {
        return _moduleIsPresent;
    }
    virtual void loop() override final;

private:
    // Device handle
    spi_device_handle_t _spiHandle = nullptr;

    // nBUSY - active low
    int _moduleNBusyPin = -1;

    // SPI CS pin
    int _rfidSPICS0Pin = -1;

    // Flag indicating module is present
    bool _moduleIsPresent = false;

    // Error count
    int _errorCount = 0;

    // RFID header len
    static const uint32_t RFID_HEADER_LEN = 3;

    // RFID result position
    static const uint32_t RFID_RESULT_POS = 3;

    // Version string max
    static const uint32_t RFID_VERSION_STR_MAX = 100;

    // Tag read max length
    static const uint32_t RFID_TAG_READ_MAX_LEN = 50;

    // RFID module addresses
    static const uint32_t RFID_MODULE_RESULT_ADDR = 0x00;
    static const uint32_t RFID_MODULE_COMMAND_ADDR = 0x01;
    static const uint32_t RFID_MODULE_CMD_PARAMS_ADDR = 0x02;
    static const uint32_t RFID_MODULE_CMD_PARAMS_LEN = 18;
    static const uint32_t RFID_TAG_UID_ADDR = 0x14;
    static const uint32_t RFID_TAG_TYPE_ADDR = 0x1E;
    static const uint32_t RFID_TAG_UID_SIZE = 0x1f;
    static const uint32_t RFID_DATA_BUFFER_ADDR = 0x20;

    // RFID data exchange
    static const uint8_t RFID_DATA_EXCHANGE_READ = 0x00;
    static const uint8_t RFID_DATA_EXCHANGE_READ_WRITE = 0x01;

    // RFID commands
    static const uint8_t RFID_CMD_GET_MODULE_VERSION = 0x1d;
    static const uint8_t RFID_CMD_RESET_DEFAULTS = 0x1e;
    static const uint8_t RFID_CMD_ENUMERATE_TAGS = 0x1f;

    // RFID result register values
    static const uint8_t RFID_RESULT_SUCCESS = 0x00;
    static const uint8_t RFID_RESULT_INVALID_CMD = 0x01;
    static const uint8_t RFID_RESULT_INVALID_PARAM = 0x02;
    static const uint8_t RFID_RESULT_INDEXES_OUT_OF_RANGE = 0x03;
    static const uint8_t RFID_RESULT_ERROR_WRITING_TO_NV = 0x04;
    static const uint8_t RFID_RESULT_SYSTEM_ERROR = 0x05;
    static const uint8_t RFID_RESULT_TAG_CRC_ERROR = 0x06;
    static const uint8_t RFID_RESULT_TAG_COLLISION = 0x07;
    static const uint8_t RFID_RESULT_TAG_NOT_PRESENT = 0x08;
    static const uint8_t RFID_RESULT_TAG_NOT_AUTHENTICATED = 0x09;
    static const uint8_t RFID_RESULT_TAG_VALUE_CORRUPTED = 0x0a;
    static const uint8_t RFID_RESULT_MODULE_OVERHEAT = 0x0b;
    static const uint8_t RFID_RESULT_TAG_NOT_SUPPORTED = 0x0c;
    static const uint8_t RFID_RESULT_TAG_COMMS_ERROR = 0x0d;
    static const uint8_t RFID_RESULT_INVALID_PASSWORD = 0x0e;
    static const uint8_t RFID_RESULT_ALREADY_LOCKED = 0x0f;
    static const uint8_t RFID_RESULT_MODULE_BUSY = 0xff;

    // RFID result as string
    static const char* getRfidResultStr(uint8_t result)
    {
        switch (result)
        {
        case RFID_RESULT_SUCCESS:
            return "Success";
        case RFID_RESULT_INVALID_CMD:
            return "Invalid command";
        case RFID_RESULT_INVALID_PARAM:
            return "Invalid parameter";
        case RFID_RESULT_INDEXES_OUT_OF_RANGE:
            return "Indexes out of range";
        case RFID_RESULT_ERROR_WRITING_TO_NV:
            return "Error writing to NV";
        case RFID_RESULT_SYSTEM_ERROR:
            return "System error";
        case RFID_RESULT_TAG_CRC_ERROR:
            return "Tag CRC error";
        case RFID_RESULT_TAG_COLLISION:
            return "Tag collision";
        case RFID_RESULT_TAG_NOT_PRESENT:
            return "Tag not present";
        case RFID_RESULT_TAG_NOT_AUTHENTICATED:
            return "Tag not authenticated";
        case RFID_RESULT_TAG_VALUE_CORRUPTED:
            return "Tag value corrupted";
        case RFID_RESULT_MODULE_OVERHEAT:
            return "Module overheated";
        case RFID_RESULT_TAG_NOT_SUPPORTED:
            return "Tag not supported";
        case RFID_RESULT_TAG_COMMS_ERROR:
            return "Tag comms error";
        case RFID_RESULT_INVALID_PASSWORD:
            return "Invalid password";
        case RFID_RESULT_ALREADY_LOCKED:
            return "Already locked";
        case RFID_RESULT_MODULE_BUSY:
            return "Module busy";
        default:
            return "Unknown";
        }
    }

    // Receive buffer
    static const int MAX_DATA_LEN = 128;
    uint8_t _rxBuffer[MAX_DATA_LEN];
    uint8_t _txBuffer[MAX_DATA_LEN];

    // Polling
    bool _isPolling = false;
    int _pollingState = POLLING_STATE_IDLE;
    uint32_t _pollingStateTimeMs = 0;
    enum
    {
        POLLING_STATE_IDLE = 0,
        POLLING_STATE_REQ
    };

private:
    // Timing
    static const uint32_t N_BUSY_MAX_WAIT_MS = 100;
    static const uint32_t CS_TO_SPI_TIME_US = 1000;
    static const uint32_t TIME_BETWEEN_POLLS_MS = 500;
    static const uint32_t DELAY_BETWEEN_SPI_ACTIONS_US = 150;
    static const uint32_t MAX_ERROR_COUNT_BEFORE_RESET = 5;

    // Exec command over SPI
    bool rfidExecCommand(const uint8_t cmdBuf[], int cmdLen);
    bool rfidReadMemory(int startAddr, int readLen, bool clearRxBufferFirst = true);
    
    // Wait while busy
    bool waitWhileBusy();

    // Polling
    void pollingStart();
    void pollingStop();
    void pollingSetState(int newState);
    void pollingloop();

    // Helpers
    bool resetDefaultsCmd();
    bool checkModulePresent();
};
