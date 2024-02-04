# Set the target Espressif chip
set(IDF_TARGET "esp32")

# System version
add_compile_definitions(SYSTEM_VERSION="6.0.7")

# Raft components
set(RAFT_COMPONENTS
    RaftSysMods@ReWorkConfigBase
    RaftI2C@ReWorkConfigBase
    RaftMotorControl@ReWorkConfigBase
    RaftWebServer@ReWorkConfigBase
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
