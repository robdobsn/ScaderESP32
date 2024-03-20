BUILDCONF=${1:-Scader}
IP_ADDR=${2:-}
FW_IMAGE_NAME=${BUILDCONF}.bin
echo "Flashing ${BUILDCONF} FW image is ${FW_IMAGE_NAME}"
if [[ $SERIALPORT_OR_IP =~ ^[[:digit:]] ]]
  then
    echo Sending OTA
    ./scripts/otaWebUIUpdate.sh $BUILDCONF $SERIALPORT_OR_IP
    curl -F "file=@./build/$BUILDCONF/$FW_IMAGE_NAME" "http://$SERIALPORT_OR_IP/api/espFwUpdate"
  else
    echo "Please provide a valid IP address or serial port"
    exit 1
fi
