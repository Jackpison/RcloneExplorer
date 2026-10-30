#include "sound_player.h"
#include "pch.h"

#if defined(Q_OS_WIN)
#include <mmsystem.h>
#endif

namespace SoundPlayer {
namespace {

QString resourceOf(Sound sound) {
  return sound == Finished ? QStringLiteral(":/sounds/finished.wav")
                           : QStringLiteral(":/sounds/failed.wav");
}

#if !defined(Q_OS_WIN)
// Players take a file, so the bundled sound is written out once.
QString soundFile(Sound sound) {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (dir.isEmpty() || !QDir().mkpath(dir)) {
    return QString();
  }
  const QString path =
      QDir(dir).filePath(sound == Finished ? "finished.wav" : "failed.wav");
  QFile source(resourceOf(sound));
  if (!source.open(QIODevice::ReadOnly)) {
    return QString();
  }
  const QByteArray bytes = source.readAll();
  QFile target(path);
  if (target.size() != bytes.size()) {
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        target.write(bytes) != bytes.size()) {
      return QString();
    }
  }
  return path;
}
#endif

} // namespace

void play(Sound sound) {
#if defined(Q_OS_WIN)
  // PlaySound with SND_ASYNC keeps reading the buffer after it returns, so
  // the bytes have to stay alive: keep them for the life of the program.
  static const QByteArray finished = [] {
    QFile f(resourceOf(Finished));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  }();
  static const QByteArray failed = [] {
    QFile f(resourceOf(Failed));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  }();
  const QByteArray &data = sound == Finished ? finished : failed;
  if (data.isEmpty() ||
      !PlaySoundW(reinterpret_cast<LPCWSTR>(data.constData()), nullptr,
                  SND_MEMORY | SND_ASYNC | SND_NODEFAULT)) {
    QApplication::beep();
  }
#else
  const QString file = soundFile(sound);
  if (!file.isEmpty()) {
    struct Player {
      const char *program;
      QStringList args;
    };
    const Player players[] = {
        {"paplay", {}}, {"pw-play", {}}, {"aplay", {"-q"}}, {"play", {"-q"}}};
    for (const Player &p : players) {
      const QString exe = QStandardPaths::findExecutable(p.program);
      if (!exe.isEmpty()) {
        QProcess::startDetached(exe, p.args + QStringList{file});
        return;
      }
    }
  }
  QApplication::beep();
#endif
}

} // namespace SoundPlayer
