#include "rclone_updater.h"
#include "utils.h"

namespace {

QString exeName() {
#ifdef Q_OS_WIN
  return "rclone.exe";
#else
  return "rclone";
#endif
}

const char *kUserAgent = "RcloneExplorer/" RCLONE_BROWSER_VERSION;

} // namespace

RcloneUpdater::RcloneUpdater(QObject *parent)
    : QObject(parent), mNet(new QNetworkAccessManager(this)) {
  mSources = {
      {"https://downloads.rclone.org/version.txt",
       "https://downloads.rclone.org/%1"},
      {QString(), "https://github.com/rclone/rclone/releases/download/%1"},
  };

  // Honour the proxy configured in Rclone Explorer's preferences; otherwise
  // use the Windows/system proxy settings.
  auto settings = GetSettings();
  QUrl proxyUrl(settings->value("Settings/https_proxy").toString());
  if (settings->value("Settings/useProxy").toBool() && proxyUrl.isValid() &&
      !proxyUrl.host().isEmpty()) {
    mNet->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                                 quint16(proxyUrl.port(8080)),
                                 proxyUrl.userName(), proxyUrl.password()));
  } else {
    QNetworkProxyFactory::setUseSystemConfiguration(true);
  }
}

QString RcloneUpdater::managedPath() {
  QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (IsPortableMode()) {
#ifdef Q_OS_WIN
    base = qApp->applicationDirPath();
#else
    // An AppImage is read-only: keep rclone next to the .AppImage file.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty()) {
      base = QFileInfo(appImage).absolutePath();
    }
#endif
  }
  return QDir(base).filePath("rclone/" + exeName());
}

QString RcloneUpdater::bundledPath() {
#ifdef Q_OS_WIN
  // The portable zip names rclone in its ini. The installer puts rclone next
  // to the exe, where it stays until an update downloads a newer one.
  if (IsPortableMode()) {
    return QString();
  }
  const QString path = QDir(qApp->applicationDirPath()).filePath("rclone/rclone.exe");
  return QFileInfo::exists(path) ? path : QString();
#else
  // Inside an AppImage, rclone is bundled next to the app binary. Its path
  // changes on every start (temporary mount), so it is never saved.
  if (IsFlatpak()) {
    return QStringLiteral("/app/bin/rclone"); // updated with the Flatpak
  }
  const QString appDir = qEnvironmentVariable("APPDIR");
  if (appDir.isEmpty()) {
    return QString();
  }
  const QString path = QDir(appDir).filePath("usr/bin/rclone");
  return QFileInfo(path).isExecutable() ? path : QString();
#endif
}

QString RcloneUpdater::managedSettingValue() {
  // GetRclone() resolves relative paths against the app folder (Windows) or
  // the AppImage's folder (Linux) in portable mode, so the folder keeps
  // working after being moved or renamed.
  if (IsPortableMode()) {
#ifdef Q_OS_WIN
    return "rclone/" + exeName();
#else
    if (!qEnvironmentVariable("APPIMAGE").isEmpty()) {
      return "rclone/" + exeName();
    }
#endif
  }
  return managedPath();
}

void RcloneUpdater::cleanup() {
  QFile::remove(managedPath() + ".old");
}

int RcloneUpdater::compareVersions(const QString &a, const QString &b) {
  static const QRegularExpression rx(R"((\d+)(?:\.(\d+))?(?:\.(\d+))?)");
  auto parts = [](const QString &v) {
    QList<int> out{0, 0, 0};
    auto m = rx.match(v);
    if (m.hasMatch()) {
      for (int i = 0; i < 3; ++i) {
        out[i] = m.captured(i + 1).toInt();
      }
    }
    return out;
  };
  const auto pa = parts(a), pb = parts(b);
  for (int i = 0; i < 3; ++i) {
    if (pa[i] != pb[i]) {
      return pa[i] < pb[i] ? -1 : 1;
    }
  }
  return 0;
}

QString RcloneUpdater::platformSuffix() {
#if defined(Q_OS_WIN)
  const QString os = "windows";
#elif defined(Q_OS_MACOS)
  const QString os = "osx";
#else
  const QString os = "linux";
#endif
  const QString cpu = QSysInfo::currentCpuArchitecture();
  QString arch = "amd64";
  if (cpu == "arm64") {
    arch = "arm64";
  } else if (cpu == "i386") {
    arch = "386";
  }
  return os + "-" + arch;
}

QNetworkReply *RcloneUpdater::get(const QUrl &url) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(30000);
  mReply = mNet->get(req);
  return mReply;
}

void RcloneUpdater::fail(const QString &error) {
  mBusy = false;
  mWorkDir.reset();
  emit failed(mCancelled ? tr("Cancelled.") : error);
}

void RcloneUpdater::cancel() {
  mCancelled = true;
  if (mReply) {
    mReply->abort();
  }
}

// ---------------------------------------------------------------- check ----

void RcloneUpdater::checkLatest() {
  if (mBusy) {
    return;
  }
  mBusy = true;
  mCancelled = false;
  checkFrom(0);
}

void RcloneUpdater::checkFrom(int i) {
  if (i >= mSources.size()) {
    fail(tr("Could not reach rclone.org or GitHub to check the latest "
            "rclone version."));
    return;
  }
  const Source &src = mSources[i];
  const bool useRedirect = src.versionUrl.isEmpty();
  QNetworkReply *reply =
      get(QUrl(useRedirect ? "https://github.com/rclone/rclone/releases/latest"
                           : src.versionUrl));

  connect(reply, &QNetworkReply::finished, this, [=, this]() {
    reply->deleteLater();
    if (mCancelled) {
      fail(QString());
      return;
    }
    static const QRegularExpression rx(R"(v\d+\.\d+\.\d+)");
    QString version;
    if (reply->error() == QNetworkReply::NoError) {
      // version.txt contains "rclone v1.75.1"; the GitHub page redirects to
      // .../releases/tag/v1.75.1
      const QString text = useRedirect ? reply->url().toString()
                                       : QString::fromUtf8(reply->readAll());
      version = rx.match(text).captured(0);
    }
    if (version.isEmpty()) {
      checkFrom(i + 1);
      return;
    }
    mBusy = false;
    emit latestVersionFound(version);
  });
}

// -------------------------------------------------------------- install ----

void RcloneUpdater::install(const QString &version) {
  if (mBusy) {
    return;
  }
  mBusy = true;
  mCancelled = false;
  mWorkDir = std::make_unique<QTemporaryDir>();
  if (!mWorkDir->isValid()) {
    fail(tr("Cannot create a temporary folder."));
    return;
  }
  downloadFrom(0, version);
}

void RcloneUpdater::downloadFrom(int i, const QString &version) {
  if (i >= mSources.size()) {
    fail(tr("Downloading rclone %1 failed from all sources.").arg(version));
    return;
  }
  const QString base = mSources[i].baseUrl.arg(version);
  const QString zipName =
      QString("rclone-%1-%2.zip").arg(version, platformSuffix());
  const QString host = QUrl(base).host();

  // 1) checksums (small)  2) the archive itself (with progress)
  emit status(tr("Fetching checksums from %1…").arg(host));
  QNetworkReply *sumsReply = get(QUrl(base + "/SHA256SUMS"));
  connect(sumsReply, &QNetworkReply::finished, this, [=, this]() {
    sumsReply->deleteLater();
    if (mCancelled) {
      fail(QString());
      return;
    }
    if (sumsReply->error() != QNetworkReply::NoError) {
      downloadFrom(i + 1, version);
      return;
    }
    const QByteArray sums = sumsReply->readAll();

    emit status(tr("Downloading %1 from %2…").arg(zipName, host));
    QNetworkReply *zipReply = get(QUrl(base + "/" + zipName));
    connect(zipReply, &QNetworkReply::downloadProgress, this,
            [this, zipReply](qint64 got, qint64 total) {
              // rclone zips are ~30 MB; refuse anything absurdly large
              constexpr qint64 kMaxDownload = 256LL * 1024 * 1024;
              if (got > kMaxDownload || total > kMaxDownload) {
                zipReply->abort();
                return;
              }
              emit progress(got, total);
            });
    connect(zipReply, &QNetworkReply::finished, this, [=, this]() {
      zipReply->deleteLater();
      if (mCancelled) {
        fail(QString());
        return;
      }
      // after redirects the file must still have come over HTTPS
      if (zipReply->error() != QNetworkReply::NoError ||
          zipReply->url().scheme() != QLatin1String("https")) {
        downloadFrom(i + 1, version);
        return;
      }
      verifyAndExtract(version, zipReply->readAll(), sums);
    });
  });
}

void RcloneUpdater::verifyAndExtract(const QString &version,
                                     const QByteArray &zip,
                                     const QByteArray &sums) {
  const QString zipName =
      QString("rclone-%1-%2.zip").arg(version, platformSuffix());

  emit status(tr("Verifying download…"));
  QString expected;
  const QRegularExpression rx("^([0-9a-fA-F]{64})\\s+\\*?" +
                              QRegularExpression::escape(zipName) + "\\s*$");
  for (const QByteArray &line : sums.split('\n')) {
    auto m = rx.match(QString::fromUtf8(line).trimmed());
    if (m.hasMatch()) {
      expected = m.captured(1).toLower();
      break;
    }
  }
  if (expected.isEmpty()) {
    fail(tr("No checksum published for %1 – refusing to install.").arg(zipName));
    return;
  }
  const QString actual =
      QString::fromLatin1(QCryptographicHash::hash(zip, QCryptographicHash::Sha256).toHex());
  if (actual != expected) {
    fail(tr("Checksum mismatch for %1 – the download is corrupt or has been "
            "tampered with. Nothing was installed.").arg(zipName));
    return;
  }

  const QString zipPath = mWorkDir->filePath(zipName);
  QFile f(zipPath);
  if (!f.open(QIODevice::WriteOnly) || f.write(zip) != zip.size()) {
    fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(zipPath)));
    return;
  }
  f.close();
  extract(version, zipPath, 0);
}

void RcloneUpdater::extract(const QString &version, const QString &zipPath,
                            int attempt) {
  const QString outDir = mWorkDir->filePath("x");
  QDir().mkpath(outDir);

  // Qt has no public unzip API. Windows 10 (1803+) and 11 ship bsdtar as
  // tar.exe which reads zip; PowerShell's Expand-Archive is the fallback.
  QString program;
  QStringList args;
#ifdef Q_OS_WIN
  const QString sys32 = QDir(qEnvironmentVariable("SystemRoot", "C:\\Windows"))
                            .filePath("System32");
  if (attempt == 0) {
    program = QDir(sys32).filePath("tar.exe");
    args << "-xf" << QDir::toNativeSeparators(zipPath) << "-C"
         << QDir::toNativeSeparators(outDir);
  } else if (attempt == 1) {
    auto quote = [](QString s) { return "'" + s.replace("'", "''") + "'"; };
    program = QDir(sys32).filePath("WindowsPowerShell/v1.0/powershell.exe");
    args << "-NoProfile" << "-NonInteractive" << "-Command"
         << "Expand-Archive -LiteralPath " +
                quote(QDir::toNativeSeparators(zipPath)) +
                " -DestinationPath " + quote(QDir::toNativeSeparators(outDir)) +
                " -Force";
  }
#else
  // unzip is not installed everywhere; Python's zipfile nearly always is
  if (attempt == 0) {
    program = "unzip";
    args << "-q" << "-o" << zipPath << "-d" << outDir;
  } else if (attempt == 1) {
    program = "bsdtar";
    args << "-xf" << zipPath << "-C" << outDir;
  } else if (attempt == 2) {
    program = "python3";
    args << "-m" << "zipfile" << "-e" << zipPath << outDir;
  }
#endif
  if (program.isEmpty()) {
    fail(tr("Could not unpack the rclone archive (no tar.exe or PowerShell "
            "found)."));
    return;
  }

  emit status(tr("Unpacking…"));
  auto *p = new QProcess(this);
  connect(p, &QProcess::errorOccurred, this, [=, this](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      p->deleteLater();
      extract(version, zipPath, attempt + 1);
    }
  });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [=, this](int code, QProcess::ExitStatus st) {
            p->deleteLater();
            if (code != 0 || st != QProcess::NormalExit) {
              extract(version, zipPath, attempt + 1);
              return;
            }
            finishInstall(version, outDir);
          });
  p->start(program, args);
}

void RcloneUpdater::finishInstall(const QString &version,
                                  const QString &extractDir) {
  QString newExe;
  QDirIterator it(extractDir, {exeName()}, QDir::Files,
                  QDirIterator::Subdirectories);
  if (it.hasNext()) {
    newExe = it.next();
  }
  if (newExe.isEmpty()) {
    fail(tr("%1 not found in the downloaded archive.").arg(exeName()));
    return;
  }

#ifndef Q_OS_WIN
  // some extractors (python's zipfile) drop the executable bit
  QFile::setPermissions(newExe, QFile::permissions(newExe) | QFileDevice::ExeOwner |
                                    QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif

  // Sanity check: the new binary must run and report the expected version.
  // Without blocking the window: a virus scanner may hold a freshly
  // downloaded program for a few seconds on its first start.
  emit status(tr("Checking the new rclone…"));
  auto *p = new QProcess(this);
  auto *timeout = new QTimer(p);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, p, [p]() { p->kill(); });
  connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      p->deleteLater();
      fail(tr("The downloaded rclone did not run correctly; nothing was changed."));
    }
  });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, p, version, newExe](int, QProcess::ExitStatus) {
            p->deleteLater();
            const QString out = QString::fromUtf8(p->readAllStandardOutput());
            if (!out.startsWith("rclone " + version)) {
              fail(tr("The downloaded rclone did not run correctly; nothing was changed."));
              return;
            }
            replaceExe(version, newExe);
          });
  p->start(newExe, {"version"});
  timeout->start(60000);
}

void RcloneUpdater::replaceExe(const QString &version, const QString &newExe) {
  emit status(tr("Installing…"));
  const QString target = managedPath();
  const QString old = target + ".old";
  QDir().mkpath(QFileInfo(target).absolutePath());
  QFile::remove(old);

  // A running rclone.exe (job, mount) cannot be overwritten, but it can be
  // renamed; running processes continue with the old file.
  if (QFile::exists(target) && !QFile::rename(target, old)) {
    fail(tr("Cannot replace %1. Is the folder read-only?")
             .arg(QDir::toNativeSeparators(target)));
    return;
  }
  if (!QFile::copy(newExe, target)) {
    QFile::rename(old, target);
    fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(target)));
    return;
  }
  QFile::remove(old); // fails harmlessly if still in use; cleanup() retries

  mBusy = false;
  mWorkDir.reset();
  emit installed(version, target);
}
