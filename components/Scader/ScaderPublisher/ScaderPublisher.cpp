/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderPublisher
//
// Rob Dobson 2026
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderPublisher.h"
#include "IScaderStatusBody.h"
#include "SysManagerIF.h"
#include "CommsChannelMsg.h"

ScaderPublisher::ScaderPublisher(const char* pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
      _scaderCommon(*this, sysConfig, "Scader")
{
}

void ScaderPublisher::setup()
{
    _scaderCommon.setup();
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }
    _isInitialised = true;
    LOG_I(MODULE_PREFIX, "setup enabled");
}

void ScaderPublisher::postSetup()
{
    if (!_isInitialised)
        return;

    SysManagerIF* pSysManager = getSysManager();
    if (!pSysManager)
    {
        LOG_W(MODULE_PREFIX, "postSetup no SysManager");
        return;
    }

    pSysManager->registerDataSource("Publish", "Scader",
        [this](uint16_t topicIdx, CommsChannelMsg& msg) {
            String statusStr = buildUnifiedJSON();
            msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
            return true;
        },
        [this](uint16_t topicIdx, std::vector<uint8_t>& stateHash) {
            buildUnifiedHash(stateHash);
        }
    );

    LOG_I(MODULE_PREFIX, "postSetup registered unified Scader pub source with %d contributors",
            (int)_contributors.size());
}

void ScaderPublisher::registerContributor(const char* pModuleName, IScaderStatusBody* pBody)
{
    if (!pBody || !pModuleName)
        return;
    _contributors.push_back({String(pModuleName), pBody});
    LOG_I(MODULE_PREFIX, "registerContributor %s", pModuleName);
}

String ScaderPublisher::getStatusJSON() const
{
    return "{" + _scaderCommon.getStatusJSON() + "}";
}

String ScaderPublisher::buildUnifiedJSON() const
{
    String modulesStr;
    bool first = true;
    for (const Contributor& c : _contributors)
    {
        if (!c.pBody || !c.pBody->isScaderEnabled())
            continue;
        if (!first)
            modulesStr += ",";
        first = false;
        modulesStr += "\"";
        modulesStr += c.name;
        modulesStr += "\":{";
        modulesStr += c.pBody->getStatusBodyJSON();
        modulesStr += "}";
    }
    return "{" + _scaderCommon.getStatusJSON() + ",\"modules\":{" + modulesStr + "}}";
}

void ScaderPublisher::buildUnifiedHash(std::vector<uint8_t>& stateHash) const
{
    stateHash.clear();
    for (const Contributor& c : _contributors)
    {
        if (!c.pBody || !c.pBody->isScaderEnabled())
            continue;
        c.pBody->appendStatusBodyHash(stateHash);
    }
}
