/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderShades
//
// Rob Dobson 2013-2021
// More details at http://robdobson.com/2013/10/moving-my-window-shades-control-to-mbed/
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderShades.h"
#include <ArduinoOrAlt.h>
#include <RaftUtils.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>

static const char *MODULE_PREFIX = "ScaderShades";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderShades::ScaderShades(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Init
    _isEnabled = false;
    _isInitialised = false;
    _luxReportSecs = 10;
    _luxLastReportMs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::setup()
{
    // Get settings
    _isEnabled = configGetLong("enable", false) != 0;
    _luxReportSecs = configGetLong("luxReportSecs", 10);

    // Check enabled
    if (_isEnabled)
    {
        // Configure GPIOs
        ConfigPinMap::PinDef gpioPins[] = {
            ConfigPinMap::PinDef("HC959_SER", ConfigPinMap::GPIO_OUTPUT, &_hc595_SER),
            ConfigPinMap::PinDef("HC595_SCK", ConfigPinMap::GPIO_OUTPUT, &_hc595_SCK),
            ConfigPinMap::PinDef("HC595_LATCH", ConfigPinMap::GPIO_OUTPUT, &_hc595_LATCH),
            ConfigPinMap::PinDef("HC595_RST", ConfigPinMap::GPIO_OUTPUT, &_hc595_RST),
            ConfigPinMap::PinDef("LIGHTLVL_1", ConfigPinMap::GPIO_INPUT, &_lightLevel0),
            ConfigPinMap::PinDef("LIGHTLVL_2", ConfigPinMap::GPIO_INPUT, &_lightLevel1),
            ConfigPinMap::PinDef("LIGHTLVL_3", ConfigPinMap::GPIO_INPUT, &_lightLevel2)
        };
        ConfigPinMap::configMultiple(configGetConfig(), gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

        // Get the lux reporting period
        _luxLastReportMs = millis();

        // Check valid
        if ((_hc595_RST < 0) || (_hc595_LATCH < 0) || (_hc595_SCK < 0) || (_hc595_SER < 0))
        {
            LOG_W(MODULE_PREFIX, "setup invalid parameters for HC595 pins HC959_SER %d HC595_SCK %d HC595_LATCH %d HC595_RST %d",
                        _hc595_SER, _hc595_SCK, _hc595_LATCH, _hc595_RST);
            return;
        }

        // Set the pin value of the reset pin to inactive (high)
        digitalWrite(_hc595_RST, 1);

        // HW Now initialised
        _isInitialised = true;

        // Reset the shades parameters
        for (int i = 0; i < MAX_WINDOW_SHADES; i++)
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
            pSysManager->sendMsgGenCB("Publish", "ScaderShades", 
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

        // Debug
        LOG_I(MODULE_PREFIX, "setup enabled luxReportSecs %d HC959_SER %d HC595_SCK %d HC595_LATCH %d HC595_RST %d LIGHTLVL_1 %d LIGHTLVL_2 %d LIGHTLVL_3 %d",
                    _luxReportSecs, _hc595_SER, _hc595_SCK, _hc595_LATCH, _hc595_RST, _lightLevel0, _lightLevel1, _lightLevel2);
    }
    else
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::service()
{
    if (!_isInitialised)
        return;

    // See if any shade actions need to end
    bool somethingSet = false;
    for (int shadeIdx = 0; shadeIdx < MAX_WINDOW_SHADES; shadeIdx++)
    {
        if (_msTimeouts[shadeIdx] != 0)
        {
            if (Raft::isTimeout(millis(), _tickCounts[shadeIdx], _msTimeouts[shadeIdx]))
            {
                LOG_I(MODULE_PREFIX, "Timeout idx %d duration %d", shadeIdx, _msTimeouts[shadeIdx]);
                // Clear the command
                clearShadeBits(shadeIdx);
                // Check for sequence step complete
                if (_sequenceRunning)
                {
                    if (shadeIdx == _sequence._shadeIdx)
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
    uint32_t bitMask = 1 << (MAX_WINDOW_SHADES * BITS_PER_SHADE - 1);
    // Send the value to the shift register
    for (int bitCount = 0; bitCount < MAX_WINDOW_SHADES * BITS_PER_SHADE; bitCount++)
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

bool ScaderShades::isBusy(int shadeIdx)
{
    // Check validity
    if (shadeIdx < 0 || shadeIdx >= MAX_WINDOW_SHADES)
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
    if (shadeIdx < 0 || shadeIdx >= MAX_WINDOW_SHADES)
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

void ScaderShades::getLightLevels(int &l0, int &l1, int &l2)
{
    l0 = _lightLevel0 >= 0 ? analogRead(_lightLevel0) : 0;
    l1 = _lightLevel1 >= 0 ? analogRead(_lightLevel1) : 0;
    l2 = _lightLevel2 >= 0 ? analogRead(_lightLevel2) : 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getStatusJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderShades::getStatusJSON()
{
    int l0, l1, l2;
    getLightLevels(l0, l1, l2);
    char llStr[50];
    snprintf(llStr, sizeof(llStr), R"({"lux0":%d,"lux1":%d,"lux2":%d})", l0, l1, l2);
    return llStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sequenceStart
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderShades::sequenceStart(int shadeIdx)
{
    if (_sequence._isBusy)
        return false;
    _sequence._shadeIdx = shadeIdx;
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
    setTimedOutput(_sequence._shadeIdx, mask, pinState, msDuration, clearExisting);
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
        clearShadeBits(_sequence._shadeIdx);
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
                            std::bind(&ScaderShades::apiShadesControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Shades - /1..N/up|stop|down/pulse|on|off");
    // Alternate control shade name
    endpointManager.addEndpoint("blind", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderShades::apiShadesControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Shades - /1..N/up|stop|down/pulse|on|off");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control shades via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderShades::apiShadesControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    String shadeNumStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    int shadeNum = shadeNumStr.toInt();
    String shadeCmdStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String shadeDurationStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);

    if ((shadeNum < 1) || (shadeNum > getMaxNumShades()))
    {
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, false);
        return;
    }
    int shadeIdx = shadeNum - 1;
    bool rslt = doCommand(shadeIdx, shadeCmdStr, shadeDurationStr);
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}
