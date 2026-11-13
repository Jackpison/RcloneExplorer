# Roadmap

Ideas for future updates. They are collected here and shipped together in one
release rather than one at a time. Tick an item off (or remove it) once it is
in a release, and mention it in `CHANGELOG.md`.

## Linux desktop

- [ ] **Wayland and desktop portal.** Use the desktop portal for file dialogs and the light/dark setting (GNOME or KDE dialogs, system theme). Set the desktop file name so the window shows its proper icon and launcher under Wayland. About 20 lines in `main.cpp`. A first attempt exists on the old branch `claude/confident-sagan-ofhan9` (from the Rclone Browser days); write it fresh with Rclone Explorer names instead of reviving that branch.
- [ ] **Flatpak.** One manifest under `packaging/flatpak/` with an `io.github.jackpison.RcloneExplorer` id, and a CI job that builds it. Decide first whether to publish on Flathub.

## Mirror

- [ ] Warn when a mirror's destination is another mirror's source. Today only destinations are compared with each other, so mirror B can write into mirror A's source folder.
- [ ] New screenshots for the README and website showing **This PC** as a source and as a destination.

## Smoothness

- [ ] Switching light/dark takes about 0.14 s (the whole app is restyled). Could be shortened by styling fewer widgets through the app-wide style sheet.

## Code

- [ ] Tidy the compiler warnings about shadowed names (`-Wshadow`), mostly repeated `settings` variables in `remote_widget.cpp`.

## Housekeeping

- [ ] Delete old branches on GitHub (Code, Branches): everything except `main`. All their work is in `main` or listed above.
