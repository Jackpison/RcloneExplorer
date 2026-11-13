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

struct Target {
  QString remote; // without the colon
  QString path;   // folder on that remote, never empty
};

enum class DeleteMode {
  Trash,  // removed and replaced files are kept in _SyncTrash (default)
  Delete, // the destination matches exactly; removed files are deleted
  Keep    // only add and update, never remove (rclone copy)
};

struct Plan {
  QUuid id;
  QString sourceRemote; // without the colon
  QString sourcePath;   // empty = the whole remote
  QList<Target> targets;
  QStringList exclude;  // rclone filter patterns, e.g. "*.tmp"
  DeleteMode mode = DeleteMode::Trash;
  bool exportGoogleDocs = false; // false: skip Docs/Sheets/Slides

  // "Gallery/Trips/Paris", or the remote name for a whole remote
  QString title() const;
};

QString remotePath(const QString &remote, const QString &path);

// "\a//b/ " -> "a/b"; ".." segments make it invalid (empty + *ok = false).
QString cleanPath(const QString &path, bool *ok = nullptr);

// Trims, drops empty lines, "# comments" and duplicates. Returns an error
// text for patterns rclone would refuse (or that are too long / too many).
QStringList cleanExcludes(const QStringList &in, QString *error);

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
