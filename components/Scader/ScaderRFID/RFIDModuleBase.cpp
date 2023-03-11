/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RFID Module Base
//
// Rob Dobson 2013-2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <RFIDModuleBase.h>
#include <RaftUtils.h>

static const char* MODULE_PREFIX = "RfidBase";

RFIDModuleBase::RFIDModuleBase(int resetPin, bool resetActiveLevel)
{
    // Pins
    _rfidModuleResetPin = resetPin;
    _rfidModuleResetActive = resetActiveLevel;

    // Module reset pin
    if (_rfidModuleResetPin >= 0)
    {
        digitalWrite(_rfidModuleResetPin, _rfidModuleResetActive ? LOW : HIGH);
        pinMode(_rfidModuleResetPin, OUTPUT);
        digitalWrite(_rfidModuleResetPin,  _rfidModuleResetActive ? LOW : HIGH);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "constructor resetPin %d active %s", 
                _rfidModuleResetPin, _rfidModuleResetActive ? "Y" : "N");

}

void RFIDModuleBase::getTag(String &currentTag, bool &tagPresent, bool& changeOfState, 
                uint32_t& timeTagPresentedMs)
{
    currentTag = _curTagRead;
    timeTagPresentedMs = _curTagReadTimeMs;
    tagPresent = _curTagRead.length() > 0;
    changeOfState = _curTagRead != _lastTagReturned;
    _lastTagReturned = _curTagRead;
}

bool RFIDModuleBase::getPinCode(String& pinCode)
{
    if (_curPinCode.length() == 0)
        return false;
    // Log.trace("Pincode entered %s ms %d\n", _curPinCode.c_str(), _curPinCodeMs);
    if (Raft::isTimeout(millis(), _curPinCodeMs, MAX_PIN_CODE_AGE_MS))
    {
        _curPinCode = "";
        return false;
    }
    pinCode = _curPinCode;
    _curPinCode = "";
    return true;
}

void RFIDModuleBase::requestReset()
{
}

bool RFIDModuleBase::isModulePresent()
{
    return false;
}

void RFIDModuleBase::injectTag(String& tag)
{
    tagNowPresent(tag);
}

void RFIDModuleBase::service()
{
}

void RFIDModuleBase::rfidModuleReset()
{
    // Reset count
    _rfidModuleResetCount++;
    LOG_I(MODULE_PREFIX, "Resetting RFID module %d", _rfidModuleResetCount);

    // Set pin low then high
    if (_rfidModuleResetPin != -1)
    {
        digitalWrite(_rfidModuleResetPin, _rfidModuleResetActive ? HIGH : LOW);
        delay(10);
        digitalWrite(_rfidModuleResetPin, _rfidModuleResetActive ? LOW : HIGH);
    }
}

// No tag
void RFIDModuleBase::tagNotPresent()
{
    _curTagRead = "";
    _curTagReadTimeMs = 0;
}

// Tag present
void RFIDModuleBase::tagNowPresent(String& tagStr)
{
    // Check if tag already present
    if (_curTagReadTimeMs == 0) {
        _curTagReadTimeMs = millis();
    }
    _curTagRead = tagStr;
}

// Pin code entered
void RFIDModuleBase::pinCodeEntered(String& pinCode)
{
    _curPinCode = pinCode;
    _curPinCodeMs = millis();
}
