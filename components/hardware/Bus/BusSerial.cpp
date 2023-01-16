/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Serial Bus Handler
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "BusSerial.h"
#include <BusRequestInfo.h>
#include <ConfigBase.h>
#include <ConfigPinMap.h>
#include <HWElemConsts.h>
#include <driver/uart.h>
#include <RaftUtils.h>
#include <ArduinoOrAlt.h>
#include <esp_err.h>

static const char* MODULE_PREFIX = "BusSerial";

// #define DEBUG_BUS_SERIAL

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construct / Destruct
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusSerial::BusSerial(BusElemStatusCB busElemStatusCB, BusOperationStatusCB busOperationStatusCB)
    : BusBase(busElemStatusCB, busOperationStatusCB)
{
    // Clear vars
    _uartNum = 0;
    _rxPin = -1;
    _txPin = -1;
    _baudRate = BAUD_RATE_DEFAULT;
    _isInitialised = false;
    _rxBufSize = RX_BUF_SIZE_DEFAULT;
    _txBufSize = TX_BUF_SIZE_DEFAULT;
    _minTimeBetweenSendsMs = 0;
    _lastSendTimeMs = 0;
}

BusSerial::~BusSerial()
{
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusSerial::setup(ConfigBase& config)
{
    // Check if already configured
    if (_isInitialised)
        return false;

    // Get bus details
    _uartNum = config.getLong("uartNum", 0);
    String pinName = config.getString("rxPin", "");
    _rxPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = config.getString("txPin", "");
    _txPin = ConfigPinMap::getPinFromName(pinName.c_str());
    _baudRate = config.getLong("baudRate", BAUD_RATE_DEFAULT);
    _busName = config.getString("name", "");
    _rxBufSize = config.getLong("rxBufSize", RX_BUF_SIZE_DEFAULT);
    _txBufSize = config.getLong("txBufSize", TX_BUF_SIZE_DEFAULT);
    _minTimeBetweenSendsMs = config.getLong("minAfterSendMs", 0);

    // Check valid
    if ((_rxPin < 0) || (_txPin < 0))
    {
        LOG_W(MODULE_PREFIX, "setup INVALID PARAMS name %s uart %d Rx %d Tx %d baud %d", _busName.c_str(), _uartNum, _rxPin, _txPin, _baudRate);
        return false;
    }

    // Initialise serial
    if (!serialInit())
    {
        LOG_W(MODULE_PREFIX, "setup bus FAILED name %s uart %d Rx %d Tx %d baud %d", _busName.c_str(), _uartNum, _rxPin, _txPin, _baudRate);
        return false;
    }

    // Ok
    _isInitialised = true;

    // Debug
    LOG_I(MODULE_PREFIX, "setup bus OK name %s uart %d Rx %d Tx %d baud %d", _busName.c_str(), _uartNum, _rxPin, _txPin, _baudRate);

    // Ok
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSerial::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BusSerial::clear(bool incPolling)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusSerial::serialInit()
{
    // Configure UART. Note that REF_TICK is used so that the baud rate remains
    // correct while APB frequency is changing in light sleep mode
    const uart_config_t uart_config = {
            .baud_rate = _baudRate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 10,
            .use_ref_tick = false,
    };
    esp_err_t err = uart_param_config((uart_port_t)_uartNum, &uart_config);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "Failed to initialize uart, err %d", err);
        return false;
    }

    // Setup pins
    err = uart_set_pin((uart_port_t)_uartNum, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "Failed to set uart pins, err %d", err);
        return false;
    }

    // Delay before UART change
    // TODO - what does this achieve?
    vTaskDelay(1);

    // Install UART driver for interrupt-driven reads and writes
    err = uart_driver_install((uart_port_t)_uartNum, _rxBufSize, _txBufSize, 0, NULL, 0);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "Failed to install uart driver, err %d", err);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isReady (for new requests)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusSerial::isReady()
{
    if (_minTimeBetweenSendsMs == 0)
        return true;
    return Raft::isTimeout(millis(), _lastSendTimeMs, _minTimeBetweenSendsMs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusSerial::addRequest(BusRequestInfo& busReqInfo)
{
    // Check initialised
    if (!_isInitialised)
        return false;

    // Check for type of request
    if (busReqInfo.getBusReqType() != BUS_REQ_TYPE_STD)
        return false;

    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, 
                (const char*)busReqInfo.getWriteData(), busReqInfo.getWriteDataLen());
    if (bytesSent != busReqInfo.getWriteDataLen())
    {
        LOG_W(MODULE_PREFIX, "addRequest len %d only wrote %d bytes",
                busReqInfo.getWriteDataLen(), bytesSent);

        return false;
    }

    // Record time message sent
    _lastSendTimeMs = millis();
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear receive buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSerial::rxDataClear()
{
    // Clear received data
    uart_flush_input((uart_port_t)_uartNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received data bytes available
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t BusSerial::rxDataBytesAvailable()
{
    size_t numAvailable = 0;
    if (uart_get_buffered_data_len((uart_port_t)_uartNum, &numAvailable) == ESP_OK)
        return numAvailable;
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get rx data - returns number of bytes placed in pData buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t BusSerial::rxDataGet(uint8_t* pData, uint32_t maxLen)
{
    uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, pData, maxLen, 0);
    return bytesRead;
}
