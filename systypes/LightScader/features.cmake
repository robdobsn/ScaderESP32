# Set the target Espressif chip
set(IDF_TARGET "esp32")

# System version
add_compile_definitions(SYSTEM_VERSION="6.0.0")

# Scader
add_compile_definitions(FEATURE_INCLUDE_SCADER)

# Enable the ethernet hardware for Olimex ESP32 PoE-ISO boards
add_compile_definitions(HW_ETH_PHY_LAN87XX)

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
# set(WEB_UI_GEN_FLAGS --nogzip)