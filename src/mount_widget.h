#pragma once

#include "pch.h"
#include "ui_mount_widget.h"

class MountWidget : public QWidget {
  Q_OBJECT

public:
  MountWidget(QProcess *process, const QString &remote, const QString &folder,
              QWidget *parent = nullptr);
  ~MountWidget();

  // Windows: how to reach this mount's authenticated remote-control server
  // so it can be asked to quit (the only clean way to unmount a WinFsp drive).
  void setRemoteControl(int port, const QString &user, const QString &pass) {
    mRcPort = port;
    mRcUser = user;
    mRcPass = pass;
  }

public slots:
  // `wait`: block until the mount has ended (when quitting, so nothing is
  // left mounted); otherwise the card closes by itself once it has.
  void cancel(bool wait = false);

signals:
  void finished();
  void closed();

private:
  Ui::MountWidget ui;

  bool mRunning = true;
  bool mClosing = false; // unmount requested: close the card once it has ended
  QProcess *mProcess;
  int mRcPort = 0;
  QString mRcUser;
  QString mRcPass;
};
