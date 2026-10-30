#!/usr/bin/env bash
# Retakes the four documentation screenshots in docs/ with demo data.
#
# Linux only (Xvfb + xdotool + ImageMagick). Uses the Selawik font for a
# Windows-like look if it is installed.
#
#   docs/guide/take_screenshots.sh <path-to-rclone-explorer> <path-to-rclone>
#
# Everything runs in a throwaway HOME, so your own settings are not touched.
# Click positions assume the window sizes below; if the layout changes,
# check the images and adjust the coordinates.
set -euo pipefail

APP=$(realpath "${1:?path to the rclone-explorer binary}")
RCLONE=$(realpath "${2:?path to the rclone binary}")
DOCS=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'pkill -x rclone-explorer 2>/dev/null || true; rm -rf "$WORK"; rm -f /tmp/dl/wetransfer_sagar_album_page_001-jpg_2023-03-23_0741-with-a-very-long-name.zip*' EXIT

export HOME="$WORK/home" XDG_CONFIG_HOME="$WORK/home/.config" XDG_DATA_HOME="$WORK/home/.local/share"
mkdir -p "$XDG_CONFIG_HOME/rclone" "$XDG_CONFIG_HOME/rclone-browser"
DL=/tmp/dl   # short path for the transfer card; only the demo zip is written and removed
mkdir -p "$DL"

# ---- demo files --------------------------------------------------------------
F="$WORK/myfiles"
mkdir -p "$F/Backups" "$F/Documents" "$F/Music" "$F/Photos"
head -c 180019 /dev/urandom > "$F/Documents/Budget 2026.xlsx"
head -c 52019 /dev/urandom > "$F/Documents/Notes.txt"
head -c 919962 /dev/urandom > "$F/Documents/Report.pdf"
head -c 7025459 /dev/urandom > "$F/Music/track01.mp3"
printf 'hi\n' > "$F/readme.md"
head -c 60M /dev/urandom > "$F/Backups/wetransfer_sagar_album_page_001-jpg_2023-03-23_0741-with-a-very-long-name.zip"
for i in 1 2 3; do head -c 300000 /dev/urandom > "$F/Photos/IMG_000$i.jpg"; done
find "$F" -exec touch -d "2026-10-08 18:18:39" {} +
touch -d "2026-10-10 02:59:23" "$F/Backups"

# ---- demo remotes (only "My Files" is real; the others just show on Home) ----
cat > "$XDG_CONFIG_HOME/rclone/rclone.conf" <<EOF
[Google Drive]
type = drive

[OneDrive]
type = onedrive

[Dropbox]
type = dropbox

[Backups S3]
type = s3
provider = AWS

[Home server]
type = sftp
host = 192.0.2.10

[Vault]
type = crypt
remote = My Files:Backups
password = $("$RCLONE" obscure demo-password)

[My Files]
type = alias
remote = $F
EOF

settings() { # $1 = theme (1 light, 2 dark)
  cat > "$XDG_CONFIG_HOME/rclone-browser/rclone-browser.conf" <<EOF
[Settings]
rclone=$RCLONE
rcloneConf=$XDG_CONFIG_HOME/rclone/rclone.conf
checkRcloneUpdates=false
checkRcloneBrowserUpdates=false
theme=$1
defaultDownloadDir=$DL
defaultDownloadOptions=--bwlimit 1M
EOF
}

DISP=:97
start() { # $1 width, $2 height
  Xvfb $DISP -screen 0 1600x1000x24 >/dev/null 2>&1 &
  XVFB=$!
  sleep 1
  DISPLAY=$DISP "$APP" >/dev/null 2>&1 &
  sleep 3
  W=$(DISPLAY=$DISP xdotool search --sync --name "Rclone Explorer" | head -1)
  DISPLAY=$DISP xdotool windowmove "$W" 0 0 windowsize "$W" "$1" "$2"
  sleep 2
}
x() { DISPLAY=$DISP xdotool "$@"; }
shot() { # $1 file, $2 width, $3 height
  x mousemove 1590 990; sleep 0.5
  DISPLAY=$DISP import -window root -crop "$2x$3+0+0" +repage "$DOCS/$1"
  echo "wrote docs/$1"
}
stop() { pkill -x rclone-explorer || true; pkill -x rclone || true; sleep 1; kill "$XVFB" 2>/dev/null || true; sleep 1; }

# sidebar positions (same for every window size)
TRANSFERS="83 133"; VAULT="83 488"; MYFILES="83 530"

# 1. Home, dark
settings 2; start 1240 780
shot home.png 1240 780; stop

# 2. Browser with two tabs, Icons view, dark
settings 2; start 1400 820
x mousemove $VAULT click 1; sleep 2; x mousemove $MYFILES click 1; sleep 2.5
shot browser.png 1400 820; stop

# 3. Details view with Documents and Music expanded, light
settings 1; start 1240 780
x mousemove $MYFILES click 1; sleep 2.5; x key ctrl+shift+2; sleep 2
x mousemove 348 387 click 1; sleep 2                         # expand Documents
x mousemove 420 524 click 1; sleep 0.5; x mousemove 348 524 click 1; sleep 2   # select + expand Music
shot details-light.png 1240 780; stop

# 4. Running download with the card expanded, dark
settings 2; start 1240 780
x mousemove $MYFILES click 1; sleep 2.5
x key ctrl+shift+2; sleep 2                                  # Details view
x mousemove 348 353 click 1; sleep 2                         # expand Backups
x mousemove 520 387 click 1; sleep 0.5; x key ctrl+d; sleep 2; x key Return; sleep 2
x mousemove $TRANSFERS click 1; sleep 1
x mousemove 392 147 click 1; sleep 10                        # expand the card, let it run a bit
shot transfers-dark.png 1240 780; stop
