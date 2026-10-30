#include "mount_widget.h"
#include "theme.h"
#include "utils.h"

MountWidget::MountWidget(QProcess *process, const QString &remote,
                         const QString &folder, QWidget *parent)
    : QWidget(parent), mProcess(process) {
  ui.setupUi(this);
  ui.output->setMaximumBlockCount(5000);
  setAttribute(Qt::WA_StyledBackground);

  ui.remote->setText(remote);
  ui.folder->setText(folder);
  ui.info->setText(QString("%1 on %2").arg(remote).arg(folder));

  ui.details->setVisible(false);

  ui.output->setVisible(false);

  QObject::connect(
      ui.showDetails, &QToolButton::toggled, this, [=, this](bool checked) {
        ui.details->setVisible(checked);
        ui.showDetails->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
      });

  QObject::connect(
      ui.showOutput, &QToolButton::toggled, this, [=, this](bool checked) {
        ui.output->setVisible(checked);
        ui.showOutput->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
      });

  ui.cancel->setIcon(
      Theme::icon("close"));

  QObject::connect(ui.cancel, &QToolButton::clicked, this, [=, this]() {
    if (mRunning) {
      if (Theme::confirm(this, tr("Unmount"),
#if defined(Q_OS_WIN)
                         tr("Unmount drive %1?").arg(folder),
#else
                         tr("Unmount %1?").arg(folder),
#endif
                         tr("Programs using it will lose access to its files."),
                         tr("Unmount"), tr("Keep mounted"))) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  QObject::connect(mProcess, &QProcess::readyRead, this, [=, this]() {
    while (mProcess->canReadLine()) {
      ui.output->appendPlainText(mProcess->readLine().trimmed());
    }
  });

  QObject::connect(mProcess,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=, this](int status, QProcess::ExitStatus) {
                     mProcess->deleteLater();
                     mRunning = false;
                     if (status == 0) {
                       Theme::setStatus(ui.showDetails, "done");
                       ui.showDetails->setText("Finished");
                     } else {
                       Theme::setStatus(ui.showDetails, "error");
                       ui.showDetails->setText("Error");
                     }
                     ui.cancel->setToolTip("Close");
                     emit finished();
                   });

  Theme::setStatus(ui.showDetails, "running");
  ui.showDetails->setText("Mounted");
}

MountWidget::~MountWidget() {}

void MountWidget::cancel() {
  if (!mRunning) {
    return;
  }

  QString cmd;

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
  QProcess::startDetached("umount", QStringList() << ui.folder->text());
#else
#if defined(Q_OS_WIN32)
  QProcess *p = new QProcess();
  QStringList args;
  args << "rc";
  args << "--url" << QString("http://127.0.0.1:%1/").arg(mRcPort);
  if (!mRcUser.isEmpty()) {
    args << "--user" << mRcUser << "--pass" << mRcPass;
  }
  // requires rclone version at least 1.50
  args << "core/quit";

  UseRclonePassword(p);
  QObject::connect(p,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   p, &QObject::deleteLater);
  p->start(GetRclone(), args, QIODevice::ReadOnly);
#else
  // FUSE 3 systems (what the mount feature needs) only ship fusermount3
  const QString fusermount =
      QStandardPaths::findExecutable("fusermount3").isEmpty()
          ? QStringLiteral("fusermount")
          : QStringLiteral("fusermount3");
  QProcess::startDetached(fusermount, QStringList()
                                          << "-u" << ui.folder->text());
#endif
#endif

  // Do not freeze the window if the unmount is held up (a program still
  // uses the mount): ask rclone to stop, which unmounts as well.
  if (!mProcess->waitForFinished(8000)) {
    mProcess->terminate();
    if (!mProcess->waitForFinished(5000)) {
      mProcess->kill();
    }
  }

  emit closed();
}
