# Tidy up C++ include statements in a file.

import sys
import re
import argparse
import os

def tidyCppIncludes(filename):
    # Read in the file.
    # print("Checking file: " + filename)
    with open(filename, 'r') as file:
        # filedata = file.read()
        lines = file.readlines()

    # # Replace the include statements.
    # filedata = re.sub(r'#include "(.*?)"', r'#include <\1>', filedata)

    include_lines = []
    for line in lines:
        # Find the include statements that are bracketed with "" and don't have a .h at the end.
        if re.match(r'#include\s*"([^"]+(?<!\.h))"\s*', line):
            include_lines.append(line)
        # Find the include statements that are bracketed with <> and do have a .h at the end.
        else:
            match_rslt = re.match(r'#include\s*<([^>]+\.h)>\s*', line)
            if match_rslt:
                # Check valid headers
                valid_headers = ["ctype.h", "sys/stat.h", "Arduino.h", "avr/pgmspace.h", "assert.h", "stdio.h", "string.h", "stdlib.h", "math.h"]
                if match_rslt.group(1) not in valid_headers:
                    include_lines.append(line)

    if len(include_lines) == 0:
        return

    print("-------------------- Needs a tidy .... " + filename)
    print(include_lines)

    # # Write the file out again.
    # with open(filename, 'w') as file:
    #     file.write(filedata)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Tidy up C++ include statements in a file.")
    parser.add_argument("folder", help="folder to tidy up")
    args = parser.parse_args()

    # Recurse through the folder.
    for root, dirs, files in os.walk(args.folder):
        for file in files:
            if file.endswith(".cpp") or file.endswith(".h"):
                tidyCppIncludes(os.path.join(root, file))

