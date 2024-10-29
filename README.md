# ScaderESP32 - Firmware for Home Automation

## Building

```
raft run
```

See [RaftCLI for more information on building](https://github.com/robdobsn/RaftCLI)

## Scader Opener

Controls opening and closing a door using a motor, angle sensor, force sensor, UI panel, exit button and motion/presence sensors on each side of the door.

The MotorMechanism class handles motor, angle sensor and force sensor. The motor's setting for unitsPerRot should be 360 to represent degrees and stepsPerRot should be set to the correct value for the motor and gearbox. Movement is actioned by sending a relative motion command to the stepper motor controller to rotate to the desired position from the position reported by the angle sensor. This only works if the stepsPerRot setting is accurate.

Note that the AS5600 angle sensor is very sensitive to positioning on the rotating shaft. Ensure it is very close to the centre of rotation or inaccurate angles will be reported.

