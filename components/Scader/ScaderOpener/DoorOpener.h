/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Door Opener
//
// Rob Dobson 2013-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <Logger.h>
#include <RaftArduino.h>
#include <DebounceButton.h>
#include <RaftUtils.h>
#include <ConfigBase.h>
#include <SimpleMovingAverage.h>
#include <OpenerStatus.h>
#include "MotorAndAngleSensor.h"
#include "StateChangeDetector.h"

class DoorOpener : public OpenerStatus
{
public:
    // Constructor and destructor
    DoorOpener();
    virtual ~DoorOpener();

    // Setup
    void setup(ConfigBase& config);

    // Service
    void service();

    // Open/Close door
    void startDoorOpening(String debugStr);
    void startDoorClosing(String debugStr);

    // Stop and disable door
    void doorStopAndDisable(String debugStr);

    // Move door to specified angle
    void debugMoveToAngle(int32_t angleDegs);

    // Status
    String getStatusJSON(bool includeBraces);
    void getStatusHash(std::vector<uint8_t>& stateHash);

private:
    // Motor and angle sensor
    MotorAndAngleSensor _motorAndAngleSensor;

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
    static const int32_t DOOR_CLOSED_ANGLE_ADDITIONAL_DEGS = 5;
    static const int32_t DOOR_CLOSED_ANGLE_TOLERANCE_DEGS = 2;
    static const int32_t DOOR_OPEN_ANGLE_TOLERANCE_DEGS = 5;

    // Conservatory button
    DebounceButton _conservatoryButton;

    // Debug
    uint32_t _debugLastDisplayMs = 0;

    // Helpers
    void clear();
    void serviceDoorState();
    void onConservatoryButtonPressed(int isActive, uint32_t timeSinceLastPressMs, uint16_t repeatCount);
    void onConservatoryPIRChanged(bool isActive, uint32_t timeSinceLastChange);
    void onKitchenPIRChanged(bool isActive);
    void onOpenCloseToggleChanged(bool isActive);
    float calcDoorMoveSpeedDegsPerSec(float angleDegs, float timeSecs);
    float calcDegreesFromClosed(float measuredAngleDegrees);
    uint32_t calcTimeBeforeCloseSecs();
};
