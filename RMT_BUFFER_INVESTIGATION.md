# RMT Buffer and Timing Investigation for 1000 LEDs
## Investigation Date: December 17, 2025

## Question
Can the RMT buffer issues with 1000 LEDs be solved through:
1. Larger initial RMT buffer (no feeding needed)?
2. Higher priority interrupts?
3. Moving code to different processor cores?

## Hardware Limitations (ESP32 Original)

### Critical Limitation: NO DMA Support
**ESP32 (original) does NOT support DMA for RMT peripheral**

From ESP-IDF documentation:
- `with_dma` flag exists but is NOT supported on ESP32
- DMA is only available on ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
- Attempting to enable DMA on ESP32 returns `ESP_ERR_NOT_SUPPORTED`

**Current configuration (both implementations):**
```cpp
.flags.with_dma = false,  // No DMA on ESP32
```

### RMT Memory Architecture on ESP32

**Hardware Memory Block:**
- ESP32 has 512 × 32-bit memory blocks shared among all RMT channels
- Each channel can use 1-8 memory blocks (64-512 symbols)
- Memory is statically allocated per channel
- **Cannot be expanded beyond hardware limits**

**Symbol Format:**
- Each RMT symbol = 32 bits (4 bytes)
- Contains: duration0 (16-bit), level0 (1-bit), duration1 (16-bit), level1 (1-bit)

**Current configuration:**
```cpp
.mem_block_symbols = 64,  // 64 symbols × 4 bytes = 256 bytes
```

## Memory Requirements for 1000 LEDs

### Data Size Calculation

**Per LED:**
- 3 bytes (RGB) = 24 bits
- Each bit encoded as 1 RMT symbol
- 24 symbols per LED

**For 1000 LEDs:**
- Data: 3000 bytes (3KB)
- RMT symbols needed: 24,000 symbols
- RMT memory required: 24,000 × 4 bytes = **96KB**

**Plus overhead:**
- Reset code: 1 symbol
- Total: 24,001 symbols = **96.004KB**

### Answer to Question 1: Can buffer be big enough?

**❌ NO - Hardware limitation**

Maximum RMT memory per channel on ESP32:
- 8 memory blocks maximum
- 8 × 64 symbols = 512 symbols
- 512 × 4 bytes = **2KB maximum**

**Gap:**
- Need: 96KB
- Have: 2KB
- **Shortfall: 48× too small**

Even if we allocate maximum memory blocks:
```cpp
.mem_block_symbols = 512,  // Maximum possible
```
This only holds: 512 / 24 = **21 LEDs worth of data**

For 1000 LEDs, buffer must be refilled: 1000 / 21 = **~48 times during transmission**

## Ping-Pong Buffer Mechanism

Since buffer can't hold all data, ESP-IDF RMT driver uses **ping-pong encoding**:

### How It Works
1. **ISR Context:** Encoding happens in interrupt handler
2. **Double Buffering:** While hardware reads from one half, software fills the other
3. **Callback-Based:** Encoder's `encode()` function called multiple times
4. **Stateful:** Encoder must track position in data stream

### Critical Timing Window

**Per LED timing:**
- 24 bits × (1.25μs average) = **30μs per LED**

**Buffer refill must complete within:**
- 64 symbols ÷ 24 symbols/LED = 2.67 LEDs worth
- 2.67 × 30μs = **~80μs**

**If encoder is delayed > 80μs:**
- Hardware reads uninitialized memory
- Results in: Wrong colors, glitches, repeated patterns

## Current Interrupt Configuration

**Both implementations use:**
```cpp
.intr_priority = 0,  // Let driver choose (level 1, 2, or 3)
```

### ESP32 Interrupt Priority Levels
- **Level 0:** Disabled
- **Level 1:** Lowest priority (default for most peripherals)
- **Level 2:** Medium priority  
- **Level 3:** High priority
- **Level 4-5:** Very high priority (special)
- **Level 6-7:** NMI (Non-Maskable Interrupt, DO NOT USE)

### Answer to Question 2: Can higher priority help?

**✅ YES - But with caveats**

**Current situation:**
```cpp
.intr_priority = 0,  // Driver uses level 1, 2, or 3
```

**Can be improved to:**
```cpp
.intr_priority = 3,  // Maximum safe priority
```

**Benefits:**
- RMT interrupt preempts lower-priority ISRs (WiFi, Bluetooth, timers)
- Reduces worst-case latency
- Less likely to be blocked by other interrupts

**Limitations:**
- Still can be delayed by:
  - Level 3 interrupts from other sources
  - Cache misses (if encoder not in IRAM)
  - Flash operations (if cache disabled)
  - FreeRTOS critical sections

**To maximize effectiveness, MUST also:**
1. Put encoder in IRAM: `IRAM_ATTR` or `RMT_ENCODER_FUNC_ATTR`
2. Enable cache-safe ISR: `CONFIG_RMT_TX_ISR_CACHE_SAFE`
3. Minimize encoder complexity

## Cache-Safe Configuration

### Problem: Cache Disabled During Flash Operations
When ESP32 writes to flash (e.g., logging, NVS writes), cache is disabled:
- Code in flash cannot execute
- RMT interrupt delayed until cache re-enabled
- Can be **10-100ms delay**

### Solution: IRAM Placement
```cpp
// In sdkconfig:
CONFIG_RMT_TX_ISR_CACHE_SAFE=y

// In encoder code:
RMT_ENCODER_FUNC_ATTR 
static size_t rmt_encode_led_strip(...) {
    // Encoder code
}
```

**Effect:**
- ISR runs even when cache disabled
- Encoder code in IRAM (never cached)
- Consistent timing regardless of flash operations

**Cost:**
- Increased IRAM usage (~1-2KB for encoder)
- Worth it for real-time LED control

## Answer to Question 3: Core Affinity

### ESP32 Dual-Core Architecture
- **Core 0 (PRO_CPU):** Protocol processing, system tasks
- **Core 1 (APP_CPU):** Application tasks (default for user code)

**Current situation:**
- RMT interrupt can run on either core (driver decides)
- If WiFi/BT active on same core, they compete for CPU time

### Can pinning to different core help?

**⚠️ PARTIAL - But not directly controllable**

**RMT driver does NOT expose core affinity API**
- Interrupt core is determined by driver initialization
- Usually runs on core where `rmt_new_tx_channel()` was called
- No documented way to change after creation

**Indirect optimization:**
1. Initialize RMT from Core 1 task
2. Keep WiFi/BT on Core 0
3. Reduces interrupt latency from competing protocols

**Code pattern:**
```cpp
void ledInitTask(void* param) {
    // This runs on Core 1
    rmt_new_tx_channel(&config, &channel);
    vTaskDelete(NULL);
}

xTaskCreatePinnedToCore(ledInitTask, "LEDInit", 2048, NULL, 5, NULL, 1);
```

**Effectiveness: Limited**
- Helps if WiFi/BT causing issues
- Doesn't solve fundamental buffer size problem
- RMT still shares bus/memory with other core's operations

## Root Cause Analysis: Why Differences in Behavior?

### Comparing RaftCore vs SimpleRMTLeds

**Configuration differences that matter:**

1. **Buffer Size:**
   - Both use: 64 symbols (same)
   - Not the issue

2. **Interrupt Priority:**
   - Both use: 0 (same)
   - Could improve both

3. **Encoder Complexity:**
   - RaftCore: Uses generic encoder with callbacks
   - SimpleRMTLeds: Direct inline encoding
   - **SimpleRMTLeds likely faster**

4. **IRAM Placement:**
   - RaftCore: Depends on CONFIG_RMT_TX_ISR_CACHE_SAFE
   - SimpleRMTLeds: Depends on compilation flags
   - **Check your sdkconfig!**

5. **Timing Values (CRITICAL):**
   - Already identified in LED_TIMING_FIX_ANALYSIS.md
   - Wrong timing = wrong behavior

## Recommendations (In Priority Order)

### 1. Fix Timing Values First ⭐⭐⭐
From LED_TIMING_FIX_ANALYSIS.md:
- Update to WS2812 spec timing
- Increase resolution to 80MHz
- Fix reset code duration

**This is likely the main issue!**

### 2. Enable Cache-Safe ISR ⭐⭐⭐
Add to sdkconfig:
```
CONFIG_RMT_TX_ISR_CACHE_SAFE=y
CONFIG_RMT_RECV_FUNC_IN_IRAM=y
```

Put encoder in IRAM:
```cpp
RMT_ENCODER_FUNC_ATTR
static size_t rmt_encode_led_strip(...) {
    // encoder code
}
```

### 3. Increase Interrupt Priority ⭐⭐
Change from:
```cpp
.intr_priority = 0,
```
To:
```cpp
.intr_priority = 3,  // Highest safe priority
```

### 4. Optimize Encoder ⭐
- Minimize work in ISR context
- Avoid function calls
- Pre-calculate values where possible
- Use inline functions

### 5. Core Affinity (Optional) ⭐
- Initialize RMT from Core 1
- Keep WiFi on Core 0
- Monitor with task profiling

## Testing Strategy

### Phase 1: Measure Current Behavior
```cpp
// In encoder callback:
static uint32_t max_latency = 0;
static uint32_t last_time = 0;

uint32_t now = esp_timer_get_time();
if (last_time != 0) {
    uint32_t latency = now - last_time;
    if (latency > max_latency) {
        max_latency = latency;
        LOG_W("RMT", "Max latency: %u us", max_latency);
    }
}
last_time = now;
```

**Expected:** Should see < 80μs between encoder calls
**If seeing:** > 100μs, you have interrupt latency issues

### Phase 2: Apply Fixes Incrementally
1. Test timing fix alone
2. Add cache-safe config
3. Increase interrupt priority
4. Compare results at each step

### Phase 3: Stress Test
- Enable WiFi scanning
- Enable Bluetooth
- Perform flash writes
- Monitor LED behavior during each

## Conclusion

### Q1: Can buffer be big enough for 1000 LEDs?
**NO** - Hardware limit is 2KB, need 96KB. Must use ping-pong refilling.

### Q2: Can higher interrupt priority help?
**YES** - Set `intr_priority = 3` and enable `CONFIG_RMT_TX_ISR_CACHE_SAFE`.
This reduces latency but doesn't eliminate refilling need.

### Q3: Can different core help?
**MAYBE** - Initialize from Core 1 to avoid WiFi/BT contention on Core 0.
Limited benefit, not directly controllable.

### Most Likely Issues (In Order):
1. **Wrong timing values** (from LED_TIMING_FIX_ANALYSIS.md) ⭐⭐⭐
2. **Cache disabled during flash ops** (enable cache-safe) ⭐⭐⭐
3. **Low interrupt priority** (increase to 3) ⭐⭐
4. **Encoder not in IRAM** (add IRAM_ATTR) ⭐⭐
5. **Interrupt contention with WiFi/BT** (core affinity) ⭐

### Implementation Priority:
1. Fix timing values (LED_TIMING_FIX_ANALYSIS.md)
2. Enable CONFIG_RMT_TX_ISR_CACHE_SAFE
3. Set intr_priority = 3
4. Put encoder in IRAM
5. Test with and without WiFi/BT active
6. Consider core affinity if still issues

The ping-pong refilling is **unavoidable** on ESP32 original. The key is making it reliable through proper timing, interrupt priority, and cache-safe operation.
