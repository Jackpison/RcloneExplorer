#include "mirror.h"
#include "rclone_commands.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QRegularExpression>
#include <QSet>

namespace Mirror {

QString trashDir() { return QStringLiteral("_SyncTrash"); }
QString stampFormat() { return QStringLiteral("yyyy-MM-dd_HHmmss"); }

QString Plan::title() const {
  return sourcePath.isEmpty() ? sourceRemote : sourcePath;
}

QString remotePath(const QString &remote, const QString &path) {
  return remote + ":" + path;
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

} // namespace

QString validate(const Plan &plan, const QList<Plan> &others) {
  if (plan.sourceRemote.isEmpty()) {
    return QObject::tr("Choose the folder to mirror.");
  }
  if (firstSegment(plan.sourcePath).compare(trashDir(), Qt::CaseInsensitive) == 0) {
    return QObject::tr("The %1 folder cannot be mirrored.").arg(trashDir());
  }
  if (plan.targets.isEmpty()) {
    return QObject::tr("Switch on at least one destination.");
  }
  QList<Target> seen;
  for (const Target &t : plan.targets) {
    if (t.path.isEmpty()) {
      return QObject::tr("Choose a destination folder on %1.").arg(t.remote);
    }
    if (firstSegment(t.path).compare(trashDir(), Qt::CaseInsensitive) == 0) {
      return QObject::tr("The %1 folder cannot be a destination.").arg(trashDir());
    }
    if (t.remote.compare(plan.sourceRemote, Qt::CaseInsensitive) == 0 &&
        overlaps(t.path, plan.sourcePath)) {
      return QObject::tr("On %1, the destination \"%2\" overlaps the folder being mirrored.")
          .arg(t.remote, t.path);
    }
    for (const Target &o : seen) {
      if (o.remote.compare(t.remote, Qt::CaseInsensitive) == 0 && overlaps(o.path, t.path)) {
        return QObject::tr("On %1, \"%2\" and \"%3\" overlap. Use separate folders.")
            .arg(t.remote, o.path, t.path);
      }
    }
    for (const Plan &other : others) {
      if (other.id == plan.id) {
        continue;
      }
      for (const Target &o : other.targets) {
        if (o.remote.compare(t.remote, Qt::CaseInsensitive) == 0 && overlaps(o.path, t.path)) {
          return QObject::tr("On %1, \"%2\" overlaps the mirror of %3, which uses \"%4\". "
                             "One would delete the other's files, so use separate folders.")
              .arg(t.remote, t.path, other.title(), o.path);
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
  l << remotePath(plan.sourceRemote, plan.sourcePath)
    << remotePath(target.remote, target.path);
  l << "--create-empty-src-dirs";
  if (plan.mode == DeleteMode::Trash) {
    l << "--backup-dir"
      << remotePath(target.remote, trashDir() + "/" + o.stamp + "/" + target.path);
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
    p.sourcePath = cleanPath(o.value("sourcePath").toString());
    for (const QJsonValue &tv : o.value("targets").toArray()) {
      const QJsonObject t = tv.toObject();
      p.targets << Target{t.value("remote").toString(), cleanPath(t.value("path").toString())};
    }
    for (const QJsonValue &e : o.value("exclude").toArray()) {
      p.exclude << e.toString();
    }
    p.mode = modeFrom(o.value("mode").toString());
    p.exportGoogleDocs = o.value("exportGoogleDocs").toBool();
    if (!p.sourceRemote.isEmpty()) {
      plans << p;
    }
  }
  return plans;
}

} // namespace Mirror
