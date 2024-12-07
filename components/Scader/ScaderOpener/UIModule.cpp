/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UI Module
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"
#include "RaftUtils.h"
#include "UIModule.h"
#include "driver/uart.h"
#include "SpiramAwareAllocator.h"
#include "RaftJson.h"

static const char *MODULE_PREFIX = "UIModule";

// #define DEBUG_UI_MODULE_RX
// #define DEBUG_UI_MODULE_RX_ASCII
// #define DEBUG_UI_MODULE_RX_LOOP
// #define DEBUG_UI_MODULE_RX_LINE
// #define DEBUG_SERVICE_STATUS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UIModule::UIModule() :
    _miniHDLC(NULL, 
            std::bind(&UIModule::frameRxCB, this, std::placeholders::_1, std::placeholders::_2))
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

void UIModule::setup(RaftJsonIF &config, OpenerStatus* pOpenerParams)
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
#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5, 0, 0)
                .use_ref_tick = false,
#else
                .source_clk = UART_SCLK_DEFAULT,
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1)
                .flags = 0,
#endif
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

void UIModule::loop()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Handle any received data
    static const int MAX_BYTES_PER_CALL = 500;
    for (int i = 0; i < MAX_BYTES_PER_CALL; i++)
    {
        // Read a byte
        uint8_t rxByte = 0;
        int rxLen = uart_read_bytes((uart_port_t)_uartNum, &rxByte, 1, 0);
        if (rxLen <= 0)
            break;

        // Handle
        _miniHDLC.handleChar(rxByte);        
    }

    // Check status update to UI required
    if (_pOpenerStatus && 
            (_pOpenerStatus->uiUpdateRequired() || 
                Raft::isTimeout(millis(), _statusUpdateLastMs, STATUS_UPDATE_INTERVAL_MS))
        )
    {
        // Get status
        String statusJson = _pOpenerStatus->getJson() + "\r\n";

        // Encode to HDLC
        uint32_t maxEncodedLen = _miniHDLC.calcEncodedLen((const uint8_t*)statusJson.c_str(), statusJson.length());
        SpiramAwareUint8Vector encodedBuf;
        encodedBuf.resize(maxEncodedLen);
        uint32_t encodedLen = _miniHDLC.encodeFrame(encodedBuf.data(), maxEncodedLen, 
                        (const uint8_t*)statusJson.c_str(), statusJson.length());
        if (encodedLen == 0)
        {
            LOG_E(MODULE_PREFIX, "service encodeFrame failed");
            return;
        }

        // Send to UI
        uart_write_bytes((uart_port_t)_uartNum, encodedBuf.data(), encodedBuf.size());

        // Debug
#ifdef DEBUG_SERVICE_STATUS
        LOG_I(MODULE_PREFIX, "service status %s", statusJson.c_str());
#endif

        // Now done
        _statusUpdateLastMs = millis();
        _pOpenerStatus->uiUpdateDone();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UIModule::processStatus(const String &status)
{
    // Use JSON
    RaftJson params(status);

    // Get the status
    String cmdStr = params.getString("cmd", "");

    // Extract info
    int outEnabled = cmdStr.equalsIgnoreCase("outDisable") ? 0 : 
                cmdStr.equalsIgnoreCase("outEnable") ? 1 : -1;
    int inEnabled = cmdStr.equalsIgnoreCase("inDisable") ? 0 :
                cmdStr.equalsIgnoreCase("inEnable") ? 1 : -1;
    int openCloseToggle = cmdStr.equalsIgnoreCase("openCloseToggle") ? 1 : -1;
    int kitchenPIRActive = cmdStr.equalsIgnoreCase("kitchenPIRActive") ? 1 :
                cmdStr.equalsIgnoreCase("kitchenPIRInactive") ? 0 : -1;

    // Update status
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
        if (openCloseToggle != -1)
        {
            _pOpenerStatus->setOpenCloseToggle(openCloseToggle ? true : false);
            LOG_I(MODULE_PREFIX, "processStatus openCloseToggle %s", openCloseToggle ? "Y" : "N");
        }
        if (kitchenPIRActive != -1)
        {
            _pOpenerStatus->setKitchenPIRActive(kitchenPIRActive ? true : false);
            LOG_I(MODULE_PREFIX, "processStatus kitchenPIRActive %s", kitchenPIRActive ? "Y" : "N");
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle a frame received from HDLC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UIModule::frameRxCB(const uint8_t *framebuffer, unsigned framelength)
{
    // Check callback is valid
    if (!_pOpenerStatus)
        return;

    // Decode
    String frameStr(framebuffer, framelength);
#ifdef DEBUG_UI_MODULE_RX
    LOG_I(MODULE_PREFIX, "frameRxCB %s", frameStr.c_str());
#endif

    // Process
    processStatus(frameStr);
}
