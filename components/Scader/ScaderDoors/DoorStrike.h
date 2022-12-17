/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoorStrike
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

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

    // Constructor
    DoorStrike();

    // Destructor
    ~DoorStrike();

    // Setup strike
    // Garage mode if closedSensePin is defined
    bool setup(int doorStrikePin, bool doorStrikeOn, int doorOpenSensePin, 
                int doorClosedSensePin, bool senseWhenOpen, int defaultUnlockSecs = 10);

    // Service to handle timeouts, etc
    void service();

    // Unlock
    bool unlockWithTimeout(const char* unlockCause, int timeoutInSecs = -1);

    // Lock
    bool lock();

    // Check if locked
    bool isLocked();

    // Get status of door open/closed/etc
    DoorOpenSense getOpenStatus() const;

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
    int _doorStrikePin = -1;
    bool _doorStrikeOn = false;
    int _doorOpenSensePin = -1;
    int _doorClosedSensePin = -1;
    int _mTimeOutOnUnlockMs = 0;
    int _defaultUnlockMs = 0;
    bool _senseWhenOpen = false;
    bool _isLocked = true;
    unsigned long _unlockedTime = 0;
    const int MIN_TIMEOUT_AFTER_UNLOCK_MS = 1000;
    bool _garageMode = false;
};
