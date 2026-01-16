# RaftCore RMT LED Driver Fix Analysis

## Problem
The RaftCore ESP32RMTLedStrip implementation has spurious color problems with WS2811/WS2812 LEDs.

## Root Causes

### 1. Incorrect Default Timing Values
**RaftCore defaults (LEDStripConfig.h):**
```cpp
static constexpr const double PIX_BIT_0_0_US_DEFAULT = 0.3;   // T0H: 300ns
static constexpr const double PIX_BIT_0_1_US_DEFAULT = 0.9;   // T0L: 900ns
static constexpr const double PIX_BIT_1_0_US_DEFAULT = 0.9;   // T1H: 900ns
static constexpr const double PIX_BIT_1_1_US_DEFAULT = 0.3;   // T1L: 300ns
```

**WS2812 Specification:**
- T0H: 350ns (±150ns) - Bit 0 high time
- T0L: 800ns (±150ns) - Bit 0 low time
- T1H: 700ns (±150ns) - Bit 1 high time
- T1L: 600ns (±150ns) - Bit 1 low time

**Issue:** The defaults are too far from spec, especially:
- T0H: 300ns vs 350ns required (-50ns = -14% error)
- T1H: 900ns vs 700ns required (+200ns = +29% error)
- T1L: 300ns vs 600ns required (-300ns = -50% error)

### 2. Low Clock Resolution
**RaftCore:**
```cpp
uint32_t rmtResolutionHz = 10000000;  // 10MHz = 100ns per tick
```

**Issue:** 
- At 10MHz, each tick is 100ns
- T0H (350ns) = 3.5 ticks → rounds to 3 ticks = 300ns
- T1H (700ns) = 7 ticks = 700ns (OK by luck)
- T1L (600ns) = 6 ticks = 600ns (OK by luck)
- **Rounding errors accumulate over long LED strips**

**SimpleRMTLeds (working):**
```cpp
#define RMT_LED_STRIP_RESOLUTION_HZ 80000000  // 80MHz = 12.5ns per tick
```
- At 80MHz, each tick is 12.5ns
- T0H (350ns) = 28 ticks = 350ns (exact)
- T0L (800ns) = 64 ticks = 800ns (exact)
- T1H (700ns) = 56 ticks = 700ns (exact)
- T1L (600ns) = 48 ticks = 600ns (exact)

### 3. Reset Code Implementation Issue
**RaftCore (LEDStripEncoder.c):**
```c
led_encoder->reset_code = (rmt_symbol_word_t) {
    .level0 = 0,
    .duration0 = config->reset_ticks,  // e.g., 500 ticks
    .level1 = 0,
    .duration1 = config->reset_ticks,  // e.g., 500 ticks  <-- WRONG!
};
```
This creates a reset pulse that's **twice as long** as configured (50μs × 2 = 100μs).

**SimpleRMTLeds (correct):**
```cpp
led_encoder->reset_code.level0 = 0;
led_encoder->reset_code.duration0 = reset_ticks;  // 50μs
led_encoder->reset_code.level1 = 0;
led_encoder->reset_code.duration1 = 0;            // <-- Correct!
```

## Recommended Fixes for RaftCore

### Fix 1: Update Default Timing Values (LEDStripConfig.h)
```cpp
// Change from:
static constexpr const double PIX_BIT_0_0_US_DEFAULT = 0.3;
static constexpr const double PIX_BIT_0_1_US_DEFAULT = 0.9;
static constexpr const double PIX_BIT_1_0_US_DEFAULT = 0.9; 
static constexpr const double PIX_BIT_1_1_US_DEFAULT = 0.3;

// To:
static constexpr const double PIX_BIT_0_0_US_DEFAULT = 0.35;   // 350ns
static constexpr const double PIX_BIT_0_1_US_DEFAULT = 0.80;   // 800ns
static constexpr const double PIX_BIT_1_0_US_DEFAULT = 0.70;   // 700ns
static constexpr const double PIX_BIT_1_1_US_DEFAULT = 0.60;   // 600ns
```

### Fix 2: Increase Clock Resolution (LEDStripConfig.h)
```cpp
// Change from:
uint32_t rmtResolutionHz = 10000000;  // 10MHz

// To:
uint32_t rmtResolutionHz = 80000000;  // 80MHz
```

### Fix 3: Fix Reset Code Duration (LEDStripEncoder.c)
```c
// Change from:
led_encoder->reset_code = (rmt_symbol_word_t) {
    .level0 = 0,
    .duration0 = config->reset_ticks,
    .level1 = 0,
    .duration1 = config->reset_ticks,  // <-- Remove this
};

// To:
led_encoder->reset_code = (rmt_symbol_word_t) {
    .level0 = 0,
    .duration0 = config->reset_ticks,
    .level1 = 0,
    .duration1 = 0,  // <-- Set to 0
};
```

## Impact Analysis

### Before Fixes (10MHz, wrong timings):
- Timing errors up to 50%
- Rounding errors accumulate over 1000+ LEDs
- Reset pulse twice as long as needed
- Result: Spurious colors, glitches

### After Fixes (80MHz, correct timings):
- Exact WS2812 timing
- No rounding errors
- Correct reset duration
- Result: Reliable operation (proven by SimpleRMTLeds)

## Testing
The SimpleRMTLeds driver with these corrected values works perfectly with:
- 2x WS2811 strips
- 1000 LEDs each
- No spurious colors
- Reliable operation

## Files to Modify in RaftCore
1. `components/core/LEDPixels/LEDStripConfig.h` - Fix defaults and resolution
2. `components/core/LEDPixels/LEDStripEncoder.c` - Fix reset code duration
