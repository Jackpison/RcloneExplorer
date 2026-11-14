# Roadmap

Ideas for future updates. They are collected here and shipped together in one
release rather than one at a time. Tick an item off (or remove it) once it is
in a release, and mention it in `CHANGELOG.md`.

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

## Housekeeping

- [ ] Delete old branches on GitHub (Code, Branches): everything except `main`. All their work is in `main` or listed above.
