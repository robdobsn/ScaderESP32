/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Opener Status
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "RaftArduino.h"
#include "RaftJsonNVS.h"

class OpenerStatus
{
public:

    // Constructor
    OpenerStatus(RaftJsonNVS& nvsData) : 
        _scaderModuleState(nvsData)
    {
    }
    // Settings
    void setOutEnabled(bool enabled) 
    { 
        if (_outEnabled != enabled)
        {
            _isUIUpdateReqd = true;
            _isNVSWriteReqd = true;
            _nvsDataLastChangedMs = millis();
        }
        _outEnabled = enabled; 
    }
    void setInEnabled(bool enabled) 
    { 
        if (_inEnabled != enabled)
        {
            _isUIUpdateReqd = true;
            _isNVSWriteReqd = true;
            _nvsDataLastChangedMs = millis();
        }
        _inEnabled = enabled; 
    }
    void setKitchenPIRActive(bool isActive) 
    { 
        if (_kitchenPIRValue != isActive)
        {
            _kitchenPIRStateChange = true;
            _kitchenPIRValue = isActive;
        }
    }
    void setOpenCloseToggle(bool isActive)
    {
        if (_openCloseToggleValue != isActive)
        {
            _openCloseToggleStateChange = true;
            _openCloseToggleValue = isActive;
        }
    }
    void uiModuleSetStatusStr(int idx, const String& statusStr)
    {
        if (idx < 0 || idx >= _statusStrs.size())
            return;
        if (_statusStrs[idx] != statusStr)
        {
            _isUIUpdateReqd = true;
            _statusStrs[idx] = statusStr;
        }
    }
    void uiModuleSetOpenCloseButtonLabel(const String& buttonLabel)
    { 
        if (_openCloseBtnLabel != buttonLabel)
        {
            _isUIUpdateReqd = true;
            _openCloseBtnLabel = buttonLabel;
        }
    }

    // Get JSON
    String getJson()
    {
        String json = "{";
        json += "\"inEnabled\":" + String(_inEnabled ? "true" : "false") + ",";
        json += "\"outEnabled\":" + String(_outEnabled ? "true" : "false") + ",";
        json += "\"statusStr1\":\"" + _statusStrs[0] + "\",";
        json += "\"statusStr2\":\"" + _statusStrs[1] + "\",";
        json += "\"statusStr3\":\"" + _statusStrs[2] + "\",";
        json += "\"openCloseLabel\":\"" + _openCloseBtnLabel + "\"";
        json += "}";
        return json;
    }

    // Check if UI update required
    bool uiUpdateRequired() { return _isUIUpdateReqd; }
    void uiUpdateDone() { _isUIUpdateReqd = false; }

    // Check if kitchen PIR state changed
    bool getKitchenPIRStateChangedAndClear(bool& kitchenPIRValue) {
        if (!_kitchenPIRStateChange)
            return false;
        kitchenPIRValue = _kitchenPIRValue;
        _kitchenPIRStateChange = false;
        return true;
    }

    // Check if open/close toggle state changed
    bool getOpenCloseToggleStateChangedAndClear(bool& openCloseToggleValue) {
        if (!_openCloseToggleStateChange)
            return false;
        openCloseToggleValue = _openCloseToggleValue;
        _openCloseToggleStateChange = false;
        return true;
    }

    // Read/save non-volatile-storage (NVS)
    void readFromNVS();
    void saveToNVSIfRequired();

    // Door opener state
    enum DoorOpenerState
    {
        DOOR_STATE_AJAR,
        DOOR_STATE_CLOSED,
        DOOR_STATE_OPENING,
        DOOR_STATE_OPEN,
        DOOR_STATE_CLOSING,
    };

    String getOpenerStateStr(DoorOpenerState doorState) const
    {
        switch (doorState)
        {
            case DOOR_STATE_AJAR:
                return "Ajar";
            case DOOR_STATE_CLOSED:
                return "Closed";
            case DOOR_STATE_OPENING:
                return "Opening";
            case DOOR_STATE_OPEN:
                return "Open";
            case DOOR_STATE_CLOSING:
                return "Closing";
        }
        return "Unknown";
    }

    // Get/Set opener state
    DoorOpenerState getOpenerState() const { return _doorOpenerState; }
    void setOpenerState(DoorOpenerState newState, const String& debugMsg);
    uint32_t getOpenerStateLastMs() const { return _doorOpenerStateLastMs; }

protected:

    // Params
    bool _inEnabled = false;
    bool _outEnabled = false;

    // Kitchen (out) PIR value
    bool _kitchenPIRValue = false;
    bool _kitchenPIRStateChange = false;

    // Open close toggle value
    bool _openCloseToggleValue = false;
    bool _openCloseToggleStateChange = false;

private:

    // State
    DoorOpenerState _doorOpenerState = DOOR_STATE_AJAR;
    uint32_t _doorOpenerStateLastMs = 0;

    // Open/Close button label
    String _openCloseBtnLabel = "Open";

    // UI module requires update
    bool _isUIUpdateReqd = false;

    // Non-volatile-storage (NVS) requires write
    bool _isNVSWriteReqd = false;
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _nvsDataLastChangedMs = 0;

    // Status strings
    std::vector<String> _statusStrs = { "", "", "" };

    // Opener state NVS
    RaftJsonNVS& _scaderModuleState;
};
