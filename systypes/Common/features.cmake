# RaftCore device types
set(DEV_TYPE_JSON_FILES "devtypes/DeviceTypeRecords.json")

# Raft components
set(RAFT_COMPONENTS
    RaftCore@v1.48.1
    RaftSysMods@v1.18.1
    RaftI2C@v1.16.1
    RaftMotorControl@v1.6.2
    RaftWebServer@v1.12.1
)

# File system
set(FS_TYPE "littlefs")
set(FS_IMAGE_PATH "../Common/FSImage")

# SD card support (enable per SysType if needed)
set(RAFT_ENABLE_SD OFF)

# W5500 SPI Ethernet support is OFF by default (zero firmware-size impact). A
# SysType that needs it opts in from its own features.cmake with three lines:
#   set(ENV{RAFT_ENABLE_ETH_W5500} "1")          # gates RaftCore REQUIRES (early-expansion safe)
#   add_compile_definitions(HW_ETH_USE_W5500)    # compiles the W5500 path in NetworkSystem
#   list(APPEND OPTIONAL_COMPONENTS "${CMAKE_SOURCE_DIR}/optional_components/EthW5500")
# See raftdevlibs/RaftCore/devdocs/ethernet-idf6-lan87xx-w5500-plan.md §4.
# (No variable is set here on purpose: the env var must be ABSENT for non-W5500 builds.)

# Web UI
set(UI_SOURCE_PATH "../Common/WebUI")
# Uncomment the following line if you do not want to gzip the web UI
# set(WEB_UI_GEN_FLAGS ${WEB_UI_GEN_FLAGS} --nogzip)
# Uncomment the following line to include a source map for the web UI - this will increase the size of the web UI
# set(WEB_UI_GEN_FLAGS ${WEB_UI_GEN_FLAGS} --incmap)

# Uncomment to debug
# add_compile_definitions(DEBUG_USING_GLOBAL_VALUES)
# add_compile_definitions(DEBUG_NETWORK_EVENTS_DETAIL)
# add_compile_definitions(DEBUG_LIST_SYSMODS)
