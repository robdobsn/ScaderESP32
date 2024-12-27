# Set the target Espressif chip
set(IDF_TARGET "esp32")

# Include common features
# include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# ##############################
# This section is a copy of Common/features.cmake with the following changes:
# - RaftMotorControl is set to the work-on-testing branch

# System version
add_compile_definitions(SYSTEM_VERSION="6.5.8")

# Raft components
set(RAFT_COMPONENTS
    RaftSysMods@main
    RaftI2C@main
    RaftMotorControl@work-on-testing
    RaftWebServer@main
)

# File system
set(FS_TYPE "littlefs")
set(FS_IMAGE_PATH "../Common/FSImage")

# Web UI
set(UI_SOURCE_PATH "../Common/WebUI")
# Uncomment the following line if you do not want to gzip the web UI
# set(WEB_UI_GEN_FLAGS ${WEB_UI_GEN_FLAGS} --nogzip)
# Uncomment the following line to include a source map for the web UI - this will increase the size of the web UI
# set(WEB_UI_GEN_FLAGS ${WEB_UI_GEN_FLAGS} --incmap)

# add_compile_definitions(DEBUG_USING_GLOBAL_VALUES)
# add_compile_definitions(DEBUG_NETWORK_EVENTS_DETAIL)
# add_compile_definitions(DEBUG_LIST_SYSMODS)
