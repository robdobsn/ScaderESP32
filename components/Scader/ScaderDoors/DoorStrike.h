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
                int doorClosedSensePin, bool senseWhenOpen, uint32_t defaultUnlockSecs,
                uint32_t delayRelockSecs);

    // Service to handle timeouts, etc
    void service();

    // Unlock
    bool unlockWithTimeout(const char* unlockCause, int timeoutInSecs = -1);

    // Lock
    bool lock(bool forceImmediate);

    // Check if locked
    bool isLocked();

    // Get status of door open/closed/etc
    DoorOpenSense getOpenStatus() const;

    // Get all status info
    bool getStatus(DoorLockedEnum& lockedEnum, DoorOpenSense& openSense, uint32_t& timeBeforeRelock) const;

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
    uint32_t _timeOutOnUnlockMs = 0;
    uint32_t _defaultUnlockMs = 0;
    uint32_t _delayRelockSecs = 0;
    bool _senseWhenOpen = false;
    bool _isLocked = true;
    uint32_t _unlockedTimeMs = 0;
    const uint32_t MIN_TIMEOUT_AFTER_UNLOCK_MS = 1000;
    bool _garageMode = false;
    uint32_t _relockPendingTimeMs = 0;
    bool _relockPending = false;
};
