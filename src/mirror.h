#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

// Mirrors keep one folder (or a whole remote) identical in other places:
// "Google Drive:Gallery" -> "pCloud:Backups/Gallery" and "Koofr:Photos".
//
// This file holds the model and everything that can be decided without a GUI
// or a running rclone: validation, the rclone command lines, reading rclone's
// change report, picking old trash snapshots and saving to JSON. It is unit
// tested (tests/test_mirror.cpp). The page, dialogs and runs are in
// mirror_page.cpp, mirror_dialog.cpp and mirror_tools.cpp.
namespace Mirror {

// Old versions of files a sync removes or replaces are moved here, on the
// destination remote: _SyncTrash/<stamp>/<destination folder>/...
QString trashDir();
QString stampFormat(); // "yyyy-MM-dd_HHmmss"

// A destination: a folder on a remote, or, when `remote` is empty, a folder
// on this computer (`path` is then absolute: "C:/Backups/Photos" or
// "/home/me/Backups", always with forward slashes).
struct Target {
  QString remote; // without the colon; empty = this computer
  QString path;   // folder on that remote, never empty
  bool local() const { return remote.isEmpty(); }
};

enum class DeleteMode {
  Trash,  // removed and replaced files are kept in _SyncTrash (default)
  Delete, // the destination matches exactly; removed files are deleted
  Keep    // only add and update, never remove (rclone copy)
};

struct Plan {
  QUuid id;
  QString sourceRemote; // without the colon; empty = a folder on this computer
  QString sourcePath;   // empty = the whole remote; absolute on this computer
  QList<Target> targets;
  QStringList exclude;  // rclone filter patterns, e.g. "*.tmp"
  DeleteMode mode = DeleteMode::Trash;
  bool exportGoogleDocs = false; // false: skip Docs/Sheets/Slides

  // "Gallery/Trips/Paris", the remote name for a whole remote, or the full
  // path of a folder on this computer
  QString title() const;
  bool localSource() const { return sourceRemote.isEmpty(); }
};

// "remote:path", or just the path for a folder on this computer
QString remotePath(const QString &remote, const QString &path);
QString targetPath(const Target &t);

// "C:\Backups\Photos\" -> "C:/Backups/Photos"; "" stays "".
QString cleanLocalPath(const QString &path);

// Where a sync keeps the old versions of files it removes or replaces:
// remote:_SyncTrash/<stamp>/<path> on a remote, or <parent>/_SyncTrash/<stamp>/<folder>
// next to a folder on this computer (a drive's top level is not ours to fill).
QString trashFor(const Target &t, const QString &stamp);
// The _SyncTrash folder that holds those snapshots.
QString trashRoot(const Target &t);

// "\a//b/ " -> "a/b"; ".." segments make it invalid (empty + *ok = false).
QString cleanPath(const QString &path, bool *ok = nullptr);

// Trims, drops empty lines, "# comments" and duplicates. Returns an error
// text for patterns rclone would refuse (or that are too long / too many).
QStringList cleanExcludes(const QStringList &in, QString *error);

// For a folder on this PC: the protected folder it is (home, Desktop,
// Documents, a system folder, or one that holds the home folder), or "" when it
// may be a destination. Folders inside a protected folder are fine.
// A full path on a disk of this PC (C:/..., //server/share, or /...).
bool isLocalPath(const QString &path);

QString protectedFolder(const QString &localPath);

// Checks `plan` (already cleaned) against the other plans. Destinations that
// overlap would delete each other's files, so they are refused. Returns an
// empty string when the plan is fine.
QString validate(const Plan &plan, const QList<Plan> &others);

struct RunOptions {
  bool dryRun = false;
  QString stamp;      // trash snapshot name, see stampFormat()
  QString reportFile; // rclone --combined report, optional
};
// Full argument list for one destination (without config options).
QStringList syncArgs(const Plan &plan, const Target &target, const RunOptions &o);
// Options that make "rclone size" count what the plan would copy.
QStringList filterArgs(const Plan &plan);

struct Changes {
  QStringList added, updated, removed;
  int total() const { return int(added.size() + updated.size() + removed.size()); }
};
// Reads an rclone --combined report: "+ new", "* changed", "- only on the
// destination" (removed, unless the mode keeps them).
Changes parseReport(const QByteArray &report, DeleteMode mode);

// Trash snapshot folder names older than `cutoff`. Anything that is not
// exactly a timestamp is never selected.
QStringList oldSnapshots(const QStringList &names, const QDateTime &cutoff);

QJsonArray toJson(const QList<Plan> &plans);
QList<Plan> fromJson(const QJsonArray &array);

} // namespace Mirror
