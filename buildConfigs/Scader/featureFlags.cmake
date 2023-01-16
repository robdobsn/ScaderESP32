# System naming
add_compile_definitions(HW_SYSTEM_NAME=Scader)
add_compile_definitions(HW_DEFAULT_FRIENDLY_NAME=Scader)
add_compile_definitions(HW_DEFAULT_HOSTNAME=Scader)
add_compile_definitions(HW_DEFAULT_ADVNAME=Scader)

# Hardware revision detection
# add_compile_definitions(HW_REVISION_ESP32_LIMIT_ESPREV=3)
# add_compile_definitions(HW_REVISION_ESP32_LIMIT_HWREV=1)
# add_compile_definitions(HW_REVISION_LADDER_PIN=32)
# add_compile_definitions(HW_REVISION_LOW_THRESH_1=1700)
# add_compile_definitions(HW_REVISION_HIGH_THRESH_1=2050)
# add_compile_definitions(HW_REVISION_HWREV_THRESH_1=4)
# add_compile_definitions(HW_REVISION_NO_LADDER_HWREV=2)

# Main features
# add_compile_definitions(FEATURE_POWER_UP_LED_ASAP)
add_compile_definitions(FEATURE_WIFI_FUNCTIONALITY)
add_compile_definitions(FEATURE_WEB_SERVER_OR_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SERVER_STATIC_FILES)
# add_compile_definitions(FEATURE_MP3_AUDIO_PLAYER)
# add_compile_definitions(FEATURE_INCLUDE_ROBOT_CONTROLLER)
# add_compile_definitions(FEATURE_BUS_HIATUS_DURING_POWER_ON)
# add_compile_definitions(FEATURE_AUTO_CONTROL_CHARGE_ENABLE)

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

# Micropython
#set(MICROPYTHON_VERSION "v1.18")

# Add optional components
# set(OPTIONAL_COMPONENTS ${OPTIONAL_COMPONENTS} "")

# Additional hardware definitions
add_compile_definitions(FEATURE_INCLUDE_SCADER)
# add_compile_definitions(FEATURE_HWELEM_STEPPERS)
