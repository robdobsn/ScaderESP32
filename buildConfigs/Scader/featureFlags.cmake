# System naming
add_compile_definitions(HW_SYSTEM_NAME=Scader)
add_compile_definitions(HW_DEFAULT_FRIENDLY_NAME=Scader)
add_compile_definitions(HW_DEFAULT_HOSTNAME=Scader)
add_compile_definitions(HW_DEFAULT_ADVNAME=Scader)

# Main features
add_compile_definitions(FEATURE_NETWORK_FUNCTIONALITY)
add_compile_definitions(FEATURE_WEB_SERVER_OR_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SERVER_STATIC_FILES)
#add_compile_definitions(FEATURE_BLE_FUNCTIONALITY)

# LittleFS configuration
add_compile_definitions(CONFIG_LITTLEFS_MAX_PARTITIONS=3)
add_compile_definitions(CONFIG_LITTLEFS_PAGE_SIZE=256)
add_compile_definitions(CONFIG_LITTLEFS_READ_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_WRITE_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_LOOKAHEAD_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_CACHE_SIZE=512)
add_compile_definitions(CONFIG_LITTLEFS_BLOCK_CYCLES=512)

# Ethernet hardare
add_compile_definitions(ETHERNET_HARDWARE_OLIMEX)

# File system
set(FS_TYPE "spiffs")
set(FS_IMAGE_PATH "${BUILD_CONFIG_DIR}/FSImage")

# Web UI
set(UI_SOURCE_PATH "${BUILD_CONFIG_DIR}/WebUI")
set(WEB_UI_GEN_FLAGS --deletefirst)
# set(WEB_UI_GEN_FLAGS --deletefirst --nogzip)

# Firmware image name
set(FW_IMAGE_NAME "ScaderFW")

# Add optional components
# set(OPTIONAL_COMPONENTS ${OPTIONAL_COMPONENTS} "")

# Additional hardware definitions
add_compile_definitions(FEATURE_INCLUDE_SCADER)
