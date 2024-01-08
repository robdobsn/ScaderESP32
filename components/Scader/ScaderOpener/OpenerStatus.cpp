/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// OpenerStatus.cpp
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "OpenerStatus.h"
#include "RaftUtils.h"
#include "SysModBase.h"

static const char* MODULE_PREFIX = "OpenerStatus";
#define DEBUG_OPENER_MUTABLE_DATA
#define DEBUG_OPENER_STATE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpenerStatus::readFromNVS()
{
    // Get mutable data
    if (_pConfig)
    {
        _inEnabled = _pConfig->getBool("openerState/inEn", false);
        _outEnabled = _pConfig->getBool("openerState/outEn", false);
    }

    // Log
#ifdef DEBUG_OPENER_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "setup inEn %d outEn %d", _inEnabled, _outEnabled);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Save to non-volatile-storage (NVS) if required
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpenerStatus::saveToNVSIfRequired()
{
    // Check if mutable data changed
    if (!_isNVSWriteReqd)
        return;

    // Check if min time has passed
    if (!Raft::isTimeout(millis(), _nvsDataLastChangedMs, MUTABLE_DATA_SAVE_MIN_MS))
        return;

    // Form JSON
    String jsonConfig = R"("openerState":{"inEn":__INEN__,"outEn":__OUTEN__})";
    jsonConfig.replace("__INEN__", String(_inEnabled));
    jsonConfig.replace("__OUTEN__", String(_outEnabled));

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";

    // Log
#ifdef DEBUG_OPENER_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
#endif

    // Save
    if (_pConfig)
    {
        _pConfig->setJsonDoc(jsonConfig.c_str());
    }

    // No longer required
    _isNVSWriteReqd = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set opener state
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpenerStatus::setOpenerState(DoorOpenerState newState, const String& debugMsg)
{
    // Store prev state for debug
#ifdef DEBUG_OPENER_STATE
    DoorOpenerState prevState = _doorOpenerState;
#endif

    // Store new state
    _doorOpenerState = newState;
    _doorOpenerStateLastMs = millis();

    // Debug
#ifdef DEBUG_OPENER_STATE
    LOG_I(MODULE_PREFIX, "setOpenerState %s (was %s) %s", 
                getOpenerStateStr(newState).c_str(), 
                getOpenerStateStr(prevState).c_str(), 
                debugMsg.c_str());
#endif
}