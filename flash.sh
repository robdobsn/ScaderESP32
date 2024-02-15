BUILDCONF=${1:-Scader}
SERIALPORT_OR_IP=${2:-COM3}
TARGET_CHIP=${3:-esp32}
FW_IMAGE_NAME=${BUILDCONF}.bin
echo "Flashing ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
if [[ $SERIALPORT_OR_IP =~ ^[[:digit:]] ]]
  then
    echo Sending OTA
    ./scripts/otaWebUIUpdate.sh $BUILDCONF $SERIALPORT_OR_IP
    curl -F "file=@./build/$BUILDCONF/$FW_IMAGE_NAME" "http://$SERIALPORT_OR_IP/api/espFwUpdate"
  else
    SCRIPTS_FOLDER=./build/$BUILDCONF/_deps/raftcore-src/scripts
    SERIAL_MONITOR=${SCRIPTS_FOLDER}/SerialMonitor.py
    FLASHER=${SCRIPTS_FOLDER}/flashUsingPartitionCSV.py
    PARTITIONS_CSV=./buildConfigs/$BUILDCONF/partitions.csv
    if uname -r | grep -q "icrosoft"
    then
        echo "Running on Windows WSL"
        python.exe ${FLASHER} ${PARTITIONS_CSV} build/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python.exe ${SERIAL_MONITOR} $SERIALPORT_OR_IP -g
          fi
    else
        echo "Running on Linux or OSX"
        python3 ${FLASHER} ${PARTITIONS_CSV} build/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python3 ${SERIAL_MONITOR} $SERIALPORT_OR_IP -g
          fi
    fi
fi
