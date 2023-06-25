BUILDCONF=${1:-Scader}
SERIALPORT_OR_IP=${2:-COM3}
BUILD_IDF_VERS=${3:-esp-idf-v4.4.4}
TARGET_CHIP=${4:-esp32}
FLASHBAUD=${5:-2000000}
FW_IMAGE_NAME=${BUILDCONF}FW.bin
echo "Building for ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python3 ./scripts/build.py buildConfigs/$BUILDCONF &&\
if [[ $SERIALPORT_OR_IP =~ ^[[:digit:]] ]]
  then
    echo Sending OTA
    ./scripts/otaWebUIUpdate.sh $BUILDCONF $1
    curl -F "file=@./builds/$BUILDCONF/$FW_IMAGE_NAME" "http://$1/api/espFwUpdate"
  else
    if uname -r | grep -q "icrosoft"
    then
        echo "Running on Windows WSL"
        python.exe ./scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDCONF/partitions.csv builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python.exe ./scripts/SerialMonitor.py $SERIALPORT_OR_IP -g
          fi
    else
        echo "Running on Linux or OSX"
        python3 ./scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDCONF/partitions.csv builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT_OR_IP $TARGET_CHIP -b$FLASHBAUD -f spiffs.bin
        if [ $? -eq "0" ]
          then
            python3 ./scripts/SerialMonitor.py $SERIALPORT_OR_IP -g
          fi
    fi
fi
