# ScaderESP32 - Firmware for Home Automation

A flexible, modular home automation firmware for ESP32 devices built on the [Raft framework](https://github.com/robdobsn/RaftCore). ScaderESP32 provides similar functionality to ESPHome but with a different architectural approach, offering various specialized modules for controlling lights, motors, sensors, locks, and more over WiFi and BLE.

## Table of Contents

- [Overview](#overview)
- [Building](#building)
- [Architecture](#architecture)
- [Scader Modules](#scader-modules)
  - [ScaderRelays](#scaderrelays)
  - [ScaderShades](#scadershades)
  - [ScaderOpener](#scaderopener)
  - [ScaderLocks](#scaderlocks)
  - [ScaderLEDPixels](#scaderledpixels)
  - [ScaderRFID](#scaderrfid)
  - [ScaderPulseCounter](#scaderpulsecounter)
  - [ScaderElecMeters](#scaderelecmeters)
  - [ScaderBTHome](#scaderbthome)
  - [ScaderMarbleRun](#scadermarblerun)
  - [ScaderWaterer](#scaderwaterer)
  - [ScaderCat](#scadercat)
- [REST API](#rest-api)
- [Configuration](#configuration)
- [Contributing](#contributing)

## Overview

ScaderESP32 is a home automation platform designed to run on ESP32 microcontrollers. It provides:

- **Modular Architecture**: Each Scader module is self-contained and can be enabled/disabled independently
- **WiFi & BLE Connectivity**: Communicate over both WiFi and Bluetooth Low Energy
- **REST API**: Control all modules through a unified REST API
- **Raft Framework**: Built on the robust Raft library framework for embedded systems
- **Persistent State**: Module states are saved to non-volatile storage (NVS)
- **Real-time Status**: Get JSON-formatted status from all modules
- **Hardware Abstraction**: Supports various hardware configurations through the Raft bus system

## Building

Build the project using the Raft CLI:

```bash
raft run
```

See [RaftCLI documentation](https://github.com/robdobsn/RaftCLI) for more information on building and flashing.

## Architecture

The ScaderESP32 firmware uses a modular system architecture where each "Scader" module is a `RaftSysMod` (Raft System Module). All modules share common functionality through the `ScaderCommon` class, which provides:

- Network information (hostname, IP address, MAC address)
- Module enable/disable control
- Friendly name management
- Status reporting with uptime and version information

Each module implements:
- `setup()`: Initialize hardware and read configuration
- `loop()`: Main processing loop called frequently
- `addRestAPIEndpoints()`: Register REST API endpoints
- `getStatusJSON()`: Return current module status as JSON

## Scader Modules

### ScaderRelays

**Purpose**: Control multiple relays with dimming support (0-100%)

**Features**:
- Controls up to 24 relay/dimmer channels (configurable)
- SPI-based dimmer control via SPIDimmer class
- Individual channel control with names
- State persistence to NVS with automatic saving
- Supports on/off/dimming control per channel

**REST API Endpoint**: `/relay/<channel>/<value>`
- `<channel>`: Channel number(s) (can specify multiple: `0,1,2` or `all`)
- `<value>`: `on`, `off`, `0-100` (percentage)

**Example**:
```
GET /relay/0/on          # Turn relay 0 on
GET /relay/0,1,2/50      # Set relays 0,1,2 to 50% brightness
GET /relay/all/off       # Turn all relays off
```

---

### ScaderShades

**Purpose**: Control motorized window shades/blinds

**Features**:
- Controls up to 5 shade elements (configurable)
- Uses 74HC595 shift registers for control (3 bits per shade: UP/STOP/DOWN)
- Timed operations with automatic timeout (60 second maximum)
- Optional light level sensing (3 light sensors)
- Sequence control for smooth operation
- Named shade elements

**REST API Endpoint**: `/shade/<index>/<command>[/<duration>]`
- `<index>`: Shade index (0-based)
- `<command>`: `up`, `down`, `stop`
- `<duration>`: Optional duration in milliseconds (default 500ms pulse)

**Hardware**:
- 74HC595 shift register (SER, SCK, LATCH, RST pins)
- Light level sensors (optional)

**Example**:
```
GET /shade/0/up          # Move shade 0 up
GET /shade/1/down/5000   # Move shade 1 down for 5 seconds
GET /shade/2/stop        # Stop shade 2
```

---

### ScaderOpener

**Purpose**: Automated door opener with motor control, sensors, and UI

**Features**:
- Motor control with angle sensor (AS5600) and force sensor
- UI module integration for control panel
- Exit button support
- Motion/presence sensors on both sides
- Position feedback using angle sensor
- Force monitoring for obstruction detection
- State machine for door operation (opening/closing/stopped)
- Persistent state in NVS

**REST API Endpoint**: `/opener/<command>`
- `<command>`: Door control commands (open, close, stop, etc.)

**Hardware Notes**:
- AS5600 angle sensor is highly sensitive to positioning - must be very close to center of rotation
- Motor configuration requires accurate `stepsPerRot` setting for proper position control
- `unitsPerRot` should be 360 (degrees)
- Uses MotorMechanism class for coordinated control

**Example**:
```
GET /opener/open         # Open the door
GET /opener/close        # Close the door
GET /opener/stop         # Stop door movement
```

---

### ScaderLocks

**Purpose**: Electronic door lock control with door strike mechanisms

**Features**:
- Controls up to 2 door strikes (configurable)
- Bell/doorbell button detection
- Master door concept for reporting
- RFID tag reading queue
- Lock state change detection and reporting
- Named lock elements
- State persistence

**REST API Endpoints**:
- `/lock/<index>/<action>`: Lock control
  - `<action>`: `unlock`, `lock`
- `/tagread`: RFID tag read notification

**Hardware**:
- DoorStrike mechanisms (electronically controlled locks)
- Bell button input (with configurable active level)
- RFID reader interface

**Example**:
```
GET /lock/0/unlock       # Unlock door 0
GET /lock/1/lock         # Lock door 1
POST /tagread/<tagid>    # Report RFID tag read
```

---

### ScaderLEDPixels

**Purpose**: Control addressable LED strips (WS2812, etc.)

**Features**:
- Supports Raft Pixels library or FastLED library
- RGB and HSV color control
- Named pattern support (RainbowSnake, custom patterns)
- Per-pixel control and bulk operations
- Multiple LED strip segments with independent control
- Pattern parameters via query strings

**REST API Endpoints**: `/ledpix/<segment>/<command>[/<parameters>]`

Where `<segment>` is either:
- Segment name (as defined in configuration)
- Segment index number (0-based)

#### Available Commands:

1. **Set Single LED**:
   ```
   GET /ledpix/<segment>/set/<led_number>/<rgb_hex>
   GET /ledpix/<segment>/setled/<led_number>/<rgb_hex>
   ```
   - `<led_number>`: LED index (0-based)
   - `<rgb_hex>`: 6-character hex color (RRGGBB), optional '#' prefix
   
   **Examples**:
   ```
   GET /ledpix/0/set/0/FF0000        # Set LED 0 to red
   GET /ledpix/0/set/5/#00FF00       # Set LED 5 to green (with # prefix)
   GET /ledpix/strip1/setled/10/0000FF  # Set LED 10 to blue on named segment
   ```

2. **Set All LEDs to One Color**:
   ```
   GET /ledpix/<segment>/setall/<rgb_hex>[/<start_led>][/<end_led>]
   GET /ledpix/<segment>/color/<rgb_hex>[/<start_led>][/<end_led>]
   GET /ledpix/<segment>/colour/<rgb_hex>[/<start_led>][/<end_led>]
   ```
   - `<rgb_hex>`: 6-character hex color (RRGGBB), optional '#' prefix
   - `<start_led>`: Optional start LED index (default: 0)
   - `<end_led>`: Optional end LED index (default: all LEDs in segment)
   
   **Examples**:
   ```
   GET /ledpix/0/setall/FF00FF       # Set all LEDs to magenta
   GET /ledpix/0/color/FFFFFF/10/20  # Set LEDs 10-19 to white
   GET /ledpix/0/colour/#FFD700      # Set all LEDs to gold
   ```

3. **Set Multiple LEDs (Bulk Sequential)**:
   ```
   GET /ledpix/<segment>/setleds/<rgb_hex_string>
   ```
   - `<rgb_hex_string>`: Concatenated RGB values (6 chars per LED: RRGGBBRRGGBB...)
   - Each LED takes exactly 6 hex characters
   - LEDs are set sequentially starting from LED 0
   - Stops when string ends or segment is full
   
   **Examples**:
   ```
   GET /ledpix/0/setleds/FF0000FFFFFFFF     # LED 0=red, LED 1=white
   GET /ledpix/0/setleds/00FF0000000000FFFF # LED 0=green, LED 1=black, LED 2=blue
   ```
   
   **Limitations**:
   - Maximum LEDs per request depends on URL length limits:
     - **Typical HTTP server limit**: ~2048 characters (URL + headers)
     - **Practical limit**: ~300 LEDs per `setleds` command
     - Each LED requires 6 characters, so URL length ≈ base_path + (num_LEDs × 6)
     - For 300 LEDs: `/ledpix/0/setleds/` (19 chars) + 1800 chars = 1819 chars
   - For larger LED counts, use multiple requests or the `setall` command with ranges
   - Web browser URL limits vary (typically 2KB-8KB), so test your specific setup

4. **Set Multiple LEDs by Index (Sparse Updates)**:
   ```
   GET /ledpix/<segment>/setledsidx/<clear>/<data>
   ```
   - `<clear>`: Clear flag (`0` = don't clear first, `1` = clear before setting)
   - `<data>`: Repeating groups of index + color data
   - **Auto-detects RGB or RGBW mode** based on data length:
     - **RGB mode**: 10 hex chars per LED (4 for index + 6 for RGB)
     - **RGBW mode**: 12 hex chars per LED (4 for index + 8 for RGBW) - future support
   
   **RGB Mode Format** (current):
   - Each LED group: `IIIIRRGGGBB` (10 hex characters)
     - `IIII`: LED index (0000-FFFF = 0-65535)
     - `RR`: Red value (00-FF)
     - `GG`: Green value (00-FF)
     - `BB`: Blue value (00-FF)
   
   **RGBW Mode Format** (future):
   - Each LED group: `IIIIRRGGGBBWW` (12 hex characters)
     - `IIII`: LED index (0000-FFFF = 0-65535)
     - `RR`: Red value (00-FF)
     - `GG`: Green value (00-FF)
     - `BB`: Blue value (00-FF)
     - `WW`: White value (00-FF)
   
   **Examples**:
   ```
   # RGB mode - Set 3 specific LEDs without clearing first
   GET /ledpix/0/setledsidx/0/000AFF0000001400FFFFFF0064C8C8C8
   # LED 10 (000A) = red (FF0000)
   # LED 20 (0014) = white (FFFFFF)
   # LED 100 (0064) = light gray (C8C8C8)
   
   # RGB mode - Clear strip first, then set 2 LEDs
   GET /ledpix/0/setledsidx/1/000000FF00000064FF0000
   # Clear all, then LED 0 = green, LED 100 = red
   
   # RGBW mode - Future support (12 chars per LED)
   GET /ledpix/0/setledsidx/0/000AFF000000000140000FF00
   # LED 10 = red (R=FF, G=00, B=00, W=00)
   # LED 20 = blue+white (R=00, G=00, B=FF, W=00)
   ```
   
   **Advantages**:
   - **Sparse updates**: Only set LEDs that need to change
   - **Efficient**: For sparse patterns, uses much less URL space than `setleds`
   - **Flexible**: Set any LEDs in any order
   - **Clear control**: Explicit flag to clear strip before or not
   - **Future-proof**: Auto-detects RGBW when supported
   
   **Limitations**:
   - RGB mode: ~160 LEDs max per request (10 chars each + overhead)
   - RGBW mode: ~130 LEDs max per request (12 chars each + overhead)
   - Invalid LED indices are skipped with warning
   - Data length must be exact multiple of 10 (RGB) or 12 (RGBW)
   
   **Use Cases**:
   - Updating a few LEDs in a large strip
   - Creating sparse patterns or indicators
   - Animations that only affect certain LEDs
   - Overlay effects (with clear=0)

6. **Clear All LEDs**:
   ```
   GET /ledpix/<segment>/clear
   GET /ledpix/<segment>/off
   ```
   - Turns off all LEDs in the segment (sets to black/0,0,0)
   
   **Examples**:
   ```
   GET /ledpix/0/clear              # Turn off all LEDs
   GET /ledpix/strip1/off           # Turn off named segment
   ```

7. **Run Pattern**:
   ```
   GET /ledpix/<segment>/pattern/<pattern_name>[?param1=value1&param2=value2...]
   ```
   - `<pattern_name>`: Name of registered pattern (e.g., "RainbowSnake")
   - Optional query parameters are pattern-specific
   
   **Examples**:
   ```
   GET /ledpix/0/pattern/RainbowSnake
   GET /ledpix/0/pattern/RainbowSnake?speed=50&length=20
   ```

8. **List Available Patterns**:
   ```
   GET /ledpix/<segment>/listpatterns
   ```
   - Returns JSON array of available pattern names
   
   **Example Response**:
   ```json
   {
     "rslt": "ok",
     "patterns": ["RainbowSnake", "CustomPattern1"]
   }
   ```

**Supported Hardware**:
- WS2812/WS2812B LED strips (via Raft Pixels or FastLED)
- WS2811 LED strips
- Other addressable LED strips supported by the configured library

**Configuration Example**:
```json
{
  "ScaderLEDPixels": {
    "enable": true,
    "strips": [
      {"pin": 32, "num": 100, "name": "strip1"},
      {"pin": 33, "num": 50, "name": "strip2"}
    ],
    "brightnessPC": 20
  }
}
```

**Performance Notes**:
- Bulk operations (`setleds`, `setall`) stop any running patterns
- The `show()` command is called automatically after set operations
- For animated effects, use the pattern system rather than rapid API calls
- Pattern system handles timing and smooth animations internally

---

### ScaderRFID

**Purpose**: RFID reader integration for access control

**Features**:
- Multiple RFID reader support (up to 2)
- Activity LED indication
- Tag LED indication
- Buzzer feedback
- RFIDModuleBase abstraction for different reader types
- Door status change notifications
- State persistence

**REST API Endpoint**: `/rfid/doorstatus`
- Door status change notifications from lock system

**Hardware**:
- RFID reader module (configurable type)
- Activity LED
- Tag detection LED
- Buzzer for audio feedback

---

### ScaderPulseCounter

**Purpose**: Count pulses from sensors (utility meters, flow sensors, etc.)

**Features**:
- Single pulse input with debouncing
- Persistent pulse count in NVS
- Named counter
- Automatic state saving
- Debounce button handling for noise immunity

**REST API Endpoint**: `/pulsecounter/<command>`
- Commands to reset, read counter

**Use Cases**:
- Water meter pulse counting
- Gas meter monitoring
- Electricity meter pulse counting
- General event counting

**Example**:
```
GET /pulsecounter/read      # Read current pulse count
GET /pulsecounter/reset     # Reset pulse count
```

---

### ScaderElecMeters

**Purpose**: Multi-channel electricity monitoring using current transformers

**Features**:
- Up to 16 channels of current monitoring (configurable)
- SPI-based ADC reading (multiple chips)
- Real-time power calculation
- Current transformer (CT) processing
- Calibration support per channel
- 50Hz AC signal sampling (50 samples per cycle)
- RMS current and power calculation
- Dedicated worker task for high-speed sampling
- ISR-based sampling with configurable intervals

**REST API Endpoint**: `/elecmeters/<command>`
- Read power consumption per channel

**Hardware**:
- Current transformers (CTs) on each monitored circuit
- SPI-based ADC chips (up to 2 chips, 8 channels each)
- Precise timing via ESP32 timer

**Specifications**:
- Sampling rate: 2500 samples/second (50 Hz × 50 samples/cycle)
- Default mains voltage: 236V RMS
- Default CT calibration: 0.089 A per ADC unit
- Batch processing: 100 samples (2 cycles) every 5 seconds

---

### ScaderBTHome

**Purpose**: Bluetooth Low Energy device monitoring using BTHome protocol

**Features**:
- Monitors BLE devices broadcasting BTHome format
- Device data decoding
- Thread-safe update queue
- Integration with Raft bus system for BLE devices
- State reporting for monitored devices

**Use Cases**:
- Temperature/humidity sensors
- Motion sensors
- Door/window sensors
- Any BTHome-compatible BLE device

**REST API**: Status reporting via `/status`

---

### ScaderMarbleRun

**Purpose**: Control system for a marble run/kinetic sculpture

**Features**:
- Motor device control via Raft bus
- State persistence in NVS
- Custom control logic for marble run sequences

**REST API Endpoint**: `/marblerun/<command>`

**Hardware**:
- Motor controller via Raft device interface
- Position/state sensors as needed

---

### ScaderWaterer

**Purpose**: Automated plant watering system

**Features**:
- Multiple moisture sensor monitoring
- Pump control with timing
- Moisture threshold-based automation
- Named plant/zone support
- Busy state tracking

**REST API Endpoint**: `/waterer/<command>`
- Control pumps and read moisture levels

**Hardware**:
- Moisture sensors (MoistureSensors class)
- Water pumps (PumpControl class)

---

### ScaderCat

**Purpose**: Pet-related automation control (feeders, doors, etc.)

**Features**:
- Multiple timed output control
- Named outputs
- Configurable active levels (high/low)
- Duration-based activation
- State tracking per output

**REST API Endpoint**: `/cat/<output>/<action>[/<duration>]`

**Use Cases**:
- Pet feeder control
- Pet door control
- Pet camera control
- General timed outputs

## REST API

All Scader modules expose REST API endpoints following these conventions:

### Common Patterns

1. **Control Endpoints**: `GET /<module>/<element>/<action>[/<parameters>]`
   - Most modules use GET requests for simplicity
   - Actions are module-specific (on/off, open/close, up/down, etc.)

2. **Status Endpoint**: All modules report status via the system status endpoint
   - Returns JSON with module state, network info, uptime, version

3. **Response Format**: JSON responses with status and result data
   ```json
   {
     "rslt": "ok",
     "module": "ScaderRelays",
     "name": "Living Room Lights",
     "version": "1.0.0",
     ...
   }
   ```

### Authentication

REST API authentication and access control is handled by the Raft framework's networking layer.

## Configuration

Each Scader module is configured through the system configuration (typically JSON format). Common configuration elements:

```json
{
  "ScaderRelays": {
    "enable": true,
    "ScaderCommon": {
      "name": "Living Room Controller",
      "hostname": "scader-livingroom"
    },
    "maxElems": 24,
    "elemNames": ["Light 1", "Light 2", "Fan"],
    ...
  }
}
```

### Common Configuration Parameters

All modules support:
- `enable`: Boolean to enable/disable the module
- `ScaderCommon/name`: Friendly name shown in UI
- `ScaderCommon/hostname`: Network hostname

Module-specific configuration is documented in each module's header file.

## Hardware Support

ScaderESP32 uses the Raft bus system for hardware abstraction, supporting:

- **I2C devices**: Sensors, displays, motor controllers
- **SPI devices**: LED drivers, ADCs
- **GPIO**: Direct pin control
- **UART**: Serial devices
- **Motors**: Stepper motors, servo motors via MotorControl
- **Sensors**: HX711 load cells, AS5600 angle sensors, MMWave motion detection

Hardware detection is performed automatically at startup via `DetectHardware` class.

## Contributing

This project is built on the Raft framework. For contributions:

1. Follow the existing module pattern (inherit from `RaftSysMod`)
2. Use `ScaderCommon` for standard functionality
3. Implement REST API endpoints via `addRestAPIEndpoints()`
4. Persist state using `RaftJsonNVS`
5. Document your module in this README

## License

See LICENSE file for details.

## More Information

- **Raft Framework**: [https://github.com/robdobsn/RaftCore](https://github.com/robdobsn/RaftCore)
- **RaftCLI**: [https://github.com/robdobsn/RaftCLI](https://github.com/robdobsn/RaftCLI)
- **Author**: Rob Dobson (http://robdobson.com)

