<p align="center"><img src="docs/logo.png" width="96" alt="Rclone Explorer logo"></p>

<h1 align="center">Rclone Explorer – rclone GUI for Windows and Linux</h1>

<p align="center"><i>Formerly <b>Rclone Browser</b>. Same project, new name: a revival of the original by mmozeiko, DinCahill and kapitainsky.</i></p>

<p align="center">A modern rclone GUI: browse, transfer and sync files on all your cloud storage, powered by <a href="https://rclone.org/">rclone</a>.<br>Portable, nothing to install: unzip or run it from anywhere, even a USB stick.</p>

<p align="center"><b><a href="../../releases/latest">Download the latest release</a></b> · <a href="https://jackpison.github.io/RcloneExplorer/">Website</a> · <a href="docs/Rclone-Explorer-Guide.pdf">User guide (PDF)</a> · <a href="docs/remote-icons.md">Remote icons</a> · <a href="CHANGELOG.md">Changelog</a></p>

![Browsing a remote with tabs in dark mode](docs/browser.png)

## Features

Rclone Explorer gives rclone a full graphical interface, so you can manage cloud files without the command line:

- **Modern design** in black and gold or warm light, with a one-click **light / dark** switch next to the app name
- **All your clouds in one place**: Google Drive, OneDrive, Dropbox, pCloud, S3, SFTP, encrypted remotes and every other rclone storage type, using your existing rclone configuration. Logos for 34 popular services are included; you can [add your own](docs/remote-icons.md).
- **File Explorer–style tabs**: open several remotes, or the same remote several times (Ctrl+T)
- **Icons and Details views**: extra large to small icons (Ctrl + mouse wheel), or a sortable list with name, size, modified date, type, extension and path
- Upload, download, create folders, rename, move and delete; drag and drop files from your file manager to upload
- **Transfers you can pause and resume**, with live progress, speed and time left
- Saved **tasks**: store a transfer once, run it again with one click (or as a dry run)
- Sidebar remotes sortable by name, type or your own order (drag and drop)
- Saved tasks sortable by name or type
- Breadcrumb address bar, filter the current folder (Ctrl+F)
- Mount remotes as drives and stream media to a player such as [VLC](https://www.videolan.org/)
- **Built-in rclone updater**: rclone is bundled and kept up to date, every download checksum-verified

![Remotes on the Home page](docs/home.png)

![A running transfer](docs/transfers-dark.png)

![Details view in light mode](docs/details-light.png)

## Getting started

### Windows 10 / 11

1. Download `RcloneExplorer-<version>-windows-x64.zip` from the [latest release](../../releases/latest).
2. Extract it to any folder and run `Rclone Explorer.exe`.
3. Windows may show "Windows protected your PC" because the app is not code-signed: click **More info**, then **Run anyway**.

Mounting remotes as drive letters needs [WinFsp](https://winfsp.dev/).

#### With Scoop

```
scoop bucket add rclone-explorer https://github.com/Jackpison/RcloneExplorer
scoop install rclone-explorer
```

Settings, saved tasks and the bundled rclone are kept when Scoop updates the app.

### Linux (x86_64)

1. Download `RcloneExplorer-<version>-x86_64.AppImage` from the [latest release](../../releases/latest).
2. Make it executable: `chmod +x RcloneExplorer-*.AppImage` (or *Properties → Allow executing as program*).
3. Run it.

Works on most distributions (Ubuntu, Fedora, Debian, Mint, Arch, openSUSE…). If it does not start, install FUSE 2 (`sudo apt install libfuse2t64` on Ubuntu 24.04+, `libfuse2` on older releases) or run it with `--appimage-extract-and-run`. Mounting needs FUSE 3 (`fuse3`).

Your existing rclone remotes appear automatically on both systems. If you have none yet, click **New remote** on the Home page (or **+** next to *Remotes* in the sidebar) to set one up with rclone's assistant. The **[user guide](docs/Rclone-Explorer-Guide.pdf)** explains every feature in detail.

## Upgrading from Rclone Browser (4.x and older)

- **Windows, portable:** extract the new zip to a new folder, then copy your old `RcloneBrowser.ini` (4.x) or `RcloneExplorer.ini` (5.0), and `tasks.bin`, next to `Rclone Explorer.exe`. The settings are picked up automatically on the first start; the old file stays untouched as a backup.
- **Windows, not portable (no ini file):** nothing to do, settings are kept.
- **Linux:** nothing to do. Settings stay in `~/.config/rclone-browser/` on purpose.

## Where things are stored

**Windows (portable):** `Rclone Explorer.ini` next to the exe switches portable mode on, and everything stays in the app folder:

| What | Where |
|---|---|
| rclone | `rclone\rclone.exe` (updated from inside the app) |
| rclone configuration | `%APPDATA%\rclone\rclone.conf` by default; copy it to `rclone\rclone.conf` to take your remotes with you |
| Settings / saved tasks | `Rclone Explorer.ini` / `tasks.bin` |
| Remote logos | `icons\remotes\<type>.png` (see [remote-icons.md](docs/remote-icons.md)) |
| Documentation | `docs\` |
| Qt plugins | `qt\plugins` (located via `qt.conf`) |

**Linux:** settings in `~/.config/rclone-browser/`, the rclone updated by the app in `~/.local/share/rclone-browser/rclone-browser/`, your rclone configuration in its usual place (`~/.config/rclone/rclone.conf`). To make the AppImage portable, create a folder named like the AppImage plus `.config` next to it (for example `RcloneExplorer-<version>-x86_64.AppImage.config`); settings and an updated rclone then stay beside the AppImage. Your own remote logos go in `~/.local/share/rclone-browser/rclone-browser/icons/remotes/` (or `icons/remotes/` next to the AppImage in portable mode).

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+T / Ctrl+W | New tab / close tab (or middle-click the tab) |
| Ctrl+F | Filter the current folder |
| Ctrl+Shift+1 / Ctrl+Shift+2 | Icons view / Details view |
| Ctrl + mouse wheel | Bigger / smaller icons |
| Enter, double-click | Open folder |
| Alt+Up | Parent folder |
| F5 · F7 · F2 · Del | Refresh · New folder · Rename · Delete |
| Ctrl+U / Ctrl+D | Upload / Download |

## Security

- Cloud credentials stay in rclone's own configuration; the app never stores them. A config password is kept only in memory and passed to rclone via an environment variable.
- rclone updates are HTTPS-only, verified against rclone's SHA-256 checksums, size-limited and test-run before installation.
- rclone and media players are started without a shell, so file names cannot inject commands.
- The Windows build enables Control Flow Guard, CET shadow-stack compatibility, ASLR and DEP.
- Releases are built by GitHub Actions from this repository with every action and build tool pinned to an exact version and checksum; each release publishes SHA-256 checksums. Both packages are launched in a self-test before publishing.

## Feedback

Feedback and bug reports are welcome! Please [open an issue](../../issues) and include the output from *Show Output* on the transfer card if something fails.

## Building from source

Every push is built by GitHub Actions (`.github/workflows/update.yml`) for Windows and Linux; pushing a tag such as `v4.0.0` publishes a release with both packages.

**Windows:** Visual Studio 2022 or newer (C++ desktop development), CMake 3.21+, Ninja and Qt 6.10+ for MSVC 64-bit (including Qt SVG). From a *x64 Native Tools Command Prompt*:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\msvc2022_64
cmake --build build
ctest --test-dir build
```

**Linux:** a C++20 compiler, CMake, Ninja and Qt 6.10+ (Qt SVG included), e.g. on Ubuntu `sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev`:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/build/rclone-explorer
```

The user guide is generated with `python docs/guide/build_guide.py` (needs `pip install reportlab`). The website lives in `site/` and is published to GitHub Pages by `.github/workflows/pages.yml`.

## Credits

Based on Rclone Browser by [mmozeiko](https://github.com/mmozeiko/RcloneBrowser), [DinCahill](https://github.com/DinCahill/RcloneBrowser) and [kapitainsky](https://github.com/kapitainsky/RcloneBrowser). Interface icons from [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) by Microsoft (MIT). Licensed under the MIT license.
