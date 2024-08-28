/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoorStrike
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"
#include "RaftArduino.h"
#include "DoorStrike.h"
#include "RaftUtils.h"

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
            int doorClosedSensePin, bool senseWhenOpen, uint32_t defaultUnlockSecs,
            uint32_t delayRelockSecs)
{
    _defaultUnlockMs = defaultUnlockSecs * 1000;
    _delayRelockSecs = delayRelockSecs;
    _doorStrikePin = doorStrikePin;
    _doorStrikeOn = doorStrikeOn;
    _doorOpenSensePin = doorOpenSensePin;
    _doorClosedSensePin = doorClosedSensePin;
    _garageMode = (_doorClosedSensePin >= 0);
    _senseWhenOpen = senseWhenOpen;
    return _doorStrikePin >= 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service to handle timeouts, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoorStrike::loop()
{
    // Handle re-locking door if unlocked
    if (!_garageMode)
    {
        if (!_isLocked)
        {
            // Check for re-lock conditions
            if ((getOpenStatus() == DoorOpenSense::Open) && Raft::isTimeout(millis(), _unlockedTimeMs, MIN_TIMEOUT_AFTER_UNLOCK_MS))
            {
                // Check if door is open and unlock occurred more than a minimum time ago
                lock(true);
            }
            else if (Raft::isTimeout(millis(), _unlockedTimeMs, _timeOutOnUnlockMs))
            {
                // Door unlocked max time
                lock(true);
            }
            else if (_relockPending && Raft::isTimeout(millis(), _relockPendingTimeMs, _delayRelockSecs * 1000))
            {
                // Relock pending
                lock(true);
                _relockPending = false;
            }
        }
    }
    else
    {
        if (!_isLocked && Raft::isTimeout(millis(), _unlockedTimeMs, _timeOutOnUnlockMs))
            lock(true);
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
    _relockPending = false;
    // Store timeout
    _timeOutOnUnlockMs = (timeoutInSecs == -1 ? _defaultUnlockMs : timeoutInSecs * 1000);
    // Set timer to time how long to leave door unlocked
    _unlockedTimeMs = millis();
    LOG_I(MODULE_PREFIX, "%s from %s pin %d unlockLevel %d timeoutms %d",
                (_garageMode ? "Closing door toggle relay" : "Unlocking door"),
                unlockCause,
                _doorStrikePin, 
                _doorStrikeOn,
                _timeOutOnUnlockMs);
    return true;
}

bool DoorStrike::lock(bool forceImmediate)
{
    // Handle immediate
    if (forceImmediate)
    {
        // Lock door
        digitalWrite(_doorStrikePin, !_doorStrikeOn);
        // Indicate now locked
        _isLocked = true;
        LOG_I(MODULE_PREFIX, "%s pin %d level %d", (_garageMode ? "Opening relay" : "Locking door"), 
                _doorStrikePin,
                !_doorStrikeOn);
    }
    else
    {
        // Relock is pending
        _relockPending = true;
        _relockPendingTimeMs = millis();
    }
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

bool DoorStrike::getStatus(DoorLockedEnum& lockedEnum, DoorOpenSense& openSense, uint32_t& timeBeforeRelock) const
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
    timeBeforeRelock = Raft::timeToTimeout(millis(), _unlockedTimeMs, _timeOutOnUnlockMs);
    return true;
}

void DoorStrike::getStatusHash(std::vector<uint8_t>& stateHash) const
{
    DoorLockedEnum lockedEnum = DoorLockedEnum::LockStateUnknown;
    DoorOpenSense openSense = DoorOpenSense::DoorSenseUnknown;
    uint32_t timeBeforeRelock = 0;
    getStatus(lockedEnum, openSense, timeBeforeRelock);
    stateHash.push_back(lockedEnum + (openSense << 4));
}

String DoorStrike::getStatusJson(bool incBraces) const
{
    DoorLockedEnum lockedEnum = DoorLockedEnum::LockStateUnknown;
    DoorOpenSense openEnum = DoorOpenSense::DoorSenseUnknown;
    uint32_t timeBeforeLockMs = 0;
    getStatus(lockedEnum, openEnum, timeBeforeLockMs);
    char statusStr[200];
    snprintf(statusStr, sizeof(statusStr), R"(%s"locked":"%s","open":"%s","toLockMs":%d%s)",
                        incBraces ? "{" : "", 
                        getDoorLockedStr(lockedEnum), 
                        getDoorOpenSenseStr(openEnum),
                        (int)timeBeforeLockMs,
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
