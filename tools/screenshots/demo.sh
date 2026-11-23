#!/bin/bash
# Demo set-up for the README and website screenshots: see README.md here.
#   demo.sh setup | run <1 light, 2 dark> | capture
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
D=${SHOTS:-/tmp/rclone-explorer-shots}
REAL=${RCLONE:-$(command -v rclone || true)}
APP=$REPO/build/build/rclone-explorer
export DISPLAY=${DISPLAY_SHOTS:-:99}

setup() {
[ -x "$REAL" ] || { echo "rclone not found: put it on the PATH or set RCLONE" >&2; exit 1; }
rm -rf "${D:?}/home" "${D:?}/data"
mkdir -p "$D/home/.config/rclone" "$D/data" "$D/bin" "$D/dl"
mk() { mkdir -p "$(dirname "$1")"; truncate -s "$2" "$1"; touch -d "$3" "$1"; }
# Google Drive
G=$D/data/gdrive
for y in Paris Rome Lisbon; do for i in $(seq -w 1 24); do mk "$G/Photos/Trips/$y/IMG_20$i.jpg" $((3000000+RANDOM*60)) "2026-0$((RANDOM%9+1))-1$((RANDOM%9)) 10:00"; done; done
for i in $(seq -w 1 12); do mk "$G/Videos/Holiday_$i.mp4" $((400000000+RANDOM*9000)) "2026-07-0$((RANDOM%9+1)) 18:00"; done
mk "$G/Documents/Budget 2026.xlsx" 48213 "2026-11-02 09:12"; mk "$G/Documents/Lease agreement.pdf" 1843210 "2026-03-14 16:40"
mk "$G/Documents/Notes.txt" 3120 "2026-11-20 08:01"; mk "$G/Documents/Presentation.pptx" 8421337 "2026-10-28 14:22"
mkdir -p "$G/Music" "$G/Backups"; for i in $(seq -w 1 9); do mk "$G/Music/Track $i.flac" $((30000000+RANDOM*300)) "2025-12-0$i 12:00"; done
mk "$G/readme.md" 2048 "2026-11-01 12:00"; mk "$G/Taxes/2025/Return.pdf" 912000 "2026-04-10 10:00"; mk "$G/Taxes/2025/Receipts.zip" 23400000 "2026-04-10 10:00"
# copies
cp -a "$G/Photos/Trips/Paris" "$D/data/pcloud-Paris"; mkdir -p "$D/data/pcloud/Backups"; mv "$D/data/pcloud-Paris" "$D/data/pcloud/Backups/Paris"
mkdir -p "$D/data/koofr/Photos/Paris"; cp -a "$G"/Photos/Trips/Paris/IMG_200{1..9}.jpg "$D/data/koofr/Photos/Paris/" 2>/dev/null || true
cp -a "$G"/Photos/Trips/Paris/IMG_201* "$D/data/koofr/Photos/Paris/"
mkdir -p "$D/data/onedrive/Archive"; cp -a "$G/Videos" "$D/data/onedrive/Archive/Videos"; mkdir -p "$D/data/onedrive/Work" "$D/data/onedrive/Pictures"
mkdir -p "$D/data/dropbox/Shared" "$D/data/dropbox/Apps"; mk "$D/data/dropbox/Getting Started.pdf" 249000 "2025-01-01 10:00"
mkdir -p "$D/data/s3/backups-2026" "$D/data/filen" "$D/data/pc/Holidays"
cp -a "$G/Photos/Trips/Rome" "$D/data/pc/Holidays/Rome"
# folders get plausible dates too (not "today")
n=0; find "$D/data" -mindepth 1 -type d | sort -r | while read -r d; do n=$((n+1)); touch -d "2026-1$((n%2))-$(printf %02d $((n%20+1))) $(printf %02d $((n%12+8))):$(printf %02d $((n*7%60)))" "$d"; done
cat > "$D/home/.config/rclone/rclone.conf" <<CONF
[Google Drive]
type = alias
remote = $D/data/gdrive

[OneDrive]
type = alias
remote = $D/data/onedrive

[Dropbox]
type = alias
remote = $D/data/dropbox

[pCloud]
type = alias
remote = $D/data/pcloud

[Koofr]
type = alias
remote = $D/data/koofr

[Filen]
type = alias
remote = $D/data/filen

[Backups S3]
type = alias
remote = $D/data/s3
CONF
cat > "$D/bin/rclone" <<WRAP
#!/bin/bash
# real rclone, but "listremotes --long" reports the type each demo remote pretends to be
if [[ " \$* " == *" listremotes "* && " \$* " == *" --long "* ]]; then
  "$REAL" "\$@" | sed -E 's/^(Google Drive: +)alias/\1drive/; s/^(OneDrive: +)alias/\1onedrive/; s/^(Dropbox: +)alias/\1dropbox/; s/^(pCloud: +)alias/\1pcloud/; s/^(Koofr: +)alias/\1koofr/; s/^(Filen: +)alias/\1filen/; s/^(Backups S3: +)alias/\1s3/'
  exit \${PIPESTATUS[0]}
fi
exec "$REAL" "\$@"
WRAP
chmod +x "$D/bin/rclone"
}

window() { xdotool search --name "^Rclone Explorer" | head -1; }
shot() { import -window "$(window)" "$1"; }

run() {
  pkill -f "$APP" || true; sleep 0.5
  pgrep -f "Xvfb $DISPLAY" >/dev/null || { Xvfb "$DISPLAY" -screen 0 1600x1000x24 >/dev/null 2>&1 & sleep 1; }
  mkdir -p "$D/home/.config/rclone-browser" "$D/runtime"; chmod 700 "$D/runtime"
  # the remote logos, where the app looks for them next to the program
  mkdir -p "$(dirname "$APP")/icons/remotes"; cp "$REPO"/icons/remotes/*.png "$(dirname "$APP")/icons/remotes/"
  cat > "$D/home/.config/rclone-browser/rclone-browser.conf" <<CONF
[Settings]
rclone=$D/bin/rclone
rcloneConf=
checkAppUpdates=false
checkRcloneUpdates=false
theme=${1:-2}
iconSize=medium
defaultDownloadDir=$D/dl
bandwidthLimit=4M
showFolderSizes=true
CONF
  (cd "$D/home" && HOME="$D/home" XDG_CONFIG_HOME="$D/home/.config" XDG_RUNTIME_DIR="$D/runtime" \
    QT_QPA_PLATFORMTHEME= nohup "$APP" > "$D/app.log" 2>&1 &) </dev/null >/dev/null 2>&1
  sleep 4
  xdotool windowmove "$(window)" 0 0 windowsize "$(window)" 1400 820; sleep 1.5
}

capture() {
  mkdir -p "$D/out"
  park() { xdotool mousemove 1390 810; sleep 1; }
  run 2; park
  shot "$D/out/home.png"
  xdotool mousemove 120 361 click 1; sleep 2; xdotool mousemove 120 319 click 1; sleep 3; park
  shot "$D/out/browser.png"
  xdotool mousemove 261 36 click 1; sleep 2; park
  shot "$D/out/browser-light.png"
  xdotool key ctrl+shift+2; sleep 1.5; xdotool mousemove 430 369 click --repeat 2 --delay 80 1; sleep 2
  xdotool mousemove 430 402 click 1; park
  sleep 3 # folder sizes are worked out in the background
  shot "$D/out/details-light.png"
  # Search, from the top of the remote: the Search window on its own
  xdotool mousemove 434 217 click 1; sleep 1.5 # breadcrumb: Google Drive
  xdotool key ctrl+shift+f; sleep 1.5
  local search; search=$(xdotool search --name "^Search Google Drive" | head -1)
  xdotool windowfocus --sync "$search"; sleep 0.3
  xdotool type --delay 40 "img_202"; xdotool key Return; sleep 3
  import -window "$search" "$D/out/search-light.png"
  echo "Screenshots in $D/out"
}

case "$1" in
  setup) setup ;;
  run) run "$2" ;;
  capture) setup; capture ;;
  *) echo "usage: $0 setup | run <1 light, 2 dark> | capture" >&2; exit 1 ;;
esac
