# Set the target Espressif chip
set(IDF_TARGET "esp32s3")

# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

# Ethernet: W5500 over SPI (no internal EMAC on this board). Compiles the W5500
# path in NetworkSystem (HW_ETH_USE_W5500), links the driver (RAFT_ENABLE_ETH_W5500
# gates RaftCore's REQUIRES espressif__w5500), and brings the espressif/w5500
# managed component into the build ONLY for this SysType via the optional carrier
# component (so other SysTypes are unaffected).
# RaftCore gates its `REQUIRES espressif__w5500` on this ENV var: component
# requirements are captured during ESP-IDF's early-expansion pass, where cache
# variables are not visible but the environment is. Must be an env var, not a
# plain/cache CMake variable.
set(ENV{RAFT_ENABLE_ETH_W5500} "1")
add_compile_definitions(HW_ETH_USE_W5500)
# Bring the espressif/w5500 managed driver into the build for this SysType only.
list(APPEND OPTIONAL_COMPONENTS "${CMAKE_SOURCE_DIR}/optional_components/EthW5500")
