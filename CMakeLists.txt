# Raft Project
cmake_minimum_required(VERSION 3.16)

# Bootstrap using RaftBootstrap script
set(BOOTSTRAP_URL "https://github.com/robdobsn/RaftCore/releases/download/v1.17.11/RaftBootstrap.cmake")
file(DOWNLOAD ${BOOTSTRAP_URL} "${CMAKE_BINARY_DIR}/RaftBootstrap.cmake")
include("${CMAKE_BINARY_DIR}/RaftBootstrap.cmake")

# Define the project dependencies
project(${_build_config_name} DEPENDS ${ADDED_PROJECT_DEPENDENCIES})