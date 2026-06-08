/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderPublisher — emits one unified MQTT message per cycle on the "Scader" pubTopic, aggregating
// the bodies of every Scader SysMod that has called registerContributor(). Replaces per-module
// pubSources on the scader/out topic; per-module Publish registrations remain registered (but with
// no subscribers) for graceful fallback.
//
// Rob Dobson 2026
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "RaftSysMod.h"
#include "ScaderCommon.h"

class IScaderStatusBody;

class ScaderPublisher : public RaftSysMod
{
public:
    ScaderPublisher(const char* pModuleName, RaftJsonIF& sysConfig);

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new ScaderPublisher(pModuleName, sysConfig);
    }

    // Called from each contributing Scader module's setup() to enrol it in the unified message.
    void registerContributor(const char* pModuleName, IScaderStatusBody* pBody);

protected:
    virtual void setup() override final;
    virtual void postSetup() override final;
    virtual String getStatusJSON() const override final;

private:
    ScaderCommon _scaderCommon;
    bool _isInitialised = false;

    struct Contributor
    {
        String name;
        IScaderStatusBody* pBody;
    };
    std::vector<Contributor> _contributors;

    String buildUnifiedJSON() const;
    void buildUnifiedHash(std::vector<uint8_t>& stateHash) const;

    // Debug
    static constexpr const char* MODULE_PREFIX = "ScaderPublisher";
};
