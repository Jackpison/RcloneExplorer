# Roadmap

Ideas for future updates. They are collected here and shipped together in one
release rather than one at a time. Tick an item off (or remove it) once it is
in a release, and mention it in `CHANGELOG.md`.

## Next release: 5.4.5

Agreed on 2026-11-14. Items are described in the sections below.

1. Update Rclone Explorer from inside the app (Windows)
2. Mirror: warn when a destination is another mirror's source
3. Linux: desktop portal (native dialogs, system light/dark) and the Wayland window icon
4. New Mirror screenshots with This PC (README and website)
5. Tidy the shadowed-name compiler warnings
6. Faster light/dark switch, if a clear improvement is possible
7. Windows installer (`.exe` setup) next to the portable zip
8. Flatpak ready for Flathub (manifest, store listing, CI build); submitting is a separate step, see below. Waiting for a yes

## Windows

- [ ] **Update Rclone Explorer from inside the app**, the way rclone is already updated: check GitHub for a newer release, download the Windows zip, verify it against the release's `SHA256SUMS.txt`, replace the program files and restart. Your settings (`Rclone Explorer.ini`), `tasks.bin`, `mirrors.json`, `mirror_history.json`, `rclone\` and your own logos in `icons\remotes\` are kept. Today a manual update means renaming the old folder, extracting the new zip and copying those files back. The check is a new network call, so add it to the README privacy statement first, with a setting to turn it off.
- [ ] **Installer (`.exe` setup) next to the portable zip.** Built with Inno Setup in the release workflow. Installs per user into `%LOCALAPPDATA%\Programs` (no administrator rights needed), adds a Start menu shortcut and an uninstaller, and runs in normal (not portable) mode, so settings live in `%APPDATA%` and an update never touches them. Also the better fit for winget.
- [ ] **Code signing with SignPath, once the app has grown.** SignPath Foundation offers free signing for open-source projects but turned down the first request (November 2026); apply again when the project has more users. Until then releases are unsigned and users check downloads against `SHA256SUMS.txt`.

## Linux desktop

- [ ] **Wayland and desktop portal.** Use the desktop portal for file dialogs and the light/dark setting (GNOME or KDE dialogs, system theme). Set the desktop file name so the window shows its proper icon and launcher under Wayland. About 20 lines in `main.cpp`. A first attempt exists on the old branch `claude/confident-sagan-ofhan9` (from the Rclone Browser days); write it fresh with Rclone Explorer names instead of reviving that branch.
- [ ] **Flatpak, published on Flathub** (a loose `.flatpak` file gets no updates, so it adds nothing over the AppImage).
  - In this repo: manifest under `packaging/flatpak/` (KDE runtime with Qt 6, rclone bundled with its checksum), AppStream store listing with screenshots, desktop file and icon, and a CI job that builds and test-runs it. App id `io.github.jackpison.RcloneExplorer`.
  - Sandbox: mounting needs FUSE and is hidden in the Flatpak (use the AppImage for that); home folder access for folders on this PC; the app and rclone updaters are off because Flathub delivers updates.
  - Submitting: a pull request to `flathub/flathub` from the owner's GitHub account, then Flathub's review (days to weeks, often with requested changes). After approval Flathub creates the app's own repository, where each release is a small update.

## Mirror

- [ ] Warn when a mirror's destination is another mirror's source. Today only destinations are compared with each other, so mirror B can write into mirror A's source folder.
- [ ] New screenshots for the README and website showing **This PC** as a source and as a destination.

## Smoothness

- [ ] Switching light/dark takes about 0.14 s (the whole app is restyled). Could be shortened by styling fewer widgets through the app-wide style sheet.

## Code

- [ ] Tidy the compiler warnings about shadowed names (`-Wshadow`), mostly repeated `settings` variables in `remote_widget.cpp`.

## Housekeeping

- [ ] Delete old branches on GitHub (Code, Branches): everything except `main`. All their work is in `main` or listed above.
