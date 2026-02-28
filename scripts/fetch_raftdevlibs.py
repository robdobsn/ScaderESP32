#!/usr/bin/env python3
"""
Fetch the latest versions of Raft development libraries from GitHub
into the ./raftdevlibs folder.
"""

import argparse
import os
import subprocess
import sys

DEFAULT_LIBS = [
    "RaftCore",
    "RaftSysMods",
    "RaftI2C",
    "RaftWebServer",
    "RaftMotorControl",
]

DEFAULT_GITHUB_ACCOUNT = "robdobsn"


def fetch_lib(account: str, lib: str, dest_dir: str, branch: str) -> bool:
    """Clone or pull the latest version of a library."""
    repo_url = f"https://github.com/{account}/{lib}.git"
    lib_path = os.path.join(dest_dir, lib)

    if os.path.isdir(os.path.join(lib_path, ".git")):
        print(f"Updating {lib} from {account}...")
        try:
            subprocess.run(
                ["git", "-C", lib_path, "fetch", "--all"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", lib_path, "checkout", branch],
                check=True,
            )
            subprocess.run(
                ["git", "-C", lib_path, "pull", "origin", branch],
                check=True,
            )
        except subprocess.CalledProcessError as e:
            print(f"  ERROR updating {lib}: {e}", file=sys.stderr)
            return False
    else:
        print(f"Cloning {lib} from {account}...")
        try:
            subprocess.run(
                ["git", "clone", "--branch", branch, repo_url, lib_path],
                check=True,
            )
        except subprocess.CalledProcessError as e:
            print(f"  ERROR cloning {lib}: {e}", file=sys.stderr)
            return False

    print(f"  {lib} OK")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Fetch latest Raft dev libraries from GitHub.",
    )
    parser.add_argument(
        "--account",
        default=DEFAULT_GITHUB_ACCOUNT,
        help=f"GitHub account/organisation (default: {DEFAULT_GITHUB_ACCOUNT})",
    )
    parser.add_argument(
        "--libs",
        nargs="+",
        default=DEFAULT_LIBS,
        help=f"Libraries to fetch (default: {' '.join(DEFAULT_LIBS)})",
    )
    parser.add_argument(
        "--dest",
        default=os.path.join(os.path.dirname(os.path.dirname(__file__)), "raftdevlibs"),
        help="Destination directory (default: ./raftdevlibs relative to repo root)",
    )
    parser.add_argument(
        "--branch",
        default="main",
        help="Branch to checkout (default: main)",
    )
    args = parser.parse_args()

    os.makedirs(args.dest, exist_ok=True)

    failures = []
    for lib in args.libs:
        if not fetch_lib(args.account, lib, args.dest, args.branch):
            failures.append(lib)

    if failures:
        print(f"\nFailed to fetch: {', '.join(failures)}", file=sys.stderr)
        sys.exit(1)

    print("\nAll libraries fetched successfully.")


if __name__ == "__main__":
    main()
