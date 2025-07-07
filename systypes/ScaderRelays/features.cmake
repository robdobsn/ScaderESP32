# Set the target Espressif chip
set(IDF_TARGET "esp32")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# Enable the ethernet hardware for Olimex ESP32 PoE-ISO boards
add_compile_definitions(HW_ETH_PHY_LAN87XX)

# add_compile_definitions(DEBUG_USING_GLOBAL_VALUES)
# add_compile_definitions(DEBUG_NETWORK_EVENTS_DETAIL)
# add_compile_definitions(DEBUG_LIST_SYSMODS)

set(NETWORK_MDNS_DISABLED ON)
