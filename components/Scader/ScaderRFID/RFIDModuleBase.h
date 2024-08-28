/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RFID Module Base
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "Logger.h"

class RFIDModuleBase
{
public:
    RFIDModuleBase(int resetPin, bool resetActiveLevel);
    virtual void getTag(String &currentTag, bool &tagPresent, bool& changeOfState, uint32_t& timeTagPresentedMs);
    virtual bool getPinCode(String& pinCode);
    virtual void requestReset();
    virtual bool isModulePresent();
    virtual void injectTag(String& tag);
    virtual void loop();

protected:
    // Tag read
    String _curTagRead;
    String _lastTagReturned;
    uint32_t _curTagReadTimeMs = 0;

    // PIN code
    String _curPinCode;
    uint32_t _curPinCodeMs = 0;
    static const uint32_t MAX_PIN_CODE_AGE_MS = 10000;

    // RFID module reset
    int _rfidModuleResetCount = 0;
    int _rfidModuleResetPin = -1;
    bool _rfidModuleResetActive = LOW;

    // Helpers
    virtual void rfidModuleReset();
    void tagNotPresent();
    void tagNowPresent(String& tagStr);
    void pinCodeEntered(String& pinCode);
};
