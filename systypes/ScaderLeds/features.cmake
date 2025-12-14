# Set the target Espressif chip
set(IDF_TARGET "esp32")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# Enable the ethernet hardware for Olimex ESP32 PoE-ISO boards
add_compile_definitions(HW_ETH_PHY_LAN87XX)

# ScaderLEDPixels now uses SimpleRMTLeds driver (no external dependencies needed)
message(STATUS "ScaderLeds: Using SimpleRMTLeds driver with LEDPixels wrapper")
