#pragma once

#include "job_options.h"
#include "schedule.h"
#include "pch.h"

class RcloneUpdater;
#include "ui_main_window.h"

class JobWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

private slots:
  void rcloneGetVersion();
  void rcloneConfig();
  void rcloneListRemotes();
  void listTasks();

  JobWidget *addTransfer(const QString &message, const QString &source,
                         const QString &dest, const QStringList &args);
  void addMount(const QString &remote, const QString &folder,
                const QStringList &mountOptions);
  void addStream(const QString &remote, const QString &stream);

private:
  Ui::MainWindow ui;

  QSystemTrayIcon mSystemTray;
  JobWidget *mLastFinished = nullptr;

  bool mAlwaysShowInTray;
  bool mCloseToTray;
  bool mNotifyFinishedTransfers;

  QLabel *mStatusMessage;


  bool mFirstTime = true;

  // rclone download / update
  RcloneUpdater *mUpdater = nullptr;
  QLabel *mRcloneStatus = nullptr;
  QAction *mAutoCheckAction = nullptr;
  QString mRcloneVersion;      // installed, without leading "v"
  QString mLatestRclone;       // e.g. "v1.75.1", empty if unknown
  void setupRcloneUpdater();
  void updateRcloneStatus();
  void maybeAutoCheckRclone();
  void checkRcloneUpdate(bool interactive);
  void offerRcloneDownload();
  void installRclone(const QString &version);
  int mJobCount = 0;

  bool canClose();
  void closeEvent(QCloseEvent *ev) override;
  bool getConfigPassword(QProcess *p);

  void addEmptyJobsMessage();

  JobWidget *runItem(JobOptionsListWidgetItem *item, bool dryrun = false);
  void editSelectedTask();
  JobWidget *runTask(JobOptions *jo, bool dryrun);

  // Scheduled tasks (see schedule.h): a timer checks once in a while whether
  // a task is due. Schedules are kept in the settings, keyed by task id.
  QTimer *mScheduleTimer = nullptr;
  QHash<QUuid, Schedule> mSchedules;
  QHash<QUuid, QDateTime> mNextRun;
  QSet<QUuid> mScheduledRunning; // still running: do not start it again
  QPushButton *mButtonSchedule = nullptr;
  void loadSchedules();
  void setSchedule(const QUuid &task, const Schedule &s);
  void scheduleTick();
  void editSelectedSchedule();
  QIcon mUploadIcon;
  QIcon mDownloadIcon;

  // Windows 11 shell (main_window_shell.cpp)
  QListWidget *mNav = nullptr;
  QListWidgetItem *mNavTransfers = nullptr;
  QWidget *mNavRemotesHeader = nullptr;
  QListWidget *mNavRemotes = nullptr;
  QLabel *mNavRemoteCount = nullptr;
  QToolButton *mThemeButton = nullptr;
  QStackedWidget *mPages = nullptr; // Home, Transfers, Tasks, remote tabs
  void showPage(int page);
  QToolButton *mNewTab = nullptr;
  void updateNewTabButton();
  void updateThemeButton();
  QComboBox *mTasksSort = nullptr;
  QLabel *mHomeSubtitle = nullptr;
  QLabel *mHomeEmpty = nullptr;
  bool mSyncingNav = false;
  void buildShell();
  int remoteTab(const QString &name) const;
  void openRemote(const QString &name, const QString &type, bool newTab = false);
  void closeRemote(const QString &name);
  void closeRemoteWidget(QWidget *w);
  bool eventFilter(QObject *o, QEvent *e) override;
  void syncNavToCurrentPage();
  void rebuildNavRemotes();
  void setJobsTabText(const QString &text);
};
