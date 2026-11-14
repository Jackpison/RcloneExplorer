#pragma once

#include "pch.h"

// Downloads, verifies and installs rclone releases.
//
// Sources: downloads.rclone.org (official) with GitHub Releases as fallback.
// Every download is checked against rclone's published SHA256SUMS before it
// is used. The installed copy lives next to Rclone Explorer in portable mode
// (<app dir>/rclone/rclone.exe, stored as a relative path so the folder can
// be moved) or in the per-user app data folder otherwise.
//
// Replacing the binary is safe while rclone jobs or mounts are running: on
// Windows a running .exe can be renamed but not overwritten, so the old file
// is renamed to rclone.exe.old (cleaned up on next start) and running
// processes keep using it until they exit.
class RcloneUpdater : public QObject {
  Q_OBJECT

public:
  explicit RcloneUpdater(QObject *parent = nullptr);

  // Where Rclone Explorer keeps its own rclone copy (absolute path).
  static QString managedPath();
  // Value to store in Settings/rclone for the managed copy (relative in
  // portable mode).
  static QString managedSettingValue();
  // rclone bundled inside the running AppImage (Linux), or empty.
  static QString bundledPath();
  // Remove leftovers (rclone.exe.old) from a previous update.
  static void cleanup();
  // -1 if a < b, 0 if equal, 1 if a > b. Accepts "v1.2.3", "1.2.3-beta..."
  static int compareVersions(const QString &a, const QString &b);
  // "windows-amd64", "linux-arm64", ... for the running system.
  static QString platformSuffix();

  bool isBusy() const { return mBusy; }

public slots:
  // Emits latestVersionFound() or failed().
  void checkLatest();
  // Emits progress()/status() during, then installed() or failed().
  void install(const QString &version);
  void cancel();

signals:
  void latestVersionFound(const QString &version);
  void status(const QString &message);
  void progress(qint64 received, qint64 total);
  void installed(const QString &version, const QString &path);
  void failed(const QString &error);

private:
  struct Source {
    QString versionUrl;              // empty = use GitHub redirect
    QString baseUrl;                 // %1 = version, e.g. "v1.75.1"
  };

  QNetworkReply *get(const QUrl &url);
  void checkFrom(int sourceIndex);
  void downloadFrom(int sourceIndex, const QString &version);
  void verifyAndExtract(const QString &version, const QByteArray &zip,
                        const QByteArray &sums);
  void extract(const QString &version, const QString &zipPath, int attempt);
  void finishInstall(const QString &version, const QString &extractDir);
  void replaceExe(const QString &version, const QString &newExe);
  void fail(const QString &error);

  QNetworkAccessManager *mNet;
  QList<Source> mSources;
  QPointer<QNetworkReply> mReply;
  std::unique_ptr<QTemporaryDir> mWorkDir;
  bool mBusy = false;
  bool mCancelled = false;
};
