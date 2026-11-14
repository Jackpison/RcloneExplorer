#include "app_update.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace AppUpdate {

namespace {
const char *kRepoDownloads = "https://github.com/Jackpison/RcloneExplorer/releases/download/";
const char *kLeftovers = "update-leftovers.txt";
} // namespace

const Asset *Release::asset(const QString &name) const {
  for (const Asset &a : assets) {
    if (a.name == name) {
      return &a;
    }
  }
  return nullptr;
}

bool isScoopFolder(const QString &appDir) {
  // Scoop writes these two files into every version folder of an app, also
  // when Scoop lives somewhere other than ...\scoop
  const QDir dir(appDir);
  if (!appDir.isEmpty() && dir.exists("install.json") && dir.exists("manifest.json")) {
    return true;
  }
  QString path = appDir;
  path.replace('\\', '/'); // either kind of separator, on any system
  return path.contains("/scoop/apps/", Qt::CaseInsensitive);
}

Kind kind(bool portableMode, const QString &appDir) {
  if (!qEnvironmentVariableIsEmpty("FLATPAK_ID")) {
    return Kind::Flatpak;
  }
  if (!qEnvironmentVariableIsEmpty("APPIMAGE")) {
    return Kind::AppImage;
  }
#ifdef Q_OS_WIN
  // the portable zip ships "Rclone Explorer.ini" next to the exe
  if (isScoopFolder(appDir)) {
    return Kind::Scoop; // Scoop owns the folder and keeps its own record of the version
  }
  return portableMode ? Kind::PortableZip : Kind::Installer;
#else
  Q_UNUSED(portableMode);
  Q_UNUSED(appDir);
  return Kind::Other;
#endif
}

bool trustedAssetUrl(const QUrl &url, const QString &version) {
  const QString prefix = QString::fromLatin1(kRepoDownloads) + "v" + version + "/";
  const QString s = url.toString(QUrl::FullyEncoded);
  return url.scheme() == QLatin1String("https") && s.startsWith(prefix) &&
         !s.mid(prefix.size()).contains('/') && s.size() > prefix.size();
}

bool parseRelease(const QByteArray &json, Release *out) {
  const QJsonObject o = QJsonDocument::fromJson(json).object();
  static const QRegularExpression rx(QStringLiteral(R"(^v?(\d{1,4}\.\d{1,4}\.\d{1,4})$)"));
  const auto m = rx.match(o.value("tag_name").toString());
  if (!m.hasMatch() || o.value("draft").toBool() || o.value("prerelease").toBool()) {
    return false;
  }
  Release r;
  r.version = m.captured(1);
  const QUrl page(o.value("html_url").toString());
  if (page.scheme() == QLatin1String("https") && page.host() == QLatin1String("github.com")) {
    r.page = page;
  }
  for (const QJsonValue &v : o.value("assets").toArray()) {
    const QJsonObject a = v.toObject();
    Asset asset{a.value("name").toString(), QUrl(a.value("browser_download_url").toString()),
                a.value("size").toInteger()};
    if (!asset.name.isEmpty() && trustedAssetUrl(asset.url, r.version) &&
        asset.url.fileName() == asset.name) {
      r.assets << asset;
    }
  }
  *out = r;
  return true;
}

int compareVersions(const QString &a, const QString &b) {
  const QStringList x = a.split('.'), y = b.split('.');
  for (int i = 0; i < qMax(x.size(), y.size()); ++i) {
    const int p = x.value(i).toInt(), q = y.value(i).toInt();
    if (p != q) {
      return p < q ? -1 : 1;
    }
  }
  return 0;
}

QString zipName(const QString &version) {
  return QString("RcloneExplorer-%1-windows-x64-portable.zip").arg(version);
}

QString legacyZipName(const QString &version) {
  return QString("RcloneExplorer-%1-windows-x64.zip").arg(version);
}

QString setupName(const QString &version) {
  return QString("RcloneExplorer-%1-windows-x64-setup.exe").arg(version);
}

QString checksumFor(const QByteArray &sums, const QString &name) {
  for (const QByteArray &raw : sums.split('\n')) {
    const QString line = QString::fromUtf8(raw).trimmed();
    static const QRegularExpression rx(QStringLiteral(R"(^([0-9a-fA-F]{64})\s+\*?(.+)$)"));
    const auto m = rx.match(line);
    if (m.hasMatch() && m.captured(2) == name) {
      return m.captured(1).toLower();
    }
  }
  return QString();
}

Rule ruleFor(const QString &relativePath) {
  const QString p = relativePath.toLower();
  static const QStringList userFiles{"rclone explorer.ini", "rclonebrowser.ini",
                                     "rcloneexplorer.ini", "tasks.bin", "mirrors.json",
                                     "mirror_history.json", QString::fromLatin1(kLeftovers)};
  if (userFiles.contains(p) || p.startsWith("rclone/")) {
    return Rule::Skip; // the user's settings, and the rclone the app keeps up to date
  }
  if (p.startsWith("icons/remotes/")) {
    return Rule::KeepExisting;
  }
  return Rule::Replace;
}

QString leftoversFile(const QString &appDir) { return QDir(appDir).filePath(kLeftovers); }

void removeLeftovers(const QString &appDir) {
  QFile list(leftoversFile(appDir));
  if (!list.open(QIODevice::ReadOnly)) {
    return;
  }
  QStringList still;
  const QDir dir(appDir);
  for (const QByteArray &raw : list.readAll().split('\n')) {
    const QString rel = QString::fromUtf8(raw).trimmed();
    // only "<file>.old" inside the app folder, never anything else
    if (rel.isEmpty() || !rel.endsWith(".old") || rel.contains("..") || rel.contains(':') ||
        rel.contains('\\') || QDir::isAbsolutePath(rel)) {
      continue;
    }
    const QString path = dir.filePath(rel);
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
      still << rel; // still in use: try again next time
    }
  }
  list.close();
  if (still.isEmpty()) {
    QFile::remove(leftoversFile(appDir));
  } else if (list.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    list.write(still.join('\n').toUtf8());
  }
}

bool apply(const QString &newRootIn, const QString &appDirIn, QString *error) {
  // full paths: relative ones would make the file list below relative to the
  // working folder instead of to the update
  const QString newRoot = QFileInfo(newRootIn).absoluteFilePath();
  const QString appDir = QFileInfo(appDirIn).absoluteFilePath();
  const QDir from(newRoot), to(appDir);
  struct Step {
    QString rel;
    bool hadOld = false;
  };
  QList<Step> done;
  QStringList newDirs;

  auto rollback = [&]() {
    for (auto it = done.crbegin(); it != done.crend(); ++it) {
      const QString path = to.filePath(it->rel);
      QFile::remove(path);
      if (it->hadOld) {
        QFile::rename(path + ".old", path);
      }
    }
    for (auto it = newDirs.crbegin(); it != newDirs.crend(); ++it) {
      to.rmdir(*it);
    }
  };

  QDirIterator files(newRoot, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  QStringList rels;
  while (files.hasNext()) {
    rels << from.relativeFilePath(files.next());
  }
  rels.sort(); // the same order every time
  if (!rels.contains(QStringLiteral("Rclone Explorer.exe")) &&
      !rels.contains(QStringLiteral("rclone-explorer"))) {
    *error = QObject::tr("The downloaded update does not contain Rclone Explorer.");
    return false;
  }

  for (const QString &rel : std::as_const(rels)) {
    const Rule rule = ruleFor(rel);
    const QString target = to.filePath(rel);
    if (rule == Rule::Skip || (rule == Rule::KeepExisting && QFileInfo::exists(target))) {
      continue;
    }
    const QString folder = QFileInfo(rel).path();
    if (folder != "." && !to.exists(folder)) {
      // remember each folder created, outermost first, for the rollback
      QStringList parts = folder.split('/');
      QString sub;
      for (const QString &part : parts) {
        sub = sub.isEmpty() ? part : sub + "/" + part;
        if (!to.exists(sub)) {
          if (!to.mkdir(sub)) {
            *error = QObject::tr("Cannot create %1.").arg(QDir::toNativeSeparators(to.filePath(sub)));
            rollback();
            return false;
          }
          newDirs << sub;
        }
      }
    }
    Step step{rel, QFileInfo::exists(target)};
    if (step.hadOld) {
      QFile::remove(target + ".old"); // from an earlier, unfinished update
      if (!QFile::rename(target, target + ".old")) {
        *error = QObject::tr("Cannot replace %1. Is the folder read-only?")
                     .arg(QDir::toNativeSeparators(target));
        rollback();
        return false;
      }
    }
    done << step;
    if (!QFile::copy(from.filePath(rel), target)) {
      *error = QObject::tr("Cannot write %1.").arg(QDir::toNativeSeparators(target));
      rollback();
      return false;
    }
  }

  QStringList leftovers;
  for (const Step &s : std::as_const(done)) {
    if (s.hadOld) {
      leftovers << s.rel + ".old";
    }
  }
  QFile list(leftoversFile(appDir));
  if (!leftovers.isEmpty() && list.open(QIODevice::WriteOnly | QIODevice::Append)) {
    list.write((leftovers.join('\n') + '\n').toUtf8());
  }
  return true;
}

} // namespace AppUpdate
