# Changelog

## [Unreleased]

### Added
-   **Linux: the AppImage can update itself with AppImageUpdate** and similar tools. It now carries update information, and each release includes a matching `.zsync` file
-   Linux: richer AppStream metadata (description, screenshots, version), which software centres and the AppImage catalog show

-   **Install with Scoop:** `scoop bucket add rclone-explorer https://github.com/Jackpison/RcloneExplorer`, then `scoop install rclone-explorer`. Settings, saved tasks and the bundled rclone are kept across updates

### Changed
-   Release notes no longer end with an invitation to send feedback
-   Portable mode writes the settings file in place instead of replacing it, so package managers that keep it linked across updates (Scoop) do not lose changes
-   The user guide is updated for Linux, Saved tasks sorting, the new status bar and the new confirmation prompts, with fresh screenshots
-   `LICENSE` is now recognised as MIT by GitHub

## [5.3.0] - 2026-10-10

### Fixed
-   **Linux: the AppImage runs on older distributions again.** 5.2.1 and earlier were built on Ubuntu 24.04 and needed glibc 2.38, so they quit right away on Ubuntu 22.04, Debian 12 and similar systems. The AppImage is now built on Ubuntu 22.04 (glibc 2.35), and the build fails if anything inside it needs a newer glibc

### Changed
-   **Linux: the AppImage file name no longer contains "linux"**: `RcloneExplorer-<version>-x86_64.AppImage`, as the AppImage catalog expects. Settings are not affected. If you use portable mode, rename the `.config` folder next to the AppImage to match the new file name

## [5.2.1] - 2026-10-10

### Fixed
-   The Run, Dry run, Edit and Delete buttons on the Saved tasks page sat at the very bottom of the window and were cut off. They are now a bar right under the page title, above the list, with Run first
-   The Run button on the Saved tasks page now shows in the accent colour when a task is selected, like other main buttons

### Changed
-   The status bar now shows the app version on the left and the rclone version on the right

## [5.2.0] - 2026-10-10

### Added
-   **Saved tasks can be sorted.** A "Sort by" menu on the Saved tasks page offers Order saved (the default, as before), Name (A to Z), Name (Z to A) and Type (downloads first). Names are sorted ignoring case and with numbers in natural order, so "Task 2" comes before "Task 10". The choice is remembered, and the selected task stays selected when you change the sort. Sorting only changes how the list is shown; the saved tasks file is not touched
-   The status bar shows the app version on the right, opposite the rclone version on the left

### Changed
-   Redesigned About dialog: version on its own line, website and source links, and text that wraps instead of being cut off
-   Clearer confirmation prompts when cancelling a transfer, unmounting, stopping a stream or quitting with jobs running: a short question, an explanation on a second line, buttons that say what they do (for example "Cancel transfer" and "Keep running") and more space around the icon. Escape always keeps things running

### Fixed
-   A saved task that was run showed a number instead of Copy, Move or Sync in its transfer card title

## [5.1.0] - 2026-10-10

### Changed
-   **Windows: the program and its folder are now called `Rclone Explorer.exe` in `Rclone Explorer`** (with a space). The download is still `RcloneExplorer-<version>-windows-x64.zip`, so existing links keep working. The portable settings file is now `Rclone Explorer.ini`; an existing `RcloneExplorer.ini` (5.0) or `RcloneBrowser.ini` (4.x) next to the program is copied automatically (the newest one wins, and a settings file that is already in use is never overwritten)
-   The interface font is back to Segoe UI Variable / Segoe UI (Windows), with Selawik, Noto Sans and similar fonts as fallbacks elsewhere, instead of Trebuchet MS

### Fixed
-   Per-file progress bars sat at the top of their row instead of lining up with the file name. Each row is now name, bar and percentage, vertically centred; long file names are shortened in the middle

### Faster
-   Icons are rendered once and cached. Before, every repaint re-parsed an SVG: in a benchmark, painting 40,000 icons went from 1.65 s to 0.03 s (about 48 times faster), so the sidebar and long file lists repaint and scroll more smoothly
-   Colour lookups (1.8x faster) and the interface font (4.6x faster) are cached; both are used on every repaint
-   File types and extensions are worked out once per file and without building a `QFileInfo`, row lookups no longer search linearly, and rclone log lines that are not progress lines skip all pattern matching

### Improved
-   Removed the background thread that loaded file icons: less code and no cross-thread sharing
-   Log windows keep their newest 5000 lines instead of clearing completely at 10000
-   Tests: the file-extension logic is checked against Qt's own for 19 tricky names, and the settings migration has 12 tests

## [5.0.0] - 2026-10-10

### Changed
-   **New name: Rclone Explorer** (formerly Rclone Browser). The app now works like a file explorer (tabs, breadcrumbs, icon and detail views), and the old name was shared by the original project and several forks. Window titles, About box, downloads (`RcloneExplorer-<version>-windows-x64.zip`, `RcloneExplorer-<version>-linux-x86_64.AppImage`), the program (`RcloneExplorer.exe`, `rclone-explorer`), documentation and website all use the new name
-   The repository is now `Jackpison/RcloneExplorer` and the website `https://jackpison.github.io/RcloneExplorer/` (old repository links redirect; the old website address does not)
-   The PDF guide is now `Rclone-Explorer-Guide.pdf`

### Upgrading
-   **Settings carry over.** On Windows, a portable `RcloneBrowser.ini` next to the new exe is copied to `RcloneExplorer.ini` automatically (an ini that is already in use is never overwritten). Registry settings on Windows and `~/.config/rclone-browser` on Linux keep their location on purpose

### Fixed
-   `site/sitemap.xml` was empty since 4.0.0; it is regenerated for the new address
-   A few dialogs (e.g. the config password prompt) showed the internal name `rclone-browser` as their title

## [4.1.0] - 2026-10-09

### Fixed
-   **Text size, for good.** Parts of the app (the file list in cloud folders, headers, menus, popups) started with Windows' small default font and only became the right size after switching the theme. Cause: Windows keeps its own font per widget type and can reset them while the app runs, and the app only patched individual widgets, one report at a time. Now the style sheet itself sets the text size for every widget, a guard restores the fonts if the system resets them, and the 15 scattered per-widget font fixes are gone
-   Breadcrumb arrows in the address bar kept the old theme's colour after a theme switch
-   A new self-test runs on both platforms before every release: it checks 17 kinds of widget (including dialogs, menus and the file list) at startup, after a simulated Windows font reset, and after theme switches, and blocks the release on any mismatch

### Changed
-   Sidebar footer is now **Settings, GitHub, Help & Update** (was GitHub, Settings, Update & About)
-   Removed leftover code from the original dark-mode feature and the old remote icon sizes
-   Theme switching no longer re-creates the widget style each time, so it is faster and re-polishes widgets once

## [4.0.1] - 2026-10-09

### Fixed
-   Deleting a single file could take a very long time: it used rclone's bulk `delete` command, which lists the whole parent folder (recursively with `--fast-list`) to find the file. It now uses `deletefile`, a single API call
-   Text in message boxes and other popups was too small on Windows

### Changed
-   Clearer delete confirmation: "Delete “name”?", a note that it cannot be undone, a red Delete button and Cancel as the default

## [4.0.0] - 2026-10-09

### New
-   **Linux version**: a portable AppImage for x86_64 (Ubuntu, Fedora, Debian, Mint, Arch and more) with rclone included, built and self-tested alongside the Windows version
-   Details view: new Type, Extension and Path columns; right-click the column header to choose which are shown
-   Sort remotes in the sidebar by name, type or your own drag-and-drop order
-   GitHub link in the sidebar
-   Logos for 34 services included (new: Azure Blob, Backblaze B2, 1Fichier, FileLu, Files.com, Google Cloud Storage, Gofile, Google Photos, HiDrive, iCloud Drive, Internxt, Jottacloud, Mail.ru, OpenDrive, PikPak, Pixeldrain, premiumize.me, put.io, Seafile, Storj, Uloz.to, Zoho WorkDrive)

### Changed
-   Trebuchet MS as the interface font (on Linux when installed; otherwise a similar font)
-   New hard-disk icon for remote roots; larger text in the file list
-   "Help & about" is now "Update & About"; the rclone version is shown on the left of the status bar
-   Tab close button sits lower and turns red on hover
-   Brighter placeholder text in the search box
-   The build workflow is now called "Update" and builds Windows and Linux; releases include both packages and one checksum file

### Fixed
-   "Show Output" in the Folder size and Folder tree windows squeezed the log into a tiny window
-   On Linux, the settings path was wrong when XDG_CONFIG_HOME was not set
-   A saved rclone path that no longer exists is ignored instead of failing
-   Custom remote logos could not be added on Linux (the app folder of an AppImage is read-only)

## [3.7.0] - 2026-10-09

### New
-   Website: https://jackpison.github.io/RcloneExplorer/ (source in `site/`, published automatically)
-   Tabs redesigned like Windows 11 File Explorer: tab strip, selected tab merges into the page, separators, "+" right after the last tab

### Changed
-   Tidier portable folder: Qt plugins in `qt\plugins` (via `qt.conf`), documents in `docs\`, the single-instance lock file in the temp folder
-   Unused Qt plugins (styles, generic, networkinformation) are no longer shipped
-   Every build starts the packaged app in a self-test mode before publishing, so a broken folder layout cannot be released

## [3.6.2] - 2026-10-09

### Fixed
-   Tab titles (and possibly other text) started too small until the theme was switched once; the theme is now applied before any window is created
-   File icons in the Icons view sat too close to the panel edge

### Changed
-   About box links to the original project at github.com/mmozeiko/RcloneBrowser
-   Release pages now show the changelog for that version

## [3.6.0] - 2026-10-09

### New
-   Icon sizes for the Icons view: Extra large, Large, Medium, Small (arrow next to the Icons button), and Ctrl + mouse wheel to zoom
-   New folder icon, pre-rendered at every size for a crisp outline

### Changed
-   Tab close button: larger, centred on the text line, round hover highlight; tab titles are bold
-   Download is now named `RcloneBrowser-<version>-windows-x64.zip`
-   Windows build hardened with Control Flow Guard and CET shadow-stack compatibility (plus ASLR / DEP)

### Fixed
-   Possible crash while browsing: file icons were created on a background thread while the main thread used the same icon cache (data race)
-   The new icon size setting clashed with an old setting of the same name, so icons could come out small

## [3.5.0] - 2026-10-09

### New
-   Logos for Box, Drime, Dropbox, Filen, Google Drive, Koofr, Mega, OneDrive, pCloud, Proton Drive and S3 are included (in `icons\remotes`)
-   Light / dark switch is now an icon next to the app name
-   Command bar adapts to the window width: all labels, labels on the main three buttons, or icons only (names as tooltips), so buttons never overlap

### Changed
-   The app is called "Rclone Browser" everywhere (no "Portable" suffix)
-   About box: updated copyright, maintainer and links
-   Sidebar: brighter text, filled icon for the selected item, footer styled like the menu, "Saved tasks" and "Help & about" wording
-   "Shared with me" for Google Drive moved into the More menu
-   Larger default window size on first start

### Fixed
-   Command bar labels overlapped on narrower windows ("New folder" / "Upload")
-   "Shared with me" crowded the view buttons
-   Tab close button was oddly placed and sized
-   Remote logos that are not square are no longer stretched

## [3.1.0] - 2026-10-09

### Fixed
-   Sidebar, Home cards, file lists and task list could start with Windows' small default list font (correct only after switching the theme); the app now sets its own fonts everywhere
-   Transfer output ("Show Output") used the tiny system fixed font; now Cascadia Mono / Consolas at a readable size (also in the Folder size / Folder tree window)
-   Size and Modified columns in the Details view used the small fixed font
-   Finished transfers kept showing the last live reading ("100% · 3.9 MiB/s · 0s left"); they now show "Done · 47.3 MiB in 11.3s" or "Stopped after …"
-   "Time left" showed rclone's raw "-" instead of a dash placeholder

### Changed
-   New gold logo

## [3.0.0] - 2026-10-09

### Design
-   New black-and-gold look (and a warm light variant) with bright gold accents
-   Always-visible Light / Dark switch in the sidebar
-   Larger text throughout (12 pt body, 28 pt page titles)
-   Reorganised sidebar: brand title, gold selection, transfer count badge, Remotes section with count and add button, separated footer
-   Icons view with large file and folder icons (now the default); Details view one click away
-   Tabs redesigned as rounded chips; title bar reads "Rclone Browser Portable"
-   New app logo

### New
-   Remote icon reference for all 69 rclone storage types (docs/remote-icons.md, also inside the portable zip)
-   17-page PDF user guide (docs/Rclone-Browser-Guide.pdf), generated by docs/guide/build_guide.py
-   Drag and drop upload also works in the Icons view
-   Releases include a SHA-256 checksum file

### Faster
-   Opening a folder runs one `rclone lsjson` instead of two rclone calls (lsd + lsl)
-   Faster drawing of large folders (uniform row heights, batched icon layout)

### Security
-   rclone updater: HTTPS-only final URL and download size limit, in addition to SHA-256 verification
-   Build workflow: every action pinned to an exact commit, least-privilege permissions, credentials not persisted

### Fixed
-   Pages could show through each other when tabs were opened or closed
-   Google Cloud Storage, Google Photos and Oracle Object Storage remotes showed the generic tile
-   Command bar labels could be cut off
-   Removed the "Remotes icons size" setting, which no longer had any effect

### Changed
-   C++20, Qt 6.10, modern Qt CMake setup; GitHub Actions updated to current versions (Node 24)

## [2.0.0] - 2026-10-08
First release of the revived, Windows-focused Rclone Browser.

### Design
-   Windows 11 design: navigation pane with all remotes, Home page with remote cards, command bar, breadcrumb address bar, Explorer-style tabs
-   Fluent UI icons, WinUI 3 colours in light and dark, Windows accent colour, Segoe UI Variable, tinted title bar, rounded menus
-   Larger text and higher contrast; boxed sections for the command bar, address bar, file list and transfer details
-   Colour-coded file type icons (folders, images, video, audio, archives, PDFs, documents, spreadsheets, code)
-   Colour-coded tile per storage service; custom icons can be placed in `icons\remotes\<type>.png` next to the exe
-   New app icon
-   Theme setting: Same as Windows / Light / Dark, applied instantly

### New
-   Pause and resume transfers
-   Tabs: open several remotes, or the same remote several times (Ctrl+T, Ctrl+W, middle-click to close)
-   Transfer cards show progress, speed and time left at a glance; details as labelled tiles
-   Filter the current folder (Ctrl+F), Up (Alt+Up), copy path, Ctrl+U / Ctrl+D for upload and download
-   Built-in rclone updater: daily check, SHA-256 verified download, safe while transfers are running
-   First run without rclone offers a one-click download
-   Portable mode picks up `rclone\rclone.conf` so remotes travel with the folder
-   Windows portable zip built by GitHub Actions, with VC++ runtime and the latest rclone bundled

### Fixed
-   Transfer progress, checks and error counts were not shown with rclone 1.56 and newer
-   Transfer details could be cut off on the right; status text was invisible in dark mode
-   Files whose names start or end with a space could not be opened or downloaded
-   "Export list of files" could silently drop entries
-   Startup froze while checking for updates on slow or offline networks
-   Media streaming with a player path containing spaces
-   Files smaller than 10 bytes showed size 0; sizes now read "6.7 MB"
-   Enter in the transfer dialog started a dry run instead of the transfer

### Changed
-   Ported to Qt 6; Windows only (macOS, Linux and BSD packaging removed)
-   rclone output parsing moved into one module with unit tests

## Earlier versions

The entries below are from the original Rclone Browser project.

## [1.8.0][1.8.0] - 2020-02-17
-   NEW: http(s) proxy configuration for rclone
-   NEW: remotes icons size option selector
-   NEW: directories tree display for remotes
-   NEW: rclone extra default options for all operations (e.g. --fast-list)
-   NEW: added "Public Link" button to remote view
-   FIXED: option to show hidden files and folders was not always working as expected
-   FIXED: for sftp server default to home user directory (as normal sftp would do)
-   FIXED: an issue when on Windows local remote only allowed to browse drive C:
-   FIXED: problem using rclone and rclone.conf when path contained spaces
-   FIXED: bandwidth box on jobs tab is too small for fast connections
-   bunch of usual small tweaks and fixes

## [1.7.0][1.7.0] - 2019-11-27
-   NEW: built all releases with the latest Qt 5.13.2
-   NEW: changed Linux releases format to AppImage only
-   NEW: changed macOS release format to dmg image file
-   NEW: added installer for Windows releases - implemented using [Inno Setup](https://github.com/jrsoftware/issrc)
-   NEW: added Linux i386 release
-   NEW: changed macOS release compilation options to make it work on all macOS versions starting with 10.9
-   NEW: added portable mode for macOS and Linux
-   NEW: on Linux multiple terminals are tried for rclone config ($TERMINAL then gnome-terminal followed by xfce4-terminal, xterm, x-terminal-emulator and konsole)
-   NEW: enabled Qt HighDpiScaling - should help people with high DPI monitors
-   NEW: added dark mode - configurable via preferences or system setting (newer macOS) - thank you @noaione for initial PR
-   changed preferences window - added tabs to create more space for new options
-   fixed Windows portable mode
-   fixed mount/unmount on FreeBSD
-   disabled mount on OpenBSD and NetBSD (as not supported by rclone)
-   updated build and install for Linux - now all files will be installed in /usr/local root
-   fixed possible crashes when old rclone is used (with different version information output)
-   fixed an issue with long file names leading sometimes to inaccurate transfer progress bar display
-   added additional info to file progress bar tooltip - individual file stats
-   changed program icon
-   bunch of usual small tweaks and fixes

## [1.6.0][1.6.0] - 2019-10-27
-   fixed Windows mount/unmount (requires rclone v1.50+)
-   Rclone Browser checks now for used rclone version (mount is disabled in Windows if rclone <v1.50)
-   added default download/upload folders - configurable in settings
-   add default download/upload extra options - configurable in settings
-   added available updates' notifications for both Rclone Browser and rclone - can be turned on/off in settings
-   all mount options are configurable via settings - generic "rclone mount remote local" is used without any options specified
-   default mount option (in settings) is "--vfs-cache-mode writes"
-   Google Drive with "shared with me" option on is always mounted as read-only
-   Windows deployment includes now all required runtime files for users without MSVCR installed
-   added ftp, MS Azureblob and Google Photos remote icons
-   modified main application window status bar to save space
-   released binary for Windows 32 bits
-   released binary for armhf 32 bits - for Raspberry Pi running raspbian
-   bunch of usual small tweaks and fixes

## [1.5.3][1.5.3] - 2019-10-24
-   Windows only update - include all required runtime dll files

## [1.5.2][1.5.2] - 2019-09-27
-   code cleanup - clean compilation with -Werror enabled, GCC8 compilation fixed
-   add tooltips showing rclone options used to all transfer window options
-   Google "drive shared with me" caused multiple of issues - now all should work
-   as always small cosmetic UI improvements - still plenty to do but core functionality was first

## [1.5.1][1.5.1] - 2019-09-25
-   after task edit initiated by double click main window does not get proper focus back and subsequent Run click might lead to wrong task execution. For time being I disable double click edit - until proper fix is produced.

## [1.5][1.5] - 2019-09-25
-   tasks - jobs can be saved/edited/run/deleted. No need creating the same job again and again.
-   on Google drive DriveSharedWithMe can be mounted to local filesystem
-   DriveSharedWithMe checkbox is now disabled for non Google destinations - it is Google only feature and turning it on for other destinations does not make sense - could even crash the browser.
-   verbose option is now always on and has been removed from UI - which means that stats will be always displayed. No more wondering how long it is going to take for some long job to finish.
-   fixed an issue with local remote on Windows when local drive content was not properly displayed
-   replaced remote Amazon icon with generic S3 one. S3 became name on its own and almost de-facto standard in cloud access used by many rclone supported destinations
-   new application logo

## [1.4.1][1.4.1] - 2019-09-18
-   small GUI tweaks to make all progress fields always visible (they were too small for large transfers) and adjust some screen sizes to make all GUI elements visible
-   update all builds with latest Qt (5.13.1)

## [1.4][1.4] - 2019-08-23
-   Fix compliation errors and update all builds with latest Qt (5.13)
-   Fix Config button command
-   Further fix and tweak progress display. Add ETA and Total Size fields
-   Fix remotes icons display
-   Add sftp icon
-   Fix progress display for rclone > 1.37 (by DinCahill)
-   Add a Public Link option to the right-click menu (by DinCahill)
-   Add preference: Show hidden files and folders (by DinCahill)
-   Add Mega icon (by DinCahill)
-   Refresh when Shared is toggled (by DinCahill)
-   Disable Upload button for Shared (by DinCahill)
-   Support for shared Google Drive files. Enable the checkbox when you open a remote, and all rclone commands will be passed --drive-shared-with-me (by DinCahill)
-   Set cache mode for mounts (by DinCahill)
-   Fixed missing leading / in path (required for some SFTP servers) (by DinCahill)

## [1.2][1.2] - 2017-03-11
-   Calculate size of folders, issue #4
-   Copy transfer command to clipboard, issue #20
-   Support custom .rclone.conf location, #21
-   Export list of files, issue #27
-   Bugfix for folder refresh not working after rename, issue #30
-   Remember empty text fields in transfer dialog, issue #32
-   Error message when too old rclone version is selected
-   Support portable mode, issue #28
-   Create .deb packages, issue #26

## [1.1][1.1] - 2017-01-31
-   Added `--transfer` option in UI, issue #1
-   Supports encrypted `.rclone.conf` configuration file, issue #2
-   Fixed crash when canceling active stream
-   Added ETA tooltip for transfer progress bars
-   Allow to specify extra arguments for rclone, issue #7
-   Fix for browsing Hubic remotes, issue #10
-   Support high-dpi mode for macOS

## [1.0.0][1.0.0] - 2017-01-29
-   Allows to browse and modify any rclone remote, including encrypted ones
-   Uses same configuration file as rclone, no extra configuration required
-   Simultaneously navigate multiple repositories in separate tabs
-   Lists files hierarchically with file name, size and modify date
-   All rclone commands are executed asynchronously, no freezing GUI
-   File hierarchy is lazily cached in memory, for faster traversal of folders
-   Allows to upload, download, create new folders, rename or delete files and folders
-   Can process multiple upload or download jobs in background
-   Drag & drop support for dragging files from local file explorer for uploading
-   Streaming media files for playback in player like mpv or similar
-   Mount and unmount folders on macOS and GNU/Linux
-   Optionally minimizes to tray, with notifications when upload/download finishes

[1.8.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.8.0
[1.7.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.7.0
[1.6.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.6.0
[1.5.3]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.3
[1.5.2]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.2
[1.5.1]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.1
[1.5]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5
[1.4.1]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.4.1
[1.4]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.4
[1.2]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.2
[1.1]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.1
[1.0.0]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.0.0
