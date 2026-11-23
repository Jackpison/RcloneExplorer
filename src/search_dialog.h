#pragma once

#include "pch.h"

// Finds files and folders by name inside a folder of a remote and its
// subfolders. rclone lists the remote in the background (`rclone lsjson -R`
// with a name filter) and matches show up as they are found; the window
// never waits for it.
class SearchDialog : public QDialog {
  Q_OBJECT

public:
  // `folder` is the path inside the remote ("" for its top).
  SearchDialog(const QString &remote, const QString &folder, bool isLocal,
               QWidget *parent = nullptr);
  ~SearchDialog() override;

signals:
  // The match picked, relative to the folder searched ("Photos/a.jpg").
  void openRequested(const QString &relativePath);

private:
  QString mRemote;
  QString mFolder;
  bool mIsLocal;
  QLineEdit *mTerm = nullptr;
  QPushButton *mGo = nullptr;
  QPushButton *mOpen = nullptr;
  QTreeWidget *mResults = nullptr;
  QLabel *mStatus = nullptr;
  QProcess *mProcess = nullptr;
  QByteArray mPending; // the start of a line rclone has not finished yet
  QString mSearching;  // the name part being looked for
  int mFound = 0;
  bool mStopped = false;
  bool mCapped = false;

  void start();
  void stop();
  void readLines(bool all);
  void finished();
  void openCurrent();
};
