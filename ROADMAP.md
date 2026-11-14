# Roadmap

Ideas for future updates. They are collected here and shipped together in one
release rather than one at a time. Tick an item off (or remove it) once it is
in a release, and mention it in `CHANGELOG.md`.

## Shipped in 5.4.2

Rclone Explorer updates itself; Windows installer next to the portable zip;
Flatpak ready for Flathub; desktop portal on Linux (native dialogs, system
light/dark, Wayland app id); Mirror asks before writing into another mirror's
source; This PC screenshots; animated light/dark switch (inspired by Magic UI's
[Animated Theme Toggler](https://magicui.design/docs/components/animated-theme-toggler)),
which also hides the restyling pause; shadowed-name warnings tidied.

## Windows

- [ ] **Code signing with SignPath, once the app has grown.** SignPath Foundation offers free signing for open-source projects but turned down the first request (November 2026); apply again when the project has more users. Until then releases are unsigned and users check downloads against `SHA256SUMS.txt`.
- [ ] **winget with the installer.** The winget manifests still use the portable zip; the setup (`InstallerType: inno`, per-user scope) is the better fit now that it exists.

## Linux desktop

- [ ] **Submit the Flatpak to Flathub** (an owner's step, see `packaging/flatpak/README.md`): take the `flathub-manifest-x.y.z` artifact of a release and open the pull request to `flathub/flathub`. Reviewers may ask about the home folder access and the bundled rclone binary.
- [ ] **Sounds and staying awake in the Flatpak.** Both use helper programs (`paplay`, `systemd-inhibit`) that the sandbox does not have; the Flatpak quietly goes without them. They could use the portal and Qt Multimedia instead.

## Code

- [ ] **Updater: full paths inside `AppUpdate::apply()`.** It should turn both folders into absolute paths itself instead of relying on its caller. The app always passes absolute paths, so 5.4.2 is not affected, but with relative paths it refuses the update (safely, without touching anything), as a test outside the app showed. Add a test with relative paths.

## Housekeeping

- [ ] Delete old branches on GitHub (Code, Branches): everything except `main`. All their work is in `main` or listed above.
