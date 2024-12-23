/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderBTHome
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "ScaderCommon.h"
#include "DevicePollRecords_generated.h"
#include "DeviceTypeRecords.h"

class APISourceInfo;

class ScaderBTHome : public RaftSysMod
{
public:
    static const int DEFAULT_MAX_ELEMS = 24;
    // static const int ELEMS_PER_CHIP = 8;
    // static const int SPI_MAX_CHIPS = DEFAULT_MAX_ELEMS/ELEMS_PER_CHIP;
    ScaderBTHome(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderBTHome(pModuleName, sysConfig);
    }

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Status
    virtual String getStatusJSON() const override final;
    
private:

    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // BTHomeDevice
    class BTHomeUpdate
    {
    public:
        std::vector<uint8_t> msgData;
        uint32_t timestampMs = 0;
    };
    mutable ThreadSafeQueue<BTHomeUpdate> _bthomeUpdateQueue;

    // Decode function for BTHome device data
    DeviceTypeRecordDecodeFn _pDecodeFn = nullptr;
    mutable RaftBusDeviceDecodeState _decodeState;

    // Helper
    void getStatusHash(std::vector<uint8_t>& stateHash);
};
