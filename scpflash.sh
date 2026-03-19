#!/bin/bash
# SCP flash files to a remote Pi and flash via esptool
# Usage: ./scpflash.sh <SysType> <PiIP> [SerialPort]
# Example: ./scpflash.sh ScaderShades 192.168.86.235 /dev/ttyUSB0

SYSTYPE=${1:-}
PI_IP=${2:-}
SERIAL_PORT=${3:-/dev/ttyUSB0}

if [ -z "$SYSTYPE" ] || [ -z "$PI_IP" ]; then
    echo "Usage: $0 <SysType> <PiIP> [SerialPort]"
    echo "Example: $0 ScaderShades 192.168.86.235 /dev/ttyUSB0"
    exit 1
fi

BUILD_DIR="build/${SYSTYPE}"
REMOTE_DIR="/tmp/${SYSTYPE}"

if [ ! -f "${BUILD_DIR}/flash_args" ]; then
    echo "Error: ${BUILD_DIR}/flash_args not found. Build first with: raft b -s ${SYSTYPE}"
    exit 1
fi

echo "Flashing ${SYSTYPE} to ESP32 via Pi at ${PI_IP} on ${SERIAL_PORT}"

# Use SSH ControlMaster to reuse a single connection (one password prompt)
SSH_CTRL=$(mktemp -d)/ssh_ctrl
ssh -fNM -S "$SSH_CTRL" "${PI_IP}"
trap 'ssh -S "$SSH_CTRL" -O exit "${PI_IP}" 2>/dev/null; rm -rf "$(dirname "$SSH_CTRL")"' EXIT

SSH="ssh -S $SSH_CTRL"
SCP="scp -o ControlPath=$SSH_CTRL"

# Create remote directory structure
echo "Creating remote directories..."
$SSH "${PI_IP}" "mkdir -p ${REMOTE_DIR}/bootloader ${REMOTE_DIR}/partition_table"

# Copy all flash files
echo "Copying flash files..."
$SCP "${BUILD_DIR}/flash_args" \
    "${BUILD_DIR}/${SYSTYPE}.bin" \
    "${BUILD_DIR}/ota_data_initial.bin" \
    "${BUILD_DIR}/fs.bin" \
    "${PI_IP}:${REMOTE_DIR}/"

$SCP "${BUILD_DIR}/bootloader/bootloader.bin" \
    "${PI_IP}:${REMOTE_DIR}/bootloader/"

$SCP "${BUILD_DIR}/partition_table/partition-table.bin" \
    "${PI_IP}:${REMOTE_DIR}/partition_table/"

# Flash on the Pi
echo "Flashing..."
$SSH "${PI_IP}" "cd ${REMOTE_DIR} && ~/esptool-venv/bin/python3 -m esptool --chip esp32 -p ${SERIAL_PORT} -b 460800 --before default_reset --after hard_reset write_flash @flash_args"

echo "Done"
