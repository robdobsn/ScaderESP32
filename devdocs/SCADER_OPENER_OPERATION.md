# ScaderOpener Operation (Conservatory Door Opener)

Reference for how the two PIRs feed the conservatory door opener.
Sources: `components/Scader/ScaderOpener/DoorOpener.{h,cpp}`,
`components/Scader/ScaderOpener/OpenerStatus.{h,cpp}`,
`components/Scader/ScaderOpener/UIModule.cpp`,
`components/Scader/ScaderOpener/StateChangeDetector.h`,
`ScaderOpenerUI/src/main.cpp`,
`systypes/ScaderOpener/SysTypes.json`.

## Sensors and terminology

Two PIRs, one per side of the door. The code, config, JSON status, and WebUI
each use different names for the same thing — mapping:

| Physical location | Side (JSON/WebUI) | Enable flag | Config key       | Internal name(s)                                  |
|-------------------|-------------------|-------------|------------------|---------------------------------------------------|
| Conservatory      | IN                | `_inEnabled`  | `consvPirPin`  | `_consvPirSensePin`, `_consvPIRChangeDetector`    |
| Kitchen           | OUT               | `_outEnabled` | (none, remote) | `_kitchenPIRValue`, `_isKitchenPIRActive`         |

Status JSON fields: `pirSenseInActive` = conservatory, `pirSenseOutActive` = kitchen.

## Transport paths (asymmetric)

### Conservatory PIR — direct GPIO

- Configured by `consvPirPin` in `SysTypes.json` with `GPIO_INPUT_PULLDOWN`
  (`DoorOpener::setup`).
- Sampled every `DoorOpener::loop()` via `digitalRead(_consvPirSensePin)`
  and fed to `_consvPIRChangeDetector.service(...)`.
- `StateChangeDetector` fires the callback on every level transition.
  There is **no debounce or hold-off**; every GPIO edge produces an event.
- `pirSenseInActive` in the status JSON returns the *live* pin state
  (`_consvPIRChangeDetector.getState()`).

### Kitchen PIR — remote via M5Stack UI panel over UART

- The PIR is physically wired to the M5Stack UI board (`KITCHEN_PIR_PIN = 26`
  in `ScaderOpenerUI/src/main.cpp`), **not** to the main ESP32.
- The M5Stack detects edges and sends HDLC-framed JSON commands over UART:
  `{"cmd":"kitchenPIRActive"}` / `{"cmd":"kitchenPIRInactive"}`.
- `UIModule::processStatus` decodes and calls
  `OpenerStatus::setKitchenPIRActive(bool)`, which stores the value and
  sets a "changed" flag.
- `DoorOpener::loop()` drains the flag via
  `getKitchenPIRStateChangedAndClear` and calls `onKitchenPIRChanged`.
- `pirSenseOutActive` in the status JSON returns the *last received edge*
  cached in `_isKitchenPIRActive` (not a live reading).

## Trigger rule

Both sensors use the same rule (`onConservatoryPIRChanged` /
`onKitchenPIRChanged` in `DoorOpener.cpp`):

- If door state is `DOOR_STATE_CLOSED` or `DOOR_STATE_AJAR`,
  the sensor became active (rising edge), and the relevant enable flag is on,
  then `startDoorOpening(...)`.
- Otherwise the event is ignored — no re-arming while OPENING, OPEN, or
  CLOSING.

Enable-flag routing:

- Conservatory PIR → checks `_inEnabled`.
- Kitchen PIR → checks `_outEnabled`.

## Enable flags

- Persisted to NVS as `inEn` / `outEn` (`OpenerStatus::readFromNVS` and
  `saveToNVSIfRequired`), saved after `MUTABLE_DATA_SAVE_MIN_MS` (5s) of
  quiescence following a change.
- Toggled via:
  - REST: `/opener/inenable/{0|1}`, `/opener/outenable/{0|1}`
    (`ScaderOpener::apiControl`).
  - M5Stack UI buttons sending `inEnable` / `inDisable` /
    `outEnable` / `outDisable` (`UIModule::processStatus`).

## Auto-close interaction

After remaining in `DOOR_STATE_OPEN` or `DOOR_STATE_AJAR` for
`DoorRemainOpenTimeSecs` (45s in `systypes/ScaderOpener/SysTypes.json`),
`DoorOpener::serviceDoorState` starts closing **only if** at least one of
`_inEnabled` / `_outEnabled` is on. With both disabled the door stays open.

## Status reporting

`DoorOpener::getStatusJSON` emits, among other fields:

- `pirSenseInActive`  — conservatory PIR live pin state.
- `pirSenseOutActive` — last kitchen PIR edge received from the M5Stack.
- `inEnabled`, `outEnabled` — current enable flags.

`DoorOpener::getStatusHash` includes both PIR values and both enable flags,
gated by `MIN_TIME_BETWEEN_STATE_HASH_CHANGES_MS = 330` — so status hash
updates (and therefore ScaderPublisher / WebSocket pushes) are throttled to
~3 Hz max regardless of PIR chatter. The WebUI reads these fields in
`systypes/Common/WebUI/src/ScaderOpener.tsx` and shows `PIR_IN` / `PIR_OUT`
badges.

## Known quirks / gotchas

1. **Asymmetric semantics.** `pirSenseInActive` is a live pin read;
   `pirSenseOutActive` is a cached last edge. If the M5Stack UI panel
   disconnects, `pirSenseOutActive` will stay at whatever value it held —
   no timeout/reset.
2. **No debounce on conservatory PIR.** Every GPIO edge produces a
   `StateChangeDetector` callback. Any debouncing on the kitchen side lives
   on the M5Stack, not on the ESP32.
3. **Naming inconsistency.** Physical (conservatory/kitchen), directional
   (in/out), and status-field (`pirSenseIn*`/`pirSenseOut*`) names all coexist.
   The mapping table above is authoritative.
4. **Auto-close depends on enables.** With both `inEnabled` and `outEnabled`
   off, the door will stay open indefinitely after a manual/API open —
   the timeout branch in `serviceDoorState` is skipped.
5. **MQTT/publish rate.** `TODO.md` notes possible over-publishing from
   ScaderOpener. The 330 ms hash-change floor limits it, but PIR-driven
   change bursts can still fire status pushes frequently.
