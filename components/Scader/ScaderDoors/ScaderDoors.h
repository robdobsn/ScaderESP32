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
#include <DoorStrike.h>

class APISourceInfo;

class ScaderDoors : public SysModBase
{
  public:
    static const int DEFAULT_MAX_ELEMS = 2;
    ScaderDoors(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

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
    // Enabled and initalised flags
    bool _isEnabled = false;
    bool _isInitialised = false;

    // Settings
    uint32_t _maxElems = DEFAULT_MAX_ELEMS;

    // Door strike/sense pins
    int _strikeControlPins[DEFAULT_MAX_ELEMS] = { -1, -1 };
    bool _strikePinUnlockLevel[DEFAULT_MAX_ELEMS] = { false, false };
    uint32_t _unlockForSecs[DEFAULT_MAX_ELEMS] = { 1, 1 };
    int _openSensePins[DEFAULT_MAX_ELEMS] = { -1, -1 };
    bool _openSensePinLevel[DEFAULT_MAX_ELEMS] = { false, false };
    int _closedSensePins[DEFAULT_MAX_ELEMS] = { -1, -1 };

    // Bell pressed sense pin
    int _bellPressedPin = -1;
    bool _bellPressedPinLevel = false;

    // Master door index
    uint32_t _masterDoorIndex = 0;

    // Name set in Scader UI
    String _scaderFriendlyName;

    // Names of control elements
    std::vector<String> _elemNames;

    // Door strikes
    std::vector<DoorStrike> _doorStrikes;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    bool _mutableDataDirty = false;

    // Helpers
    bool applyCurrentState();

    // Helper functions
    void deinit();
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void saveMutableData();
    void debugShowCurrentState();
    void getStatusHash(std::vector<uint8_t>& stateHash);
    int parseIntList(const String &str, int *pIntList, int maxInts);
};
