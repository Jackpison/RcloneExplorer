#pragma once

#include "pch.h"

// Pick a folder on any configured remote: choose the remote, then open
// folders to look inside (they are listed when first opened). The path can
// also be typed, for a folder that does not exist yet.
class RemoteFolderDialog : public QDialog {
  Q_OBJECT

public:
  // `start` may be "remote:folder" to open that remote; anything else opens
  // the first remote.
  explicit RemoteFolderDialog(const QString &start, QWidget *parent = nullptr);

  // "remote:folder" (just "remote:" for the top level)
  QString selected() const;

private:
  QComboBox *mRemote = nullptr;
  QTreeWidget *mTree = nullptr;
  QLineEdit *mPath = nullptr;
  QLabel *mHint = nullptr;
  QPushButton *mOk = nullptr;
  QList<QProcess *> mRunning;

  void setRemote();
  void load(QTreeWidgetItem *item);
  void stopLoading();
  QTreeWidgetItem *makeFolderItem(const QString &name, const QString &path);
};
