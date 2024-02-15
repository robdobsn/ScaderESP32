/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderDoors
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftUtils.h"
#include "RaftJsonIF.h"
#include "RaftSysMod.h"
#include "ScaderCommon.h"
#include "DoorStrike.h"
#include "ThreadSafeQueue.h"

class APISourceInfo;

class ScaderDoors : public RaftSysMod
{
  public:
    static const int DEFAULT_MAX_ELEMS = 2;
    ScaderDoors(const char *pModuleName, RaftJsonIF& sysConfig);
    virtual ~ScaderDoors();

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderDoors(pModuleName, sysConfig);
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

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // Bell pressed sense pin
    int _bellPressedPin = -1;
    bool _bellPressedPinLevel = false;

    // Master door index
    uint32_t _masterDoorIndex = 0;
    bool _reportMasterDoorOnly = false;

    // Names of control elements
    std::vector<String> _elemNames;

    // Door strikes
    std::vector<DoorStrike> _doorStrikes;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // List of RFID tags read
    ThreadSafeQueue<String> _tagReadQueue;

    // State change reporting
    static const uint32_t STATE_CHANGE_MIN_MS = 1000;
    uint32_t _lockedStateChangeTestLastMs = 0;
    bool _lockedStateChangeLastLocked = false;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    RaftRetCode apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    RaftRetCode apiTagRead(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    uint32_t executeUnlockLock(std::vector<int> elemNums, bool unlock);
    void publishStateChangeToCommandSerial();
};
