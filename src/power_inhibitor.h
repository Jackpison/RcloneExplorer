#pragma once

#include <QProcess>
#include <memory>

// Keeps the computer from going to sleep while there is work to finish
// (transfers). Calls nest: the computer may sleep again once every acquire()
// has been matched by a release().
//   Windows: SetThreadExecutionState
//   Linux:   a `systemd-inhibit` helper process that lives as long as the
//            lock is wanted (it ends on its own if this program dies), or,
//            where that is missing (a Flatpak, a system without systemd), the
//            desktop portal's Inhibit call
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
  // The portal's answer arrives later (the window never waits for it): the
  // handle of the lock, and whether the lock is still wanted by then.
  struct Portal {
    QString handle;
    bool wanted = false;
    bool alive = true;
  };
  std::shared_ptr<Portal> mPortal = std::make_shared<Portal>();
#endif
  void start();
  void stop();
};
