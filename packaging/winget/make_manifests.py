#!/usr/bin/env python3
"""Writes the three winget manifests for one release into <out-dir>.

    python3 packaging/winget/make_manifests.py <version> <sha256-of-windows-zip> <out-dir>

The result is the folder layout microsoft/winget-pkgs expects:
<out-dir>/manifests/j/Jackpison/RcloneExplorer/<version>/*.yaml
Test locally on Windows before submitting:
    winget settings --enable LocalManifestFiles
    winget install --manifest <that folder>
"""
import datetime
import os
import re
import sys

ver, sha, out = sys.argv[1], sys.argv[2].upper(), sys.argv[3]
if not re.fullmatch(r"\d+\.\d+\.\d+", ver) or not re.fullmatch(r"[0-9A-F]{64}", sha):
    sys.exit("usage: make_manifests.py <x.y.z> <sha256> <out-dir>")

PKG = "Jackpison.RcloneExplorer"
REPO = "https://github.com/Jackpison/RcloneExplorer"
SCHEMA = "1.9.0"
HEAD = "# yaml-language-server: $schema=https://aka.ms/winget-manifest.{kind}.{v}.schema.json\n"

files = {
    f"{PKG}.yaml": HEAD.format(kind="version", v=SCHEMA) + f"""
PackageIdentifier: {PKG}
PackageVersion: {ver}
DefaultLocale: en-US
ManifestType: version
ManifestVersion: {SCHEMA}
""",
    f"{PKG}.installer.yaml": HEAD.format(kind="installer", v=SCHEMA) + f"""
PackageIdentifier: {PKG}
PackageVersion: {ver}
InstallerType: zip
NestedInstallerType: portable
NestedInstallerFiles:
- RelativeFilePath: Rclone Explorer\\Rclone Explorer.exe
  PortableCommandAlias: rclone-explorer
ReleaseDate: {datetime.date.today().isoformat()}
Installers:
- Architecture: x64
  InstallerUrl: {REPO}/releases/download/v{ver}/RcloneExplorer-{ver}-windows-x64.zip
  InstallerSha256: {sha}
ManifestType: installer
ManifestVersion: {SCHEMA}
""",
    f"{PKG}.locale.en-US.yaml": HEAD.format(kind="defaultLocale", v=SCHEMA) + f"""
PackageIdentifier: {PKG}
PackageVersion: {ver}
PackageLocale: en-US
Publisher: Jackpison
PublisherUrl: https://github.com/Jackpison
PackageName: Rclone Explorer
PackageUrl: https://jackpison.github.io/RcloneExplorer/
License: MIT
LicenseUrl: {REPO}/blob/main/LICENSE
ShortDescription: Portable rclone GUI for browsing, transferring and syncing cloud storage.
Description: |-
  Rclone Explorer (formerly Rclone Browser) is a modern GUI for rclone. Browse, upload, download and sync
  files on Google Drive, OneDrive, Dropbox, pCloud, S3 and 70+ other services, with tabs, icon and details
  views, pausable transfers and saved tasks. rclone is bundled and kept up to date.
Moniker: rclone-explorer
Tags:
- rclone
- cloud-storage
- google-drive
- onedrive
- dropbox
- s3
- sync
- backup
ReleaseNotesUrl: {REPO}/releases/tag/v{ver}
ManifestType: defaultLocale
ManifestVersion: {SCHEMA}
""",
}

target = os.path.join(out, "manifests", "j", "Jackpison", "RcloneExplorer", ver)
os.makedirs(target, exist_ok=True)
for name, text in files.items():
    with open(os.path.join(target, name), "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
print("wrote", target)
