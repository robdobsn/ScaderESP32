/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RaftUtils.h>
#include <CommandSerial.h>
#include <CommsCoreIF.h>
#include <CommsChannelMsg.h>
#include <CommsChannelSettings.h>
#include <RestAPIEndpointManager.h>
#include <SpiramAwareAllocator.h>
#include <driver/uart.h>

static const char *MODULE_PREFIX = "CommandSerial";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandSerial::CommandSerial(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Config variables
    _isEnabled = false;
    _uartNum = 0;
    _baudRate = 912600;
    _txPin = 0;
    _rxPin = 0;
    _rxBufSize = 1024;
    _txBufSize = 1024;
    _isInitialised = false;

    // ChannelID
    _commsChannelID = CommsCoreIF::CHANNEL_ID_UNDEFINED;
}

CommandSerial::~CommandSerial()
{
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::setup()
{
    // Apply config
    applySetup();
}

void CommandSerial::applySetup()
{
    // Clear previous config if we've been here before
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
    _isInitialised = false;

    // Enable
    _isEnabled = configGetBool("enable", false);

    // Port
    _uartNum = configGetLong("uartNum", 80);

    // Baud
    _baudRate = configGetLong("baudRate", 912600);

    // Protocol
    _protocol = configGetString("protocol", "");

    // Pins
    _rxPin = configGetLong("rxPin", -1);
    _txPin = configGetLong("txPin", -1);

    // Buffers
    _rxBufSize = configGetLong("rxBufSize", 1024);
    _txBufSize = configGetLong("txBufSize", 1024);

    LOG_I(MODULE_PREFIX, "setup enabled %s uartNum %d baudRate %d txPin %d rxPin %d rxBufSize %d txBufSize %d protocol %s", 
                    _isEnabled ? "YES" : "NO", _uartNum, _baudRate, _txPin, _rxPin, _rxBufSize, _txBufSize, _protocol.c_str());

    if (_isEnabled && (_rxPin != -1) && (_txPin != -1))
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
            return;
        }

        // Setup pins
        err = uart_set_pin((uart_port_t)_uartNum, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to set uart pins, err %d", err);
            return;
        }

        // Delay before UART change
        // TODO - what does this achieve?
        vTaskDelay(1);

        // Install UART driver for interrupt-driven reads and writes
        err = uart_driver_install((uart_port_t)_uartNum, _rxBufSize, _txBufSize, 0, NULL, 0);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to install uart driver, err %d", err);
            return;
        }

        _isInitialised = true;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::service()
{
    if (!_isInitialised || !getCommsCore())
        return;

    // Check anything available
    static const int MAX_BYTES_PER_CALL = 500;
    size_t numCharsAvailable = 0;
    esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
    if ((err == ESP_OK) && (numCharsAvailable > 0))
    {
        // Get data
        uint32_t bytesToGet = numCharsAvailable < MAX_BYTES_PER_CALL ? numCharsAvailable : MAX_BYTES_PER_CALL;
        std::vector<uint8_t, SpiramAwareAllocator<uint8_t>> charBuf;
        charBuf.resize(bytesToGet);        
        uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, charBuf.data(), bytesToGet, 1);
        if (bytesRead != 0)
        {
            // LOG_D(MODULE_PREFIX, "service charsAvail %d ch %02x", numCharsAvailable, buf[0]);
            // Send to comms channel
            getCommsCore()->handleInboundMessage(_commsChannelID, charBuf.data(), bytesRead);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms channels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::addCommsChannels(CommsCoreIF& commsCoreIF)
{
    // Comms channel
    static const CommsChannelSettings commsChannelSettings;

    // Register as a message channel
    _commsChannelID = commsCoreIF.registerChannel(_protocol.c_str(),
            modName(),
            modName(),
            std::bind(&CommandSerial::sendMsg, this, std::placeholders::_1),
            [this](uint32_t channelID, bool& noConn) {
                return true;
            },
            &commsChannelSettings);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSerial::sendMsg(CommsChannelMsg& msg)
{
    // Debug
    // LOG_I(MODULE_PREFIX, "sendMsg channelID %d, msgType %s msgNum %d, len %d",
    //         msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen());

    if (!_isInitialised)
        return false;

    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, (const char*)msg.getBuf(), msg.getBufLen());
    if (bytesSent != msg.getBufLen())
    {
        LOG_W(MODULE_PREFIX, "sendMsg channelID %d, msgType %s msgNum %d, len %d only wrote %d bytes",
                msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen(), bytesSent);

        return false;
    }
    return true;
}

