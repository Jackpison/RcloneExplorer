#include "tool_dialog.h"
#include "remote_folder_dialog.h"
#include "rclone_commands.h"
#include "theme.h"
#include "utils.h"

ToolDialog::ToolDialog(Tool tool, const QString &source, QWidget *parent)
    : QDialog(parent), mTool(tool), mSource(source) {
  setMinimumWidth(480);

  auto *layout = new QVBoxLayout(this);
  auto *form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto *info = new QLabel;
  info->setWordWrap(true);
  info->setProperty("secondary", true);

  auto addOther = [&](const QString &label) {
    mOther = new QLineEdit;
    mOther->setPlaceholderText(tr("remote:folder  or a folder on this PC"));
    auto *completer = new QCompleter(ListRemoteNames(), mOther);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    mOther->setCompleter(completer);
    auto *browseRemote = new QToolButton;
    browseRemote->setText(tr("Remote…"));
    browseRemote->setToolTip(tr("Pick a folder on one of your remotes"));
    QObject::connect(browseRemote, &QToolButton::clicked, this, [this]() {
      RemoteFolderDialog picker(mOther->text().trimmed(), this);
      if (picker.exec() == QDialog::Accepted) {
        mOther->setText(picker.selected());
      }
    });
    auto *browseLocal = new QToolButton;
    browseLocal->setText(tr("This PC…"));
    browseLocal->setToolTip(tr("Pick a folder on this PC"));
    QObject::connect(browseLocal, &QToolButton::clicked, this, [this]() {
      const QString dir = QFileDialog::getExistingDirectory(this, windowTitle());
      if (!dir.isEmpty()) {
        mOther->setText(QDir::toNativeSeparators(dir));
      }
    });
    auto *row = new QHBoxLayout;
    row->addWidget(mOther, 1);
    row->addWidget(browseRemote);
    row->addWidget(browseLocal);
    form->addRow(label, row);
  };

  auto *from = new QLineEdit(source);
  from->setReadOnly(true);
  QString okText = tr("OK");

  switch (tool) {
  case Compare:
    setWindowTitle(tr("Compare folders"));
    okText = tr("Compare");
    info->setText(tr("Checks that both places hold the same files. Nothing "
                     "is changed; differences are listed in the job output."));
    form->addRow(tr("This folder"), from);
    addOther(tr("Compare with"));
    mOptionA = new QCheckBox(tr("Only report files missing or different in "
                                "the other folder"));
    mOptionB = new QCheckBox(tr("Compare sizes only (faster)"));
    form->addRow(QString(), mOptionA);
    form->addRow(QString(), mOptionB);
    break;
  case Dedupe:
    setWindowTitle(tr("Remove duplicates"));
    okText = tr("Remove duplicates");
    info->setText(tr("Finds files with the same name in the same folder, as "
                     "Google Drive allows. Choose which copy to keep."));
    form->addRow(tr("Folder"), from);
    mMode = new QComboBox;
    mMode->addItem(tr("Keep the newest"), "newest");
    mMode->addItem(tr("Keep the oldest"), "oldest");
    mMode->addItem(tr("Keep the largest"), "largest");
    mMode->addItem(tr("Keep the smallest"), "smallest");
    mMode->addItem(tr("Keep the first, delete the rest"), "first");
    mMode->addItem(tr("Rename them, delete nothing"), "rename");
    form->addRow(tr("Duplicates"), mMode);
    break;
  case CopyTo:
    setWindowTitle(tr("Copy to another remote"));
    okText = tr("Copy");
    info->setText(tr("Copies between two places. Within one provider rclone "
                     "copies on the server without downloading anything."));
    form->addRow(tr("From"), from);
    addOther(tr("To"));
    mOptionA = new QCheckBox(tr("Move instead: delete files from here "
                                "after they are copied"));
    form->addRow(QString(), mOptionA);
    break;
  case Bisync:
    setWindowTitle(tr("Two-way sync"));
    okText = tr("Sync");
    info->setText(tr("Makes both folders match: changes on either side are "
                     "copied to the other, and deletions are carried over. "
                     "The first run between two folders needs “First run”."));
    form->addRow(tr("This folder"), from);
    addOther(tr("Sync with"));
    mOptionA = new QCheckBox(tr("First run (build the starting point)"));
    mOptionB = new QCheckBox(tr("Dry run: show what would happen"));
    mOptionB->setChecked(true);
    form->addRow(QString(), mOptionA);
    form->addRow(QString(), mOptionB);
    break;
  }

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(okText);
  QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                   &ToolDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);

  layout->addWidget(info);
  layout->addLayout(form);
  layout->addWidget(buttons);
}

void ToolDialog::accept() {
  using namespace RcloneCommands;
  if (mOther) {
    mDest = mOther->text().trimmed();
    if (mDest.isEmpty()) {
      QMessageBox::warning(this, windowTitle(),
                           tr("Enter the other folder first."));
      return;
    }
    if (mDest == mSource) {
      QMessageBox::warning(this, windowTitle(),
                           tr("Choose a different folder."));
      return;
    }
  }

  switch (mTool) {
  case Compare: {
    CheckOptions o;
    o.oneWay = mOptionA->isChecked();
    o.sizeOnly = mOptionB->isChecked();
    mArgs = check(mSource, mDest, o);
    mMessage = tr("Compare %1").arg(mSource);
    break;
  }
  case Dedupe: {
    const QString mode = mMode->currentData().toString();
    if (mode != "rename" &&
        !Theme::confirm(this, tr("Remove duplicates"),
                        tr("Delete duplicate files in %1?").arg(mSource),
                        tr("The copies you do not keep are deleted. Some "
                           "providers keep deleted files in a trash."),
                        tr("Remove duplicates"), tr("Cancel"))) {
      return;
    }
    mArgs = dedupe(mSource, mode);
    mMessage = tr("Remove duplicates in %1").arg(mSource);
    mDest.clear();
    break;
  }
  case CopyTo: {
    const bool move = mOptionA->isChecked();
    if (move && !Theme::confirm(
                    this, tr("Move files"),
                    tr("Move files from %1 to %2?").arg(mSource, mDest),
                    tr("Files are deleted from %1 after they are copied.")
                        .arg(mSource),
                    tr("Move"), tr("Cancel"))) {
      return;
    }
    mArgs = copy(mSource, mDest, move);
    mMessage = (move ? tr("Move %1") : tr("Copy %1")).arg(mSource);
    break;
  }
  case Bisync: {
    BisyncOptions o;
    o.resync = mOptionA->isChecked();
    o.dryRun = mOptionB->isChecked();
    mArgs = bisync(mSource, mDest, o);
    mMessage = tr("Two-way sync %1").arg(mSource);
    break;
  }
  }
  QDialog::accept();
}
