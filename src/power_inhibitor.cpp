#include "power_inhibitor.h"
#include "pch.h"

PowerInhibitor::~PowerInhibitor() {
  if (mCount > 0) {
    stop();
  }
}

void PowerInhibitor::acquire() {
  ++mCount;
  if (mCount == 1) {
    start();
  }
}

void PowerInhibitor::release() {
  if (mCount == 0) {
    return;
  }
  --mCount;
  if (mCount == 0) {
    stop();
  }
}

#if defined(Q_OS_WIN)

void PowerInhibitor::start() {
  SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
}

void PowerInhibitor::stop() { SetThreadExecutionState(ES_CONTINUOUS); }

#else

void PowerInhibitor::start() {
  const QString exe = QStandardPaths::findExecutable("systemd-inhibit");
  if (exe.isEmpty()) {
    return; // no systemd (or no logind): nothing to ask
  }
  mHelper = new QProcess;
  // `cat` waits on its standard input; closing it ends the lock
  mHelper->start(exe, {"--what=sleep", "--who=Rclone Explorer",
                       "--why=Transfers are running", "--mode=block", "cat"});
}

void PowerInhibitor::stop() {
  if (!mHelper) {
    return;
  }
  mHelper->closeWriteChannel();
  if (!mHelper->waitForFinished(1500)) {
    mHelper->kill();
    mHelper->waitForFinished(500);
  }
  delete mHelper;
  mHelper = nullptr;
}

#endif
