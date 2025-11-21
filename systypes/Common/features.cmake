# RaftCore device types
set(DEV_TYPE_JSON_FILES "devtypes/DeviceTypeRecords.json")

# Raft components
set(RAFT_COMPONENTS
    RaftCore@main
    RaftSysMods@main
    RaftI2C@main
    RaftMotorControl@main
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
