// SimpleRMTLeds.cpp
// Simple RMT-based LED strip driver for WS2811/WS2812 LEDs
// Rob Dobson 2024

#include "SimpleRMTLeds.h"
#include "Logger.h"
#include "esp_check.h"
#include "driver/rmt_encoder.h"
#include "esp_attr.h"

static const char* MODULE_PREFIX = "SimpleRMTLeds";

// RMT resolution (80MHz = 12.5ns per tick)
#define RMT_LED_STRIP_RESOLUTION_HZ 80000000

// Encoder for WS2812/WS2811 protocol
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

// Encoder callback - placed in IRAM for fast interrupt response
static IRAM_ATTR size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                   const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    
    switch (led_encoder->state) {
        case 0: // send RGB data
            encoded_symbols += led_encoder->bytes_encoder->encode(led_encoder->bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = 1; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
                goto out; // yield if there's no free space to put other encoding artifacts
            }
        // fall-through
        case 1: // send reset code
            encoded_symbols += led_encoder->copy_encoder->encode(led_encoder->copy_encoder, channel, &led_encoder->reset_code,
                                                                sizeof(rmt_symbol_word_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
                state = (rmt_encode_state_t)(state | RMT_ENCODING_COMPLETE);
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
                goto out; // yield if there's no free space to put other encoding artifacts
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static IRAM_ATTR esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static IRAM_ATTR esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

// Create LED strip encoder
static esp_err_t rmt_new_led_strip_encoder(rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    
    led_encoder = (rmt_led_strip_encoder_t*)calloc(1, sizeof(rmt_led_strip_encoder_t));
    if (!led_encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    
    // WS2812 timing (in RMT ticks at 80MHz = 12.5ns per tick)
    // T0H: 350ns = 28 ticks (actual: 350ns)
    // T0L: 800ns = 64 ticks (actual: 800ns)
    // T1H: 700ns = 56 ticks (actual: 700ns)
    // T1L: 600ns = 48 ticks (actual: 600ns)
    rmt_bytes_encoder_config_t bytes_encoder_config = {};
    bytes_encoder_config.bit0.level0 = 1;
    bytes_encoder_config.bit0.duration0 = 28;  // 350ns
    bytes_encoder_config.bit0.level1 = 0;
    bytes_encoder_config.bit0.duration1 = 64;  // 800ns
    bytes_encoder_config.bit1.level0 = 1;
    bytes_encoder_config.bit1.duration0 = 56;  // 700ns
    bytes_encoder_config.bit1.level1 = 0;
    bytes_encoder_config.bit1.duration1 = 48;  // 600ns
    bytes_encoder_config.flags.msb_first = 1;   // WS2812 is MSB first
    
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        if (led_encoder) {
            free(led_encoder);
        }
        return ret;
    }
    
    // Reset code: low for >50us
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder);
    if (ret != ESP_OK) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder) {
            free(led_encoder);
        }
        return ret;
    }
    
    uint32_t reset_ticks = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50; // 50us
    led_encoder->reset_code.level0 = 0;
    led_encoder->reset_code.duration0 = reset_ticks;
    led_encoder->reset_code.level1 = 0;
    led_encoder->reset_code.duration1 = 0;
    
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
}

SimpleRMTLeds::SimpleRMTLeds()
    : _rmtChannel(nullptr)
    , _encoder(nullptr)
    , _numPixels(0)
    , _pin(-1)
    , _initialized(false)
{
}

SimpleRMTLeds::~SimpleRMTLeds()
{
    if (_encoder) {
        rmt_del_encoder(_encoder);
    }
    if (_rmtChannel) {
        rmt_disable(_rmtChannel);
        rmt_del_channel(_rmtChannel);
    }
}

bool SimpleRMTLeds::init(int pin, uint32_t numPixels)
{
    if (_initialized) {
        LOG_W(MODULE_PREFIX, "Already initialized");
        return false;
    }

    _pin = pin;
    _numPixels = numPixels;
    _pixelData.resize(numPixels * 3, 0);  // 3 bytes per pixel (RGB)

    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {};
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_chan_config.gpio_num = (gpio_num_t)pin;
    tx_chan_config.mem_block_symbols = 96;  // Max for ESP32-S3 with 2 channels (192 total / 2)
    tx_chan_config.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ;
    tx_chan_config.trans_queue_depth = 4;
    tx_chan_config.intr_priority = 3;  // High priority for reliable interrupt-driven refills
    tx_chan_config.flags.invert_out = false;
    // DMA disabled: allows both channels to work simultaneously without GDMA resource conflicts
    tx_chan_config.flags.with_dma = false;

    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &_rmtChannel);
    if (ret != ESP_OK) {
        LOG_E(MODULE_PREFIX, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        return false;
    }

    // Create LED strip encoder
    ret = rmt_new_led_strip_encoder(&_encoder);
    if (ret != ESP_OK) {
        LOG_E(MODULE_PREFIX, "Failed to create LED strip encoder: %s", esp_err_to_name(ret));
        rmt_del_channel(_rmtChannel);
        _rmtChannel = nullptr;
        return false;
    }

    // Enable RMT channel
    ret = rmt_enable(_rmtChannel);
    if (ret != ESP_OK) {
        LOG_E(MODULE_PREFIX, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        rmt_del_encoder(_encoder);
        rmt_del_channel(_rmtChannel);
        _encoder = nullptr;
        _rmtChannel = nullptr;
        return false;
    }

    _initialized = true;
    LOG_I(MODULE_PREFIX, "Initialized pin=%d pixels=%d", pin, numPixels);
    return true;
}

void SimpleRMTLeds::setPixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= _numPixels) {
        return;
    }

    // WS2812/WS2811 order is GRB
    uint32_t offset = index * 3;
    _pixelData[offset + 0] = g;
    _pixelData[offset + 1] = r;
    _pixelData[offset + 2] = b;
}

void SimpleRMTLeds::clear()
{
    std::fill(_pixelData.begin(), _pixelData.end(), 0);
}

bool SimpleRMTLeds::show()
{
    if (!_initialized) {
        return false;
    }

    // Transmit data
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;  // no loop

    esp_err_t ret = rmt_transmit(_rmtChannel, _encoder, _pixelData.data(), _pixelData.size(), &tx_config);
    if (ret != ESP_OK) {
        LOG_W(MODULE_PREFIX, "Failed to transmit: %s", esp_err_to_name(ret));
        return false;
    }

    // Wait for transmission to complete
    ret = rmt_tx_wait_all_done(_rmtChannel, 100);  // 100ms timeout
    if (ret != ESP_OK) {
        LOG_W(MODULE_PREFIX, "Transmission timeout");
        return false;
    }

    return true;
}
