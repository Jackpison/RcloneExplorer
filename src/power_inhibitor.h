#pragma once

#include <QProcess>

// Keeps the computer from going to sleep while there is work to finish
// (transfers). Calls nest: the computer may sleep again once every acquire()
// has been matched by a release().
//   Windows: SetThreadExecutionState
//   Linux:   a `systemd-inhibit` helper process that lives as long as the
//            lock is wanted (it ends on its own if this program dies)
class PowerInhibitor {
public:
  PowerInhibitor() = default;
  ~PowerInhibitor();

  void acquire();
  void release();
  bool active() const { return mCount > 0; }

private:
  int mCount = 0;
#if !defined(Q_OS_WIN)
  QProcess *mHelper = nullptr;
#endif
  void start();
  void stop();
};
