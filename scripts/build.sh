BUILDCONF=${1:-Scader}
SERIALPORT=${2:-COM3}
BUILD_IDF_VERS=${3:-esp-idf-v4.4.3}
IPORHOSTNAME=${4:-}
FW_IMAGE_NAME=${BUILDCONF}FW.bin
echo "Building for ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python ./scripts/build.py buildConfigs/$BUILDCONF &&\
if [ -z "$4" ]
  then
    python.exe scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDCONF/partitions.csv builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT -b2000000 -f spiffs.bin
  else
    echo Sending to RIC OTA
    curl -F "file=@./builds/$BUILDCONF/$FW_IMAGE_NAME" "http://$4/api/espFwUpdate"
fi
if [ $? -eq "0" ]
  then
    python.exe scripts/SerialMonitor.py $SERIALPORT -g
fi
