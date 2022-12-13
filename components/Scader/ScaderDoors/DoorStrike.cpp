/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoorStrike
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArduinoOrAlt.h>
#include "DoorStrike.h"
#include <RaftUtils.h>

static const char* MODULE_PREFIX = "DoorStrike";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service to handle timeouts, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorStrike::service()
{
    // Handle re-locking door if unlocked
    if (!_garageMode)
    {
        if (!_isLocked)
        {
            // Check for re-lock conditions
            if ((getOpenStatus() == DoorOpenSense::Open) && Raft::isTimeout(millis(), _unlockedTime, MIN_TIMEOUT_AFTER_UNLOCK_MS))
            {
                // Check if door is open and unlock occurred more than a minimum time ago
                lock();
            }
            else if (Raft::isTimeout(millis(), _unlockedTime, _mTimeOutOnUnlockMs))
            {
                // Door unlocked max time
                lock();
            }
        }
    }
    else
    {
        if (!_isLocked && Raft::isTimeout(millis(), _unlockedTime, _mTimeOutOnUnlockMs))
            lock();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Unlock / lock / isLocked
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DoorStrike::unlockWithTimeout(const char* unlockCause, int timeoutInSecs)
{
    // Allow door to open
    digitalWrite(_doorStrikePin, _doorStrikeOn);
    _isLocked = false;
    // Store timeout
    _mTimeOutOnUnlockMs = (timeoutInSecs == -1 ? _defaultUnlockMs : timeoutInSecs * 1000);
    // Set timer to time how long to leave door unlocked
    _unlockedTime = millis();
    LOG_I(MODULE_PREFIX, "%s%s %s pin %d, timeoutms %d",
                (_garageMode ? "Closing door toggle relay" : "Unlocking door"),
                unlockCause,
                _doorStrikePin, _mTimeOutOnUnlockMs);
    return true;
}

bool DoorStrike::lock()
{
    // Lock door
    digitalWrite(_doorStrikePin, !_doorStrikeOn);
    // Indicate now locked
    _isLocked = true;
    LOG_I(MODULE_PREFIX, "%s%s pin %d", (_garageMode ? "Opening relay" : "Locking door"), _doorStrikePin);
    return true;
}

// Check if locked
bool DoorStrike::isLocked()
{
    return _isLocked;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if the door is open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DoorStrike::DoorOpenSense DoorStrike::getOpenStatus() const
{
    // LOG_I(MODULE_PREFIX, "DoorStrike: sensePin %d", _doorOpenSensePin);
    if (_doorOpenSensePin == -1)
        return DoorOpenSense::DoorSenseUnknown;

    bool doorOpen = digitalRead(_doorOpenSensePin) == _senseWhenOpen;
    bool doorClosed = false;
    if (_garageMode && _doorClosedSensePin) 
        doorClosed = (digitalRead(_doorClosedSensePin) == _senseWhenOpen);

    // LOG_I(MODULE_PREFIX, "DoorStrike: doorOpen %d doorClosed %d garageMode %d", doorOpen, doorClosed, _garageMode);
    if (doorOpen)
        return DoorOpenSense::Open;
    if (_garageMode && !doorClosed)
        return DoorOpenSense::DoorSenseUnknown;
    return DoorOpenSense::Closed;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get all status info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DoorStrike::getStatus(DoorLockedEnum& lockedEnum, DoorOpenSense& openSense, int& timeBeforeRelock) const
{
    openSense = getOpenStatus();
    if (_garageMode)
    {
        if (openSense == DoorOpenSense::DoorSenseUnknown)
            lockedEnum = DoorLockedEnum::LockStateUnknown;
        else if (openSense == DoorOpenSense::Open)
            lockedEnum = DoorLockedEnum::Unlocked;
        else
            lockedEnum = DoorLockedEnum::Locked;
    }
    else
    {
        lockedEnum = _isLocked ? DoorLockedEnum::Locked : DoorLockedEnum::Unlocked;
    }
    timeBeforeRelock = Raft::timeToTimeout(millis(), _unlockedTime, _mTimeOutOnUnlockMs);
    return true;
}

void DoorStrike::getStatusHash(std::vector<uint8_t>& stateHash) const
{
    DoorLockedEnum lockedEnum;
    DoorOpenSense openSense;
    int timeBeforeRelock;
    getStatus(lockedEnum, openSense, timeBeforeRelock);
    stateHash.push_back(lockedEnum + (openSense << 4));
}

String DoorStrike::getStatusJson(bool incBraces) const
{
    DoorLockedEnum lockedEnum = DoorLockedEnum::LockStateUnknown;
    DoorOpenSense openEnum = DoorOpenSense::DoorSenseUnknown;
    int timeBeforeLockMs = 0;
    getStatus(lockedEnum, openEnum, timeBeforeLockMs);
    char statusStr[200];
    snprintf(statusStr, sizeof(statusStr), R"(%s"locked":%s,"open":"%s","toLockMs":%d%s)",
                        incBraces ? "{" : "", 
                        getDoorLockedStr(lockedEnum), 
                        getDoorOpenSenseStr(openEnum),
                        timeBeforeLockMs,
                        incBraces ? "}" : "");
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State strings
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* DoorStrike::getDoorOpenSenseStr(DoorStrike::DoorOpenSense& openSense)
{
    static const char* doorOpenStr = "Y";
    static const char* doorClosedStr = "N";
    static const char* doorUnknownStr = "K";

    switch(openSense)
    {
        case Open: return doorOpenStr;
        case Closed: return doorClosedStr;
        default: return doorUnknownStr;
    }
    return doorUnknownStr;
}

// Door locked state string
const char* DoorStrike::getDoorLockedStr(DoorStrike::DoorLockedEnum& lockedEnum)
{
    static const char* doorLockedStr = "Y";
    static const char* doorUnlockedStr = "N";
    static const char* doorUnknownStr = "K";

    switch(lockedEnum)
    {
        case Locked: return doorLockedStr;
        case Unlocked: return doorUnlockedStr;
        default: return doorUnknownStr;
    }
    return doorUnknownStr;
}
