/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RFID Module Eccel A1 SPI
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RFIDModule_EccelA1SPI.h"
#include "driver/spi_master.h"

static const char *MODULE_PREFIX = "RfidA1";

// #define DEBUG_RFID_MODULE_SEND_RECV_DETAIL
// #define DEBUG_TAG_NOT_PRESENT
// #define DEBUG_RFID_MODULE_VERSION_HEX
// #define DEBUG_NUMBER_OF_TAGS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RFIDModule_EccelA1SPI::RFIDModule_EccelA1SPI(int rfidSPIMOSIPin, int rfidSPIMISOPin,
                                             int rfidSPIClkPin, int rfidSPICS0Pin,
                                             int rfidSPIHostID, int nBusyPin,
                                             int resetPin, bool resetActiveLevel) :
        RFIDModuleBase(resetPin, resetActiveLevel)
{
    // SPI pins
    _rfidSPICS0Pin = rfidSPICS0Pin;
    _moduleNBusyPin = nBusyPin;
    if (_moduleNBusyPin >= 0)
    {
        pinMode(_moduleNBusyPin, INPUT);
    }

    // Chip select pin setup
    if (_rfidSPICS0Pin >= 0)
    {
        digitalWrite(_rfidSPICS0Pin, HIGH);
        pinMode(_rfidSPICS0Pin, OUTPUT);
        digitalWrite(_rfidSPICS0Pin, HIGH);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "constructor SPIHost %d MOSIPin %d MISOPin %d CLKPin %d CSPin %d nBusyPin %d",
          rfidSPIHostID, rfidSPIMOSIPin, rfidSPIMISOPin, rfidSPIClkPin,
          _rfidSPICS0Pin, _moduleNBusyPin);

    // Don't continue if not configured
    if (_rfidSPICS0Pin < 0)
        return;

    // Init SPI bus
    spi_bus_config_t bus_config;
    memset(&bus_config, 0, sizeof(bus_config));
    bus_config.mosi_io_num = rfidSPIMOSIPin;
    bus_config.miso_io_num = rfidSPIMISOPin;
    bus_config.sclk_io_num = rfidSPIClkPin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 0;
    esp_err_t err = spi_bus_initialize((spi_host_device_t)rfidSPIHostID, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "constructor failed to init SPI bus hostID %d", rfidSPIHostID);
        return;
    }

    // Init SPI device for ECCEL A1 RFID module
    // Note that this can work at a max of 500KHz
    spi_device_interface_config_t dev_config;
    memset(&dev_config, 0, sizeof(dev_config));
    dev_config.clock_speed_hz = 100000;
    dev_config.mode = 0;
    dev_config.spics_io_num = _rfidSPICS0Pin;
    dev_config.queue_size = 1;
    dev_config.pre_cb = NULL;
    dev_config.cs_ena_pretrans = 16;
    dev_config.cs_ena_posttrans = 2;
    err = spi_bus_add_device((spi_host_device_t)rfidSPIHostID, &dev_config, &_spiHandle);
    if (err != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "constructor failed to add SPI device hostID %d", rfidSPIHostID);
        return;
    }

    // Reset module
    requestReset();

    // Check module
    _moduleIsPresent = checkModulePresent();
    LOG_I(MODULE_PREFIX, "constructor moduleIsPresent %s", _moduleIsPresent ? "Y" : "N");

    // Start polling
    pollingStart();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Request reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::requestReset()
{
    // Just reset to create most havoc for testing
    rfidModuleReset();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start polling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::pollingStart()
{
    // Start polling for tags
    if (isModulePresent())
    {
        _isPolling = true;
        pollingSetState(POLLING_STATE_IDLE);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop polling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::pollingStop()
{
    // Stop polling for tags
    _isPolling = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset defaults
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RFIDModule_EccelA1SPI::resetDefaultsCmd()
{
    static const uint8_t resetDefaultsCmd[] = {RFID_CMD_RESET_DEFAULTS};
    if (!rfidExecCommand(resetDefaultsCmd, sizeof(resetDefaultsCmd) / sizeof(uint8_t)))
    {
        LOG_E(MODULE_PREFIX, "resetDefaultsCmd failed to exec - RFID_CHIP_BUSY_BAR timeout");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if the ECCEL A1 RFID module is present
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RFIDModule_EccelA1SPI::checkModulePresent()
{
    // Check if module is present - get module version string
    const uint8_t getVersionCmd[] = {RFID_CMD_GET_MODULE_VERSION};
    if (!rfidExecCommand(getVersionCmd, sizeof(getVersionCmd) / sizeof(uint8_t)))
    {
        LOG_W(MODULE_PREFIX, "checkModulePresent NOT present (busy on version)");
        return false;
    }

    // Read the result
    _rxBuffer[RFID_RESULT_POS] = 0x55;
    if (!rfidReadMemory(0, 1, false))
    {
        LOG_W(MODULE_PREFIX, "checkModulePresent FAILED to read mem version");
        return false;
    }
    uint8_t result = _rxBuffer[RFID_RESULT_POS];
    if (result != RFID_RESULT_SUCCESS)
    {
        LOG_W(MODULE_PREFIX, "checkModulePresent NOT present - result %s (0x%02x)", 
                getRfidResultStr(result), result);
        return false;
    }

    // Read the version string from memory
    const int bytesToRead = RFID_VERSION_STR_MAX + RFID_HEADER_LEN;
    if (!rfidReadMemory(RFID_DATA_BUFFER_ADDR, bytesToRead))
    {
        return false;
    }
    if (!waitWhileBusy())
        return false;

    // Debug
#ifdef DEBUG_RFID_MODULE_VERSION_HEX
    String outStr;
    Raft::getHexStrFromBytes(_rxBuffer, bytesToRead, outStr);
    LOG_I(MODULE_PREFIX, "checkModulePresent RFID version hex %s", outStr.c_str());
#endif

    // Debug
    _rxBuffer[bytesToRead] = 0;
    LOG_I(MODULE_PREFIX, "checkModulePresent RFID module version string %s", _rxBuffer+RFID_HEADER_LEN);

    // Reset defaults
    resetDefaultsCmd();

    // Result
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::service()
{
    // Service base class
    RFIDModuleBase::service();

    // Handle polling state machine
    pollingService();

    // Check errors
    if (_errorCount > MAX_ERROR_COUNT_BEFORE_RESET)
    {
        rfidModuleReset();
        pollingSetState(POLLING_STATE_IDLE);
        _errorCount = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exec command over SPI
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RFIDModule_EccelA1SPI::rfidExecCommand(const uint8_t cmdBuf[], int cmdLen)
{
    // Wait for not busy
    if (!waitWhileBusy())
    {
        LOG_W(MODULE_PREFIX, "rfidExecCommand failed to exec - RFID_CHIP_BUSY_BAR timeout cmd 0x%x", cmdBuf[0]);
        return false;
    }

    // Send a command
    delayMicroseconds(DELAY_BETWEEN_SPI_ACTIONS_US);
    
    // Command buffer starts at 0x0001 and the address is sent LSByte first
    uint32_t txLen = cmdLen + RFID_HEADER_LEN < MAX_DATA_LEN ? cmdLen + RFID_HEADER_LEN : MAX_DATA_LEN;
    memset(_txBuffer, 0, MAX_DATA_LEN);
    _txBuffer[0] = RFID_MODULE_COMMAND_ADDR & 0xff;
    _txBuffer[1] = RFID_MODULE_COMMAND_ADDR >> 8;
    _txBuffer[2] = RFID_DATA_EXCHANGE_READ_WRITE;
    memcpy(_txBuffer + RFID_HEADER_LEN, cmdBuf, cmdLen);

    // SPI
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = txLen * 8;
    t.tx_buffer = _txBuffer;
    t.rx_buffer = _rxBuffer;
    esp_err_t err = spi_device_polling_transmit(_spiHandle, &t);
    if (err != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "rfidExecCommand failed to exec - SPI error %d", err);
        return false;
    }

    // Debug
#ifdef DEBUG_RFID_MODULE_SEND_RECV_DETAIL
    String sendStr, recvStr;
    Raft::getHexStrFromBytes(_txBuffer, txLen, sendStr);
    Raft::getHexStrFromBytes(_rxBuffer, t.rxlength/8, recvStr);
    LOG_I(MODULE_PREFIX, "rfidExecCommand sent %s received %s", sendStr.c_str(), recvStr.c_str());
#endif

    // Result
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read memory over SPI
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RFIDModule_EccelA1SPI::rfidReadMemory(int startAddr, int readLen, bool clearRxBufferFirst)
{
    // Wait for not busy
    if (!waitWhileBusy())
    {
        LOG_W(MODULE_PREFIX, "rfidReadMemory failed to exec - RFID_CHIP_BUSY_BAR timeout read %d @ %d", readLen, startAddr);
        return false;
    }
    // Read
#ifdef DEBUG_RFID_MODULE_SEND_RECV_DETAIL
    LOG_I(MODULE_PREFIX, "rfidReadMemory %d bytes @ 0x%x", readLen, startAddr)
#endif
    delayMicroseconds(DELAY_BETWEEN_SPI_ACTIONS_US);

    // Tx buffer
    memset(_txBuffer, 0, MAX_DATA_LEN);
    _txBuffer[0] = startAddr & 0xff;
    _txBuffer[1] = startAddr >> 8;
    _txBuffer[2] = RFID_DATA_EXCHANGE_READ;

    // Rx buffer
    if (clearRxBufferFirst)
        memset(_rxBuffer, 0, MAX_DATA_LEN);

    // SPI
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (readLen + RFID_HEADER_LEN);
    t.tx_buffer = _txBuffer;
    t.rxlength = 8 * (readLen + RFID_HEADER_LEN);
    t.rx_buffer = _rxBuffer;
    t.flags = SPI_DEVICE_HALFDUPLEX;
    esp_err_t err = spi_device_polling_transmit(_spiHandle, &t);
    if (err != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "rfidReadMemory failed to exec - SPI error %d", err);
        return false;
    }

    // Debug
#ifdef DEBUG_RFID_MODULE_SEND_RECV_DETAIL
    String sendStr, recvStr;
    Raft::getHexStrFromBytes(_txBuffer, RFID_HEADER_LEN, sendStr);
    Raft::getHexStrFromBytes(_rxBuffer, t.rxlength/8, recvStr);
    LOG_I(MODULE_PREFIX, "rfidReadMemory sent %s received %s", sendStr.c_str(), recvStr.c_str());
#endif

    // Result
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait while busy (with timeout)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RFIDModule_EccelA1SPI::waitWhileBusy()
{
    uint32_t waitStartMs = millis();
    if (_moduleNBusyPin != -1)
    {
        while (digitalRead(_moduleNBusyPin) == LOW)
        {
            if (Raft::isTimeout(millis(), waitStartMs, N_BUSY_MAX_WAIT_MS))
                return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set polling state
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::pollingSetState(int newState)
{
    _pollingState = newState;
    _pollingStateTimeMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service polling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RFIDModule_EccelA1SPI::pollingService()
{
    // Check CS pin valid
    if (_rfidSPICS0Pin == -1)
        return;

    // Handle state
    switch (_pollingState)
    {
    case POLLING_STATE_IDLE:
    {
        if (_isPolling)
        {
            // Check if time for a poll
            if (Raft::isTimeout(millis(), _pollingStateTimeMs, TIME_BETWEEN_POLLS_MS))
            {
                // Go into request state
                pollingSetState(POLLING_STATE_REQ);
            }
        }
        break;
    }
    case POLLING_STATE_REQ:
    {
        // Poll for RFID tags
        const uint8_t enumerateTagsCmd[] = {RFID_CMD_ENUMERATE_TAGS};
        if (!rfidExecCommand(enumerateTagsCmd, sizeof(enumerateTagsCmd) / sizeof(uint8_t)))
        {
            LOG_W(MODULE_PREFIX, "pollingService failed to exec enumerate tags");
            pollingSetState(POLLING_STATE_IDLE);
            _errorCount++;
            break;
        }

        // Check result code
        _rxBuffer[RFID_RESULT_POS] = 0x55;
        if (!rfidReadMemory(0, 1, false))
        {
            LOG_W(MODULE_PREFIX, "pollingService failed to read enumerate result");
            pollingSetState(POLLING_STATE_IDLE);
            _errorCount++;
            break;
        }

        // Check busy - shouldn't be as we already checked for busy
        uint8_t result = _rxBuffer[RFID_RESULT_POS];
        if (result == RFID_RESULT_MODULE_BUSY)
        {
            LOG_W(MODULE_PREFIX, "pollingService FAILED SHOULD NOT BE BUSY 0x%x", result);
            pollingSetState(POLLING_STATE_IDLE);
            _errorCount++;
            break;
        }
        else if (result == RFID_RESULT_TAG_NOT_PRESENT)
        {
#ifdef DEBUG_TAG_NOT_PRESENT
            LOG_I(MODULE_PREFIX, "pollingService NO TAGS PRESENT");
#endif
            tagNotPresent();
            pollingSetState(POLLING_STATE_IDLE);
            break;
        }
        else if (result != RFID_RESULT_SUCCESS)
        {
            LOG_W(MODULE_PREFIX, "pollingService FAILED result %s (0x%x)", 
                    getRfidResultStr(result), result);
            tagNotPresent();
            pollingSetState(POLLING_STATE_IDLE);
            _errorCount++;
            break;
        }

        // Get enumerated tag info
        const uint32_t bytesToRead = RFID_TAG_READ_MAX_LEN;
        if (!rfidReadMemory(RFID_DATA_BUFFER_ADDR, bytesToRead))
        {
            LOG_W(MODULE_PREFIX, "pollingService failed to read enumerate info");
            pollingSetState(POLLING_STATE_IDLE);
            break;
        }

        // Get number of tags
        uint16_t tagUIDLen = _rxBuffer[RFID_RESULT_POS + 1];
#ifdef DEBUG_NUMBER_OF_TAGS
        uint8_t numTags = _rxBuffer[RFID_RESULT_POS];
        LOG_I(MODULE_PREFIX, "pollingService %d tags present 1st tag len %d bytes", 
                        numTags, numTags > 0 ? tagUIDLen : 0);
#endif

        // Get UID length of first tag
        if (tagUIDLen > bytesToRead - 2 - RFID_RESULT_POS)
        {
            LOG_W(MODULE_PREFIX, "pollingService tag UID too long %d", tagUIDLen);
            pollingSetState(POLLING_STATE_IDLE);
            break;
        }

        // Read the UID
        String tagStr;
        Raft::getHexStrFromBytes(_rxBuffer + RFID_RESULT_POS + 2, tagUIDLen, tagStr);
        tagNowPresent(tagStr);
        LOG_I(MODULE_PREFIX, "pollingService tag UID %s", _curTagRead.c_str());
        pollingSetState(POLLING_STATE_IDLE);
    }
    }
}
