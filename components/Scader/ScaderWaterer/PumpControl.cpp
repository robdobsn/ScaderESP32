/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pump Control
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PumpControl.h"
#include "ConfigPinMap.h"

static const char* MODULE_PREFIX = "ScaderPumpControl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PumpControl::PumpControl()
{
}

PumpControl::~PumpControl()
{
    deinit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::setup(RaftJsonIF& config)
{
    // Deinit
    deinit();

    // Prepare PWM outputs
    const char* pwmOutputNames[] = {"Pump0", "Pump1", "Pump2", "Pump3"};
    const int numPWMOutputs = sizeof(pwmOutputNames)/sizeof(pwmOutputNames[0]);
    int pinVars[] = {0,0,0,0};

    // PWM timer
    ledc_timer_config_t ledcTimer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
        .duty_resolution = LEDC_TIMER_12_BIT, // resolution of PWM duty
        .timer_num = LEDC_TIMER_0,            // timer index
        .freq_hz = 19500,                      // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledcTimer);
    
    // Configure GPIOs
    ConfigPinMap::PinDef gpioPins[] = {
        ConfigPinMap::PinDef("Pump0", ConfigPinMap::GPIO_OUTPUT, &pinVars[0]),
        ConfigPinMap::PinDef("Pump1", ConfigPinMap::GPIO_OUTPUT, &pinVars[1]),
        ConfigPinMap::PinDef("Pump2", ConfigPinMap::GPIO_OUTPUT, &pinVars[2]),
        ConfigPinMap::PinDef("Pump3", ConfigPinMap::GPIO_OUTPUT, &pinVars[3])
    };
    ConfigPinMap::configMultiple(config, gpioPins, sizeof(gpioPins) / sizeof(gpioPins[0]));

    // Add to the PWM outputs
    for (uint32_t i = 0; i < numPWMOutputs; i++)
    {
        // LEDC channel
        uint32_t ledcChanNum = LEDC_CHANNEL_2 + i;
        // Setup LEDC PWM handler
        ledc_channel_config_t ledc_channel = {
            .gpio_num   = pinVars[i],
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel    = (ledc_channel_t)ledcChanNum,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
            .flags{}
        };
        ledc_channel_config(&ledc_channel);

        // Setup output object
        PWMOutput pwmOutput(pwmOutputNames[i], pinVars[i], ledcChanNum, 4096);
        _pwmOutputs.push_back(pwmOutput);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup Pump pins %d %d %d %d", pinVars[0], pinVars[1], pinVars[2], pinVars[3]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::loop()
{
    // Update PWM outputs
    for (auto& pwmOutput : _pwmOutputs)
        pwmOutput.loop();

    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, 1000))
    {
        _debugLastDisplayMs = millis();
        LOG_I(MODULE_PREFIX, "service");
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PumpControl::isBusy()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// pumpControl
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::setFlow(uint32_t pumpIdx, float flowRate, float durationSecs)
{
    // Pump
    if (pumpIdx < _pwmOutputs.size())
    {
        // Set flow rate
        LOG_I(MODULE_PREFIX, "setFlow %d %f %f", pumpIdx, flowRate, durationSecs);
        _pwmOutputs[pumpIdx].set(flowRate, (uint32_t)(durationSecs * 1000));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deinit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::deinit()
{
    // Change pin modes
    for (auto& pwmOutput : _pwmOutputs)
    {
        pwmOutput.deinit();
    }

    // Deinit
    _pwmOutputs.clear();

    // Debug
    LOG_I(MODULE_PREFIX, "deinit");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getStatusJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String PumpControl::getStatusJSON(bool includeBraces)
{
    String statusStr;
    for (auto& op : _pwmOutputs)
        statusStr += (statusStr.length() > 0 ? "," : "") + op.getStatusStrJSON();
    if (includeBraces)
        statusStr = "{" + statusStr + "}";
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::getStatusHash(std::vector<uint8_t>& stateHash)
{
    stateHash.clear();
    for (auto& op : _pwmOutputs)
        stateHash.push_back(op.getStateHashByte());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output set
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::PWMOutput::set(float pwmRatio, uint32_t durationMs)
{
    // Set
    _timerActive = false;
    _isOn = pwmRatio != 0;

    // Check setup
    if (_isSetup)
    {
        // Set PWM level
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_ledcChanNum, (uint32_t)(pwmRatio * _maxDuty));
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_ledcChanNum);
        LOG_I(MODULE_PREFIX, "set %d %f %d", _ledcChanNum, pwmRatio, (uint32_t)(pwmRatio * _maxDuty));

        // Handle duration
        if (durationMs > 0)
        {
            _timerActive = true;
            _durationMs = durationMs;
            _startTimeMs = millis();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::PWMOutput::loop()
{
    // Check if timer is active
    if (_timerActive)
    {
        // Check if the output change is due
        if (Raft::isTimeout(millis(), _startTimeMs, _durationMs))
        {
            // Change the output
            _isOn = !_isOn;
            LOG_I(MODULE_PREFIX, "service turning %s %s", _name.c_str(), _isOn ? "on" : "off");
            if (_isSetup)
            {
                // Clear PWM level
                ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_ledcChanNum, 0);
                ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_ledcChanNum);
            }
            _timerActive = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timed output deinit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PumpControl::PWMOutput::deinit()
{
    // Close timer channel
    if (_isSetup)
    {
        ledc_stop(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_ledcChanNum, 0);
    }

    // Change pin mode
    if (_pinNum >= 0)
    {
        pinMode(_pinNum, INPUT);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String PumpControl::PWMOutput::getStatusStrJSON()
{
    char statusStr[50];
    snprintf(statusStr, sizeof(statusStr), R"("%s":%d)", _name.c_str(), _isOn ? 1 : 0);
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t PumpControl::PWMOutput::getStateHashByte()
{
    return _isOn ? 1 : 0;
}
