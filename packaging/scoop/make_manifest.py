#!/usr/bin/env python3
"""Writes bucket/rclone-explorer.json, the Scoop manifest, for one release.

    python3 packaging/scoop/make_manifest.py <version> <sha256-of-windows-zip>

Run by the release job after a release is published. The repository itself is
the Scoop bucket:

    scoop bucket add rclone-explorer https://github.com/Jackpison/RcloneExplorer
    scoop install rclone-explorer
"""
import json
import os
import re
import sys

ver, sha = sys.argv[1], sys.argv[2].lower()
if not re.fullmatch(r"\d+\.\d+\.\d+", ver) or not re.fullmatch(r"[0-9a-f]{64}", sha):
    sys.exit("usage: make_manifest.py <x.y.z> <sha256>")

repo = "https://github.com/Jackpison/RcloneExplorer"
zip_url = repo + "/releases/download/v{0}/RcloneExplorer-{0}-windows-x64.zip"

manifest = {
    "version": ver,
    "description": "Portable rclone GUI: browse, transfer and sync files on Google Drive, OneDrive, "
                   "Dropbox, S3 and 70+ other cloud services.",
    "homepage": "https://jackpison.github.io/RcloneExplorer/",
    "license": "MIT",
    "architecture": {"64bit": {"url": zip_url.format(ver), "hash": sha}},
    "extract_dir": "Rclone Explorer",
    # Scoop would create a folder for a persisted file that does not exist yet.
    "pre_install": 'if (!(Test-Path "$persist_dir\\tasks.bin")) { New-Item "$dir\\tasks.bin" -ItemType File | Out-Null }',
    "shortcuts": [["Rclone Explorer.exe", "Rclone Explorer"]],
    # Settings, saved tasks and the bundled rclone (which the app keeps up to
    # date itself, and where a portable rclone.conf can live) survive updates.
    "persist": ["Rclone Explorer.ini", "tasks.bin", "rclone"],
    "checkver": "github",
    "autoupdate": {
        "architecture": {"64bit": {"url": zip_url.replace("{0}", "$version")}},
        "hash": {"url": "$baseurl/SHA256SUMS.txt"},
    },
}

out = os.path.join(os.path.dirname(__file__), "..", "..", "bucket", "rclone-explorer.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as f:
    json.dump(manifest, f, indent=4)
    f.write("\n")
print("wrote bucket/rclone-explorer.json for", ver)
