# Screenshots

The screenshots in `docs/` (used by the README and the website) are taken
from a demo set-up: remotes named Google Drive, OneDrive, Dropbox, pCloud,
Koofr, Filen and Backups S3, which are really folders on this computer, with
made-up files. No cloud account is needed. The remotes show the real logos
from `icons/remotes/`.

Needs Linux with `Xvfb`, `xdotool` and ImageMagick (`import`), a built
`build/build/rclone-explorer` and rclone on the `PATH` (or in `$RCLONE`).

```bash
tools/screenshots/demo.sh setup     # demo files and rclone.conf
tools/screenshots/demo.sh capture   # home, browser, browser-light, details-light, search-light
tools/screenshots/demo.sh run 2     # start the app (1 = light, 2 = dark) for other shots
```

Shots land in `$SHOTS/out` (default `/tmp/rclone-explorer-shots/out`); look
at them, then copy the good ones to `docs/`. Everything is kept outside the
repository and your own settings: the app runs with a separate home folder.

The Transfers shot needs a running transfer: `run 2`, open Google Drive →
Videos, select a video, **Download**, set the destination and **Run**
(the demo's speed limit is 4 MB/s, so the card stays on screen).
