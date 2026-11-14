#!/usr/bin/env python3
"""Writes the Flathub copy of the Flatpak manifest for one release.

    python3 packaging/flatpak/flathub_manifest.py <version> <commit> <out-dir>

The manifest in this folder builds the local checkout (`type: dir`); Flathub
builds from the release's git tag instead, pinned to its commit.
"""
import os
import re
import sys

ver, commit, out = sys.argv[1], sys.argv[2], sys.argv[3]
if not re.fullmatch(r"\d+\.\d+\.\d+", ver) or not re.fullmatch(r"[0-9a-f]{40}", commit):
    sys.exit("usage: flathub_manifest.py <x.y.z> <40-hex commit> <out-dir>")

here = os.path.dirname(os.path.abspath(__file__))
name = "io.github.jackpison.RcloneExplorer.yml"
text = open(os.path.join(here, name), encoding="utf-8").read()
local = re.search(r"      - type: dir\n        path: \.\./\.\.\n(        skip:\n(?:          - .*\n)+)?", text)
if not local:
    sys.exit("the app source (type: dir) was not found in the manifest")
git = ("      - type: git\n"
       "        url: https://github.com/Jackpison/RcloneExplorer.git\n"
       f"        tag: v{ver}\n"
       f"        commit: {commit}\n"
       "        x-checker-data:\n"
       "          type: git\n"
       "          tag-pattern: ^v([\\d.]+)$\n")
text = text[:local.start()] + git + text[local.end():]
# the header comment describes the local build only
text = re.sub(r"\A(#.*\n)+", "# Rclone Explorer on Flathub. Generated for v%s by the release workflow.\n" % ver, text)
os.makedirs(out, exist_ok=True)
open(os.path.join(out, name), "w", encoding="utf-8").write(text)
