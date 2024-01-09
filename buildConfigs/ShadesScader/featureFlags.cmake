# System naming
add_compile_definitions(HW_SYSTEM_NAME=ShadesScader)
add_compile_definitions(HW_DEFAULT_FRIENDLY_NAME=ShadesScader)
add_compile_definitions(HW_DEFAULT_HOSTNAME=Scader)
add_compile_definitions(HW_DEFAULT_ADVNAME=Scader)
add_compile_definitions(HW_SERIAL_NUMBER_BYTES=16)
add_compile_definitions(HW_SERIAL_SET_MAGIC_STR=Magic)

# Main features
add_compile_definitions(FEATURE_NETWORK_FUNCTIONALITY)
add_compile_definitions(FEATURE_WEB_SERVER_OR_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SERVER_STATIC_FILES)
add_compile_definitions(FEATURE_MQTT_MANAGER)
# add_compile_definitions(FEATURE_BLE_FUNCTIONALITY)
add_compile_definitions(FEATURE_INCLUDE_SCADER)
add_compile_definitions(FEATURE_ETHERNET_HARDWARE_OLIMEX)
add_compile_definitions(FEATURE_FORCE_HARDWARE_REVISION=4)

# LittleFS configuration
add_compile_definitions(CONFIG_LITTLEFS_MAX_PARTITIONS=3)
add_compile_definitions(CONFIG_LITTLEFS_PAGE_SIZE=256)
add_compile_definitions(CONFIG_LITTLEFS_READ_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_WRITE_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_LOOKAHEAD_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_CACHE_SIZE=512)
add_compile_definitions(CONFIG_LITTLEFS_BLOCK_CYCLES=512)

# File system
set(FS_TYPE "spiffs")
set(FS_IMAGE_PATH "${BUILD_CONFIG_COMMON_DIR}/FSImage")

# Web UI
set(UI_SOURCE_PATH "${BUILD_CONFIG_COMMON_DIR}/WebUI")
set(WEB_UI_GEN_FLAGS --deletefirst)
# set(WEB_UI_GEN_FLAGS --deletefirst --nogzip)

# Firmware image name
set(FW_IMAGE_NAME "ShadesScaderFW")

# Add optional components
# set(OPTIONAL_COMPONENTS ${OPTIONAL_COMPONENTS} "")
