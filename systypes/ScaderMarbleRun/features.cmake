# Set the target Espressif chip
set(IDF_TARGET "esp32")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# Uncomment the following line to include a source map for the web UI - this will increase the size of the web UI
# set(WEB_UI_GEN_FLAGS ${WEB_UI_GEN_FLAGS} --incmap)