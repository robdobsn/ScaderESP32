# Set the target Espressif chip
set(IDF_TARGET "esp32s3")

# Include the test system module
add_compile_definitions(FEATURE_INCLUDE_SCADER_TEST_SYS_MOD)

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")
