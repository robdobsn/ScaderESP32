/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoorStrike
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class DoorStrike
{
public:
    enum DoorOpenSense
    {
        DoorSenseUnknown,
        Closed,
        Open
    };
    enum DoorLockedEnum
    {
        LockStateUnknown,
        Locked,
        Unlocked
    };

    // Setup strike
    // Garage mode if closedSensePin is defined
    DoorStrike(int doorStrikePin, bool doorStrikeOn, int doorOpenSensePin, 
                int doorClosedSensePin, bool senseWhenOpen, int defaultUnlockSecs = 10)
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

        digitalWrite(_doorStrikePin, !doorStrikeOn);
        pinMode(_doorStrikePin, OUTPUT);
        digitalWrite(_doorStrikePin, !doorStrikeOn);
        if (_doorClosedSensePin >= 0)
            pinMode(_doorClosedSensePin, INPUT);
        if (_doorClosedSensePin >= 0)
            pinMode(_doorClosedSensePin, INPUT_PULLUP);
    }

    // Restore
    ~DoorStrike()
    {
        pinMode(_doorStrikePin, INPUT);
    }

    // Service to handle timeouts, etc
    void service();

    // Unlock
    bool unlockWithTimeout(const char* unlockCause, int timeoutInSecs = -1);

    // Lock
    bool lock();

    // Check if locked
    bool isLocked();

    // Get status of door open/closed/etc
    DoorOpenSense getOpenStatus();

    // Get all status info
    bool getStatus(DoorLockedEnum& lockedEnum, DoorOpenSense& openSense, int& timeBeforeRelock) const;

    // Get status hash
    void getStatusHash(std::vector<uint8_t>& stateHash) const;

    // Get JSON status
    String getStatusJson(bool incBraces) const;

    // Door open state string
    static const char* getDoorOpenSenseStr(DoorOpenSense& openSense);

    // Door locked state string
    static const char* getDoorLockedStr(DoorStrike::DoorLockedEnum& lockedEnum);

    // Get debug string
    String getDebugStr() const
    {
        return getStatusJson(false);
    }

private:
    int _doorStrikePin;
    bool _doorStrikeOn;
    int _doorOpenSensePin;
    int _doorClosedSensePin;
    int _mTimeOutOnUnlockMs;
    int _defaultUnlockMs;
    bool _senseWhenOpen;
    bool _isLocked;
    unsigned long _unlockedTime;
    const int MIN_TIMEOUT_AFTER_UNLOCK_MS = 1000;
    bool _garageMode;    
};
