# Security

## Reporting a problem

Please report security problems **privately**, not in a public issue:

1. Open the repository's **Security** tab on GitHub.
2. Click **Report a vulnerability**.
3. Describe what you found and how to reproduce it.

Only the maintainer sees the report. You will get an answer, and the fix is
published in a new release with a note in the changelog.

## Supported versions

Only the latest release receives fixes. Rclone Explorer updates itself
(**Help & Update**), so please check that you run the latest version first.

## What Rclone Explorer does to keep you safe

- Passwords never go on a command line or into a log: the rclone config
  password and the mount's remote-control password go through the
  environment.
- Every download (app updates and rclone) is checked against a published
  SHA-256 checksum before it is used, and only from GitHub or rclone.org.
- Settings, tasks and mirrors are saved so that a crash or a full disk
  cannot destroy them.
- The only network connections the app itself makes are listed in the
  README's *Privacy* section. Everything else is rclone talking to your
  remotes.

Rclone itself is a separate project: please report problems in rclone to
[rclone's security contact](https://github.com/rclone/rclone/security).
