# Carrier component for the W5500 SPI Ethernet driver.
#
# It has no sources of its own; its only purpose is to carry the
# espressif/w5500 managed dependency (see idf_component.yml) so that the driver
# is pulled into the build for W5500 SysTypes only. RaftCore REQUIRES
# espressif__w5500 (gated on RAFT_ENABLE_ETH_W5500) for the actual include/link.
idf_component_register(REQUIRES espressif__w5500)
