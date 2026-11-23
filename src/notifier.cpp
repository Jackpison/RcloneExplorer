#include "notifier.h"
#include "utils.h"

#if !defined(Q_OS_WIN)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

Notifier::Notifier(QSystemTrayIcon *tray, std::function<bool()> keepTray, QObject *parent)
    : QObject(parent), mTray(tray), mKeepTray(std::move(keepTray)) {}

void Notifier::notify(const QString &title, const QString &text, bool problem) {
#if !defined(Q_OS_WIN)
  const Message m{title, text, problem};
  if (mMarkup == Markup::Yes || mMarkup == Markup::No) {
    send(m);
    return;
  }
  mWaiting << m;
  if (mMarkup == Markup::Asking) {
    return;
  }
  mMarkup = Markup::Asking;
  const QDBusMessage ask = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("GetCapabilities"));
  auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(ask), this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
    watcher->deleteLater();
    const QDBusPendingReply<QStringList> reply(*watcher);
    // no answer: assume markup, as most services read it (escaped text is
    // still readable where they do not)
    mMarkup = reply.isError() || reply.value().contains(QStringLiteral("body-markup"))
                  ? Markup::Yes
                  : Markup::No;
    const QList<Message> waiting = std::exchange(mWaiting, {});
    for (const Message &w : waiting) {
      send(w);
    }
  });
#else
  viaTray(title, text, problem);
#endif
}

#if !defined(Q_OS_WIN)
void Notifier::send(const Message &m) {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
  const QString icon = IsFlatpak() ? qEnvironmentVariable("FLATPAK_ID") : QStringLiteral("rclone-explorer");
  // a name such as "<b>.txt" must not be read as markup
  const QString body = mMarkup == Markup::No ? m.text : m.text.toHtmlEscaped();
  call << QStringLiteral("Rclone Explorer") << uint(0) << icon << m.title << body << QStringList()
       << QVariantMap{{"urgency", QVariant::fromValue(uchar(m.problem ? 2 : 1))}} << int(-1);
  auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, m]() {
    watcher->deleteLater();
    if (QDBusPendingReply<uint>(*watcher).isError()) {
      viaTray(m.title, m.text, m.problem); // no notification service
    }
  });
}
#endif

void Notifier::viaTray(const QString &title, const QString &text, bool problem) {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    return;
  }
  // shown for these messages (not by the user's choice): hidden again once
  // the last of them is gone
  if (mShown > 0 || !mTray->isVisible()) {
    mTray->show();
    ++mShown;
    QTimer::singleShot(10000, this, [this]() {
      if (--mShown == 0 && !mKeepTray()) {
        mTray->hide();
      }
    });
  }
  mTray->showMessage(title, text,
                     problem ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information);
}
