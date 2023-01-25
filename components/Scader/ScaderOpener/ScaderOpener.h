/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderOpener
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include <ConfigBase.h>
#include <ScaderCommon.h>
#include <RaftUtils.h>
#include <SysModBase.h>
#include <DoorOpener.h>

class APISourceInfo;

class ScaderOpener : public SysModBase
{
public:
    ScaderOpener(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);

    // Check if moving
    bool isBusy();

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

    // Initalised flag
    bool _isInitialised = false;

    // Opener hardware
    DoorOpener _doorOpener;

    // Mutable data saving
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _mutableDataChangeLastMs = 0;
    
    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void getStatusHash(std::vector<uint8_t>& stateHash);
    void saveMutableData();
};
