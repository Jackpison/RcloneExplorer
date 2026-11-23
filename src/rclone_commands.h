#pragma once

#include <QString>
#include <QStringList>

// Builders for the rclone command lines behind the Tools menu and the mount
// dialog. They only build argument lists (no GUI, no processes), so they are
// unit tested.
namespace RcloneCommands {

// Progress lines every job card understands.
QStringList statsArgs();

struct CheckOptions {
  bool oneWay = false;   // only report files missing/different on the destination
  bool sizeOnly = false; // do not compare checksums or modification times
};
QStringList check(const QString &source, const QString &dest,
                  const CheckOptions &o);

// Non-interactive dedupe modes only (the interactive modes need a terminal).
QStringList dedupeModes();
QStringList dedupe(const QString &path, const QString &mode);

QStringList cleanup(const QString &remote);

QStringList copy(const QString &source, const QString &dest, bool move);

struct BisyncOptions {
  bool resync = false;
  bool dryRun = false;
};
QStringList bisync(const QString &path1, const QString &path2,
                   const BisyncOptions &o);

struct MountOptions {
  QString cacheMode = "writes"; // off, minimal, writes, full
  bool readOnly = false;
  QString volumeName; // Windows / macOS only
  QString extra;      // free-form options typed in the dialog
};
QStringList cacheModes();
// Options placed before the remote and mount point. `globalOptions` is the
// mount field from Preferences; it wins over the dialog for any option it
// sets itself, so existing setups behave as before.
QStringList mountArgs(const MountOptions &o, const QString &globalOptions);

// A file name part as an rclone filter pattern that matches it literally
// (*, ?, [, ], {, } and \ have meanings there).
QString globEscape(const QString &text);

// The speed limit from Preferences, in rclone's --bwlimit form: "10M",
// "off", "10M:2M" (upload:download) or a timetable such as
// "08:00,2M 19:00,off" or "Mon-08:00,1M Sat-00:00,off". Empty means none.
bool validBandwidth(const QString &limit);
// `args` with --bwlimit added, unless the limit is empty or invalid, or the
// arguments already have their own (a saved task's limit wins).
QStringList withBandwidth(const QStringList &args, const QString &limit);

} // namespace RcloneCommands
