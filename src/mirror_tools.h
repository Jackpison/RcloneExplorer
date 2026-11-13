#pragma once

#include "mirror.h"
#include "pch.h"

// Storage, rclone helpers and the two small dialogs (History, Clean up trash)
// used by the Mirror page.
namespace MirrorStore {

// Same folder as tasks.bin: next to the exe in portable mode, otherwise the
// user's application data folder.
QString dataDir();

QList<Mirror::Plan> loadPlans();
void savePlans(const QList<Mirror::Plan> &plans);

struct HistPair {
  QString source; // "remote:path"
  QString dest;
  QString status; // done, error, cancelled
  Mirror::Changes changes;
  int added = 0, updated = 0, removed = 0; // full counts (lists are capped)
  QStringList errors;
  QString trash;
};
struct HistEntry {
  bool sync = true; // false: a check (dry run)
  QDateTime started;
  QList<HistPair> pairs;
};
QList<HistEntry> loadHistory();
void addHistory(const HistEntry &entry); // keeps the newest 100
void clearHistory();

} // namespace MirrorStore

namespace MirrorUtil {

// "6.7 MB" style, as in the Details view
QString niceSize(qint64 bytes);
QString countText(qint64 files);
// "1 change" / "3 changes" (the app ships no translations, so no %n plurals)
QString plural(qint64 n, const QString &one, const QString &many);

// Runs rclone in the background (with the configured config file, password
// and default options) and calls `done` with the exit code and output.
QProcess *runRclone(QObject *owner, const QStringList &args,
                    std::function<void(int code, const QByteArray &out,
                                       const QString &err)> done);

// Last meaningful line of rclone's error output.
QString lastError(const QString &stderrText);

} // namespace MirrorUtil

class MirrorHistoryDialog : public QDialog {
  Q_OBJECT
public:
  explicit MirrorHistoryDialog(QWidget *parent = nullptr);

private:
  QTreeWidget *mTree = nullptr;
  void fill();
};

class MirrorTrashDialog : public QDialog {
  Q_OBJECT
public:
  // A _SyncTrash folder: on a remote, or next to a mirrored folder on this PC.
  struct Place {
    QString title; // "pCloud", or "This PC · C:\\Backups"
    QString type;  // remote type for the icon ("local" on this PC)
    QString root;  // "pCloud:_SyncTrash" or "C:/Backups/_SyncTrash"
  };
  MirrorTrashDialog(const QList<Place> &places, QWidget *parent = nullptr);

private:
  struct Row {
    QString root;
    QLabel *text = nullptr;
    QStringList snapshots, old;
    qint64 files = 0, bytes = 0;
    int cleaned = 0;
    QString error;
    bool busy = false;
  };
  QList<Row> mRows;
  QComboBox *mAge = nullptr;
  QLabel *mTotal = nullptr;
  QPushButton *mDelete = nullptr;
  int mGeneration = 0; // results of an older scan are ignored
  bool mCleaning = false;

  QDateTime cutoff() const;
  void scan();
  void scanRow(int i, int generation);
  void sizeNext(int i, int k, int generation);
  void clean();
  void cleanNext(int i, int k);
  void update();
};
