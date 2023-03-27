/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Opener Status
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <ArduinoOrAlt.h>
#include <vector>

class OpenerStatus
{
public:

    // Settings
    void setOutEnabled(bool enabled) 
    { 
        if (_outEnabled != enabled)
            _isDirty = true;
        _outEnabled = enabled; 
    }
    void setInEnabled(bool enabled) 
    { 
        if (_inEnabled != enabled)
            _isDirty = true;
        _inEnabled = enabled; 
    }
    void setStatusString(int idx, const String& statusStr)
    {
        if (idx < 0 || idx >= _statusStrs.size())
            return;
        if (_statusStrs[idx] != statusStr)
        {
            _isDirty = true;
            _statusStrs[idx] = statusStr;
        }
    }
    void setOpenCloseStatus(bool isOpen, bool isClosed, bool isMoving) 
    { 
        String label = isOpen ? "Close" : (isClosed ? "Open" : "Stop");
        if (_openCloseBtnLabel != label)
        {
            _isDirty = true;
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

    // Params
    bool _inEnabled = false;
    bool _outEnabled = false;

    // Open/Close button label
    String _openCloseBtnLabel = "Open";

    // Kitchen (out) PIR value
    bool _pirOutValue = false;

    // Dirty flag
    bool _isDirty = false;

    // Status strings
    std::vector<String> _statusStrs = { "Cat Policing", "", "" };
};
