#pragma once

#include "mirror.h"
#include "pch.h"

// New / edit mirror, in two steps:
//  1. the folder to mirror: a remote, then any folder or sub-folder (sizes
//     and file counts are shown for the selected folder)
//  2. the destinations: switch remotes on and choose the folder on each,
//     plus files to skip and what happens to removed files.
class MirrorDialog : public QDialog {
  Q_OBJECT

public:
  // `remotes`: name -> type. `existing` is null for a new mirror.
  MirrorDialog(const QMap<QString, QString> &remotes, const QStringList &order,
               const QList<Mirror::Plan> &all,
               const Mirror::Plan *existing, QWidget *parent = nullptr);
  ~MirrorDialog();

  Mirror::Plan plan() const { return mPlan; }

private:
  QMap<QString, QString> mRemotes;
  QStringList mOrder; // remotes in sidebar order
  QList<Mirror::Plan> mAll;
  Mirror::Plan mPlan;
  bool mEditing = false;

  QLabel *mSteps = nullptr;
  QStackedWidget *mPages = nullptr;
  QPushButton *mBack = nullptr;
  QPushButton *mNext = nullptr;

  // step 1
  QComboBox *mSource = nullptr;
  QTreeWidget *mTree = nullptr;
  QLabel *mSelected = nullptr;
  QList<QProcess *> mRunning;
  QHash<QString, QString> mSizes; // "remote:path" -> "1.4 GB · 214 files"
  QString mSourcePath;             // the chosen folder ("" = whole remote)
  bool mWalk = false;              // editing: open the tree down to mSourcePath
  int mSerial = 0;                 // tells answers of older size checks apart

  // step 2
  QLabel *mSourceIcon = nullptr;
  QLabel *mSourceText = nullptr;
  QVBoxLayout *mDestLayout = nullptr;
  struct Dest {
    QString remote;
    QCheckBox *on = nullptr;
    QWidget *body = nullptr;
    QLineEdit *path = nullptr;
    QLabel *info = nullptr;
    enum State { Unknown, Loading, Missing, Empty, Holds, Failed } state = Unknown;
    qint64 files = 0, bytes = 0;
    int generation = 0;
  };
  QList<Dest> mDests;
  QToolButton *mSkipToggle = nullptr;
  QWidget *mSkipBox = nullptr;
  QPlainTextEdit *mExclude = nullptr;
  QComboBox *mMode = nullptr;
  QCheckBox *mDocs = nullptr;
  QLabel *mError = nullptr;

  QString sourceRemote() const;
  QString selectedPath() const;
  void showStep(int step);
  QWidget *buildFolderStep();
  QWidget *buildDestinationStep();
  void setSource();
  void loadFolder(QTreeWidgetItem *item);
  void folderSelected();
  void stopLoading();
  void fillDestinations();
  void checkDest(int i);
  void showDestInfo(int i);
  void save();
};
