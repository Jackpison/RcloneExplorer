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
  void check();
  void install(const AppUpdate::Release &release);
  void cancel();

signals:
  void found(const AppUpdate::Release &release);
  void failed(const QString &error);
  void status(const QString &text);
  void progress(qint64 done, qint64 total);
  // the new version is in place (or its setup started): quit now
  void readyToRestart();

private:
  QNetworkAccessManager *mNet;
  QPointer<QNetworkReply> mReply;
  std::unique_ptr<QTemporaryDir> mWork;
  bool mBusy = false;
  bool mCancelled = false;

  QNetworkReply *get(const QUrl &url);
  void fail(const QString &error);
  void verified(const AppUpdate::Release &release, const QString &file);
  void unpackAndApply(const QString &zip);
};
