#!/usr/bin/env python3
"""Writes the AppStream metadata for one build.

    python3 packaging/linux/metainfo.py <version> <out.xml> [desktop-id]

Adds this version to <releases> (with today's date and the release page) and,
when given, points <launchable> at another desktop file name (the Flatpak
installs it as io.github.jackpison.RcloneExplorer.desktop).
"""
import datetime
import os
import re
import sys

ver, out = sys.argv[1], sys.argv[2]
desktop = sys.argv[3] if len(sys.argv) > 3 else None
if not re.fullmatch(r"\d+\.\d+\.\d+", ver):
    sys.exit("version must look like 1.2.3")

src = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "io.github.jackpison.RcloneExplorer.metainfo.xml")
xml = open(src, encoding="utf-8").read()
today = datetime.date.today().isoformat()
rel = (f'  <releases>\n    <release version="{ver}" date="{today}">\n'
       f'      <url>https://github.com/Jackpison/RcloneExplorer/releases/tag/v{ver}</url>\n'
       f'    </release>\n  </releases>\n')
xml = xml.replace("</component>", rel + "</component>")
if desktop:
    xml = re.sub(r'<launchable type="desktop-id">[^<]*</launchable>',
                 f'<launchable type="desktop-id">{desktop}</launchable>', xml)
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
open(out, "w", encoding="utf-8").write(xml)
