#!/usr/bin/env python3

# from jinja2 import Environment, FileSystemLoader
import argparse
import logging
from pathlib import Path
import os
import subprocess
import gzip
import sys

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                    level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else Path(__file__).name)

def dir_path(string):
    if os.path.isdir(string):
        return string
    else:
        raise NotADirectoryError(string)

def parseArgs():
    parser = argparse.ArgumentParser(description="Generate Web UI")
    parser.add_argument('source',
                        nargs='?',
                        type=dir_path,
                        help="folder containing input files")
    parser.add_argument('dest',
                        nargs='?',
                        type=dir_path,
                        help="folder to output files to")
    parser.add_argument('--nogzip', action='store_false', dest='gzipContent',
                        help='gzip the resulting files')
    return parser.parse_args()

def generateWebUI(sourceFolder, destFolder, gzipContent):

    _log.info("GenWebUI source '%s' dest '%s' gzip %s", sourceFolder, destFolder, "Y" if gzipContent else "N")

    # Execute npm run build in the source folder
    # Copy the resulting files to the destination folder
    # If gzipContent is true, gzip the files
    rslt = subprocess.run(["npm", "run", "build"], cwd=sourceFolder)
    if rslt.returncode != 0:
        _log.error("GenWebUI failed to build Web UI")
        return rslt.returncode
    
    buildFolder = os.path.join(sourceFolder, "build")
    for fname in os.listdir(buildFolder):
        if fname.endswith('.html') or fname.endswith('.js') or fname.endswith('.css'):
            if gzipContent:
                with open(os.path.join(buildFolder, fname), 'rb') as f:
                    renderedStr = f.read()
                with gzip.open(os.path.join(destFolder, fname + '.gz'), 'wb') as f:
                    f.write(renderedStr)
                _log.info(f"GenWebUI created Web UI from {fname} to {fname}.gz")
            else:
                os.rename(os.path.join(buildFolder, fname), os.path.join(destFolder, fname))
                # _log.info(f"GenWebUI created Web UI from {fname} to {fname}")
        else:
            os.rename(os.path.join(buildFolder, fname), os.path.join(destFolder, fname))
            # _log.info(f"GenWebUI created Web UI from {fname} to {fname}")
    return 0

def main():
    args = parseArgs()
    return generateWebUI(args.source, args.dest, args.gzipContent)

if __name__ == '__main__':
    rslt = main()
    sys.exit(rslt)
    
