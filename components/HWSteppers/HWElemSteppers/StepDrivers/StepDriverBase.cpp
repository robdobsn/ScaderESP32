/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StepDriverBase
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "StepDriverBase.h"
#include <ArduinoOrAlt.h>
#include <BusBase.h>
#include <BusRequestInfo.h>

static const char* MODULE_PREFIX = "StepDrvBase";

// Warning on CRC error
#define WARN_ON_CRC_ERROR

// Debug
// #define DEBUG_REGISTER_READ_PROCESS
#define DEBUG_REGISTER_READ_VALUE
#define DEBUG_REGISTER_WRITE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StepDriverBase::StepDriverBase()
{
    _serialBusAddress = 0;
    _readBytesRequired = 0;
    _readStartTimeMs = 0;
    _readRegisterIdx = 0;
    _singleWireReadWrite = false;
    _tmcSyncByte = 0;
    _useBusForDirectionReversal = false;
    _hwIsSetup = false;
    _pSerialBus = nullptr;
    _usingISR = true; 
}

StepDriverBase::~StepDriverBase()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StepDriverBase::setup(const String& stepperName, const StepDriverParams& stepperParams, bool usingISR)
{
    // Store config
    _name = stepperName;
    _stepperParams = stepperParams;
    _usingISR = usingISR;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup bus to use for serial comms with driver
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverBase::setupSerialBus(BusBase* pBus, bool useBusForDirectionReversal)
{
    _pSerialBus = pBus;
    _useBusForDirectionReversal = useBusForDirectionReversal;
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverBase::service()
{
    // Check if we are reading
    if (isReadInProgress())
    {
#ifdef DEBUG_REGISTER_READ_PROCESS
            LOG_I(MODULE_PREFIX, "service read axis %s rxAvail %d rdBytesToIgnore %d rdBytesReqd %d", 
                        _name.c_str(), _pSerialBus->rxDataBytesAvailable(), _readBytesToIgnore, _readBytesRequired);
#endif
        // Check for enough data to fulfill read
        if (busValid() && _pSerialBus->rxDataBytesAvailable() >= _readBytesToIgnore + _readBytesRequired)
        {
            // Read the data
            uint32_t reqLen = _readBytesToIgnore + _readBytesRequired;
            uint8_t readData[reqLen];
            if (_pSerialBus->rxDataGet(readData, reqLen) == reqLen)
            {
                // Clear read in progress
                clearReadInProgress();

#ifdef DEBUG_REGISTER_READ_PROCESS
                String debugStr;
                Raft::getHexStrFromBytes(readData, reqLen, debugStr);
                LOG_I(MODULE_PREFIX, "service read axis %s regIdx %d rawread 0x%s", 
                                _name.c_str(), _readRegisterIdx, debugStr.c_str());
#endif

                // Chec register index is valid
                if (_readRegisterIdx < _driverRegisters.size())
                {
                    // Check the CRC
                    uint8_t replyCRC = readData[_readBytesToIgnore + TMC_REPLY_CRC_POS];
                    uint8_t calculatedCRC = calcTrinamicsCRC(readData + _readBytesToIgnore, TMC_REPLY_DATAGRAM_LEN-1);
                    if (replyCRC != calculatedCRC)
                    {
#ifdef WARN_ON_CRC_ERROR
                        LOG_W(MODULE_PREFIX, "service read CRC error 0x%02x 0x%02x axis %s stepperAddr 0x%02x regIdx %d regAddr 0x%02x", 
                                    replyCRC, 
                                    calculatedCRC,
                                    _name.c_str(),
                                    _stepperParams.address,
                                    _readRegisterIdx, 
                                    _driverRegisters[_readRegisterIdx].regAddr);
#endif
                    }
                    else
                    {
                        // Data pointer
                        const uint8_t* pData = readData + _readBytesToIgnore + TMC_REPLY_DATA_POS;
                        _driverRegisters[_readRegisterIdx].regValCur = Raft::getBEUint32AndInc(pData);

#ifdef DEBUG_REGISTER_READ_VALUE
                        LOG_I(MODULE_PREFIX, "service read axis %s reg %s(0x%02x) data 0x%08x", 
                                    _name.c_str(),
                                    _driverRegisters[_readRegisterIdx].regName.c_str(),
                                    _driverRegisters[_readRegisterIdx].regAddr,
                                    _driverRegisters[_readRegisterIdx].regValCur);
#endif
                    }
                }
            }
        }
    }

    // Check timeout
    if (isReadInProgress() && Raft::isTimeout(millis(), _readStartTimeMs, READ_TIMEOUT_MS))
    {
#ifdef DEBUG_READ
        LOG_I(MODULE_PREFIX, "service name %s read timed out", _name.c_str());
#endif
        clearReadInProgress();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check busy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StepDriverBase::driverBusy()
{
    if (isReadInProgress())
        return true;
    if (busValid() && !_pSerialBus->isReady())
        return true;
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set microsteps
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverBase::setMicrosteps(uint32_t microsteps)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write register in Trinamics driver
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverBase::writeTrinamicsRegister(const char* pRegName, uint8_t regAddr, uint32_t data)
{
    // Check valid
    if (!busValid() || driverBusy())
        return;

    // Form datagram
    uint8_t datagram[] = {
        _tmcSyncByte,
        _stepperParams.address,
        uint8_t(regAddr | 0x80u),
        uint8_t((data >> 24) & 0xff),
        uint8_t((data >> 16) & 0xff),
        uint8_t((data >> 8) & 0xff),
        uint8_t(data & 0xff),
        0   // This is where the CRC will go
    };
    datagram[sizeof(datagram)-1] = calcTrinamicsCRC(datagram, sizeof(datagram)-1);

    // Form the command
    BusRequestInfo reqInfo(_name, _stepperParams.address, datagram, sizeof(datagram));

    // Send request
    _pSerialBus->addRequest(reqInfo);

#ifdef DEBUG_REGISTER_WRITE
    String datagramStr;
    Raft::getHexStrFromBytes(datagram, sizeof(datagram), datagramStr);
    LOG_I(MODULE_PREFIX, "writeTrinamicsRegister axis %s reg %s(0x%02x) value 0x%08x datagram %s", 
                    _name.c_str(), pRegName, regAddr, data, datagramStr.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start a read from Trinamics register
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepDriverBase::startReadTrinamicsRegister(uint32_t readRegisterIdx)
{
    // Check valid
    if (!busValid() || driverBusy())
    {
        LOG_W(MODULE_PREFIX, "startReadTrinamicsRegister name %s readRegisterIdx %d failed busValid %d busy %d",
                    _name.c_str(), readRegisterIdx, busValid(), driverBusy());
        return;
    }
    if (readRegisterIdx >= _driverRegisters.size())
    {
        LOG_W(MODULE_PREFIX, "startReadTrinamicsRegister name %s readRegisterIdx %d failed out of range",
                    _name.c_str(), readRegisterIdx);
        return;
    }

    // Form datagram
    uint8_t datagram[] = {
        _tmcSyncByte,
        _stepperParams.address,
        _driverRegisters[readRegisterIdx].regAddr,
        0   // This is where the CRC will go
    };
    datagram[sizeof(datagram)-1] = calcTrinamicsCRC(datagram, sizeof(datagram)-1);

    // Form the command
    BusRequestInfo reqInfo(_name, _stepperParams.address, datagram, sizeof(datagram));

    // Clear any currently read data
    _pSerialBus->rxDataClear();

    // Send request
    _pSerialBus->addRequest(reqInfo);

    // Bytes required to be read
    _readBytesToIgnore = _singleWireReadWrite ? sizeof(datagram) : 0;
    _readBytesRequired = TMC_REPLY_DATAGRAM_LEN;
    _readRegisterIdx = readRegisterIdx;
    _readStartTimeMs = millis();
    _driverRegisters[readRegisterIdx].readInProgress = true;

#ifdef DEBUG_READ
        LOG_I(MODULE_PREFIX, "startReadTrinamicsRegister name %s regIdx %d regAddr %d", 
                    _name.c_str(),
                    _readRegisterIdx, 
                    _driverRegisters[readRegisterIdx].regAddr);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate trinamics CRC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t StepDriverBase::calcTrinamicsCRC(const uint8_t* pData, uint32_t len)
{
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t currentByte = pData[i];
        for (uint32_t j = 0; j < 8; j++)
        {
            // update CRC based result of XOR operation
            if ((crc >> 7) ^ (currentByte & 0x01)) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = (crc << 1);
            }
            crc &= 0xff;
            currentByte = currentByte >> 1;
        }
    }
    return crc;
}
