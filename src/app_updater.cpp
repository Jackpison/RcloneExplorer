#include "app_updater.h"
#include "utils.h"

namespace {
const char *kLatest = "https://api.github.com/repos/Jackpison/RcloneExplorer/releases/latest";
const char *kUserAgent = "RcloneExplorer/" RCLONE_BROWSER_VERSION;
constexpr qint64 kMaxDownload = 400LL * 1024 * 1024; // the zip is about 60 MB
} // namespace

AppUpdater::AppUpdater(QObject *parent)
    : QObject(parent), mNet(new QNetworkAccessManager(this)) {
  // the proxy from the preferences, as for rclone updates
  auto settings = GetSettings();
  const QUrl proxyUrl(settings->value("Settings/https_proxy").toString());
  if (settings->value("Settings/useProxy").toBool() && proxyUrl.isValid() &&
      !proxyUrl.host().isEmpty()) {
    mNet->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                                 quint16(proxyUrl.port(8080)), proxyUrl.userName(),
                                 proxyUrl.password()));
  } else {
    QNetworkProxyFactory::setUseSystemConfiguration(true);
  }
}

AppUpdate::Kind AppUpdater::kind() {
  return AppUpdate::kind(IsPortableMode(), QCoreApplication::applicationDirPath());
}

bool AppUpdater::checksWanted() { return kind() != AppUpdate::Kind::Flatpak; }

bool AppUpdater::canInstall() {
  const auto k = kind();
  return k == AppUpdate::Kind::PortableZip || k == AppUpdate::Kind::Installer;
}

QNetworkReply *AppUpdater::get(const QUrl &url) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
  req.setRawHeader("Accept", "application/vnd.github+json");
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(30000);
  mReply = mNet->get(req);
  return mReply;
}

void AppUpdater::fail(const QString &error) {
  mBusy = false;
  mWork.reset();
  emit failed(mCancelled ? tr("Cancelled.") : error);
}

void AppUpdater::cancel() {
  mCancelled = true;
  if (mReply) {
    mReply->abort();
  }
}

void AppUpdater::check() {
  if (mBusy) {
    return;
  }
  mBusy = true;
  mCancelled = false;
  QNetworkReply *reply = get(QUrl(kLatest));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    AppUpdate::Release release;
    if (reply->error() != QNetworkReply::NoError ||
        !AppUpdate::parseRelease(reply->readAll(), &release)) {
      fail(tr("Could not reach GitHub to check for a new Rclone Explorer."));
      return;
    }
    mBusy = false;
    emit found(release);
  });
}

void AppUpdater::install(const AppUpdate::Release &release) {
  if (mBusy || !canInstall()) {
    return;
  }
  const bool zip = kind() == AppUpdate::Kind::PortableZip;
  const QString name = zip ? AppUpdate::zipName(release.version)
                           : AppUpdate::setupName(release.version);
  const AppUpdate::Asset *file = release.asset(name);
  if (!file && zip) { // releases up to 5.4.2 named the zip without "portable"
    file = release.asset(AppUpdate::legacyZipName(release.version));
  }
  const AppUpdate::Asset *sums = release.asset(QStringLiteral("SHA256SUMS.txt"));
  if (!file || !sums) {
    emit failed(tr("Rclone Explorer %1 has no %2 to download.").arg(release.version, name));
    return;
  }
  mBusy = true;
  mCancelled = false;
  mWork = std::make_unique<QTemporaryDir>();
  if (!mWork->isValid()) {
    fail(tr("Cannot create a temporary folder."));
    return;
  }

  emit status(tr("Fetching checksums…"));
  QNetworkReply *sumsReply = get(sums->url);
  const AppUpdate::Asset target = *file;
  connect(sumsReply, &QNetworkReply::finished, this, [this, sumsReply, target, release]() {
    sumsReply->deleteLater();
    if (mCancelled || sumsReply->error() != QNetworkReply::NoError) {
      fail(tr("Could not download the checksums of Rclone Explorer %1.").arg(release.version));
      return;
    }
    const QString expected = AppUpdate::checksumFor(sumsReply->readAll(), target.name);
    if (expected.isEmpty()) {
      fail(tr("No checksum is published for %1, so it is not installed.").arg(target.name));
      return;
    }

    emit status(tr("Downloading %1…").arg(target.name));
    QNetworkReply *reply = get(target.url);
    // written to disk as it arrives, and hashed on the way
    auto *out = new QFile(mWork->filePath(target.name), reply);
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    if (!out->open(QIODevice::WriteOnly)) {
      reply->abort();
      fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(out->fileName())));
      return;
    }
    connect(reply, &QIODevice::readyRead, this, [reply, out, hash]() {
      const QByteArray chunk = reply->readAll();
      hash->addData(chunk);
      out->write(chunk);
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 got, qint64 total) {
              if (got > kMaxDownload || total > kMaxDownload) {
                reply->abort();
                return;
              }
              emit progress(got, total);
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, out, hash, expected, target, release]() {
              reply->deleteLater();
              const QByteArray rest = reply->readAll();
              hash->addData(rest);
              out->write(rest);
              out->close();
              // after redirects the file must still have come over HTTPS
              if (mCancelled || reply->error() != QNetworkReply::NoError ||
                  reply->url().scheme() != QLatin1String("https")) {
                fail(tr("Downloading %1 failed.").arg(target.name));
                return;
              }
              if (QString::fromLatin1(hash->result().toHex()) != expected) {
                fail(tr("The download of %1 does not match its published checksum; it may be "
                        "damaged or tampered with. Nothing was changed.")
                         .arg(target.name));
                return;
              }
              verified(release, out->fileName());
            });
  });
}

void AppUpdater::verified(const AppUpdate::Release &release, const QString &file) {
  Q_UNUSED(release);
  if (kind() == AppUpdate::Kind::Installer) {
    // The setup closes nothing by itself: the app quits right after starting
    // it, the setup waits for that, installs and starts the new version.
    emit status(tr("Starting the setup…"));
    const QString logFile = QDir(mWork->path()).filePath("setup.log");
    mWork->setAutoRemove(false); // the setup still runs from there after we quit
    if (!QProcess::startDetached(file, {"/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                                        "/CLOSEAPPLICATIONS", "/LOG=" + logFile})) {
      mWork->setAutoRemove(true);
      fail(tr("The setup could not be started."));
      return;
    }
    mBusy = false;
    emit readyToRestart();
    return;
  }
  unpackAndApply(file);
}

void AppUpdater::unpackAndApply(const QString &zip) {
  emit status(tr("Unpacking…"));
  const QString outDir = mWork->filePath("new");
  QDir().mkpath(outDir);
  // Windows 10 (1803+) and 11 include tar.exe, which reads zip files
  const QString tar =
      QDir(qEnvironmentVariable("SystemRoot", "C:\\Windows")).filePath("System32/tar.exe");
  auto *p = new QProcess(this);
  connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      p->deleteLater();
      fail(tr("Could not unpack the update (tar.exe is missing)."));
    }
  });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, p, outDir](int code, QProcess::ExitStatus st) {
            p->deleteLater();
            if (code != 0 || st != QProcess::NormalExit) {
              fail(tr("Could not unpack the update."));
              return;
            }
            // the zip holds one folder, "Rclone Explorer", with the program
            QDirIterator it(outDir, {QStringLiteral("Rclone Explorer.exe")}, QDir::Files,
                            QDirIterator::Subdirectories);
            if (!it.hasNext()) {
              fail(tr("The downloaded update does not contain Rclone Explorer."));
              return;
            }
            const QString newRoot = QFileInfo(it.next()).absolutePath();
            emit status(tr("Installing…"));
            QString error;
            if (!AppUpdate::apply(newRoot, QCoreApplication::applicationDirPath(), &error)) {
              fail(error + " " + tr("Nothing was changed."));
              return;
            }
            mBusy = false;
            mWork.reset();
            emit readyToRestart();
          });
  p->start(tar, {"-xf", QDir::toNativeSeparators(zip), "-C", QDir::toNativeSeparators(outDir)});
}
