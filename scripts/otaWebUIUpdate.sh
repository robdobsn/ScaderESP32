BUILDREV=${1:-Scader}
IPADDR=${2:-192.168.86.193}
echo "usage: otaWebUIUpdate.sh <buildConfig> <IPaddress>"
echo "running: curl for $BUILDREV to $IPADDR"
for i in $(ls ./buildConfigs/$BUILDREV/FSImage/*.gz); do
  echo "curl -F "data=@$i" http://$IPADDR/api/fileupload | cat"
  curl -F "data=@$i" http://$IPADDR/api/fileupload | cat
  echo ""
done
curl -F "data=@./buildConfigs/$BUILDREV/FSImage/favicon.ico" http://$IPADDR/api/fileupload | cat
curl -F "data=@./buildConfigs/$BUILDREV/FSImage/manifest.json" http://$IPADDR/api/fileupload | cat
echo ""
