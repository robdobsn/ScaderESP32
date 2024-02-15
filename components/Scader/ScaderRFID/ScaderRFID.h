/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderDoors
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftUtils.h"
#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "RFIDModuleBase.h"
#include "StatusIndicator.h"

class APISourceInfo;

class ScaderRFID : public RaftSysMod
{
  public:
    static const int DEFAULT_MAX_ELEMS = 2;
    ScaderRFID(const char *pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderRFID(pModuleName, sysConfig);
    }
    
protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Status
    virtual String getStatusJSON() override final;
    
private:
    // Common
    ScaderCommon _scaderCommon;

    // Initialised flag
    bool _isInitialised = false;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // ACT LED pin
    StatusIndicator _actLed;

    // Tag LED pin
    int _tagLedPin = -1;
    
    // RFID module
    RFIDModuleBase* _pRFIDModule = nullptr;
    uint32_t _rfidStatusLastMs = 0;
    static const uint32_t RFID_STATUS_CHECK_MS = 200;

    // Buzzer
    StatusIndicator _buzzer;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    RaftRetCode apiDoorStatusChange(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
