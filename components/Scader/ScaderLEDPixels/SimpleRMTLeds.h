// SimpleRMTLeds.h
// Simple RMT-based LED strip driver for WS2811/WS2812 LEDs
// Based on working FastLED RMT implementation
// Rob Dobson 2024

#pragma once

#include <stdint.h>
#include <vector>
#include "driver/rmt_tx.h"
#include "esp_err.h"

class SimpleRMTLeds
{
public:
    SimpleRMTLeds();
    ~SimpleRMTLeds();

    // Initialize the LED strip
    // pin: GPIO pin number
    // numPixels: number of LEDs in the strip  
    // Returns true on success
    bool init(int pin, uint32_t numPixels);

    // Set a single pixel color (RGB)
    void setPixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

    // Clear all pixels
    void clear();

    // Update the strip with current pixel data
    bool show();

    // Get number of pixels
    uint32_t getNumPixels() const { return _numPixels; }

private:
    // RMT channel handle
    rmt_channel_handle_t _rmtChannel;
    rmt_encoder_handle_t _encoder;
    
    // LED data
    std::vector<uint8_t> _pixelData;  // RGB data (3 bytes per pixel)
    uint32_t _numPixels;
    int _pin;
    bool _initialized;

    // Encoder configuration
    static const uint32_t WS2811_T0H_NS = 350;   // 0 bit high time (ns)
    static const uint32_t WS2811_T0L_NS = 800;   // 0 bit low time (ns)
    static const uint32_t WS2811_T1H_NS = 700;   // 1 bit high time (ns)
    static const uint32_t WS2811_T1L_NS = 600;   // 1 bit low time (ns)
    static const uint32_t WS2811_RESET_US = 50;  // Reset time (us)
};
