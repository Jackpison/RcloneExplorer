# Roadmap

Ideas for future updates. They are collected here and shipped together in one
release rather than one at a time. Tick an item off (or remove it) once it is
in a release, and mention it in `CHANGELOG.md`.

## Planned for 5.4.4

Fixes from a second code review (November 2026). None of these are new in
5.4.3; they were found by reading the code, not reported by users.

### Losing or mixing up work

- [ ] **Quitting with several jobs stops only every other one.** The others are killed without a clean stop, which on Linux can leave a broken mount behind. Collect the cards first, then stop each one (`MainWindow::canClose`).
- [ ] **Closing an old mirror card can break a mirror run in progress.** A card from an earlier run, when closed, marks a copy of the current run as failed and lets a second sync into the same folder start. Ignore cards of earlier runs (`MirrorPage::startPair`).
- [ ] **Saved tasks are not saved safely.** `tasks.bin` is emptied and rewritten in place; a crash or a full disk at that moment loses every task. Save it the way `mirrors.json` is saved since 5.4.3, and check the file when reading it.
- [ ] **A folder that fails to load shows as empty.** When rclone cannot list a folder (offline, expired login), its files disappear from view with no message. Keep what was shown and say what went wrong (`ItemModel::load`).
- [ ] **Changed sizes and dates may not show after a refresh** (the model reports the change for the wrong row).
- [ ] **Two copies of the AppImage can run at once** (the "only one at a time" lock uses a folder name that changes on every start), so schedules and mirrors run twice. On Linux the lock also belongs in a per-user folder.

### Updates

- [ ] **"Keep running" is ignored when an update restarts the app.** If a scheduled task started during the download, the app asks whether to stop it and then quits anyway.
- [ ] **Cancel does not cancel while an update is unpacking**: the update is installed and the app restarts. The same goes for rclone updates.
- [ ] **The window freezes while a portable update copies its files** (tens of seconds on a USB stick). Do the copying in the background.
- [ ] **The checksum is taken from the download, not from the saved file**, and write errors are ignored, so a full disk can leave a broken setup that still "passes". Check the saved file.
- [ ] **The progress dialog can hang on "Preparing…"** if an update check is still running when you click Update now; and a quick failure can show two error boxes.
- [ ] **Old files stay behind after a portable update**: files a new version no longer has (DLLs, Qt plugins) are never removed.
- [ ] **Release workflow: refuse a tag that does not match `VERSION`.** Otherwise every copy is told about the update and none can install it. Also update the release notes text: the setup now asks "for me" or "for all users".

### Security and smoothness

- [ ] **Portable mode keeps a working copy of the settings in the shared temp folder.** On a Linux PC with several users, another user could plant settings there (including the rclone program to run). Keep it in a private folder.
- [ ] **Waits that still block the window**: looking up remote names (up to 4 s), ending the sleep lock (up to 2 s), cancelling a transfer or stream.
- [ ] **The sleep lock can stay on in the Flatpak** if transfers stop and start again within a split second.
- [ ] **Some labels can show remote file names as formatted text** (mount and stream cards, tooltips, trash dialog errors). Make them plain text.

### Small things

- [ ] **Deleting a saved task asks nothing** (every other delete asks first).
- [ ] **After deleting a file, the parent folder is selected** instead of the next file.

## Shipped in 5.4.3

"portable" in the zip's name (also published under the old name for now);
winget installs the setup; the updater uses full paths; sounds and staying
awake in the Flatpak; installer choice between "for me" and "for all users";
plus fixes from a full code review (Scoop kept no mirrors, freezes when
unmounting and checking a new rclone, unquoted Copy command, safe mirror saving).

## Shipped in 5.4.2

Rclone Explorer updates itself; Windows installer next to the portable zip;
Flatpak ready for Flathub; desktop portal on Linux (native dialogs, system
light/dark, Wayland app id); Mirror asks before writing into another mirror's
source; This PC screenshots; animated light/dark switch (inspired by Magic UI's
[Animated Theme Toggler](https://magicui.design/docs/components/animated-theme-toggler)),
which also hides the restyling pause; shadowed-name warnings tidied.

## Windows

- [ ] **Code signing with SignPath, once the app has grown.** SignPath Foundation offers free signing for open-source projects but turned down the first request (November 2026); apply again when the project has more users. Until then releases are unsigned and users check downloads against `SHA256SUMS.txt`.

## Linux desktop

- [ ] **Submit the Flatpak to Flathub** (an owner's step, see `packaging/flatpak/README.md`): take the `flathub-manifest-x.y.z` artifact of a release and open the pull request to `flathub/flathub`. Reviewers may ask about the home folder access and the bundled rclone binary.

## Later

- [ ] **Stop publishing the zip under its old name** (`RcloneExplorer-<version>-windows-x64.zip`) once few people still run 5.4.2: remove the `Copy-Item` line after `Compress-Archive` in the Update workflow and the release-notes mention. The app's fallback to the old name can stay.
