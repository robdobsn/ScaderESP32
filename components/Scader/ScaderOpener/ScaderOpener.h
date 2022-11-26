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

private:

    // Enabled and initalised flags
    bool _isEnabled;
    bool _isInitialised;

    // Opener hardware
    DoorOpener _doorOpener;

    void apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
};
