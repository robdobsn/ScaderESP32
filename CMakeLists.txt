# Ensure CMake supports FetchContent
cmake_minimum_required(VERSION 3.16)
include(FetchContent)

set(SUPPORTED_TARGETS esp32)

# Get build configuration folder
get_filename_component(buildConfigName ${CMAKE_BINARY_DIR} NAME)

# Check config dir exists
set(BUILD_CONFIG_DIR "${CMAKE_SOURCE_DIR}/buildConfigs/${buildConfigName}")
if((NOT EXISTS ${BUILD_CONFIG_DIR}) OR (NOT IS_DIRECTORY ${BUILD_CONFIG_DIR}))
    message(FATAL_ERROR "Config directory ${BUILD_CONFIG_DIR} not found.")
endif()

# Use sdkconfig for the selected build configuration
set(SDKCONFIG_DEFAULTS "${BUILD_CONFIG_DIR}/sdkconfig.defaults")
set(SDKCONFIG "${BUILD_CONFIG_DIR}/sdkconfig")

# Configure build config specific options, flags, etc.
include(${BUILD_CONFIG_DIR}/featureFlags.cmake)

# Configuration
message(STATUS "Configuring a build system for ${buildConfigName} from ${BUILD_CONFIG_DIR} FW_IMAGE_NAME ${FW_IMAGE_NAME}")

# Clean up helper variables
unset(buildConfigName)

# Fetch the RaftCore library
FetchContent_Declare(
  raftcore
  # GIT_REPOSITORY https://github.com/robdobsn/RaftCore.git
  # GIT_TAG        16a95d23bcc9025176e647e3c87dc447c1a2cc50
  GIT_REPOSITORY https://github.com/robdobsn/RaftCore.git
  GIT_TAG        3439fcd0bad33ba3055f56a2f74b87cabb63ebad
)

# Fetch the RaftWebServer library
FetchContent_Declare(
  raftwebserver
  GIT_REPOSITORY https://github.com/robdobsn/RaftWebServer
  GIT_TAG        v0.0.4
)

# Populate first so we have a folder that we can add in EXTRA_COMPONENT_DIRS
message(STATUS "Fetching dependencies...")
FetchContent_Populate(raftcore)
FetchContent_Populate(raftwebserver)

# Fetch raftwebserver and raftcore
set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} ${raftcore_SOURCE_DIR} ${raftwebserver_SOURCE_DIR})
# FetchContent_MakeAvailable(raftcore)
FetchContent_MakeAvailable(raftcore raftwebserver)

# Add optional component folders
set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} ${OPTIONAL_COMPONENTS})

# Configure ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
project(${FW_IMAGE_NAME} DEPENDS esp_littlefs raftcore raftwebserver)

# This makes it easy for VS Code to access the compile commands for the most
# recently built FW revision.
# TODO: A nicer solution might be to set up launch configurations properly and
#       have them find their respective launch_commands.json
add_custom_target(updateSharedCompileCommands ALL
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_BINARY_DIR}/.."
                    DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
                    COMMENT "Updating shared compile_commands.json"
                    )
