/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Moisture Sensors
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MoistureSensors.h"
#include "ConfigPinMap.h"

static const char* MODULE_PREFIX = "ScaderMoistSensor";

MoistureSensors::MoistureSensors()
{
}

MoistureSensors::~MoistureSensors()
{
    if (_pI2CBus)
        delete _pI2CBus;
}

void MoistureSensors::setup(const RaftJsonIF& config)
{
    // Clear
    if (_pI2CBus)
        delete _pI2CBus;

    // Configure bus for sensors
    _pI2CBus = new RdI2C();
    if (!_pI2CBus)
    {
        LOG_E(MODULE_PREFIX, "setup failed to create I2C bus");
        return;
    }

    // Configure bus
    uint32_t i2cPort = config.getLong("i2cPort", 0);
    String pinName = config.getString("sdaPin", "");
    int sdaPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = config.getString("sclPin", "");
    int sclPin = ConfigPinMap::getPinFromName(pinName.c_str());
    uint32_t freq = config.getLong("i2cFreq", 100000);
    uint32_t i2cFilter = config.getLong("i2cFilter", RdI2C::DEFAULT_BUS_FILTER_LEVEL);
    if (!_pI2CBus->init(i2cPort, sdaPin, sclPin, freq, i2cFilter))
    {
        LOG_E(MODULE_PREFIX, "setup FAILED name %s port %d SDA %d SCL %d FREQ %d", "I2CA", i2cPort, sdaPin, sclPin, freq);
        return;
    }

    // Address
    _adcI2CAddr = config.getLong("adcI2CAddr", 0);
   
    // Debug
    LOG_I(MODULE_PREFIX, "setup ADC I2CAddress 0x%02x", _adcI2CAddr);
}

void MoistureSensors::loop()
{
    // Check valid
    if (!_pI2CBus)
        return;

    // Check if time to sample the ADC
    if (Raft::isTimeout(millis(), _adcConvLastMs, 10))
    {
        // Remember last time
        _adcConvLastMs = millis();

        // Already in progress?
        uint32_t numRead = 0;
        if (_adcConvInProgress)
        {
            // Assume conversion complete and get result
            uint8_t cmd[] = {ADS1015_CONVERSION_REGISTER};
            uint8_t readData[2];
            if (_pI2CBus->access(_adcI2CAddr, cmd, sizeof(cmd), readData, sizeof(readData), numRead) != RdI2C::ACCESS_RESULT_OK)
            {
                LOG_E(MODULE_PREFIX, "service failed to read ADC");
                return;
            }
            uint16_t adcValue = (readData[0] >> 4) | (readData[1] << 4);
            if (adcValue >= 0x0800)
                adcValue = 0;                

            _adcAvgValues[_adcCurChannel](adcValue);
            _adcCurChannel = (_adcCurChannel + 1) % NUM_MOISTURE_SENSORS;
        }

        // Send config/convert command
        uint16_t ads1015Command = 
                    ADS1015_CONFIG_MUX_SINGLE_ENDED | // Single ended
                    ((_adcCurChannel << ADS1015_CONFIG_MUX_BIT_POS) & ADS1015_CONFIG_MUX_CHAN_MASK) | // Channel
                    ADS1015_CONFIG_PGA_2_048V | // 0..2.048V range
                    ADS1015_CONFIG_MODE_CONTINUOUS | // Continuous conversion mode
                    ADS1015_CONFIG_DATA_RATE_1600SPS | // 860SPS
                    ADS1015_CONFIG_COMP_QUE_DISABLE; // Disable comparator

        // uint8_t cmd[] = { ADS1015_CONFIG_REGISTER, (uint8_t)(0x44 | ((_adcCurChannel & 0x03) << 4)), 0x83 };
        // uint8_t cmd[] = { ADS1015_CONFIG_REGISTER, (uint8_t) (0x85 | ((_adcCurChannel & 0x03) << 4)), 0x83 };
        uint8_t cmd[] = { ADS1015_CONFIG_REGISTER, (uint8_t) (ads1015Command >> 8), (uint8_t) (ads1015Command & 0xFF) };
        if (_pI2CBus->access(_adcI2CAddr, cmd, sizeof(cmd), nullptr, 0, numRead) != RdI2C::ACCESS_RESULT_OK)
        {
            LOG_W(MODULE_PREFIX, "service failed to start ADC conversion");
            return;
        }
        _adcConvInProgress = true;
    }

    // // Service the stepper
    // if (_pI2CBus)
    //     _pI2CBus->loop();

    // // Calculate window average of vissen analogRead
    // if (Raft::isTimeout(millis(), _currentLastSampleMs, CURRENT_SAMPLE_TIME_MS))
    // {
    //     _currentLastSampleMs = millis();
    //     // TODO 2022 average current
    //     uint16_t avgCurrent = _avgCurrent(0);

    //     // Check for overcurrent
    //     if (avgCurrent > CURRENT_OVERCURRENT_THRESHOLD)
    //     {
    //         if (!_isOpen || Raft::isTimeout(millis(), _overCurrentStartTimeMs, OVER_CURRENT_IGNORE_AT_START_OF_OPEN_MS))
    //         {
    //             LOG_I(MODULE_PREFIX, "service overcurrent detected - avgCurrent=%d", avgCurrent);
    //             _isOverCurrent = true;
    //             _overCurrentStartTimeMs = millis();
    //             _overCurrentBlinkTimeMs = 0;
    //             _overCurrentBlinkCurOn = true;
    //             motorControl(false, 0);
    //         }
    //     }
    // }

    // Debug
    if (Raft::isTimeout(millis(), _debugLastDisplayMs, 1000))
    {
        _debugLastDisplayMs = millis();
        LOG_I(MODULE_PREFIX, "service ADC avgs %d(%d) %d(%d) %d(%d) %d(%d)",
                    getMoisturePercentage(0), _adcAvgValues[0].cur(),
                    getMoisturePercentage(1), _adcAvgValues[1].cur(),
                    getMoisturePercentage(2), _adcAvgValues[2].cur(),
                    getMoisturePercentage(3), _adcAvgValues[3].cur());

        // LOG_I(MODULE_PREFIX, "service");
        // LOG_I(MODULE_PREFIX, "service %savg current=%d inPIR: btn=%d actv=%d last=%d en=%d outPIR: btn=%d actv=%d last=%d en=%d", 
        //     _isOpen ? "OPEN " : "CLOSED ", 
        //     _avgCurrent.cur(),
        //     digitalRead(_pirSenseInPin), _pirSenseInActive, _pirSenseInLast, _inEnabled, 
        //     digitalRead(_pirSenseOutPin), _pirSenseOutActive, _pirSenseOutLast, _outEnabled);
        // if (_pStepper)
        // {
        //     LOG_I(MODULE_PREFIX, "service stepper %s", _pStepper->getDataJSON().c_str());
        // }
    }
}
bool MoistureSensors::isBusy()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String MoistureSensors::getStatusJSON(bool includeBrackets)
{
    // Get JSON
    String jsonStr = R"("moisture": [)";
    for (int i = 0; i < NUM_MOISTURE_SENSORS; i++)
    {
        jsonStr += getMoisturePercentage(i);
        if (i < NUM_MOISTURE_SENSORS - 1)
            jsonStr += ",";
    }
    jsonStr += "]";
    if (includeBrackets)
        jsonStr = "{" + jsonStr + "}";
    return jsonStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get moisture percentage
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t MoistureSensors::getMoisturePercentage(uint8_t sensorIndex)
{
    if (sensorIndex >= NUM_MOISTURE_SENSORS)
        return 0;
    uint32_t curVal = _adcAvgValues[sensorIndex].cur();
    if (curVal < ADC_MIN_VALUE)
        curVal = ADC_MIN_VALUE;
    if (curVal > ADC_MAX_VALUE)
        curVal = ADC_MAX_VALUE;
    return (curVal - ADC_MIN_VALUE) * 100 / (ADC_MAX_VALUE - ADC_MIN_VALUE);
}