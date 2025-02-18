/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Door Opener
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "Logger.h"
#include "RaftArduino.h"
#include "DebounceButton.h"
#include "RaftUtils.h"
#include "RaftJsonNVS.h"
#include "SimpleMovingAverage.h"
#include "OpenerStatus.h"
#include "MotorMechanism.h"
#include "StateChangeDetector.h"

class DeviceManager;

class DoorOpener : public OpenerStatus
{
public:
    // Constructor and destructor
    DoorOpener(RaftJsonNVS& nvsData);
    virtual ~DoorOpener();

    // Setup
    void setup(DeviceManager* pDevMan, RaftJsonIF& config);

    // Service
    void loop();

    // Open/Close door
    void startDoorOpening(String debugStr);
    void startDoorClosing(String debugStr);

    // Stop and disable door
    void doorStopAndDisable(String debugStr);

    // Move door to specified angle
    void debugMoveToAngle(int32_t angleDegs);

    // Status
    String getStatusJSON(bool includeBraces) const;
    void getStatusHash(std::vector<uint8_t>& stateHash);

private:
    // Motor mechanism
    MotorMechanism _motorMechanism;

    // State change detector for conservatory PIR
    StateChangeDetector _consvPIRChangeDetector;

    // Conservatory button and PIR pins
    int _conservatoryButtonPin = -1;
    int _consvPirSensePin = -1;

    // Kitchen PIR current level
    bool _isKitchenPIRActive = false;

    // Door angles for open and closed positions (degrees)
    int32_t _doorOpenAngleDegs = 120;
    int32_t _doorClosedAngleDegs = 0;

    // Door timing - time to open and time to remain open
    static const uint32_t DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS = 30;
    static const uint32_t DEFAULT_DOOR_TIME_TO_OPEN_SECS = 8;
    uint32_t _doorRemainOpenTimeSecs = DEFAULT_DOOR_REMAIN_OPEN_TIME_SECS;
    uint32_t _doorTimeToOpenSecs = DEFAULT_DOOR_TIME_TO_OPEN_SECS;
    static const int32_t DOOR_CLOSED_ANGLE_ADDITIONAL_DEGS = 0;
    static const int32_t DOOR_CLOSED_ANGLE_TOLERANCE_DEGS = 2;
    static const int32_t DOOR_OPEN_ANGLE_TOLERANCE_DEGS = 5;

    // Conservatory button
    DebounceButton _conservatoryButton;

    // State change timing
    uint32_t _lastStateChangeMs = 0;
    static const uint32_t MIN_TIME_BETWEEN_STATE_HASH_CHANGES_MS = 330;

    // Debug
    uint32_t _debugLastDisplayMs = 0;

    // Helpers
    void clear();
    void serviceDoorState();
    void onConservatoryButtonPressed(int isActive, uint32_t timeSinceLastPressMs, uint16_t repeatCount);
    void onConservatoryPIRChanged(bool isActive, uint32_t timeSinceLastChange);
    void onKitchenPIRChanged(bool isActive);
    void onOpenCloseToggleChanged(bool isActive);
    float calcDoorMoveSpeedDegsPerSec(float angleDegs, float timeSecs) const;
    float calcDegreesFromClosed(float measuredAngleDegrees) const;
    uint32_t calcTimeBeforeCloseSecs() const;
};
