BUILDREV=${1:-Scader}
IPADDR=${2:-192.168.86.193}
echo "usage: otaWebUIUpdate.sh <buildConfig> <IPaddress>"
echo "running: curl for $BUILDREV to $IPADDR"
curl -F "data=@./buildConfigs/$BUILDREV/FSImage/index.html.gz" http://$IPADDR/api/fileupload | cat
echo ""
