/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UI Module
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RaftUtils.h>
#include <UIModule.h>
#include <driver/uart.h>
#include <SpiramAwareAllocator.h>
#include <JSONParams.h>

static const char *MODULE_PREFIX = "UIModule";

// #define DEBUG_UI_MODULE_RX
// #define DEBUG_UI_MODULE_RX_ASCII
// #define DEBUG_UI_MODULE_RX_LOOP
// #define DEBUG_UI_MODULE_RX_LINE
// #define DEBUG_SERVICE_STATUS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UIModule::UIModule()
{
}

UIModule::~UIModule()
{
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UIModule::setup(ConfigBase &config, OpenerStatus* pOpenerParams)
{
    // Clear previous if we've been here before
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
    _isInitialised = false;

    // Opener params object
    _pOpenerStatus = pOpenerParams;

    // Port
    _uartNum = config.getLong("uiModule/uartNum", 1);

    // Baud
    _baudRate = config.getLong("uiModule/baudRate", 912600);

    // Pins
    _rxPin = config.getLong("uiModule/rxPin", -1);
    _txPin = config.getLong("uiModule/txPin", -1);

    // Buffers
    _rxBufSize = config.getLong("uiModule/rxBufSize", 1024);
    _txBufSize = config.getLong("uiModule/txBufSize", 1024);

    // Debug
    LOG_I(MODULE_PREFIX, "setup uartNum %d baudRate %d txPin %d rxPin %d rxBufSize %d txBufSize %d", 
                    _uartNum, _baudRate, _txPin, _rxPin, _rxBufSize, _txBufSize);

    // Initialise
    if ((_rxPin != -1) && (_txPin != -1))
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

void UIModule::service()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Check anything available
    static const int MAX_BYTES_PER_CALL = 500;
    size_t numCharsAvailable = 0;
    esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
    if ((err == ESP_OK) && (numCharsAvailable > 0))
    {
#ifdef DEBUG_UI_MODULE_RX_LOOP
        LOG_I(MODULE_PREFIX, "service charsAvail %d lineLen %d", numCharsAvailable, _rxLine.length());
#endif
        // Get data
        uint32_t bytesToGet = numCharsAvailable < MAX_BYTES_PER_CALL ? numCharsAvailable : MAX_BYTES_PER_CALL;
        std::vector<uint8_t, SpiramAwareAllocator<uint8_t>> charBuf;
        charBuf.resize(bytesToGet);        
        uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, charBuf.data(), bytesToGet, 1);
        if (bytesRead != 0)
        {
            // LOG_I(MODULE_PREFIX, "service charsAvail %d ch %02x", 
            //             bytesRead, charBuf.size() > 0 ? charBuf.data()[0] : 0);

#ifdef DEBUG_UI_MODULE_RX
            String outHexStr;
            Raft::getHexStrFromBytes(charBuf.data(), bytesRead, outHexStr);
            LOG_I(MODULE_PREFIX, "service charsAvail %d data %s", bytesRead, outHexStr.c_str());
#endif
#ifdef DEBUG_UI_MODULE_RX_ASCII
            String outStr;
            Raft::strFromBuffer(charBuf.data(), bytesRead, outStr);
            LOG_I(MODULE_PREFIX, "service charsAvail %d data %s", bytesRead, outStr.c_str());
#endif

            // Build line of text
            for (int i = 0; i < bytesRead; i++)
            {
                uint8_t ch = charBuf.data()[i];
                if ((ch == '\n') || (ch == '\r'))
                {
                    // End of line
                    if (_rxLine.length() > 0)
                    {
                        // Process line
#ifdef DEBUG_UI_MODULE_RX_LINE
                        LOG_I(MODULE_PREFIX, "service line %s", _rxLine.c_str());
#endif
                        processStatus(_rxLine);
                        _rxLine = "";
                    }
                }
                else
                {
                    // Add to line
                    if (_rxLine.length() < MAX_RX_LINE_LEN)
                        _rxLine += (char)ch;
                }
            }
        }
    }

    // Check status update to UI required
    if (_pOpenerStatus && 
            (_pOpenerStatus->_isDirty || 
                Raft::isTimeout(millis(), _statusUpdateLastMs, STATUS_UPDATE_INTERVAL_MS))
        )
    {
        // Get status
        String statusJson = _pOpenerStatus->getJson() + "\r\n";

        // Send to UI
        uart_write_bytes((uart_port_t)_uartNum, statusJson.c_str(), statusJson.length());

        // Debug
#ifdef DEBUG_SERVICE_STATUS
        LOG_I(MODULE_PREFIX, "service status %s", statusJson.c_str());
#endif

        // Now done
        _statusUpdateLastMs = millis();
        _pOpenerStatus->_isDirty = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UIModule::processStatus(const String &status)
{
    // Use JSONParams
    JSONParams params(status);

    // Get the status
    String cmdStr = params.getString("cmd", "");

    // Extract info
    int outEnabled = cmdStr.equalsIgnoreCase("outDisable") ? 0 : 
                cmdStr.equalsIgnoreCase("outEnable") ? 1 : -1;
    int inEnabled = cmdStr.equalsIgnoreCase("inDisable") ? 0 :
                cmdStr.equalsIgnoreCase("inEnable") ? 1 : -1;

    // Update UI
    if (_pOpenerStatus)
    {
        if (outEnabled != -1)
        {
            _pOpenerStatus->setOutEnabled(outEnabled ? true : false);
            LOG_I(MODULE_PREFIX, "processStatus outEnabled %s", outEnabled ? "Y" : "N");
        }
        if (inEnabled != -1)
        {
            _pOpenerStatus->setInEnabled(inEnabled ? true : false);
            LOG_I(MODULE_PREFIX, "processStatus inEnabled %s", inEnabled ? "Y" : "N");
        }
    }
}

