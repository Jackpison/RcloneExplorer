#pragma once

#include "app_update.h"
#include "pch.h"

// Checks GitHub for a newer Rclone Explorer and, on Windows, installs it:
// the portable zip is swapped in place, an installed copy runs the new setup.
// Every download is checked against the release's SHA256SUMS.txt.
class AppUpdater : public QObject {
  Q_OBJECT

public:
  explicit AppUpdater(QObject *parent = nullptr);

  static AppUpdate::Kind kind();
  // Updates are checked at all (not in a Flatpak, where Flathub does it).
  static bool checksWanted();
  // This copy can install an update by itself (Windows zip or setup).
  static bool canInstall();

  bool isBusy() const { return mBusy; }
  // files are being swapped right now (cannot be stopped halfway)
  bool isInstalling() const { return mInstalling; }
  void check();
  void install(const AppUpdate::Release &release);
  void cancel();
  // Asked just before anything is installed: false (transfers running) stops
  // the update, as the restart would end them.
  void setMayRestart(std::function<bool()> check) { mMayRestart = std::move(check); }

signals:
  void found(const AppUpdate::Release &release);
  void failed(const QString &error);
  void status(const QString &text);
  void progress(qint64 done, qint64 total);
  // false while files are being swapped: that cannot be stopped halfway
  void cancellable(bool can);
  // the new version is in place (or its setup started): quit now
  void readyToRestart();

private:
  QNetworkAccessManager *mNet;
  QPointer<QNetworkReply> mReply;
  QPointer<QProcess> mUnpack;
  std::function<bool()> mMayRestart;
  bool mInstalling = false;
  std::unique_ptr<QTemporaryDir> mWork;
  bool mBusy = false;
  bool mCancelled = false;

  QNetworkReply *get(const QUrl &url);
  void fail(const QString &error);
  void verified(const AppUpdate::Release &release, const QString &file);
  void unpackAndApply(const QString &zip);
};
