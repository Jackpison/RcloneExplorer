#include "stream_widget.h"
#include "theme.h"

StreamWidget::StreamWidget(QProcess *rclone, QProcess *player,
                           const QString &remote, const QString &stream,
                           QWidget *parent)
    : QWidget(parent), mRclone(rclone), mPlayer(player) {
  ui.setupUi(this);
  ui.output->setMaximumBlockCount(5000);
  setAttribute(Qt::WA_StyledBackground);

  ui.remote->setText(remote);
  ui.stream->setText(stream);
  ui.info->setTextFormat(Qt::PlainText); // names may hold < or &
  ui.info->setText(remote);

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
      if (Theme::confirm(this, tr("Stream"), tr("Stop this stream?"),
                         tr("%1 is still streaming.").arg(remote),
                         tr("Stop stream"), tr("Keep streaming"))) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  QObject::connect(mRclone, &QProcess::readyRead, this, [=, this]() {
    while (mRclone->canReadLine()) {
      ui.output->appendPlainText(mRclone->readLine().trimmed());
    }
  });

  QObject::connect(mRclone,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=, this]() {
                     mRclone->deleteLater();
                     mRunning = false;
                     emit finished();
                     emit closed();
                   });

  Theme::setStatus(ui.showDetails, "running");
  ui.showDetails->setText("Streaming");
}

StreamWidget::~StreamWidget() {}

void StreamWidget::cancel() {
  if (!mRunning) {
    return;
  }

  // the card closes once rclone has ended (see "finished")
  mPlayer->terminate();
  mRclone->kill();
}
