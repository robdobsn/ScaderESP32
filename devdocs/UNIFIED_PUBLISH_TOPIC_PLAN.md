# Unified Scader Publish Topic — Refactor Plan

Status: planning / proposal — no code changes yet.

## 1. Motivation

Today every Scader feature module (`ScaderRelays`, `ScaderShades`, `ScaderLocks`, `ScaderOpener`, `ScaderRFID`, `ScaderBTHome`, `ScaderPulseCounter`, `ScaderElecMeters`) registers itself as an independent `Publish`-channel data source using its own module name as the **pubTopic**. The MQTT settings on the device then map each pubTopic into an outbound MQTT path:

```jsonc
"MQTTMan": {
  "topics": [
    {
      "name": "scaderOut",
      "path": "scader/out",
      "pubSources": [
        { "pubTopic": "ScaderShades", "rateHz": 0.1, "trigger": "time_or_change" },
        { "pubTopic": "ScaderLocks",  "rateHz": 0.1, "trigger": "time_or_change" }
      ]
    }
  ]
}
```

This made sense when each firmware variant published exactly one module type, but as variants now combine multiple modules (Locks + Shades on the same controller, ElecMeters + PulseCounter on another, Opener + LEDs etc.) the configuration matrix has grown:

- Every new module/systype combination needs a fresh `pubSources` array on the device.
- The matching `MQTTManager.topics[].pubSources` block needs to be edited per systype.
- The `pubTopic` strings (`ScaderRelays`, `ScaderLocks`, …) are duplicated between firmware code and config JSON; a typo silently disables publishing.
- Devices emit N messages per cycle for N enabled modules instead of one — wasteful and harder to correlate.

A single per-device `Scader` pubTopic carrying a **device-level envelope plus a `modules{}` map of enabled module data** would let every firmware variant ship the same MQTT settings and emit one tidy message per cycle.

## 2. Current Wire Shape (Reference)

Each enabled module already emits its own envelope built by [`ScaderCommon::getStatusJSON()`](../components/Scader/ScaderCommon/ScaderCommon.h):

```json
{
  "module": "ScaderRelays",
  "name": "Scader Office Server",
  "version": "6.4.4",
  "hostname": "scaderoffsrv",
  "IP": "192.168.86.80",
  "MAC": "c8:2b:96:ba:ad:9f",
  "upMs": 52796,
  "mainsHz": 49.98,
  "elems": [{ "name": "Master Bath Fan", "state": 0 }, ...]
}
```

A 2-module device today therefore publishes two such messages back-to-back to `scader/out`, each repeating the device envelope.

## 3. Target Wire Shape

```json
{
  "module": "Scader",
  "name": "Scader Office Server",
  "version": "6.4.4",
  "hostname": "scaderoffsrv",
  "IP": "192.168.86.80",
  "MAC": "c8:2b:96:ba:ad:9f",
  "upMs": 52796,
  "modules": {
    "ScaderRelays":  { "mainsHz": 49.98, "elems": [...] },
    "ScaderLocks":   { "elems": [{ "name": "Front Door", "locked": "Y", "open": "N" }] },
    "ScaderShades":  { "elems": [...] }
  }
}
```

Notes:

- Top-level envelope identical to today's per-module envelope, but `module` is the literal `"Scader"`.
- Each entry in `modules` is the existing module body **without** the device envelope (i.e. what's currently inside `getStatusJSON()` after stripping the duplicated `module/name/version/hostname/IP/MAC/upMs` fields).
- The key in `modules` is the existing module name — no new vocabulary on the consumer side.
- `mainsHz` (and any other today-only-on-some-modules fields) stays inside its owning module entry; it is not promoted to top level.

## 4. Firmware Changes (ScaderESP32)

### 4.1 New aggregator

Introduce a thin aggregator owned by `main/main.cpp` (or a new `ScaderPublisher` SysMod). It:

1. Holds (or discovers) pointers to all enabled `Scader*` modules.
2. Exposes a single `getUnifiedStatusJSON()` that:
   - Calls `_scaderCommon.getStatusJSON()` once for the device envelope.
   - For each enabled module, calls a new `getModuleBodyJSON()` that returns just the module-body fragment (no envelope).
   - Wraps `{… envelope …, "modules": { ScaderX: <body>, … }}`.
3. Exposes a single `getUnifiedStatusHash()` that concatenates the existing per-module hashes (so trigger=`time_or_change` semantics are preserved when *any* module changes).
4. Registers ONE `pubSource`:
   ```cpp
   pSysManager->registerDataSource("Publish", "Scader",
       [this](uint16_t, CommsChannelMsg& m){ /* unified JSON */ },
       [this](uint16_t, std::vector<uint8_t>& h){ /* combined hash */ });
   ```

### 4.2 Per-module changes

Each `Scader<X>::getStatusJSON()` keeps its current public API (used by `addRestAPIEndpoints` etc.) but gains a sibling `getStatusBodyJSON()` returning only the body (no envelope). The existing call site:

```cpp
return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elemStatus + "]}";
```

becomes equivalent to:

```cpp
return "{" + _scaderCommon.getStatusJSON() + "," + getStatusBodyInner() + "}";
```

so that `getStatusBodyJSON()` (used by the aggregator) returns just `getStatusBodyInner()` wrapped in `{}`.

The per-module `registerDataSource("Publish", moduleName, …)` call gets **removed** — only the aggregator publishes.

Files touched (all eight):
- [components/Scader/ScaderRelays/ScaderRelays.cpp](../components/Scader/ScaderRelays/ScaderRelays.cpp)
- [components/Scader/ScaderShades/ScaderShades.cpp](../components/Scader/ScaderShades/ScaderShades.cpp)
- [components/Scader/ScaderLocks/ScaderLocks.cpp](../components/Scader/ScaderLocks/ScaderLocks.cpp)
- [components/Scader/ScaderOpener/ScaderOpener.cpp](../components/Scader/ScaderOpener/ScaderOpener.cpp)
- [components/Scader/ScaderRFID/ScaderRFID.cpp](../components/Scader/ScaderRFID/ScaderRFID.cpp)
- [components/Scader/ScaderBTHome/ScaderBTHome.cpp](../components/Scader/ScaderBTHome/ScaderBTHome.cpp)
- [components/Scader/ScaderPulseCounter/ScaderPulseCounter.cpp](../components/Scader/ScaderPulseCounter/ScaderPulseCounter.cpp)
- [components/Scader/ScaderElecMeters/ScaderElecMeters.cpp](../components/Scader/ScaderElecMeters/ScaderElecMeters.cpp)

### 4.3 Common MQTT settings

`systypes/Common/sdkconfig.defaults` (or whichever JSON ships the default `MQTTMan` block) collapses every variant's `pubSources` to the single line:

```jsonc
"pubSources": [{ "pubTopic": "Scader", "rateHz": 0.1, "trigger": "time_or_change" }]
```

Per-systype overrides for that block can be deleted.

### 4.4 Open questions for firmware

- **ElecMeters telegraf compatibility** — telegraf currently consumes `scader/gas` only. If/when we want energy in InfluxDB, the unified message can be parsed by telegraf with a `mqtt_consumer` + JSON path `modules.ScaderElecMeters.elems` filter. No change required for the `gas` topic which is already independent.
- **BTHome** — currently a heartbeat for the home-server but its data may have downstream consumers. Confirm whether the `scader/bthome` topic on the device is truly unused; if so, fold its body into `modules.ScaderBTHome` and drop the dedicated topic. If still needed, leave the dedicated `bthome` topic alone (it is a separate `topics[]` entry).
- **Hash composition** — naive concatenation of module hashes increases hash length and false-change detection risk. Acceptable for current cadences (0.1 Hz) but worth a short comment.
- **Memory** — the unified JSON is bigger when many modules are enabled. Inspect against `RAFTWEB_MAX_PUBLISH_PAYLOAD_BYTES` (or equivalent) and bump if necessary.

## 5. Server Changes (nodeHomeServer)

### 5.1 Where the dispatch happens

`CircuitStateHandler.handleScaderOutMsg()` switches on the JSON's top-level `module` field to route into per-module handlers. The relevant block lives in [src/CircuitStateHandler.ts](../../../../srv/RdHomeServer/nodeHomeServer/src/CircuitStateHandler.ts) around lines 175–230. Today it expects `module ∈ {ScaderRelays, ScaderLocks, ScaderDoors, ScaderElecMeters, ScaderRFID, ScaderShades, ScaderOpener, ScaderMarbleRun, ScaderBTHome, ScaderPulseCounter}`.

### 5.2 Backwards-compatible aliasing layer

Add a single normalisation step at the top of `handleScaderOutMsg()` that, when it sees `module === "Scader"` with a `modules{}` map, **fans the message out into the legacy per-module shape** and re-enters the existing dispatcher for each:

```ts
if (msgContent.module === "Scader" && msgContent.modules) {
    for (const [name, body] of Object.entries(msgContent.modules)) {
        const legacyMsg = {
            module: name,
            name: msgContent.name,
            version: msgContent.version,
            hostname: msgContent.hostname,
            IP: msgContent.IP,
            MAC: msgContent.MAC,
            upMs: msgContent.upMs,
            ...body,             // module-specific fields incl. elems
        };
        this.dispatchModule(legacyMsg, ...);   // existing switch extracted into helper
    }
    return;
}
// legacy single-module path unchanged
```

This means **all current handlers, telegraf rules, and audit logic continue to work unchanged** — they always see the legacy shape. Only the boundary code knows about the new format.

The fan-out is also where the migration kill-switch lives: a `unifiedScaderEnabled` flag (default true once stable) lets us silently re-emit legacy synthetic events; setting it to false would log a warning and ignore the unified message — useful only as a debug escape hatch.

### 5.3 Telegraf

`telegraf/telegraf.conf` currently consumes `scader/gas`. If/when energy (or any other module) needs InfluxDB writes, add a second `mqtt_consumer` for `scader/out` that uses `json_string_fields`/`json_query` to pull the relevant `modules.ScaderX` subtree. This is independent of the firmware-side migration.

### 5.4 Files touched

- [src/CircuitStateHandler.ts](../../../../srv/RdHomeServer/nodeHomeServer/src/CircuitStateHandler.ts) — add fan-out at the top of `handleScaderOutMsg()`, extract the existing `module`-switch into a `dispatchModule()` helper. Keep the legacy path verbatim.
- (Optional) `src/HomeTypes.ts` — add a `UnifiedScaderMessage` type for clarity in the new code path.
- No changes to `HomeConnector`, `AuditService`, `DeviceUptimeMonitor`, MQTT subscription setup, or routes.

### 5.5 Tests

Extend `test/test-circuit-state-handler.*` (or create one if absent) with two cases:

1. Legacy single-module message → dispatched once (regression).
2. Unified message with three modules → produces three internal dispatches with the right envelopes copied through.

## 6. Migration Strategy

1. **Land server-side aliasing first.** It is a no-op against current firmware (the unified branch is never taken), so it can deploy ahead of any firmware changes.
2. **Land firmware aggregator behind a build-time switch** (`CONFIG_SCADER_UNIFIED_PUBLISH=y`) so a release can be cut that still publishes the legacy way until validated.
3. **Tag a release with the unified format** for one systype (e.g. `ScaderRelays`) and OTA a single non-critical device. Confirm the server's fan-out produces identical UI/audit/uptime behaviour.
4. **Roll out via OTA** to remaining devices in batches.
5. **Remove the build-time switch** and update `systypes/Common/sdkconfig.defaults` to drop legacy `pubSources` once the fleet is on the new format.
6. **Optional cleanup**: keep the server-side aliasing branch indefinitely (it costs ~20 LOC) — it makes future format tweaks easier and lets a freshly imaged spare device on old firmware still work.

## 7. Risks & Watch-Outs

- **Hostname-based device lookup** in `CircuitStateHandler` already supports hostname/IP/MAC fallback; copying `hostname/IP/MAC` from the unified envelope into each synthetic legacy message keeps that path working.
- **Hash false-positives** — if combining hashes makes nearly every cycle look "changed" we will publish too often. Cap with a min-rate guard if needed.
- **Per-module rate differences** — today every module shares `rateHz: 0.1` so a single unified rate is fine; if a future module needs a faster cadence, it should publish on its own dedicated `topics[]` entry rather than fighting the aggregator.
- **Larger MQTT payload** — measure on the busiest device (e.g. ScaderElecMeters with 4 meters + ScaderPulseCounter) before flipping the build-time switch.
- **Audit log churn** — if an audit consumer ever inspects `module` directly, the unified-but-fanned-out shape preserves it. Just confirm there is no check on the envelope's `module === "ScaderX"`-as-source-of-truth that bypasses the body.

## 8. Out of Scope

- Inbound topic structure (`scader/in/...`) — control commands stay as today.
- `scader/gas`, `scader/elec`, `scader/bthome` topic *paths* are independent of pubTopic names; they are not touched.
- Any change to the device's REST API (`/api/scader/...`) endpoints.
