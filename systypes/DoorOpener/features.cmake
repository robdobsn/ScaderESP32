# Set the target Espressif chip
set(IDF_TARGET "esp32s3")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# Set door opener features
add_compile_definitions(HW_DEF_DOOR_OPENER)
