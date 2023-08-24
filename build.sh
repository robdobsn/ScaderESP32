BUILDCONF=${1:-Scader}
SERIALPORT_OR_IP=${2:-COM3}
BUILD_IDF_VERS=${3:-esp-idf-v4.4.5}
TARGET_CHIP=${4:-esp32}
FLASHBAUD=${5:-2000000}
FW_IMAGE_NAME=${BUILDCONF}FW.bin
echo "Building for ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python3 ./scripts/build.py buildConfigs/$BUILDCONF &&\
if [[ $SERIALPORT_OR_IP =~ ^[[:digit:]] ]]
  then
    echo Sending OTA
    ./scripts/otaWebUIUpdate.sh $BUILDCONF $SERIALPORT_OR_IP
    curl -F "file=@./builds/$BUILDCONF/$FW_IMAGE_NAME" "http://$SERIALPORT_OR_IP/api/espFwUpdate"
  else
    SCRIPTS_FOLDER=./builds/$BUILDCONF/_deps/raftcore-src/scripts
    SERIAL_MONITOR=${SCRIPTS_FOLDER}/SerialMonitor.py
    FLASHER=${SCRIPTS_FOLDER}/flashUsingPartitionCSV.py
    PARTITIONS_CSV=./buildConfigs/$BUILDCONF/partitions.csv
    if uname -r | grep -q "icrosoft"
    then
        echo "Running on Windows WSL"
        python.exe ${FLASHER} ${PARTITIONS_CSV} builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python.exe ${SERIAL_MONITOR} $SERIALPORT_OR_IP -g
          fi
    else
        echo "Running on Linux or OSX"
        python3 ${FLASHER} ${PARTITIONS_CSV} builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python3 ${SERIAL_MONITOR} $SERIALPORT_OR_IP -g
          fi
    fi
fi
