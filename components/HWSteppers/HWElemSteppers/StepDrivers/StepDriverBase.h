/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StepDriverBase
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdint.h"
#include "RaftUtils.h"
#include "StepDriverParams.h"

class BusBase;

class StepDriverBase
{
public:
    StepDriverBase();
    virtual ~StepDriverBase();

    // Setup
    virtual bool setup(const String& stepperName, const StepDriverParams& stepperParams, bool usingISR);

    // Called after bus has been connected
    virtual void setupSerialBus(BusBase* pBus, bool useBusForDirectionReversal);

    // Service - called frequently
    virtual void service();

    // Set microsteps
    virtual void setMicrosteps(uint32_t microsteps);

    // Set direction
    virtual void IRAM_ATTR setDirection(bool dirn, bool forceSet = false) = 0;

    // Start and end a single step
    virtual void IRAM_ATTR stepStart() = 0;
    virtual bool IRAM_ATTR stepEnd() = 0;

    virtual uint32_t getSerialAddress()
    {
        return _serialBusAddress;
    }

    virtual String getDriverType()
    {
        return "None";
    }

    virtual void setMaxMotorCurrentAmps(float maxMotorCurrentAmps)
    {
    }

protected:
    class DriverRegisterMap
    {
    public:
        DriverRegisterMap(const char* pRegName, uint8_t addr, uint32_t initVal)
        {
            regName = pRegName;
            regAddr = addr;
            regValCur = initVal;
            regWriteVal = initVal;
            writeBitsMask = 0;
            writeOrValue = 0;
            writeRequired = false;
            readPending = false;
            readInProgress = false;
            writePending = false;
        }
        String regName;
        uint8_t regAddr;
        uint32_t regValCur;
        uint32_t regWriteVal;
        uint32_t writeBitsMask;
        uint32_t writeOrValue;
        bool writeRequired : 1;
        bool readPending : 1;
        bool readInProgress : 1;
        bool writePending : 1;
    };    

    // Write register in Trinamics driver
    void writeTrinamicsRegister(const char* pRegName, uint8_t regAddr, uint32_t data);

    // Start a read from Trinamics register
    void startReadTrinamicsRegister(uint32_t readRegisterIdx);

    // Calculate Trinamics CRC
    uint8_t calcTrinamicsCRC(const uint8_t* pData, uint32_t len);

    // Bus valid
    bool busValid()
    {
        return _pSerialBus != nullptr;
    }

    // Driver busy
    bool driverBusy();

    // Is read in progress
    bool isReadInProgress()
    {
        if (!busValid())
            return false;
        if (_readRegisterIdx >= _driverRegisters.size())
            return false;
        return _driverRegisters[_readRegisterIdx].readInProgress;
    }

    // Clear read in progress
    void clearReadInProgress()
    {
        if (_readRegisterIdx >= _driverRegisters.size())
            return;
        _driverRegisters[_readRegisterIdx].readInProgress = false;
    }

    // Set bits in a register
    void setRegBits(uint32_t regCode, uint32_t bitMaskForChanges, uint32_t bitMaskToSet)
    {
        // Check valid
        if (regCode >= _driverRegisters.size())
            return;

        // Clear bit masks if no write pending
        if (!_driverRegisters[regCode].writePending)
        {
            _driverRegisters[regCode].writeBitsMask = 0;
            _driverRegisters[regCode].writeOrValue = 0;
        }

        // Merge any previous bit changes
        // The ORED values need to combine the previous values (where not overridden) with new
        uint32_t orValue = _driverRegisters[regCode].writeOrValue & ~bitMaskForChanges;
        _driverRegisters[regCode].writeOrValue = orValue | bitMaskToSet;
        _driverRegisters[regCode].writeBitsMask |= bitMaskForChanges;
        _driverRegisters[regCode].writePending = true;
    }

    // Bus used for communication with driver
    BusBase* _pSerialBus;
    uint8_t _serialBusAddress;
    String _name;
    bool _useBusForDirectionReversal;

    // Stepping parameters
    StepDriverParams _stepperParams;

    // Hardware has been initialised
    bool _hwIsSetup;

    // Single wire used for read and write (e.g. on TMC2209 UART)
    bool _singleWireReadWrite;

    // Driver registers
    std::vector<DriverRegisterMap> _driverRegisters;

    // In the case of single-wire read/write some data must be ignored
    uint32_t _readBytesToIgnore;

    // Read operation info
    uint32_t _readBytesRequired;
    uint32_t _readStartTimeMs;
    uint32_t _readRegisterIdx;

    // Sync byte for specific chip
    uint8_t _tmcSyncByte;

    // Using ISR - so avoid logging, etc
    bool _usingISR;
    
    // Consts
    static const uint32_t READ_TIMEOUT_MS = 4;
    static const uint32_t TMC_REPLY_DATAGRAM_LEN = 8;
    static const uint32_t TMC_REPLY_DATA_POS = 3;
    static const uint32_t TMC_REPLY_DATA_LEN = 4;
    static const uint32_t TMC_REPLY_CRC_POS = 7;
};
