#include "mirror.h"
#include "rclone_commands.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace Mirror {

QString trashDir() { return QStringLiteral("_SyncTrash"); }
QString stampFormat() { return QStringLiteral("yyyy-MM-dd_HHmmss"); }

QString Plan::title() const {
  if (localSource()) {
    return QDir::toNativeSeparators(sourcePath);
  }
  return sourcePath.isEmpty() ? sourceRemote : sourcePath;
}

QString remotePath(const QString &remote, const QString &path) {
  return remote.isEmpty() ? path : remote + ":" + path;
}

QString targetPath(const Target &t) { return remotePath(t.remote, t.path); }

QString cleanLocalPath(const QString &path) {
  const QString p = QDir::fromNativeSeparators(path.trimmed());
  return p.isEmpty() ? QString() : QDir::cleanPath(p);
}

QString trashRoot(const Target &t) {
  if (!t.local()) {
    return remotePath(t.remote, trashDir());
  }
  return QDir(QFileInfo(t.path).path()).filePath(trashDir());
}

QString trashFor(const Target &t, const QString &stamp) {
  if (!t.local()) {
    return remotePath(t.remote, trashDir() + "/" + stamp + "/" + t.path);
  }
  return QDir(trashRoot(t)).filePath(stamp + "/" + QFileInfo(t.path).fileName());
}

QString cleanPath(const QString &path, bool *ok) {
  QStringList parts;
  const QStringList raw = QString(path).replace('\\', '/').split('/');
  for (const QString &segment : raw) {
    const QString s = segment.trimmed();
    if (s.isEmpty() || s == ".") {
      continue;
    }
    if (s == "..") {
      if (ok) {
        *ok = false;
      }
      return QString();
    }
    parts << s;
  }
  if (ok) {
    *ok = true;
  }
  return parts.join('/');
}

QStringList cleanExcludes(const QStringList &in, QString *error) {
  QStringList out;
  QSet<QString> seen;
  for (const QString &raw : in) {
    const QString e = raw.trimmed();
    if (e.isEmpty() || e.startsWith('#') || seen.contains(e)) {
      continue;
    }
    if (e.size() > 200) {
      if (error) {
        *error = QObject::tr("The pattern \"%1…\" is too long.").arg(e.left(30));
      }
      return {};
    }
    if (e.count('[') != e.count(']') || e.count('{') != e.count('}')) {
      if (error) {
        *error = QObject::tr("The pattern \"%1\" has an unmatched bracket.").arg(e);
      }
      return {};
    }
    seen.insert(e);
    out << e;
  }
  if (out.size() > 50) {
    if (error) {
      *error = QObject::tr("Use at most 50 patterns.");
    }
    return {};
  }
  if (error) {
    error->clear();
  }
  return out;
}

namespace {

QString firstSegment(const QString &path) { return path.section('/', 0, 0); }

// Same folder, or one inside the other. Remote names and paths are compared
// without case: several cloud services treat names case-insensitively.
bool overlaps(const QString &a, const QString &b) {
  const QString x = a.toLower(), y = b.toLower();
  if (x.isEmpty() || y.isEmpty()) {
    return true; // the whole remote contains everything
  }
  return x == y || x.startsWith(y + "/") || y.startsWith(x + "/");
}

// The same folder as rclone sees it: symbolic links and junctions resolved,
// when the folder already exists.
// A folder that does not exist yet is resolved through its nearest existing
// parent, so "link/new" is seen inside the folder "link" points to.
QString resolved(const QString &localPath) {
  QString path = localPath, rest;
  while (!path.isEmpty()) {
    const QString real = QFileInfo(path).canonicalFilePath();
    if (!real.isEmpty()) {
      return rest.isEmpty() ? real : QDir::cleanPath(real + "/" + rest);
    }
    const QFileInfo fi(path);
    const QString parent = fi.path();
    if (parent == path || fi.fileName().isEmpty()) {
      break;
    }
    rest = rest.isEmpty() ? fi.fileName() : fi.fileName() + "/" + rest;
    path = parent;
  }
  return localPath;
}

bool localOverlaps(const QString &a, const QString &b) {
  return overlaps(a, b) || overlaps(resolved(a), resolved(b));
}

bool sameFolder(const QString &a, const QString &b) {
  return a.compare(b, Qt::CaseInsensitive) == 0;
}

} // namespace

QString protectedFolder(const QString &localPath) {
  if (localPath.isEmpty()) {
    return QString();
  }
  QStringList guarded; // never a destination themselves; folders inside are fine
  for (auto l : {QStandardPaths::HomeLocation, QStandardPaths::DesktopLocation,
                 QStandardPaths::DocumentsLocation, QStandardPaths::DownloadLocation,
                 QStandardPaths::PicturesLocation, QStandardPaths::MusicLocation,
                 QStandardPaths::MoviesLocation}) {
    guarded << QStandardPaths::standardLocations(l);
  }
#ifdef Q_OS_WIN
  for (const char *env : {"SystemRoot", "ProgramFiles", "ProgramFiles(x86)", "ProgramData",
                          "APPDATA", "LOCALAPPDATA", "OneDrive", "PUBLIC"}) {
    guarded << qEnvironmentVariable(env);
  }
#else
  for (const char *dir : {"/bin", "/boot", "/dev", "/etc", "/home", "/lib", "/lib32", "/lib64",
                          "/mnt", "/media", "/opt", "/proc", "/root", "/run", "/sbin", "/srv",
                          "/sys", "/tmp", "/usr", "/var", "/Applications", "/Library",
                          "/System", "/Users", "/Volumes", "/private"}) {
    guarded << dir;
  }
#endif
  const QString home = cleanLocalPath(QDir::homePath());
  for (const QString &path : {localPath, resolved(localPath)}) {
    if (QDir(path).isRoot()) {
      return QDir::toNativeSeparators(path); // a whole drive
    }
    for (const QString &g : std::as_const(guarded)) {
      const QString c = cleanLocalPath(g);
      if (!c.isEmpty() && (sameFolder(path, c) || sameFolder(path, resolved(c)))) {
        return QDir::toNativeSeparators(c);
      }
    }
    // a folder that holds the home folder, such as C:\Users
    if (!home.isEmpty() && home.toLower().startsWith(path.toLower() + "/")) {
      return QDir::toNativeSeparators(path);
    }
  }
  return QString();
}

QString validate(const Plan &plan, const QList<Plan> &others) {
  if (plan.localSource()) {
    if (plan.sourcePath.isEmpty() || !QDir::isAbsolutePath(plan.sourcePath)) {
      return QObject::tr("Choose the folder to mirror.");
    }
    if (plan.sourcePath.split('/').contains(trashDir(), Qt::CaseInsensitive)) {
      return QObject::tr("The %1 folder cannot be mirrored.").arg(trashDir());
    }
  } else if (plan.sourceRemote.startsWith('-')) {
    return QObject::tr("\"%1\" is not a valid remote name.").arg(plan.sourceRemote);
  } else if (firstSegment(plan.sourcePath).compare(trashDir(), Qt::CaseInsensitive) == 0) {
    return QObject::tr("The %1 folder cannot be mirrored.").arg(trashDir());
  }
  if (plan.targets.isEmpty()) {
    return QObject::tr("Switch on at least one destination.");
  }
  QList<Target> seen;
  for (const Target &t : plan.targets) {
    if (t.local()) {
      if (t.path.isEmpty()) {
        return QObject::tr("Choose the folder on this PC.");
      }
      if (!QDir::isAbsolutePath(t.path)) {
        return QObject::tr("Choose the full path of the folder on this PC, for example "
                           "C:\\Backups\\Photos.");
      }
      if (QDir(t.path).isRoot() || QFileInfo(t.path).fileName().isEmpty()) {
        return QObject::tr("Choose a folder on this PC, not a whole drive.");
      }
      if (t.path.split('/').contains(trashDir(), Qt::CaseInsensitive)) {
        return QObject::tr("The %1 folder cannot be a destination.").arg(trashDir());
      }
      const QString guarded = protectedFolder(t.path);
      if (!guarded.isEmpty()) {
        return QObject::tr("%1 cannot be a destination: a mirror removes everything in it that "
                           "is not in the mirrored folder. Choose or create a folder inside it, "
                           "for example %2.")
            .arg(guarded, QDir::toNativeSeparators(
                              QDir(t.path).filePath(QObject::tr("Mirror"))));
      }
    } else if (t.remote.startsWith('-')) {
      return QObject::tr("\"%1\" is not a valid remote name.").arg(t.remote);
    } else if (t.path.isEmpty()) {
      return QObject::tr("Choose a destination folder on %1.").arg(t.remote);
    } else if (firstSegment(t.path).compare(trashDir(), Qt::CaseInsensitive) == 0) {
      return QObject::tr("The %1 folder cannot be a destination.").arg(trashDir());
    }
    const auto clash = [&t](const QString &remote, const QString &path) {
      if (remote.compare(t.remote, Qt::CaseInsensitive) != 0) {
        return false;
      }
      return t.local() ? localOverlaps(path, t.path) : overlaps(path, t.path);
    };
    if (clash(plan.sourceRemote, plan.sourcePath)) {
      return QObject::tr("On %1, the destination \"%2\" overlaps the folder being mirrored.")
          .arg(t.local() ? QObject::tr("this PC") : t.remote, QDir::toNativeSeparators(t.path));
    }
    for (const Target &o : seen) {
      if (clash(o.remote, o.path)) {
        return QObject::tr("On %1, \"%2\" and \"%3\" overlap. Use separate folders.")
            .arg(t.local() ? QObject::tr("this PC") : t.remote, o.path, t.path);
      }
    }
    for (const Plan &other : others) {
      if (other.id == plan.id) {
        continue;
      }
      for (const Target &o : other.targets) {
        if (clash(o.remote, o.path)) {
          return QObject::tr("On %1, \"%2\" overlaps the mirror of %3, which uses \"%4\". "
                             "One would delete the other's files, so use separate folders.")
              .arg(t.local() ? QObject::tr("this PC") : t.remote, t.path, other.title(), o.path);
        }
      }
    }
    seen << t;
  }
  return QString();
}

QStringList filterArgs(const Plan &plan) {
  QStringList l;
  if (!plan.exportGoogleDocs) {
    l << "--drive-skip-gdocs"; // only affects Google Drive remotes
  }
  // never copy the trash of an earlier mirror when a whole remote is mirrored
  l << "--exclude=/" + trashDir() + "/**";
  for (const QString &e : plan.exclude) {
    l << "--exclude=" + e;
  }
  return l;
}

QStringList syncArgs(const Plan &plan, const Target &target, const RunOptions &o) {
  QStringList l{plan.mode == DeleteMode::Keep ? QStringLiteral("copy")
                                              : QStringLiteral("sync")};
  l << remotePath(plan.sourceRemote, plan.sourcePath) << targetPath(target);
  l << "--create-empty-src-dirs";
  if (plan.mode == DeleteMode::Trash) {
    l << "--backup-dir" << trashFor(target, o.stamp);
  }
  l << filterArgs(plan);
  if (!o.reportFile.isEmpty()) {
    l << "--combined" << o.reportFile;
  }
  if (o.dryRun) {
    l << "--dry-run";
  }
  l << "--verbose" << RcloneCommands::statsArgs();
  return l;
}

Changes parseReport(const QByteArray &report, DeleteMode mode) {
  Changes c;
  const QList<QByteArray> lines = report.split('\n');
  for (QByteArray line : lines) {
    if (line.endsWith('\r')) {
      line.chop(1);
    }
    if (line.size() < 3 || line.at(1) != ' ') {
      continue;
    }
    const QString path = QString::fromUtf8(line.mid(2));
    switch (line.at(0)) {
    case '+':
      c.added << path;
      break;
    case '*':
      c.updated << path;
      break;
    case '-':
      if (mode != DeleteMode::Keep) {
        c.removed << path;
      }
      break;
    default:
      break;
    }
  }
  return c;
}

QStringList oldSnapshots(const QStringList &names, const QDateTime &cutoff) {
  static const QRegularExpression rx(R"(^\d{4}-\d{2}-\d{2}_\d{6}$)");
  QStringList out;
  for (const QString &n : names) {
    if (!rx.match(n).hasMatch()) {
      continue;
    }
    const QDateTime t = QDateTime::fromString(n, stampFormat());
    if (t.isValid() && t < cutoff) {
      out << n;
    }
  }
  out.sort();
  return out;
}

namespace {
QString modeName(DeleteMode m) {
  return m == DeleteMode::Delete ? "delete" : m == DeleteMode::Keep ? "keep" : "trash";
}
DeleteMode modeFrom(const QString &s) {
  return s == "delete" ? DeleteMode::Delete : s == "keep" ? DeleteMode::Keep : DeleteMode::Trash;
}
} // namespace

QJsonArray toJson(const QList<Plan> &plans) {
  QJsonArray a;
  for (const Plan &p : plans) {
    QJsonArray targets;
    for (const Target &t : p.targets) {
      targets.append(QJsonObject{{"remote", t.remote}, {"path", t.path}});
    }
    a.append(QJsonObject{{"id", p.id.toString(QUuid::WithoutBraces)},
                         {"sourceRemote", p.sourceRemote},
                         {"sourcePath", p.sourcePath},
                         {"targets", targets},
                         {"exclude", QJsonArray::fromStringList(p.exclude)},
                         {"mode", modeName(p.mode)},
                         {"exportGoogleDocs", p.exportGoogleDocs}});
  }
  return a;
}

QList<Plan> fromJson(const QJsonArray &array) {
  QList<Plan> plans;
  for (const QJsonValue &v : array) {
    const QJsonObject o = v.toObject();
    Plan p;
    p.id = QUuid(o.value("id").toString());
    if (p.id.isNull()) {
      p.id = QUuid::createUuid();
    }
    p.sourceRemote = o.value("sourceRemote").toString();
    const QString sp = o.value("sourcePath").toString();
    p.sourcePath = p.sourceRemote.isEmpty() ? cleanLocalPath(sp) : cleanPath(sp);
    for (const QJsonValue &tv : o.value("targets").toArray()) {
      const QJsonObject t = tv.toObject();
      const QString remote = t.value("remote").toString();
      const QString path = t.value("path").toString();
      p.targets << Target{remote, remote.isEmpty() ? cleanLocalPath(path) : cleanPath(path)};
    }
    for (const QJsonValue &e : o.value("exclude").toArray()) {
      p.exclude << e.toString();
    }
    p.mode = modeFrom(o.value("mode").toString());
    p.exportGoogleDocs = o.value("exportGoogleDocs").toBool();
    if (!p.sourceRemote.isEmpty() || !p.sourcePath.isEmpty()) {
      plans << p;
    }
  }
  return plans;
}

} // namespace Mirror
