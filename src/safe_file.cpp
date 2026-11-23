#include "safe_file.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

QString PendingSaveFile(const QString &path) { return path + ".saving"; }

bool WriteFileSafely(const QString &path, const QByteArray &data) {
  QFile f(path);
  if (!QFileInfo::exists(PendingSaveFile(path)) && f.open(QIODevice::ReadOnly) &&
      f.readAll() == data) {
    return true; // unchanged
  }
  f.close();
  QSaveFile pending(PendingSaveFile(path));
  if (!pending.open(QIODevice::WriteOnly) || pending.write(data) != data.size() ||
      !pending.commit()) {
    return false; // nothing changed
  }
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size() &&
      f.flush()) {
    f.close();
    QFile::remove(PendingSaveFile(path));
  }
  return true; // in the file, or at least complete beside it
}
