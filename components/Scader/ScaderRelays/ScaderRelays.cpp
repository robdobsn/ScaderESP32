/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderRelays
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <time.h>
#include "Logger.h"
#include "RaftArduino.h"
#include "ScaderRelays.h"
#include "ScaderCommon.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"
#include "NetworkSystem.h"
#include "CommsChannelMsg.h"
#include "PlatformUtils.h"
#include "driver/gpio.h"

// #define CHECK_RUNNING_ON_APPROPRIATE_HW
// #define DEBUG_RELAYS_MUTABLE_DATA
#define DEBUG_RELAYS_API

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderRelays::ScaderRelays(const char *pModuleName, RaftJsonIF& sysConfig)
        : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName),
          _scaderModuleState("scaderRelays")
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

    // Get settings
    int spiMosi = configGetLong("SPI_MOSI", -1);
    int spiMiso = configGetLong("SPI_MISO", -1);
    int spiClk = configGetLong("SPI_CLK", -1);
    std::vector<int> spiChipSelects = { -1, -1, -1 };
    spiChipSelects[0] = configGetLong("SPI_CS1", -1);
    spiChipSelects[1] = configGetLong("SPI_CS2", -1);
    spiChipSelects[2] = configGetLong("SPI_CS3", -1);
    int mainsSyncPin = configGetLong("mainsSyncPin", -1);
    bool enableMainsSync = configGetBool("enableMainsSync", false);
    
    // On/Off key pin
    _onOffKey = configGetLong("onOffKey", -1);

    // Check pins for SPI are valid
    if (spiMosi < 0 || spiMiso < 0 || spiClk < 0 || 
        spiChipSelects[0] < 0 || spiChipSelects[1] < 0 || spiChipSelects[2] < 0)
    {
        LOG_E(MODULE_PREFIX, "setup FAILED invalid pins MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d mainsSyncPin %d%s onOffKey %d",
                spiMosi, spiMiso, spiClk, spiChipSelects[0], spiChipSelects[1], spiChipSelects[2], mainsSyncPin, 
                enableMainsSync ? "(ENABLED)" : "(DISABLED)", _onOffKey);
        return;
    }

    // Setup SPIDimmer
    if (!_spiDimmer.setup(spiMosi, spiClk, spiChipSelects, enableMainsSync ? mainsSyncPin : -1))
    {
        LOG_E(MODULE_PREFIX, "setup FAILED SPIDimmer");
        return;
    }

    // Clear states
    _elemStates.resize(_maxElems);
    std::fill(_elemStates.begin(), _elemStates.end(), 0);

    // Set states from scader state
    std::vector<String> elemStateStrs;
    if (_scaderModuleState.getArrayElems("relayStates", elemStateStrs))
    {
        // Set states
        for (int i = 0; i < elemStateStrs.size(); i++)
        {
            // Check if valid
            if (i < _elemStates.size())
                _elemStates[i] = getElemStateFromString(elemStateStrs[i]);

            // Set channel value
            _spiDimmer.setChannelValue(i, _elemStates[i]);
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
            RaftJson elemInfo = elemInfos[i];
            _elemNames[i] = elemInfo.getString("name", ("Relay " + String(i+1)).c_str());
            LOG_I(MODULE_PREFIX, "Relay %d name %s", i+1, _elemNames[i].c_str());
        }
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled scaderUIName %s maxRelays %d MOSI %d MISO %d CLK %d CS1 %d CS2 %d CS3 %d onOffKey %d",
                _scaderCommon.getUIName().c_str(),
                _maxElems, 
                spiMosi, spiMiso, spiClk, 
                spiChipSelects[0], spiChipSelects[1], spiChipSelects[2], 
                _onOffKey);

    // Debug show states
    debugShowCurrentState();

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->registerDataSource("Publish", _scaderCommon.getModuleName().c_str(), 
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loop (called frequently)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::loop()
{
    // Check init
    if (!_isInitialised)
        return;

    // Call the SPI dimmer loop function
    _spiDimmer.loop();

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
                            "relay/<relay>/<state> relay is 1-based, state %% on/off (but 1 is full on)");
    // LOG_I(MODULE_PREFIX, "addRestAPIEndpoints scader relays");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderRelays::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Check init
    if (!_isInitialised)
    {
        LOG_I(MODULE_PREFIX, "apiControl disabled");
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
    }

    // Get list of elems to control
    bool rslt = false;
    String elemNumsStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    std::vector<int> elemNums;
    if (elemNumsStr.length() > 0)
    { 
        // Get the relay numbers
        Raft::parseIntList(elemNumsStr.c_str(), elemNums);
    }
    else
    {
        // No relay numbers so do all
        for (int i = 0; i < _elemNames.size(); i++)
            elemNums.push_back(i + 1);
    }

    // Get newState
    String relayCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    uint32_t newDimValue = getElemStateFromString(relayCmdStr);

    // Execute command
    uint32_t numElemsSet = 0;
    for (auto elemNum : elemNums)
    {
        int elemIdx = elemNum - 1;
        if (elemIdx >= 0 && elemIdx < _elemNames.size())
        {
            // Set state
            _elemStates[elemIdx] = newDimValue;
            _mutableDataChangeLastMs = millis();
            numElemsSet++;

            // Set channel value
            _spiDimmer.setChannelValue(elemIdx, newDimValue);
        }
    }

    // Check something changed
    if (numElemsSet > 0)
    {
        // Set relays
        _mutableDataDirty = true;
        rslt = true;
        
        // Debug
#ifdef DEBUG_RELAYS_API
        String relaysStr;
        for (int i = 0; i < elemNums.size(); i++)
        {
            if (i > 0)
                relaysStr += ",";
            relaysStr += String(elemNums[i]);
        }
        LOG_I(MODULE_PREFIX, "apiControl relay%s %s set to %d%% (operation ok for %d of %d)", 
                numElemsSet > 1 ? "s" : "",
                relaysStr.c_str(), newDimValue, 
                numElemsSet, _elemNames.size());
#endif
    }
    else
    {
        // Debug
#ifdef DEBUG_RELAYS_API        
        LOG_I(MODULE_PREFIX, "apiControl no valid relays specified");
#endif
    }

    // Set result
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderRelays::getStatusJSON() const
{
    // Get status
    String elemStatus;
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            elemStatus += ",";
        elemStatus += R"({"name":")" + _elemNames[i] + R"(","state":)" + String(_elemStates[i]) + "}";
    }

    // Get mains sync status
    String mainsSyncJson = ",\"mainsHz\":" + String(_spiDimmer.getMainsHz(), 1);

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + mainsSyncJson + ",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderRelays::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (uint32_t state : _elemStates)
        stateHash.push_back(state & 0xff);
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
        jsonConfig += String(i < _elemStates.size() ? _elemStates[i] : 0);
    }
    jsonConfig += "]";

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";
#ifdef DEBUG_RELAYS_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
#endif
    _scaderModuleState.setJsonDoc(jsonConfig.c_str());
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

