# Set the target Espressif chip
set(IDF_TARGET "esp32s3")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# ScaderLEDPixels now uses SimpleRMTLeds driver (no external dependencies needed)
message(STATUS "ScaderLeds: Using SimpleRMTLeds driver with LEDPixels wrapper")
