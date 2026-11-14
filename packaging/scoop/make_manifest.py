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
zip_url = repo + "/releases/download/v{0}/RcloneExplorer-{0}-windows-x64-portable.zip"

# Files of the user's that Scoop keeps across updates. A persisted file that
# does not exist yet would become a folder, so each is created first: copied
# from the previous version's folder when it is there (mirrors.json was not
# persisted before 5.4.3), else empty.
kept_files = ["tasks.bin", "mirrors.json", "mirror_history.json"]
pre_install = (
    "$names = " + ", ".join("'%s'" % n for n in kept_files) + "; "
    "foreach ($n in $names) { "
    "if (!(Test-Path \"$persist_dir\\$n\")) { "
    "$old = Get-ChildItem (Split-Path $dir) -Directory | "
    "Where-Object { $_.FullName -ne $dir -and $_.Name -ne 'current' -and (Test-Path (Join-Path $_.FullName $n)) } | "
    "Sort-Object LastWriteTime -Descending | Select-Object -First 1; "
    "if ($old) { Copy-Item (Join-Path $old.FullName $n) \"$dir\\$n\" } "
    "else { New-Item \"$dir\\$n\" -ItemType File | Out-Null } } }"
)

manifest = {
    "version": ver,
    "description": "Portable rclone GUI: browse, transfer and sync files on Google Drive, OneDrive, "
                   "Dropbox, S3 and 70+ other cloud services.",
    "homepage": "https://jackpison.github.io/RcloneExplorer/",
    "license": "MIT",
    "architecture": {"64bit": {"url": zip_url.format(ver), "hash": sha}},
    "extract_dir": "Rclone Explorer",
    # Scoop would create a folder for a persisted file that does not exist yet.
    "pre_install": pre_install,
    "shortcuts": [["Rclone Explorer.exe", "Rclone Explorer"]],
    # Settings, saved tasks and the bundled rclone (which the app keeps up to
    # date itself, and where a portable rclone.conf can live) survive updates.
    "persist": ["Rclone Explorer.ini", "rclone"] + kept_files,
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
