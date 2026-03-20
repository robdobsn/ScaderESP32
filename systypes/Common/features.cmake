# RaftCore device types
set(DEV_TYPE_JSON_FILES "devtypes/DeviceTypeRecords.json")

# Raft components
set(RAFT_COMPONENTS
    RaftCore@v1.40.1
    RaftSysMods@v1.15.1
    RaftI2C@v1.12.1
    RaftMotorControl@v1.6.2
    RaftWebServer@v1.8.1
)

# File system
set(FS_TYPE "littlefs")
set(FS_IMAGE_PATH "../Common/FSImage")

# SD card support (enable per SysType if needed)
set(RAFT_ENABLE_SD OFF)

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
