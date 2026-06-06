#!/usr/bin/env python3
"""Generate a release manifest.json for a built ScaderESP32 SysType.

Reads `build/<systype>/flasher_args.json` plus the artifacts produced by
`raft b -s <systype>` and emits a `manifest.json` that drives both the
serial (esptool-js) and OTA (server-side) flash flows.

The manifest schema is fixed by devdocs/scader-reflash-plan.md (§4.2).
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import pathlib
import sys


# ---------------------------------------------------------------------------
# flasher_args.json file → manifest "serial.files[]" path mapping.
# The keys are the relative paths emitted by ESP-IDF inside build/<systype>/;
# the values are the on-disk paths inside the bundle zip / release asset set.
# Anything not in this map is copied through as-is under serial/.
# ---------------------------------------------------------------------------
BUNDLE_PATH_BY_FA_FILE: dict[str, str] = {
    "bootloader/bootloader.bin": "serial/bootloader.bin",
    "partition_table/partition-table.bin": "serial/partition-table.bin",
    "ota_data_initial.bin": "serial/ota_data_initial.bin",
    "fs.bin": "serial/fs.bin",
}


def _sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _bundle_path_for(fa_file: str, systype: str) -> str:
    if fa_file in BUNDLE_PATH_BY_FA_FILE:
        return BUNDLE_PATH_BY_FA_FILE[fa_file]
    if fa_file == f"{systype}.bin":
        return f"app/{systype}.bin"
    # Unknown extra artifact: place under serial/ keeping its leaf name.
    return f"serial/{pathlib.PurePosixPath(fa_file).name}"


def build_manifest(
    *,
    systype: str,
    version: str,
    git_tag: str,
    build_dir: pathlib.Path,
    built_at: str,
) -> dict:
    fa_path = build_dir / "flasher_args.json"
    if not fa_path.is_file():
        raise SystemExit(f"flasher_args.json not found at {fa_path}")
    fa = json.loads(fa_path.read_text())

    chip = fa.get("extra_esptool_args", {}).get("chip")
    if not chip:
        raise SystemExit("extra_esptool_args.chip missing from flasher_args.json")
    flash_settings = fa.get("flash_settings", {})
    flash_files: dict[str, str] = fa.get("flash_files", {})
    if not flash_files:
        raise SystemExit("flash_files missing from flasher_args.json")

    # serial.files[] sorted by numeric offset.
    serial_files = []
    sha256: dict[str, str] = {}
    for offset_str, fa_file in sorted(
        flash_files.items(), key=lambda kv: int(kv[0], 16)
    ):
        src = build_dir / fa_file
        if not src.is_file():
            raise SystemExit(f"Missing flash artifact: {src}")
        bundle_path = _bundle_path_for(fa_file, systype)
        serial_files.append({"path": bundle_path, "offset": offset_str})
        sha256[bundle_path] = _sha256(src)

    app_bundle_path = f"app/{systype}.bin"
    app_src = build_dir / f"{systype}.bin"
    if not app_src.is_file():
        raise SystemExit(f"Missing app artifact: {app_src}")
    sha256[app_bundle_path] = _sha256(app_src)

    return {
        "systype": systype,
        "version": version,
        "gitTag": git_tag,
        "chip": chip,
        "builtAt": built_at,
        "serial": {
            "flashMode": flash_settings.get("flash_mode"),
            "flashFreq": flash_settings.get("flash_freq"),
            "flashSize": flash_settings.get("flash_size"),
            "files": serial_files,
        },
        "ota": {
            "app": app_bundle_path,
            "fsImageDir": "fsimage",
            # Public mount name used by Raft's HTTP API (/api/filelist/<fs>/...).
            # The on-device storage backend is LittleFS but the API exposes it as "local".
            "fs": "local",
        },
        "sha256": sha256,
    }


def _parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--systype", required=True, help="e.g. ScaderRelays")
    p.add_argument("--version", required=True, help="semver without leading v")
    p.add_argument("--git-tag", required=True, help="release git tag, e.g. v0.1.0")
    p.add_argument(
        "--build-dir",
        type=pathlib.Path,
        required=True,
        help="build/<systype>/ produced by raft b -s <systype>",
    )
    p.add_argument(
        "--out",
        type=pathlib.Path,
        required=True,
        help="Path to write manifest.json",
    )
    p.add_argument(
        "--built-at",
        default=None,
        help="ISO-8601 UTC timestamp (default: now)",
    )
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv if argv is not None else sys.argv[1:])
    built_at = args.built_at or _dt.datetime.now(tz=_dt.timezone.utc).isoformat(
        timespec="seconds"
    )
    manifest = build_manifest(
        systype=args.systype,
        version=args.version,
        git_tag=args.git_tag,
        build_dir=args.build_dir,
        built_at=built_at,
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {args.out} ({len(manifest['sha256'])} hashed files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
