#include "mount_dialog.h"
#include "theme.h"
#include "utils.h"

#if defined(Q_OS_WIN)
namespace {
// First drive letter (from Z down) that is not in use.
QString freeDriveLetter() {
  for (char c = 'Z'; c >= 'D'; --c) {
    const QString drive = QString("%1:/").arg(QChar(c));
    if (!QDir(drive).exists()) {
      return QString("%1:").arg(QChar(c));
    }
  }
  return "Z:";
}
} // namespace
#endif

MountDialog::MountDialog(const QString &remote, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Mount %1").arg(remote));
  setMinimumWidth(460);

  auto settings = GetSettings();
  auto *form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  mPath = new QLineEdit;
#if defined(Q_OS_WIN)
  mPath->setText(freeDriveLetter());
  mPath->setToolTip(tr("A free drive letter, for example Z:"));
  form->addRow(tr("Drive"), mPath);
#else
  mPath->setPlaceholderText(tr("An empty folder"));
  auto *browse = new QToolButton;
  browse->setText(tr("Browse…"));
  QObject::connect(browse, &QToolButton::clicked, this, [this, remote]() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Mount %1").arg(remote), mPath->text());
    if (!dir.isEmpty()) {
      mPath->setText(dir);
    }
  });
  auto *row = new QHBoxLayout;
  row->addWidget(mPath, 1);
  row->addWidget(browse);
  form->addRow(tr("Mount point"), row);
#endif

  mCache = new QComboBox;
  mCache->addItem(tr("Writes (recommended)"), "writes");
  mCache->addItem(tr("Full (best for apps and media players)"), "full");
  mCache->addItem(tr("Minimal"), "minimal");
  mCache->addItem(tr("Off (read-mostly, files cannot be edited in place)"),
                  "off");
  mCache->setToolTip(
      tr("Which files rclone keeps in a local cache. Without caching, many "
         "programs cannot save to the mount.\nWrites: files you change are "
         "cached.\nFull: everything you open is cached."));
  const int ci = mCache->findData(
      settings->value("Mount/cacheMode", "writes").toString());
  mCache->setCurrentIndex(ci >= 0 ? ci : 0);
  form->addRow(tr("Cache"), mCache);

  mReadOnly = new QCheckBox(tr("Read-only"));
  mReadOnly->setChecked(settings->value("Mount/readOnly", false).toBool());
  form->addRow(QString(), mReadOnly);

#if defined(Q_OS_WIN)
  mVolume = new QLineEdit(settings->value("Mount/volumeName").toString());
  mVolume->setPlaceholderText(tr("Shown in File Explorer (optional)"));
  form->addRow(tr("Drive name"), mVolume);
#endif

  mExtra = new QLineEdit(settings->value("Mount/extra").toString());
  mExtra->setPlaceholderText(tr("Other rclone options (optional)"));
  form->addRow(tr("More options"), mExtra);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Mount"));
  QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                   &MountDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);

  auto *layout = new QVBoxLayout(this);
#if defined(Q_OS_WIN)
  auto *note = new QLabel(tr("Needs WinFsp to be installed."));
  note->setProperty("secondary", true);
  layout->addWidget(note);
#endif
  layout->addLayout(form);
  layout->addWidget(buttons);
}

RcloneCommands::MountOptions MountDialog::options() const {
  RcloneCommands::MountOptions o;
  o.cacheMode = mCache->currentData().toString();
  o.readOnly = mReadOnly->isChecked();
  o.volumeName = mVolume ? mVolume->text() : QString();
  o.extra = mExtra->text();
  return o;
}

void MountDialog::accept() {
  QString target = mPath->text().trimmed();
#if defined(Q_OS_WIN)
  // "z", "Z:" and "Z:\" all mean the drive Z:
  static const QRegularExpression drive(R"(^([A-Za-z]):?[\\/]?$)");
  const auto m = drive.match(target);
  if (!m.hasMatch()) {
    QMessageBox::warning(this, windowTitle(),
                         tr("Enter a drive letter such as Z:"));
    return;
  }
  target = m.captured(1).toUpper() + ":";
  if (QDir(target + "/").exists()) {
    QMessageBox::warning(this, windowTitle(),
                         tr("Drive %1 is already in use. Choose another "
                            "letter.").arg(target));
    return;
  }
#else
  if (target.isEmpty()) {
    QMessageBox::warning(this, windowTitle(),
                         tr("Choose the folder to mount into."));
    return;
  }
#endif
  mTarget = target;

  auto settings = GetSettings();
  settings->setValue("Mount/cacheMode", mCache->currentData());
  settings->setValue("Mount/readOnly", mReadOnly->isChecked());
  if (mVolume) {
    settings->setValue("Mount/volumeName", mVolume->text());
  }
  settings->setValue("Mount/extra", mExtra->text());
  QDialog::accept();
}
