#pragma once

#include "mirror.h"
#include "pch.h"
#include "schedule.h"

class JobWidget;

// The Mirror page: every mirror as a card (source folder, its destinations
// with size, file count and a Matches / Differs badge), Check (a dry run whose
// changes are listed right here) and Sync (normal transfers on the Transfers
// page), plus History, Clean up trash and per-mirror schedules.
class MirrorPage : public QWidget {
  Q_OBJECT

public:
  using AddTransfer = std::function<JobWidget *(const QString &message, const QString &source,
                                                const QString &dest, const QStringList &args)>;
  MirrorPage(AddTransfer addTransfer, std::function<void()> showTransfers,
             QWidget *parent = nullptr);
  ~MirrorPage();

  // name -> type of every configured remote; `order` as in the sidebar
  void setRemotes(const QMap<QString, QString> &remotes, const QStringList &order);
  bool isRunning() const { return mActive; }

private:
  AddTransfer mAddTransfer;
  std::function<void()> mShowTransfers;
  QMap<QString, QString> mRemotes;
  QStringList mOrder;
  QList<Mirror::Plan> mPlans;

  // ---- sizes ("rclone size", cached; at most 3 at a time)
  struct Size {
    enum State { Unknown, Loading, Ready, Missing, Failed } state = Unknown;
    qint64 files = 0, bytes = 0;
  };
  QHash<QString, Size> mSizes; // key: "remote:path|<filter options>"
  QList<QPair<QString, QStringList>> mSizeQueue;
  int mSizeRunning = 0;
  Size wantSize(const QString &remote, const QString &path, const Mirror::Plan &plan);
  void pumpSizes();
  void dropSizes(const QString &remotePath);
  QString sizeText(const Size &s) const;

  // ---- runs
  struct Pair {
    QUuid plan;
    int target = 0;
    QString source, dest, destRemote;
    QString status; // waiting, running, done, error, cancelled
    Mirror::Changes changes;
    QStringList errors;
    QString trash, report;
    QPointer<JobWidget> job;
    QPointer<QProcess> proc;
  };
  QList<Pair> mPairs;
  bool mSync = false;
  bool mActive = false;
  bool mStopping = false;
  bool mDismissed = false;
  QDateTime mStarted;
  QString mStamp;
  std::unique_ptr<QTemporaryDir> mTmp;
  QSet<QString> mBusy;      // destination remotes with a copy running
  QSet<QString> mOpenFiles; // "plan/target" whose file list is shown
  void start(bool sync, QList<QUuid> ids, bool interactive = true);
  void pump();
  void startPair(int i);
  void pairDone(int i, int code, const QString &error);
  void finishRun();
  void stop();

  // ---- schedules (kept in the settings, like those of saved tasks)
  QHash<QUuid, Schedule> mSchedules;
  QHash<QUuid, QDateTime> mNextRun;
  QTimer mTimer;
  void loadSchedules();
  void editSchedule(const QUuid &id);
  void tick();

  // ---- interface
  QPushButton *mNew = nullptr, *mCheckAll = nullptr, *mSyncAll = nullptr;
  QPushButton *mHistory = nullptr, *mTrash = nullptr;
  QWidget *mBanner = nullptr;
  QLabel *mBannerTitle = nullptr, *mBannerSub = nullptr;
  QPushButton *mApply = nullptr, *mGoTransfers = nullptr, *mStop = nullptr;
  QToolButton *mDismiss = nullptr;
  QScrollArea *mScroll = nullptr;
  QWidget *mListHost = nullptr;
  QTimer mRebuild;
  void rebuild();
  void scheduleRebuild() { mRebuild.start(); }
  void updateBanner();
  QWidget *planCard(const Mirror::Plan &p);
  QWidget *destRow(const Mirror::Plan &p, int t, const Size &source);
  QWidget *emptyState();
  const Mirror::Plan *find(const QUuid &id) const;
  void editPlan(const QUuid &id); // null id: new mirror
  void removePlan(const QUuid &id);
  void save();
};
