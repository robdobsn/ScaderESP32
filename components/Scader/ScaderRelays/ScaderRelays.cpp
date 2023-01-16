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
#include <ScaderCommon.h>
#include <ConfigPinMap.h>
#include <RaftUtils.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>
#include <NetworkSystem.h>
#include <JSONParams.h>
#include <ESPUtils.h>
#include <time.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderRelays";

// #define CHECK_RUNNING_ON_APPROPRIATE_HW

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderRelays::ScaderRelays(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig),
          _scaderCommon(*this, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::setup()
{
    // Common
    _scaderCommon.setup();

    // Max elems        
    _maxElems = configGetLong("maxElems", DEFAULT_MAX_ELEMS);
    if (_maxElems > DEFAULT_MAX_ELEMS)
        _maxElems = DEFAULT_MAX_ELEMS;

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

#ifdef CHECK_RUNNING_ON_APPROPRIATE_HW
    // Check that SPI_MISO pin is driven by external hardware - this indicates that the hardware
    // is valid - otherwise setting pin 16/17 (used by SPIRAM) should not be done as it causes 
    // ESP32 to crash
    int spiMisoPin = configGetLong("SPI_MISO", -1);
    if (spiMisoPin < 0)
    {
        LOG_E(MODULE_PREFIX, "setup FAILED SPI_MISO pin %d invalid", spiMisoPin);
        return;
    }
    // Set to input pullup
    pinMode(spiMisoPin, INPUT_PULLUP);
    // Get level
    bool spiMisoLevel = digitalRead(spiMisoPin);
    // Set to output pulldown
    pinMode(spiMisoPin, INPUT_PULLDOWN);
    // Check level
    if (spiMisoLevel && !digitalRead(spiMisoPin))
    {
        LOG_E(MODULE_PREFIX, "setup FAILED SPI_MISO pin %d is not driven by external hardware", 
                spiMisoPin, spiMisoLevel);
        return;
    }
#endif

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
    _elemStates.resize(_maxElems);
    std::fill(_elemStates.begin(), _elemStates.end(), false);

    // Set states from mutable config
    std::vector<String> elemStateStrs;
    if (configGetArrayElems("relayStates", elemStateStrs))
    {
        // Set states
        for (int i = 0; i < elemStateStrs.size(); i++)
        {
            // Check if valid
            bool isOn = elemStateStrs[i] == "1";
            if (i < _elemStates.size())
                _elemStates[i] = isOn;
        }
    }

    // Element names
    std::vector<String> elemInfos;
    if (configGetArrayElems("elems", elemInfos))
    {
        // Names array
        uint32_t numNames = elemInfos.size() > _maxElems ? _maxElems : elemInfos.size();
        _elemNames.resize(numNames);

        // Set names
        for (int i = 0; i < numNames; i++)
        {
            JSONParams elemInfo = elemInfos[i];
            _elemNames[i] = elemInfo.getString("name", ("Relay " + String(i+1)).c_str());
            LOG_I(MODULE_PREFIX, "Relay %d name %s", i+1, _elemNames[i].c_str());
        }
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled name %s maxRelays %d MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d onOffKey %d",
                _scaderCommon.getFriendlyName().c_str(),
                _maxElems, 
                _spiMosi, _spiMiso, _spiClk, 
                _spiChipSelects[0], _spiChipSelects[1], _spiChipSelects[2], 
                _onOffKey);

    // Debug show states
    debugShowCurrentState();

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->sendMsgGenCB("Publish", _scaderCommon.getModuleName().c_str(), 
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
    applyCurrentState();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::service()
{
    // Check init
    if (!_isInitialised)
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
    // Check init
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }

    // Get list of elems to control
    bool rslt = false;
    String elemNumsStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    int elemNums[DEFAULT_MAX_ELEMS];    
    int numElemsInList = 0;
    if (elemNumsStr.length() > 0)
    {
        // Get the relay numbers
        numElemsInList = parseIntList(elemNumsStr, elemNums, _elemNames.size());
    }
    else
    {
        // No relay numbers so do all
        for (int i = 0; i < _elemNames.size(); i++)
            elemNums[i] = i;
        numElemsInList = _elemNames.size();
    }

    // Get newState
    String relayCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool newState = (relayCmdStr == "1") || (relayCmdStr.equalsIgnoreCase("on"));

    // Execute command
    uint32_t numElemsSet = 0;
    for (int i = 0; i < numElemsInList; i++)
    {
        int elemIdx = elemNums[i] - 1;
        if (elemIdx >= 0 && elemIdx < _elemNames.size())
    {
        // Set state
        _elemStates[elemIdx] = newState;
        _mutableDataChangeLastMs = millis();
            numElemsSet++;
        }
    }

    // Check something changed
    if (numElemsSet > 0)
    {
        // Set relays
        _mutableDataDirty = true;
        applyCurrentState();
        rslt = true;
        
        // Debug
        LOG_I(MODULE_PREFIX, "apiControl %d relays (of %d) turned %s", numElemsSet, _elemNames.size(), newState ? "on" : "off");
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiControl no valid relays specified");
    }

    // Set result
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderRelays::getStatusJSON()
{
    // Get status
    String elemStatus;
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            elemStatus += ",";
        elemStatus += R"({"name":")" + _elemNames[i] + R"(","state":)" + (_elemStates[i] ? "1" : "0") + "}";
    }

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (bool state : _elemStates)
        stateHash.push_back(state ? 1 : 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write the mutable config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::saveMutableData()
{
    // Save relay states
    String jsonConfig = "\"relayStates\":[";
    for (uint32_t i = 0; i < _maxElems; i++)
    {
        if (i > 0)
            jsonConfig += ",";
        jsonConfig += String(i < _elemStates.size() ? _elemStates[i] : false);
    }
    jsonConfig += "]";

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
    SysModBase::configSaveData(jsonConfig);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// applyCurrentState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderRelays::applyCurrentState()
{
    if (!_isInitialised)
        return false;

    // Perform in three steps as each block of 8 relays is controlled by a different chip
    for (int chipIdx = 0; chipIdx < _maxElems/ELEMS_PER_CHIP; chipIdx++)
    {
        // Setup bits to transfer - 2 bits per relay, 0b10 for relay on, 0b11 for relay off
        uint16_t txVal = 0;
        for (int bitIdx = 0; bitIdx < ELEMS_PER_CHIP; bitIdx++)
        {
            uint32_t elemIdx = chipIdx*ELEMS_PER_CHIP + ELEMS_PER_CHIP - 1 - bitIdx;
            txVal = txVal << 2;
            bool isOn = elemIdx < _elemStates.size() ? _elemStates[elemIdx] : false;
            txVal |= (isOn ? 0b10 : 0b11);
            // LOG_I(MODULE_PREFIX, "applyCurrentState chipIdx %d bitIdx %d txVal %04x relayState[%d] %d", 
            //             chipIdx, bitIdx, txVal, elemIdx, isOn);
        }
        // Need to swap the two bytes
        uint16_t txValSwapped = (txVal >> 8) | (txVal << 8);
        // LOG_I(MODULE_PREFIX, "applyCurrentState chipIdx %d txVal %04x", chipIdx, txValSwapped);

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
// debug show states
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::debugShowCurrentState()
{
    String elemStr;
    for (uint32_t i = 0; i < _elemStates.size(); i++)
    {
        if (i > 0)
            elemStr += ",";
        elemStr += String(_elemStates[i]);
    }
    LOG_I(MODULE_PREFIX, "debugShowCurrentState %s", elemStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse list of integers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ScaderRelays::parseIntList(const String &str, int *pIntList, int maxInts)
{
    int numInts = 0;
    int idx = 0;
    while (idx < str.length())
    {
        int nextCommaIdx = str.indexOf(',', idx);
        if (nextCommaIdx < 0)
            nextCommaIdx = str.length();
        String intStr = str.substring(idx, nextCommaIdx);
        int intVal = intStr.toInt();
        if (numInts < maxInts)
            pIntList[numInts++] = intVal;
        idx = nextCommaIdx + 1;
    }
    return numInts;
}