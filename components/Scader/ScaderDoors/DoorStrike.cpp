/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoorStrike
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RaftArduino.h>
#include "DoorStrike.h"
#include <RaftUtils.h>

static const char* MODULE_PREFIX = "DoorStrike";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DoorStrike::DoorStrike()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DoorStrike::~DoorStrike()
{
    if (_doorStrikePin >= 0)
    {
        LOG_I(MODULE_PREFIX, "Restoring door strike pin %d", _doorStrikePin);
        pinMode(_doorStrikePin, INPUT);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup strike
// Garage mode if closedSensePin is defined
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DoorStrike::setup(int doorStrikePin, bool doorStrikeOn, int doorOpenSensePin, 
            int doorClosedSensePin, bool senseWhenOpen, int defaultUnlockSecs)
{
    _defaultUnlockMs = defaultUnlockSecs * 1000;
    _doorStrikePin = doorStrikePin;
    _doorStrikeOn = doorStrikeOn;
    _doorOpenSensePin = doorOpenSensePin;
    _doorClosedSensePin = doorClosedSensePin;
    _garageMode = (_doorClosedSensePin >= 0);
    _senseWhenOpen = senseWhenOpen;
    _unlockedTime = 0;
    _isLocked = true;
    _mTimeOutOnUnlockMs = 0;
    return _doorStrikePin >= 0;
}

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
    if (_doorStrikePin < 0)
        return false;
    // Allow door to open
    digitalWrite(_doorStrikePin, _doorStrikeOn);
    _isLocked = false;
    // Store timeout
    _mTimeOutOnUnlockMs = (timeoutInSecs == -1 ? _defaultUnlockMs : timeoutInSecs * 1000);
    // Set timer to time how long to leave door unlocked
    _unlockedTime = millis();
    LOG_I(MODULE_PREFIX, "%s from %s pin %d unlockLevel %d timeoutms %d",
                (_garageMode ? "Closing door toggle relay" : "Unlocking door"),
                unlockCause,
                _doorStrikePin, 
                _doorStrikeOn,
                _mTimeOutOnUnlockMs);
    return true;
}

bool DoorStrike::lock()
{
    // Lock door
    digitalWrite(_doorStrikePin, !_doorStrikeOn);
    // Indicate now locked
    _isLocked = true;
    LOG_I(MODULE_PREFIX, "%s pin %d level %d", (_garageMode ? "Opening relay" : "Locking door"), 
            _doorStrikePin,
            !_doorStrikeOn);
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
    if (_doorOpenSensePin < 0)
        return DoorOpenSense::DoorSenseUnknown;

    bool doorOpen = (digitalRead(_doorOpenSensePin) != 0) == _senseWhenOpen;
    bool doorClosed = false;
    if (_garageMode && (_doorClosedSensePin >= 0))
        doorClosed = (digitalRead(_doorClosedSensePin) != 0) == _senseWhenOpen;

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
    DoorLockedEnum lockedEnum = DoorLockedEnum::LockStateUnknown;
    DoorOpenSense openSense = DoorOpenSense::DoorSenseUnknown;
    int timeBeforeRelock = 0;
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
    snprintf(statusStr, sizeof(statusStr), R"(%s"locked":"%s","open":"%s","toLockMs":%d%s)",
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
