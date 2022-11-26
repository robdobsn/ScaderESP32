BUILDCONF=${1:-Scader}
IPADDR=${2:-192.168.86.193}
if [[ "${BUILDCONF}" == "Marty"* ]]; then
    FW_IMAGE_NAME="RICFirmware.bin"
else
    FW_IMAGE_NAME="${BUILDCONF}FW.bin"
fi
echo "usage: otaFWUpdate.sh <buildConfig> <IPaddress>"
echo "running: curl for $BUILDREV to $IPADDR"
curl -F "data=@./builds/$BUILDCONF/$FW_IMAGE_NAME" http://$IPADDR/api/espFwUpdate | cat
echo ""
