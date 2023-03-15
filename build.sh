SERIALPORT=${1:-COM3}
BUILD_IDF_VERS=${2:-esp-idf-v4.4.4}
IPORHOSTNAME=${3:-}
BUILDCONF=Scader
FW_IMAGE_NAME=${BUILDCONF}FW.bin
echo "Building for ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python3 ./scripts/build.py buildConfigs/$BUILDCONF &&\
if [ -z "$3" ]
  then
    python.exe scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDCONF/partitions.csv builds/$BUILDCONF $FW_IMAGE_NAME $SERIALPORT -b2000000 -f spiffs.bin
  else
    echo Sending to RIC OTA
    ./scripts/otaWebUIUpdate.sh $BUILDCONF $3
    curl -F "file=@./builds/$BUILDCONF/$FW_IMAGE_NAME" "http://$3/api/espFwUpdate"
fi
if [ $? -eq "0" ]
  then
    python.exe scripts/SerialMonitor.py $SERIALPORT -g
fi
