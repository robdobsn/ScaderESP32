/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderRelays
//
// Rob Dobson 2013-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include <ScaderRelays.h>
#include <ConfigPinMap.h>
#include <RaftUtils.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>
#include <JSONParams.h>
#include <ESPUtils.h>
#include <time.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderRelays";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderRelays::ScaderRelays(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Init
    _isEnabled = false;
    _isInitialised = false;
    _maxRelays = DEFAULT_MAX_RELAYS;
    
    // Clear SPI device handles
    for (int i = 0; i < SPI_MAX_CHIPS; i++)
    {
        _spiDeviceHandles[i] = NULL;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::setup()
{
    // Get settings
    _isEnabled = configGetLong("enable", false) != 0;
    _maxRelays = configGetLong("maxRelays", DEFAULT_MAX_RELAYS);

    // Check enabled
    if (_isEnabled)
    {
        // // Check that SPI_MISO pin is driven by external hardware - this indicates that the hardware
        // // is valid - otherwise setting pin 16/17 (used by SPIRAM) should not be done as it causes 
        // // ESP32 to crash
        // int spiMisoPin = configGetLong("SPI_MISO", -1);
        // if (spiMisoPin < 0)
        // {
        //     LOG_E(MODULE_PREFIX, "setup FAILED SPI_MISO pin %d invalid", spiMisoPin);
        //     return;
        // }
        // // Set to input pullup
        // pinMode(spiMisoPin, INPUT_PULLUP);
        // // Get level
        // bool spiMisoLevel = digitalRead(spiMisoPin);
        // // Set to output pulldown
        // pinMode(spiMisoPin, INPUT_PULLDOWN);
        // // Check level
        // if (spiMisoLevel && !digitalRead(spiMisoPin))
        // {
        //     LOG_E(MODULE_PREFIX, "setup FAILED SPI_MISO pin %d is not driven by external hardware", 
        //             spiMisoPin, spiMisoLevel);
        //     return;
        // }
        
        // Configure GPIOs
        ConfigPinMap::PinDef gpioPins[] = {
            ConfigPinMap::PinDef("SPI_MOSI", ConfigPinMap::GPIO_INPUT, &_spiMosi),
            ConfigPinMap::PinDef("SPI_MISO", ConfigPinMap::GPIO_INPUT, &_spiMiso),
            ConfigPinMap::PinDef("SPI_CLK", ConfigPinMap::GPIO_INPUT, &_spiClk),
            ConfigPinMap::PinDef("SPI_CS1", ConfigPinMap::GPIO_OUTPUT, &_spiChipSelects[0], 1),
            ConfigPinMap::PinDef("SPI_CS2", ConfigPinMap::GPIO_OUTPUT, &_spiChipSelects[1], 1),
            ConfigPinMap::PinDef("SPI_CS3", ConfigPinMap::GPIO_OUTPUT, &_spiChipSelects[2], 1),
            ConfigPinMap::PinDef("ONOFF_KEY", ConfigPinMap::GPIO_INPUT, &_onOffKey),
        };
        ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

        // Check SPI config valid
        if ((_spiMosi < 0) || (_spiMiso < 0) || (_spiClk < 0) || (_spiChipSelects[0] < 0))
        {
            LOG_E(MODULE_PREFIX, "setup INVALID MOSI %d, MISO %d, CLK %d, CS1 %d, CS2 %d, CS3 %d, onOffKey %d",
                    _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], _spiChipSelects[2], _onOffKey);
            return;
        }

        // Configure SPI
        const spi_bus_config_t buscfg = {
            .mosi_io_num = _spiMosi,
            .miso_io_num = _spiMiso,
            .sclk_io_num = _spiClk,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .data4_io_num = -1,
            .data5_io_num = -1,
            .data6_io_num = -1,
            .data7_io_num = -1,
            .max_transfer_sz = 0,
            .flags = 0,
            .intr_flags = 0,
        };

        //Initialize the SPI bus
        esp_err_t espErr = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
        if (espErr != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setup SPI failed MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d retc %d",
                    _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], _spiChipSelects[2], espErr);
            return;
        }

        // Initialise the SPI devices
        for (int i = 0; i < SPI_MAX_CHIPS; i++)
        {
            // Check if configured
            if (_spiChipSelects[i] < 0)
                continue;

            // From datasheet https://www.onsemi.com/pub/Collateral/NCV7240-D.PDF
            // And Wikipedia https://en.wikipedia.org/wiki/Serial_Peripheral_Interface
            // CPOL = 0, CPHA = 1
            spi_device_interface_config_t devCfg =
            {
                .command_bits = 0,
                .address_bits = 0,
                .dummy_bits = 0,
                .mode = 1,
                .duty_cycle_pos = 128,
                .cs_ena_pretrans = 1,
                .cs_ena_posttrans = 0,
                .clock_speed_hz = 1000000,
                .input_delay_ns = 0,
                .spics_io_num=_spiChipSelects[i],
                .flags = 0,
                .queue_size=3,
                .pre_cb = nullptr,
                .post_cb = nullptr
            };
            esp_err_t ret = spi_bus_add_device(HSPI_HOST, &devCfg, &_spiDeviceHandles[i]);
            if (ret != ESP_OK)
            {
                LOG_E(MODULE_PREFIX, "setup add SPI device failed MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d retc %d",
                        _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], _spiChipSelects[2], ret);
                return;
            }
        }

        // Clear states
        _relayStates.resize(_maxRelays);
        std::fill(_relayStates.begin(), _relayStates.end(), false);

        // Set states from mutable config
        std::vector<String> relayStateStrs;
        if (configGetArrayElems("relayStates", relayStateStrs))
        {
            // Set states
            for (int i = 0; i < relayStateStrs.size(); i++)
            {
                // Check if valid
                bool isOn = relayStateStrs[i] == "1";
                if (i < _relayStates.size())
                    _relayStates[i] = isOn;
            }
        }

        // Relay panel name
        _relayPanelName = configGetString("ScaderCommon/name", "Scader");
        LOG_I(MODULE_PREFIX, "setup relayPanelName %s", _relayPanelName.c_str());

        // Relay names
        std::vector<String> relayInfos;
        if (configGetArrayElems("ScaderRelays/relays", relayInfos))
        {
            // Names array
            _relayNames.resize(relayInfos.size());

            // Set names
            for (int i = 0; i < relayInfos.size(); i++)
            {
                JSONParams relayInfo = relayInfos[i];
                int idx = relayInfo.getLong("idx", -1);
                if ((idx < 0) || (idx >= _relayNames.size()))
                    continue;
                _relayNames[idx] = relayInfo.getString("name", ("Relay " + String(idx)).c_str());

                LOG_I(MODULE_PREFIX, "Relay %d name %s", idx, _relayNames[idx].c_str());
            }
        }

        // Debug
        LOG_I(MODULE_PREFIX, "setup enabled maxRelays %d MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d onOffKey %d",
                    _maxRelays, _spiMosi, _spiMiso, _spiClk, _spiChipSelects[0], _spiChipSelects[1], _spiChipSelects[2], _onOffKey);

        // Debug show states
        debugShowRelayStates();

        // Setup publisher with callback functions
        SysManager* pSysManager = getSysManager();
        if (pSysManager)
        {
            // Register publish message generator
            pSysManager->sendMsgGenCB("Publish", "ScaderRelays", 
                [this](const char* messageName, CommsChannelMsg& msg) {
                    String statusStr = getStatusJSON();
                    msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
                    return true;
                },
                [this](const char* messageName, std::vector<uint8_t>& stateHash) {
                    return getStatusHash(stateHash);
                }
            );
        }

        // HW Now initialised
        _isInitialised = true;
        setRelays();
    }
    else
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::service()
{
    // Check enabled
    if (!_isEnabled)
        return;

    // Check if mutable data changed
    if (_mutableDataDirty)
    {
        // Check if min time has passed
        if (Raft::isTimeout(millis(), _mutableDataChangeLastMs, MUTABLE_DATA_SAVE_MIN_MS))
        {
            // Save mutable data
            saveMutableData();
            _mutableDataDirty = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("relay", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderRelays::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "control relays, relay/<relay>/<state> where relay is 1-based and state is 0 or 1 for off or on");
    LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader relays");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check enabled
    if (!_isEnabled)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    bool rslt = false;
    
    // Get relay number
    int relayNum = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1).toInt();

    // Get newState
    String newStateStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool newState = (newStateStr == "1") || (newStateStr.equalsIgnoreCase("on"));

    // Check validity
    if ((relayNum >= 1) && (relayNum <= _relayStates.size()))
    {
        uint32_t relayIdx = relayNum - 1;

        // Set state
        _relayStates[relayIdx] = newState;
        _mutableDataChangeLastMs = millis();
        _mutableDataDirty = true;
        rslt = true;

        // Set relays
        setRelays();
        
        // Debug
        LOG_I(MODULE_PREFIX, "apiControl relayIdx %d turned %s", relayIdx, newState ? "on" : "off");
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiControl invalid relayNum %d", relayNum);
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderRelays::getStatusJSON()
{
    // Get length of JSON
    uint32_t jsonLen = 200 + _relayPanelName.length();
    for (int i = 0; i < _relayNames.size(); i++)
        jsonLen += 30 + _relayNames[i].length();
    static const uint32_t MAX_JSON_STR_LEN = 4500;
    if (jsonLen > MAX_JSON_STR_LEN)
    {
        LOG_E(MODULE_PREFIX, "getStatusJSON jsonLen %d > MAX_JSON_STR_LEN %d", jsonLen, MAX_JSON_STR_LEN);
        return "{}";
    }

    // Get network information
    JSONParams networkJson = sysModGetStatusJSON("NetMan");

    // Extract hostname
    String hostname = networkJson.getString("Hostname", "");

    // Check if ethernet connected
    bool ethConnected = networkJson.getLong("ethConn", false) != 0;

    // Extract MAC address
    String macAddress;
    String ipAddress;
    if (ethConnected)
    {
        macAddress = getSystemMACAddressStr(ESP_MAC_ETH, ":").c_str(),
        ipAddress = networkJson.getString("ethIP", "");
    }
    else
    {
        macAddress = getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str(),
        ipAddress = networkJson.getString("IP", "");
    }

    // Create JSON string
    char* pJsonStr = new char[jsonLen];
    if (!pJsonStr)
        return "{}";

    // Format JSON
    snprintf(pJsonStr, jsonLen, R"({"numRelays":%d,"name":"%s","version":"%s","hostname":"%s","IP":"%s","MAC":"%s","relays":[)", 
                _relayNames.size(), _relayPanelName.c_str(), 
                getSysManager()->getSystemVersion().c_str(),
                hostname.c_str(), ipAddress.c_str(), macAddress.c_str());
    for (int i = 0; i < _relayNames.size(); i++)
    {
        if (i > 0)
            strncat(pJsonStr, ",", jsonLen);
        snprintf(pJsonStr + strlen(pJsonStr), jsonLen - strlen(pJsonStr), R"({"num":%d,"name":"%s","state":%s})", 
                            i + 1, _relayNames[i].c_str(), _relayStates[i] ? "1" : "0");
    }
    strncat(pJsonStr, "]}", jsonLen);
    String jsonStr = pJsonStr;
    delete[] pJsonStr;
    return jsonStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (bool state : _relayStates)
        stateHash.push_back(state ? 1 : 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write the mutable config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::saveMutableData()
{
    // Save relay states
    String jsonConfig = "\"relayStates\":[";
    for (uint32_t i = 0; i < _maxRelays; i++)
    {
        if (i > 0)
            jsonConfig += ",";
        jsonConfig += String(i < _relayStates.size() ? _relayStates[i] : false);
    }
    jsonConfig += "]";

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
    SysModBase::configSaveData(jsonConfig);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setRelays
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderRelays::setRelays()
{
    if (!_isInitialised)
        return false;

    // Perform in three steps as each block of 8 relays is controlled by a different chip
    for (int chipIdx = 0; chipIdx < _maxRelays/RELAYS_PER_CHIP; chipIdx++)
    {
        // Setup bits to transfer - 2 bits per relay, 0b10 for relay on, 0b11 for relay off
        uint16_t txVal = 0;
        for (int bitIdx = 0; bitIdx < RELAYS_PER_CHIP; bitIdx++)
        {
            uint32_t relayIdx = chipIdx*RELAYS_PER_CHIP + RELAYS_PER_CHIP - 1 - bitIdx;
            txVal = txVal << 2;
            bool isOn = relayIdx < _relayStates.size() ? _relayStates[relayIdx] : false;
            txVal |= (isOn ? 0b10 : 0b11);
            // LOG_I(MODULE_PREFIX, "setRelays chipIdx %d bitIdx %d txVal %04x relayState[%d] %d", 
            //             chipIdx, bitIdx, txVal, relayIdx, isOn);
        }
        // Need to swap the two bytes
        uint16_t txValSwapped = (txVal >> 8) | (txVal << 8);
        // LOG_I(MODULE_PREFIX, "setRelays chipIdx %d txVal %04x", chipIdx, txValSwapped);

        // SPI transaction
        uint16_t rxVal = 0;
        spi_transaction_t spiTransaction = {
            .flags = 0,
            .cmd = 0,
            .addr = 0,
            .length = 16 /* bits */,
            .rxlength = 16 /* bits */,
            .user = NULL,
            .tx_buffer = &txValSwapped,
            .rx_buffer = &rxVal,
        };

        // Send the data
        spi_device_acquire_bus(_spiDeviceHandles[chipIdx], portMAX_DELAY);
        spi_device_transmit(_spiDeviceHandles[chipIdx], &spiTransaction);
        spi_device_release_bus(_spiDeviceHandles[chipIdx]);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// debug show relay states
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::debugShowRelayStates()
{
    String relaysStr;
    for (uint32_t i = 0; i < _relayStates.size(); i++)
    {
        if (i > 0)
            relaysStr += ",";
        relaysStr += String(_relayStates[i]);
    }
    LOG_I(MODULE_PREFIX, "debugShowRelayStates %s", relaysStr.c_str());
}
