/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Protocol Definition
// Definition for protocol messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <ArduinoOrAlt.h>
#include <ProtocolBase.h>

class ProtocolCodecFactoryHelper
{
public:
    String protocolName;
    ProtocolCreateFnType createFn;
    String paramsJSON;
    CommsChannelMsgCB frameRxCB;
    CommsChannelReadyToRxCB readyToRxCB;
};
