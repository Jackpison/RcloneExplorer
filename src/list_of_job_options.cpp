#include "list_of_job_options.h"
#include <QDataStream>
#include <qdir.h>
#include <qlogging.h>
#include <qstandardpaths.h>
#include <utils.h>

static QDataStream &operator>>(QDataStream &dataStream, JobOptions &jo);
static QDataStream &operator<<(QDataStream &dataStream, JobOptions &jo);
static QDataStream &operator>>(QDataStream &in, JobOptions::Operation &e);
static QDataStream &operator>>(QDataStream &in, JobOptions::SyncTiming &e);
static QDataStream &operator>>(QDataStream &in, JobOptions::CompareOption &e);
static QDataStream &operator>>(QDataStream &in, JobOptions::JobType &e);

ListOfJobOptions *ListOfJobOptions::SavedJobOptions = nullptr;
const QString ListOfJobOptions::persistenceFileName = "tasks.bin";

ListOfJobOptions::ListOfJobOptions() {}

ListOfJobOptions *ListOfJobOptions::getInstance() {
  if (SavedJobOptions == nullptr) {
    SavedJobOptions = new ListOfJobOptions();
    RestoreFromUserData(*SavedJobOptions);
  }
  return SavedJobOptions;
}

bool ListOfJobOptions::Persist(JobOptions *jo) {
  bool isNew = !this->tasks.contains(jo);
  if (isNew)
    this->tasks.append(jo);
  else {
    //    int ix = tasks.indexOf(jo);
    //    JobOptions *old = tasks[ix];
  }
  PersistToUserData();
  return isNew;
}

bool ListOfJobOptions::Forget(JobOptions *jo) {
  bool isKnown = this->tasks.contains(jo);
  if (!isKnown)
    return false;
  int ix = tasks.indexOf(jo);
  tasks.removeAt(ix);
  PersistToUserData();
  return isKnown;
}

QString ListOfJobOptions::persistencePath() {
  QDir outputDir;

  if (IsPortableMode()) {
    // in portable mode tasks' file will be saved in the same folder as
    // excecutable
#ifdef Q_OS_MACOS
    // on macOS excecutable file is located in
    // ./rclone-browser.app/Contents/MasOS/ to get actual bundle folder we have
    // to traverse three levels up
    outputDir = QDir(qApp->applicationDirPath() + "/../../..");
#else
#ifdef Q_OS_WIN
    outputDir = QDir(qApp->applicationDirPath());
#else
    QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
    outputDir = QDir(xdg_config_home + "/rclone-browser");
#endif
#endif

  } else {

    // get data location folder from Qt  - OS dependend
    outputDir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
  }

  if (!outputDir.exists()) {
    outputDir.mkpath(".");
  }
  return outputDir.absoluteFilePath(persistenceFileName);
}

namespace {

// The tasks in `data`. False if it is damaged (cut off by a crash, for
// example): `out` then holds the tasks before the damage.
bool readTasks(const QByteArray &data, QList<JobOptions *> *out) {
  QDataStream in(data);
  in.setVersion(QDataStream::Qt_5_2);
  while (!in.atEnd()) {
    auto *jo = new JobOptions();
    bool ok = true;
    try {
      in >> *jo;
    } catch (SerializationException &) {
      ok = false;
    }
    if (!ok || in.status() != QDataStream::Ok) {
      delete jo;
      return false;
    }
    out->append(jo);
  }
  return true;
}

} // namespace

bool ListOfJobOptions::RestoreFromUserData(ListOfJobOptions &dataIn) {
  const QString path = persistencePath();
  // a complete copy left beside the file by an interrupted save is the newest
  QFile pending(PendingSaveFile(path));
  if (pending.open(QIODevice::ReadOnly)) {
    QList<JobOptions *> tasks;
    if (readTasks(pending.readAll(), &tasks)) {
      dataIn.tasks = tasks;
      return true;
    }
    qDeleteAll(tasks);
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return false;
  }
  QList<JobOptions *> tasks;
  const bool ok = readTasks(f.readAll(), &tasks);
  dataIn.tasks = tasks; // a damaged file: at least the tasks before the damage
  if (!ok) {
    // kept, as the next save writes over it
    const QString copy = path + ".damaged";
    QFile::remove(copy);
    QFile::copy(path, copy);
  }
  return ok;
}

bool ListOfJobOptions::PersistToUserData() {
  QByteArray data;
  {
    QDataStream outstream(&data, QIODevice::WriteOnly);
    outstream.setVersion(QDataStream::Qt_5_2);
    for (JobOptions *it : tasks) {
      outstream << *it;
    }
  }
  const bool saved = WriteFileSafely(persistencePath(), data); // see utils.h
  emit tasksListUpdated();
  return saved;
}

QDataStream &operator<<(QDataStream &stream, JobOptions &jo) {
  stream << jo.myName() << JobOptions::classVersion << jo.description
         << jo.jobType << jo.operation << /* jo.dryRun <<*/ jo.sync
         << jo.syncTiming << jo.skipNewer << jo.skipExisting << jo.compare
         << jo.compareOption << jo.verbose << jo.sameFilesystem
         << jo.dontUpdateModified << jo.transfers << jo.checkers << jo.bandwidth
         << jo.minSize << jo.minAge << jo.maxAge << jo.maxDepth
         << jo.connectTimeout << jo.idleTimeout << jo.retries
         << jo.lowLevelRetries << jo.deleteExcluded << jo.excluded << jo.extra
         << jo.DriveSharedWithMe << jo.source << jo.dest << jo.isFolder
         << jo.uniqueId;

  return stream;
}

QDataStream &operator>>(QDataStream &stream, JobOptions &jo) {
  QString actualName;
  qint32 actualVersion;

  stream >> actualName;
  if (QString::compare(actualName, jo.myName()) != 0)
    throw SerializationException("incorrect class");

  stream >> actualVersion;
  if (actualVersion > JobOptions::classVersion)
    throw SerializationException("stored version is newer");

  stream >> jo.description >> jo.jobType >> jo.operation >>
      /* jo.dryRun >> */ jo.sync >> jo.syncTiming >> jo.skipNewer >>
      jo.skipExisting >> jo.compare >> jo.compareOption >> jo.verbose >>
      jo.sameFilesystem >> jo.dontUpdateModified >> jo.transfers >>
      jo.checkers >> jo.bandwidth >> jo.minSize >> jo.minAge >> jo.maxAge >>
      jo.maxDepth >> jo.connectTimeout >> jo.idleTimeout >> jo.retries >>
      jo.lowLevelRetries >> jo.deleteExcluded >> jo.excluded >> jo.extra >>
      jo.DriveSharedWithMe >> jo.source >> jo.dest;

  // as fields are added in later revisions, check actualVersion here and
  // conditionally extract any new fields iff they are expected based on the
  // stream value
  if (actualVersion >= 2) {
    stream >> jo.isFolder;
    if (actualVersion >= 3) {
      stream >> jo.uniqueId;
    }
  }

  return stream;
}

QDataStream &operator>>(QDataStream &in, JobOptions::Operation &e) {
  in >> (quint32 &)e;
  return in;
}

QDataStream &operator>>(QDataStream &in, JobOptions::SyncTiming &e) {
  in >> (quint32 &)e;
  return in;
}

QDataStream &operator>>(QDataStream &in, JobOptions::CompareOption &e) {
  in >> (quint32 &)e;
  return in;
}

QDataStream &operator>>(QDataStream &in, JobOptions::JobType &e) {
  in >> (quint32 &)e;
  return in;
}
