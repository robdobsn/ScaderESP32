BUILDREV=${1:-Scader}
IPADDR=${2:-192.168.86.193}
echo "usage: otaWebUIUpdate.sh <buildConfig> <IPaddress>"
echo "running: curl for $BUILDREV to $IPADDR"
for i in $(ls ./build_raft_artifacts/FSImage/*.gz); do
  echo "curl -F "data=@$i" http://$IPADDR/api/fileupload | cat"
  curl -F "data=@$i" http://$IPADDR/api/fileupload | cat
  echo ""
done
echo ""
