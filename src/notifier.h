#pragma once

#include "pch.h"

// Desktop notifications (a transfer finished, a mirror was checked...).
//   Linux:   the desktop's notification service over D-Bus, without waiting
//            for it; the tray icon if there is no such service
//   Windows: a tray balloon. Windows shows one only while the tray icon is
//            visible, so a hidden icon is shown for as long as the message is.
class Notifier : public QObject {
  Q_OBJECT

public:
  // `keepTray`: whether the user wants the tray icon shown anyway
  Notifier(QSystemTrayIcon *tray, std::function<bool()> keepTray, QObject *parent = nullptr);

  // `text` is plain text (file names may hold < or &)
  void notify(const QString &title, const QString &text, bool problem = false);

private:
  QSystemTrayIcon *mTray;
  std::function<bool()> mKeepTray;
  int mShown = 0; // balloons on screen that showed the icon for themselves
#if !defined(Q_OS_WIN)
  // Whether the service reads the body as markup (then < and & are escaped):
  // asked once, in the background; messages wait for the answer.
  enum class Markup { Unknown, Asking, Yes, No };
  Markup mMarkup = Markup::Unknown;
  struct Message {
    QString title, text;
    bool problem;
  };
  QList<Message> mWaiting;
  void send(const Message &m);
#endif
  void viaTray(const QString &title, const QString &text, bool problem);
};
