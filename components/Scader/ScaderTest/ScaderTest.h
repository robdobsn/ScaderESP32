/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderTest
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftUtils.h"
#include "RaftSysMod.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define TEST_SERIAL_PORT
#define TEST_RADAR_POWER
#define DEBUG_SERIAL_RX

class APISourceInfo;

class ScaderTest : public RaftSysMod
{
public:
    ScaderTest(const char *pModuleName, RaftJsonIF& sysConfig)
            : RaftSysMod(pModuleName, sysConfig)
    {
    }

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderTest(pModuleName, sysConfig);
    }
    
protected:

    // Setup
    virtual void setup() override final
    {
        // Debug
        LOG_I(MODULE_PREFIX, "----------------------- setup enabled");
    
#ifdef TEST_SERIAL_PORT
        // Setup serial port
        // Configure UART. Note that REF_TICK is used so that the baud rate remains
        // correct while APB frequency is changing in light sleep mode
        const uart_config_t uart_config = {
                .baud_rate = 115200,
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
            LOG_E(MODULE_PREFIX, "Failed to initialize uartNum %d baudRate %d err %d", 
                            _uartNum, _baudRate, err);
            return;
        }

        // Setup pins
        err = uart_set_pin((uart_port_t)_uartNum, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to set uartNum %d txPin %d rxPin %d err %d", 
                            _uartNum, _txPin, _rxPin, err);
            return;
        }

        if (_rxPullup)
            gpio_pullup_en((gpio_num_t)_rxPin);

        // Delay before UART change
        vTaskDelay(1);

        // Install UART driver for interrupt-driven reads and writes
        err = uart_driver_install((uart_port_t)_uartNum, _rxBufSize, _txBufSize, 0, NULL, 0);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to install uartNum %d rxBufSize %d txBufSize %d err %d", 
                            _uartNum, (int)_rxBufSize, (int)_txBufSize, err);
            return;
        }

        // Init ok
        LOG_I(MODULE_PREFIX, "setup ok uartNum %d baudRate %d txPin %d rxPin %d%s rxBufSize %d txBufSize %d", 
                    _uartNum, _baudRate, _txPin, _rxPin, _rxPullup ? "(pullup)" : "", 
                    (int)_rxBufSize, (int)_txBufSize);
#endif

#ifdef TEST_RADAR_POWER
        pinMode(10, OUTPUT);
        digitalWrite(10, HIGH);
#endif
    }

    // Loop (called frequently)
    virtual void loop() override final
    {
        std::vector<uint8_t> data;
        // Check anything available
        static const int MAX_BYTES_PER_CALL = 2000;
        size_t numCharsAvailable = 0;
        esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
        if ((err == ESP_OK) && (numCharsAvailable > 0))
        {
            // Get data
            uint32_t bytesToGet = numCharsAvailable < MAX_BYTES_PER_CALL ? numCharsAvailable : MAX_BYTES_PER_CALL;
            data.resize(bytesToGet);        
            uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, data.data(), bytesToGet, 1);
            if (bytesRead != 0)
            {
                data.resize(bytesRead);

    #ifdef DEBUG_SERIAL_RX
                // Check if all data is printable
                bool allPrintable = true;
                for (uint32_t i = 0; i < data.size(); i++)
                {
                    if (((data[i] < 32) || (data[i] > 126)) && (data[i] != 0x0a) && (data[i] != 0x0d))
                    {
                        allPrintable = false;
                        break;
                    }
                }
                if (allPrintable)
                {
                    String outStr(data.data(), data.size());
                    outStr.replace("\n", " ");
                    outStr.replace("\r", "");
                    LOG_I(MODULE_PREFIX, "getData uartNum %d %s", _uartNum, outStr.c_str());
                }
                else
                {
                    String outStr;
                    Raft::getHexStrFromBytes(data.data(), data.size(), outStr);
                    LOG_I(MODULE_PREFIX, "getData uartNum %d dataLen %d data %s", _uartNum, data.size(), outStr.c_str());
                }
    #endif
            }
        }
    }

    // Get named value
    virtual double getNamedValue(const char* valueName, bool& isValid) override final
    {
        LOG_I(MODULE_PREFIX, "----------------- getNamedValue %s", valueName);
        isValid = true;
        if (String(valueName).equalsIgnoreCase("batteryPC"))
        {
            battPC += 1;
            if (battPC >= 100)
                battPC = 0;
            return battPC;
        }
        else if (String(valueName).equalsIgnoreCase("heartRate"))
        {
            if (heartRateUp)
            {
                heartRate += 5;
                if (heartRate >= 150)
                    heartRateUp = false;
            }
            else
            {
                heartRate -= 5;
                if (heartRate <= 60)
                    heartRateUp = true;
            }
            return heartRate;
        }
        isValid = false;
        return 0;
    }

    // Test value
    uint32_t battPC = 0;
    uint32_t heartRate = 60;
    bool heartRateUp = true;

    // UART
    uint32_t _baudRate = 115200;
    uint32_t _uartNum = 1;
    int _txPin = 22;
    int _rxPin = 23;
    uint32_t _rxBufSize = 1024;
    uint32_t _txBufSize = 1024;
    bool _rxPullup = false;

    static constexpr const char *MODULE_PREFIX = "ScaderTest";
};