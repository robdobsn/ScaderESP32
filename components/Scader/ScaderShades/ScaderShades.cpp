/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderShades
//
// Rob Dobson 2013-2021
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderShades.h"
#include "RaftArduino.h"
#include "RaftUtils.h"
#include "ConfigPinMap.h"
#include "RestAPIEndpointManager.h"
#include "SysManager.h"
#include "CommsChannelMsg.h"
#include "PlatformUtils.h"
#include "NetworkSystem.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO - config is global was passed to RaftSysMod constructor
ScaderShades::ScaderShades(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Get settings
    _maxElems = configGetLong("maxElems", DEFAULT_MAX_ELEMS);
    if (_maxElems > DEFAULT_MAX_ELEMS)
        _maxElems = DEFAULT_MAX_ELEMS;
    _lightLevelsEnabled = configGetLong("enableLightLevels", false) != 0;
        
    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Configure GPIOs
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("HC595_SER", ConfigPinMap::GPIO_OUTPUT, &_hc595_SER),
        ConfigPinMap::PinDef("HC595_SCK", ConfigPinMap::GPIO_OUTPUT, &_hc595_SCK),
        ConfigPinMap::PinDef("HC595_LATCH", ConfigPinMap::GPIO_OUTPUT, &_hc595_LATCH),
        ConfigPinMap::PinDef("HC595_RST", ConfigPinMap::GPIO_OUTPUT, &_hc595_RST),
        ConfigPinMap::PinDef("LIGHTLVLPINS[0]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[0]),
        ConfigPinMap::PinDef("LIGHTLVLPINS[1]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[1]),
        ConfigPinMap::PinDef("LIGHTLVLPINS[2]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[2])
    };
    ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Check valid
    if ((_hc595_RST < 0) || (_hc595_LATCH < 0) || (_hc595_SCK < 0) || (_hc595_SER < 0))
    {
        LOG_W(MODULE_PREFIX, "setup invalid parameters for HC595 pins HC595_SER %d HC595_SCK %d HC595_LATCH %d HC595_RST %d",
                    _hc595_SER, _hc595_SCK, _hc595_LATCH, _hc595_RST);
        return;
    }

    // Set the pin value of the HC595 reset pin to inactive (high)
    digitalWrite(_hc595_RST, 1);

    // Check if light-levels are used - only want to do this if door control is not used as they share a pin currrently
    if (_lightLevelsEnabled)
    {
        // Configure GPIOs
        ConfigPinMap::PinDef lightLevelPins[] = {
            ConfigPinMap::PinDef("LIGHTLVLPINS[0]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[0]),
            ConfigPinMap::PinDef("LIGHTLVLPINS[1]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[1]),
            ConfigPinMap::PinDef("LIGHTLVLPINS[2]", ConfigPinMap::GPIO_INPUT, &_lightLevelPins[2])
        };
        ConfigPinMap::configMultiple(configGetConfig(), lightLevelPins, sizeof(lightLevelPins) / sizeof(lightLevelPins[0]));

        // Debug
        LOG_I(MODULE_PREFIX, "setup light-level pins %d %d %d", _lightLevelPins[0], _lightLevelPins[1], _lightLevelPins[2]);
    }

    // HW Now initialised
    _isInitialised = true;

    // Reset the shades parameters
    for (int i = 0; i < DEFAULT_MAX_ELEMS; i++)
    {
        _msTimeouts[i] = 0;
        _tickCounts[i] = 0;
    }
    _curShadeCtrlBits = 0;

    // Send to shift register
    sendBitsToShiftRegister();

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
            nullptr
            // [this](const char* messageName, std::vector<uint8_t>& stateHash) {
            // }
            );
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
            _elemNames[i] = elemInfo.getString("name", ("Shade " + String(i+1)).c_str());
            LOG_I(MODULE_PREFIX, "Shade %d name %s", i+1, _elemNames[i].c_str());
        }
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled name %s HC595_SER %d HC595_SCK %d HC595_LATCH %d HC595_RST %d",
                _scaderCommon.getUIName().c_str(),
                _hc595_SER, _hc595_SCK, _hc595_LATCH, _hc595_RST);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loop (called frequently)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::loop()
{
    if (!_isInitialised)
        return;

    // See if any shade actions need to end
    bool somethingSet = false;
    for (int shadeIdx = 0; shadeIdx < DEFAULT_MAX_ELEMS; shadeIdx++)
    {
        if (_msTimeouts[shadeIdx] != 0)
        {
            if (Raft::isTimeout(millis(), _tickCounts[shadeIdx], _msTimeouts[shadeIdx]))
            {
                LOG_I(MODULE_PREFIX, "Timeout idx %d duration %d enableLightLevels %d", 
                        shadeIdx, _msTimeouts[shadeIdx], _lightLevelsEnabled);
                // Clear the command
                clearShadeBits(shadeIdx);
                // Check for sequence step complete
                if (_sequenceRunning)
                {
                    if (shadeIdx == _sequence._elemIdx)
                    {
                        LOG_I(MODULE_PREFIX, "sequenceStepComplete");
                        sequenceStepComplete();
                        return;
                    }
                }
                // Collate changes
                somethingSet = true;
            }
        }
    }
    if (somethingSet)
    {
        sendBitsToShiftRegister();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sendBitsToShiftRegister
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::sendBitsToShiftRegister()
{
    if (!_isInitialised)
        return false;

    char settingStr[20];
    sprintf(settingStr, "%08x", _curShadeCtrlBits);
    LOG_I(MODULE_PREFIX, "sendBitsToShiftRegister %s", settingStr);
    uint32_t dataVal = _curShadeCtrlBits;
    uint32_t bitMask = 1 << (DEFAULT_MAX_ELEMS * BITS_PER_SHADE - 1);
    // Send the value to the shift register
    for (int bitCount = 0; bitCount < DEFAULT_MAX_ELEMS * BITS_PER_SHADE; bitCount++)
    {
        // Set the data
        digitalWrite(_hc595_SER, (dataVal & bitMask) != 0);
        delayMicroseconds(5);
        bitMask = bitMask >> 1;
        // Clock the data into the shift-register
        digitalWrite(_hc595_SCK, HIGH);
        delayMicroseconds(5);
        digitalWrite(_hc595_SCK, LOW);
        delayMicroseconds(5);
    }
    // Move the value into the output register
    digitalWrite(_hc595_LATCH, HIGH);
    delayMicroseconds(5);
    digitalWrite(_hc595_LATCH, LOW);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::isBusy(int shadeIdx) const
{
    // Check validity
    if (shadeIdx < 0 || shadeIdx >= DEFAULT_MAX_ELEMS)
        return false;
    return _msTimeouts[shadeIdx] != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setShadeBit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::setShadeBit(int shadeIdx, int bitMask, int bitIsOn)
{
    if (!_isInitialised)
        return false;

    uint32_t movedMask = bitMask << (shadeIdx * BITS_PER_SHADE);

    if (bitIsOn)
    {
        _curShadeCtrlBits |= movedMask;
    }
    else
    {
        _curShadeCtrlBits &= (~movedMask);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// clearShadeBits
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::clearShadeBits(int shadeIdx)
{
    // Clear any existing command
    _msTimeouts[shadeIdx] = 0;
    _tickCounts[shadeIdx] = 0;
    return setShadeBit(shadeIdx, SHADE_UP_BIT_MASK | SHADE_STOP_BIT_MASK | SHADE_DOWN_BIT_MASK, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setTimedOutput
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::setTimedOutput(int shadeIdx, int bitMask, bool bitOn, uint32_t msDuration, bool bClearExisting)
{
    LOG_I(MODULE_PREFIX, "setTimedOutput idx %d mask %d bitOn %d duration %d clear %d", shadeIdx, bitMask, bitOn, msDuration, bClearExisting);

    if (bClearExisting)
    {
        clearShadeBits(shadeIdx);
    }
    setShadeBit(shadeIdx, bitMask, bitOn);
    if (msDuration != 0)
    {
        _msTimeouts[shadeIdx] = msDuration;
        _tickCounts[shadeIdx] = millis();
    }
    return sendBitsToShiftRegister();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// doCommand
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::doCommand(int shadeIdx, String &cmdStr, String &durationStr)
{
    // Check validity
    if (shadeIdx < 0 || shadeIdx >= DEFAULT_MAX_ELEMS)
        return false;

    // Get duration and on/off
    int pinOn = false;
    int msDuration = 0;

    if (durationStr.equalsIgnoreCase("on"))
    {
        pinOn = true;
        msDuration = MAX_SHADE_ON_MILLSECS;
    }
    else if (durationStr.equalsIgnoreCase("off"))
    {
        pinOn = false;
        msDuration = 0;
    }
    else if (durationStr.equalsIgnoreCase("pulse"))
    {
        pinOn = true;
        msDuration = PULSE_ON_MILLISECS;
    }
    else
    {
        pinOn = true;
        msDuration = durationStr.toInt();
    }

    // Handle commands
    if (cmdStr.equalsIgnoreCase("up"))
    {
        setTimedOutput(shadeIdx, SHADE_UP_BIT_MASK, pinOn, msDuration, true);
    }
    else if (cmdStr.equalsIgnoreCase("stop"))
    {
        setTimedOutput(shadeIdx, SHADE_STOP_BIT_MASK, pinOn, msDuration, true);
    }
    else if (cmdStr.equalsIgnoreCase("down"))
    {
        setTimedOutput(shadeIdx, SHADE_DOWN_BIT_MASK, pinOn, msDuration, true);
    }
    else if (cmdStr.equalsIgnoreCase("setuplimit"))
    {
        if (sequenceStart(shadeIdx))
        {
            sequenceAdd(SHADE_DOWN_BIT_MASK | SHADE_STOP_BIT_MASK, true, 1500, true);
            sequenceAdd(SHADE_STOP_BIT_MASK, true, 500, true);
            sequenceRun();
            LOG_I(MODULE_PREFIX, "doCommand sequenceStarted for set up limit");
        }
        else
        {
            LOG_I(MODULE_PREFIX, "doCommand sequence can't start as busy");
        }
    }
    else if (cmdStr.equalsIgnoreCase("setdownlimit"))
    {
        if (sequenceStart(shadeIdx))
        {
            sequenceAdd(SHADE_UP_BIT_MASK | SHADE_STOP_BIT_MASK, true, 1500, true);
            sequenceAdd(SHADE_STOP_BIT_MASK, true, 500, true);
            sequenceAdd(SHADE_STOP_BIT_MASK, false, 2000, true);
            sequenceAdd(SHADE_STOP_BIT_MASK, true, 7000, true);
            sequenceRun();
            LOG_I(MODULE_PREFIX, "doCommand sequenceStarted for set down limit and record");
        }
        else
        {
            LOG_I(MODULE_PREFIX, "doCommand sequence can't start as busy");
        }
    }
    else if (cmdStr.equalsIgnoreCase("setfavourite"))
    {
        setTimedOutput(shadeIdx, SHADE_STOP_BIT_MASK, pinOn, 7000, true);
    }
    else if (cmdStr.equalsIgnoreCase("resetmemory"))
    {
        setTimedOutput(shadeIdx, SHADE_STOP_BIT_MASK | SHADE_DOWN_BIT_MASK | SHADE_UP_BIT_MASK, pinOn, 15000, true);
    }
    else if (cmdStr.equalsIgnoreCase("reversedirn"))
    {
        setTimedOutput(shadeIdx, SHADE_STOP_BIT_MASK, pinOn, 7000, true);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getLightLevels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::getLightLevels(int lightLevels[], int numLevels) const
{
    // Read light levels
    for (int i = 0; i < numLevels; i++)
    {
        if (_lightLevelsEnabled && (_lightLevelPins[i] >= 0))
            lightLevels[i] = analogRead(_lightLevelPins[i]);
        else
            lightLevels[i] = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getStatusJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderShades::getStatusJSON() const
{
    String lightLevelsStr;
    if (_lightLevelsEnabled)
    {
        int lightLevels[NUM_LIGHT_LEVELS];
        getLightLevels(lightLevels, NUM_LIGHT_LEVELS);
        for (int i = 0; i < NUM_LIGHT_LEVELS; i++)
        {
            lightLevelsStr += String(lightLevels[i]);
            if (i < (NUM_LIGHT_LEVELS - 1))
                lightLevelsStr += ",";
        }
    }

    // Get status
    String elemStatus;
    for (int i = 0; i < _elemNames.size(); i++)
    {
        if (i > 0)
            elemStatus += ",";
        elemStatus += R"({"name":")" + _elemNames[i] + R"(","state":)" + String(isBusy(i) ? "1" : "0") + "}";
    }

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elemStatus + "],\"lux\":[" + lightLevelsStr + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceStart
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::sequenceStart(int shadeIdx)
{
    if (_sequence._isBusy)
        return false;
    _sequence._elemIdx = shadeIdx;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceAdd
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::sequenceAdd(int mask, bool pinOn, uint32_t msDuration, bool clearExisting)
{
    WindowShadesSeqStep step(mask, pinOn, msDuration, clearExisting);
    _sequence.addStep(step);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceRun
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::sequenceRun()
{
    _sequence._curStep = 0;
    _sequenceRunning = true;
    sequenceStartStep(_sequence._curStep);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceStartStep
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::sequenceStartStep(int stepIdx)
{
    if (stepIdx < 0 || stepIdx >= _sequence.MAX_SEQ_STEPS)
        return;
    LOG_I(MODULE_PREFIX, "sequenceStartStep stepIdx %d", stepIdx);
    int mask = _sequence._steps[stepIdx]._bitMask;
    int pinState = _sequence._steps[stepIdx]._pinState;
    uint32_t msDuration = _sequence._steps[stepIdx]._msDuration;
    bool clearExisting = _sequence._steps[stepIdx]._clearExisting;
    setTimedOutput(_sequence._elemIdx, mask, pinState, msDuration, clearExisting);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceStepComplete
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::sequenceStepComplete()
{
    _sequence._curStep++;
    if (_sequence._curStep >= _sequence._numSteps)
    {
        LOG_I(MODULE_PREFIX, "sequenceStepComplete");
        _sequence._isBusy = false;
        _sequence._curStep = 0;
        _sequence._numSteps = 0;
        clearShadeBits(_sequence._elemIdx);
        sendBitsToShiftRegister();
    }
    else
    {
        // Start next step
        sequenceStartStep(_sequence._curStep);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("shade", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderShades::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Shades - /1..N/up|stop|down/pulse|on|off");
    // Alternate control shade name
    endpointManager.addEndpoint("blind", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderShades::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Shades - /1..N/up|stop|down/pulse|on|off");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control shades via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RaftRetCode ScaderShades::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    String shadeNumStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    int shadeNum = shadeNumStr.toInt();
    String shadeCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String shadeDurationStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);

    if ((shadeNum < 1) || (shadeNum > _maxElems))
    {
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
    }
    int shadeIdx = shadeNum - 1;
    bool rslt = doCommand(shadeIdx, shadeCmdStr, shadeDurationStr);
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}
