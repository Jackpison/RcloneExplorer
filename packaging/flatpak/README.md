# Flatpak

`io.github.jackpison.RcloneExplorer.yml` builds Rclone Explorer as a Flatpak on
the KDE runtime, with rclone bundled (checksum-pinned, for x86_64 and aarch64).

## Build and try it locally

```
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --user --install --install-deps-from=flathub --force-clean build-flatpak packaging/flatpak/io.github.jackpison.RcloneExplorer.yml
flatpak run io.github.jackpison.RcloneExplorer
```

The **Flatpak** job of the Update workflow does the same on every push and
starts the app once (`--selftest`).

## In the sandbox

- **No mounting:** rclone mounts need FUSE, which the sandbox does not offer;
  the Mount button is hidden. Use the AppImage or the Windows version to mount.
- **Files:** the home folder is shared (`--filesystem=home`), because transfers
  and mirrors use folders typed or picked anywhere in it, and
  `~/.config/rclone` is shared so the remotes are the same as for an rclone
  installed on the system.
- **Updates:** Flathub updates the app and its rclone; the app's own update
  checks are switched off.

## Publishing on Flathub (first time)

1. Release the version as usual. The release workflow uploads the artifact
   **flathub-manifest-x.y.z**: the same manifest, building the release's git tag
   and commit instead of a local folder.
2. Fork <https://github.com/flathub/flathub>, create a branch from its
   `new-pr` branch, add that manifest file, and open a pull request against
   `new-pr` titled `Add io.github.jackpison.RcloneExplorer`.
3. Flathub's reviewers build it and comment, usually within days to a few
   weeks. Expect questions about `--filesystem=home` (explained above) and the
   prebuilt rclone binary (official, checksum-pinned release builds).
4. Once merged, Flathub creates `github.com/flathub/io.github.jackpison.RcloneExplorer`
   with you as maintainer. Each later release is a small pull request there
   with the new manifest; Flathub's external data checker proposes new rclone
   versions by itself (`x-checker-data`).
