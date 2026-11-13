#include "progress_dialog.h"
#include "theme.h"

ProgressDialog::ProgressDialog(const QString &title, const QString &operation,
                               const QString &message, QProcess *process,
                               QWidget *parent, bool close, bool trim)
    : QDialog(parent) {
  ui.setupUi(this);
  setMinimumWidth(460);
  resize(width(), 0);

  setWindowTitle(title);
  ui.labelOperation->setText(operation);
  ui.labelInfo->setTextFormat(Qt::PlainText); // holds remote paths
  ui.labelInfo->setText(message);

  ui.output->setVisible(false);

  QObject::connect(ui.buttonBox, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);

  QObject::connect(ui.buttonShowOutput, &QPushButton::toggled, this,
                   [=, this](bool checked) {
                     ui.output->setVisible(checked);
                     ui.buttonShowOutput->setArrowType(
                         checked ? Qt::DownArrow : Qt::RightArrow);
                     if (checked) {
                       // the dialog starts at its minimum height; give the
                       // log real room instead of squeezing it
                       ui.output->setMinimumHeight(260);
                       resize(qMax(width(), 680), qMax(height(), 440));
                     } else {
                       ui.output->setMinimumHeight(0);
                       adjustSize();
                     }
                   });

  QObject::connect(process,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=, this](int code, QProcess::ExitStatus status) {
                     if (status == QProcess::NormalExit && code == 0) {
                       if (close) {
                         emit accept();
                       }
                     } else {
                       ui.buttonShowOutput->setChecked(true);
                       ui.buttonBox->setEnabled(true);
                     }
                   });

  QObject::connect(process, &QProcess::readyRead, this, [=, this]() {
    QString output = QString::fromUtf8(process->readAll());
    if (trim) {
      output = output.trimmed();
    }
    ui.output->appendPlainText(output);
    emit outputAvailable(output);
  });

  process->setProcessChannelMode(QProcess::MergedChannels);
  process->start(QIODevice::ReadOnly);
}

ProgressDialog::~ProgressDialog() {}

void ProgressDialog::expand() { ui.buttonShowOutput->setChecked(true); }

void ProgressDialog::allowToClose() { ui.buttonBox->setEnabled(true); }
//
// QString ProgressDialog::getOutput() const
//{
//    return ui.output->toPlainText();
//}
