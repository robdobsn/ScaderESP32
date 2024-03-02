#!/usr/bin/env python3

import argparse
import logging
from pathlib import Path
import requests
import sys

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                    level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else Path(__file__).name)

def parseArgs():
    parser = argparse.ArgumentParser(description="OTA Update Web UI")
    # Add optional source folder
    parser.add_argument('--source',
                        type=str,
                        default="./buildConfigs/Scader/FSImage",
                        help="folder containing Web UI files")
    # Add IP address of device
    parser.add_argument('ip',
                        type=str,
                        help="IP address of device")
    return parser.parse_args()

def matchWithWildcards(s, wildcard):
    return s == wildcard or wildcard.endswith("*") and s.startswith(wildcard[:-1])

def otaUpdate(sourceFolder, ip):

    _log.info(f"OTAUpdateWebUI source '{sourceFolder}' ip '{ip}'")

    # Get list of files using api/filelist
    url = f"http://{ip}/api/filelist"
    rslt = requests.get(url)
    if rslt.status_code != 200:
        _log.error(f"OTAUpdateWebUI failed to get file list from {ip}")
        return rslt.status_code
    try:
        fileListResp = rslt.json()
    except:
        _log.error(f"OTAUpdateWebUI failed to parse file list from {ip} as JSON - {rslt.text}")
        return 1
    # _log.info(f"OTAUpdateWebUI got file list from {ip}: {fileListResp}")

    # Extract list from response files element
    fileList = fileListResp.get("files", [])

    # Files to delete on device
    filesToDeleteIncWildcards = [
        "asset-manifest.json", 
        "favicon.ico", 
        "index.html", 
        "index.html.gz",
        "logo192.png", 
        "logo512.png", 
        "manifest.json", 
        "precache-manifest.*", 
        "robots.txt", 
        "service-worker.js", 
        "main.*"
        ]
    filesToDelete = [finfo.get("name", "") for finfo in fileList if any(matchWithWildcards(finfo.get("name",""), wildcard) for wildcard in filesToDeleteIncWildcards)]
    _log.info(f"OTAUpdateWebUI deleting {filesToDelete} from {ip}")

    # Delete the files
    for fname in filesToDelete:
        url = f"http://{ip}/api/filedelete//{fname}"
        rslt = requests.get(url)
        if rslt.status_code != 200:
            _log.error(f"OTAUpdateWebUI failed to delete {fname} from {ip}")
            return rslt.status_code
        try:
            deleteResult = rslt.json()
        except:
            _log.error(f"OTAUpdateWebUI failed to parse delete result from {ip} as JSON - {rslt.text}")
            return 1
        if deleteResult.get("rslt", "") != "ok":
            _log.error(f"OTAUpdateWebUI failed to delete {fname} from {ip} - {deleteResult}")
        else:
            _log.info(f"OTAUpdateWebUI deleted {fname} from {ip}")

    # Send files to the device
    if sourceFolder is not None:
        # Get list of files to send
        filesToSend = [f for f in Path(sourceFolder).iterdir() for wildcard in filesToDeleteIncWildcards if f.is_file() and matchWithWildcards(f.name, wildcard)]
        _log.info(f"OTAUpdateWebUI sending {filesToSend} to {ip}")

        # Send the files
        for fname in filesToSend:
            url = f"http://{ip}/api/fileupload//{fname.name}"
            rslt = requests.post(url, data=fname.read_bytes())
            if rslt.status_code != 200:
                _log.error(f"OTAUpdateWebUI failed to send {fname} to {ip}")
                return rslt.status_code
            try:
                sendResult = rslt.json()
            except:
                _log.error(f"OTAUpdateWebUI failed to parse send result from {ip} as JSON - {rslt.text}")
                return 1
            if sendResult.get("rslt", "") != "ok":
                _log.error(f"OTAUpdateWebUI failed to send {fname} to {ip} - {sendResult}")
            else:
                _log.info(f"OTAUpdateWebUI sent {fname} to {ip} - {sendResult}")
    return 0

def main():
    args = parseArgs()
    return otaUpdate(args.source, args.ip)

if __name__ == '__main__':
    rslt = main()
    sys.exit(rslt)
    
