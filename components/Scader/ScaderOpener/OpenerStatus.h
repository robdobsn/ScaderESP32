/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Opener Status
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <ArduinoOrAlt.h>
#include <ConfigBase.h>
#include <vector>

class OpenerStatus
{
public:

    // Setup
    void recordConfig(ConfigBase& config)
    {
        _pConfig = &config;
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
    void uiModuleSetOpenStatus(bool isClosed, bool isMoving) 
    { 
        String label = isMoving ? "Stop" : (isClosed ? "Open" : "Close");
        if (_openCloseBtnLabel != label)
        {
            _isUIUpdateReqd = true;
            _openCloseBtnLabel = label;
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

    // Read/save non-volatile-storage (NVS)
    void readFromNVS();
    void saveToNVSIfRequired();

protected:

    // Params
    bool _inEnabled = false;
    bool _outEnabled = false;

    // Kitchen (out) PIR value
    bool _pirOutValue = false;

private:

    // Open/Close button label
    String _openCloseBtnLabel = "Open";

    // UI module requires update
    bool _isUIUpdateReqd = false;

    // Non-volatile-storage (NVS) requires write
    bool _isNVSWriteReqd = false;
    static const uint32_t MUTABLE_DATA_SAVE_MIN_MS = 5000;
    uint32_t _nvsDataLastChangedMs = 0;

    // Status strings
    std::vector<String> _statusStrs = { "Cat Policing", "", "" };

    // Config
    ConfigBase* _pConfig = nullptr;
};
