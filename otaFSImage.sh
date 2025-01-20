BUILDCONF=${1:-Scader}
IP_ADDR=${2:-}
echo "Flashing ${BUILDCONF} FSImage to ${IP_ADDR}"
if [[ $IP_ADDR =~ ^[[:digit:]] ]]
  then
    TARGET_DIR="./build/${BUILDCONF}/build_raft_artifacts/FSImage"
    UPLOAD_URL="http://${IP_ADDR}/api/fileupload/"
    for file in "$TARGET_DIR"/*; do
    # Skip if the file has the .built extension
    if [[ "$file" == *.built ]]; then
        continue
    fi

    # Get the base name of the file (without the directory path)
    filename=$(basename "$file")

    # Use curl to upload the file
    echo "Uploading $filename..."
    curl -X POST -F "file=@$file" -F "filename=$filename" "$UPLOAD_URL"
    
    # Check the result of the curl command
    if [[ $? -ne 0 ]]; then
        echo "Failed to upload $filename"
    else
        echo "Successfully uploaded $filename"
    fi
done
  else
    echo "Please provide a valid IP address or serial port"
    exit 1
fi