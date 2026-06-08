/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IScaderStatusBody — interface implemented by Scader SysMods that contribute to the unified
// per-device "Scader" publish message. Lets ScaderPublisher pull each module's body fragment
// (status JSON without the device envelope) and combined hash without coupling to concrete types.
//
// Rob Dobson 2026
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "RaftArduino.h"

class IScaderStatusBody
{
public:
    virtual ~IScaderStatusBody() = default;

    // Return the module's status body fragment with no enclosing braces and no leading comma.
    // Empty string is allowed for modules that have no body fields beyond the device envelope.
    virtual String getStatusBodyJSON() const = 0;

    // Append this module's contribution to the combined state-change hash.
    virtual void appendStatusBodyHash(std::vector<uint8_t>& stateHash) = 0;

    // Whether this module is enabled (mirrors ScaderCommon::isEnabled()).
    virtual bool isScaderEnabled() const = 0;
};
