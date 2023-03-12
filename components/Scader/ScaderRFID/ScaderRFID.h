/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderDoors
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SysModBase.h>
#include <ScaderCommon.h>
#include <RFIDModuleBase.h>
#include <StatusIndicator.h>

class APISourceInfo;

class ScaderRFID : public SysModBase
{
  public:
    static const int DEFAULT_MAX_ELEMS = 2;
    ScaderRFID(const char *pModuleName, ConfigBase &defaultConfig, 
                ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

protected:

    // Setup
    virtual void setup() override final;

    // Service (called frequently)
    virtual void service() override final;

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
    int _actLedPin = -1;
    uint32_t _actLedLastMs = 0;
    bool _actLedState = false;

    // Tag LED pin
    int _tagLedPin = -1;
    
    // RFID module
    RFIDModuleBase* _pRFIDModule = nullptr;

    // Buzzer
    StatusIndicator _buzzer;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    int parseIntList(const String &str, int *pIntList, int maxInts);
    void apiDoorStatusChange(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
