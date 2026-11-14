#include "power_inhibitor.h"
#include "pch.h"

#if !defined(Q_OS_WIN)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

PowerInhibitor::~PowerInhibitor() {
  if (mCount > 0) {
    stop();
  }
#if !defined(Q_OS_WIN)
  mPortal->alive = false; // an answer still on its way finds nobody
#endif
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

namespace {
const char *kPortal = "org.freedesktop.portal.Desktop";
const char *kPortalPath = "/org/freedesktop/portal/desktop";

// Ends a portal lock (closing its request), without waiting for an answer.
void closePortalRequest(const QString &handle) {
  QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
      kPortal, handle, "org.freedesktop.portal.Request", "Close"));
}
} // namespace

void PowerInhibitor::start() {
  const QString exe = QStandardPaths::findExecutable("systemd-inhibit");
  if (exe.isEmpty()) {
    // the desktop portal: Inhibit(window, flags, options) -> request handle,
    // asked without waiting (the portal may first have to be started)
    QDBusMessage call = QDBusMessage::createMethodCall(
        kPortal, kPortalPath, "org.freedesktop.portal.Inhibit", "Inhibit");
    constexpr uint kSuspend = 4;
    call << QString() << kSuspend
         << QVariantMap{{"reason", QStringLiteral("Transfers are running")}};
    mPortal->wanted = true;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call));
    std::shared_ptr<Portal> portal = mPortal;
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [portal](QDBusPendingCallWatcher *w) {
                       w->deleteLater();
                       const QDBusPendingReply<QDBusObjectPath> reply = *w;
                       if (reply.isError()) {
                         return; // no portal either: nothing to ask
                       }
                       const QString handle = reply.value().path();
                       if (portal->alive && portal->wanted) {
                         portal->handle = handle;
                       } else {
                         closePortalRequest(handle); // no longer wanted meanwhile
                       }
                     });
    return;
  }
  mHelper = new QProcess;
  // `cat` waits on its standard input; closing it ends the lock
  mHelper->start(exe, {"--what=sleep", "--who=Rclone Explorer",
                       "--why=Transfers are running", "--mode=block", "cat"});
}

void PowerInhibitor::stop() {
  mPortal->wanted = false;
  if (!mPortal->handle.isEmpty()) {
    closePortalRequest(mPortal->handle); // closing the request ends the lock
    mPortal->handle.clear();
  }
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
