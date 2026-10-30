#include "remote_folder_dialog.h"
#include "natural_sort.h"
#include "rclone_output.h"
#include "theme.h"
#include "utils.h"

namespace {
enum { PathRole = Qt::UserRole, PlaceholderRole = Qt::UserRole + 1 };

QString joinPath(const QString &parent, const QString &name) {
  return parent.isEmpty() ? name : parent + "/" + name;
}
} // namespace

RemoteFolderDialog::RemoteFolderDialog(const QString &start, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Choose a folder on a remote"));
  resize(480, 520);

  auto *layout = new QVBoxLayout(this);

  mRemote = new QComboBox;
  const QStringList remotes = ListRemoteNames();
  for (const QString &name : remotes) {
    mRemote->addItem(name.chopped(1), name); // shown without the colon
  }
  auto *form = new QFormLayout;
  form->addRow(tr("Remote"), mRemote);
  layout->addLayout(form);

  mTree = new QTreeWidget;
  mTree->setHeaderHidden(true);
  mTree->setUniformRowHeights(true);
  layout->addWidget(mTree, 1);

  mHint = new QLabel;
  mHint->setWordWrap(true);
  mHint->setProperty("secondary", true);
  layout->addWidget(mHint);

  mPath = new QLineEdit;
  mPath->setPlaceholderText(tr("Folder (empty for the top level)"));
  mPath->setClearButtonEnabled(true);
  auto *pathForm = new QFormLayout;
  pathForm->addRow(tr("Folder"), mPath);
  layout->addLayout(pathForm);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  mOk = buttons->button(QDialogButtonBox::Ok);
  mOk->setText(tr("Select"));
  QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  QObject::connect(mRemote, &QComboBox::currentIndexChanged, this,
                   [this]() { setRemote(); });
  QObject::connect(mTree, &QTreeWidget::itemExpanded, this,
                   [this](QTreeWidgetItem *item) {
                     if (item->childCount() == 1 &&
                         item->child(0)->data(0, PlaceholderRole).toBool()) {
                       load(item);
                     }
                   });
  QObject::connect(mTree, &QTreeWidget::currentItemChanged, this,
                   [this](QTreeWidgetItem *item) {
                     if (item && !item->data(0, PlaceholderRole).toBool()) {
                       mPath->setText(item->data(0, PathRole).toString());
                     }
                   });

  if (remotes.isEmpty()) {
    mHint->setText(tr("No remotes are configured yet."));
    mOk->setEnabled(false);
    mRemote->setEnabled(false);
    mPath->setEnabled(false);
    return;
  }

  // "remote:folder" opens that remote with the folder filled in
  QString startPath;
  const int colon = start.indexOf(':');
  if (colon > 0) {
    const int i = mRemote->findData(start.left(colon + 1));
    if (i >= 0) {
      mRemote->setCurrentIndex(i);
      startPath = start.mid(colon + 1);
    }
  }
  setRemote();
  mPath->setText(startPath);
}

QString RemoteFolderDialog::selected() const {
  QString path = mPath->text().trimmed();
  while (path.startsWith('/')) {
    path.remove(0, 1);
  }
  return mRemote->currentData().toString() + path;
}

QTreeWidgetItem *RemoteFolderDialog::makeFolderItem(const QString &name,
                                                    const QString &path) {
  auto *item = new QTreeWidgetItem;
  item->setText(0, name);
  item->setIcon(0, Theme::icon("folder"));
  item->setData(0, PathRole, path);
  // a folder shows its arrow until we know it is empty
  auto *placeholder = new QTreeWidgetItem(item);
  placeholder->setText(0, tr("Loading…"));
  placeholder->setData(0, PlaceholderRole, true);
  placeholder->setFlags(Qt::NoItemFlags);
  return item;
}

void RemoteFolderDialog::stopLoading() {
  const auto running = mRunning;
  for (QProcess *p : running) {
    p->disconnect(this);
    p->kill();
    p->deleteLater();
  }
  mRunning.clear();
}

void RemoteFolderDialog::setRemote() {
  stopLoading();
  mTree->clear();
  mPath->clear();
  mHint->clear();
  auto *root = makeFolderItem(tr("Top level"), QString());
  mTree->addTopLevelItem(root);
  mTree->setCurrentItem(root);
  root->setExpanded(true); // lists it
}

void RemoteFolderDialog::load(QTreeWidgetItem *item) {
  const QString remote = mRemote->currentData().toString();
  const QString path = item->data(0, PathRole).toString();
  // the item can disappear (another remote chosen) while rclone is running
  const QPersistentModelIndex where(mTree->indexFromItem(item));

  auto *p = new QProcess(this);
  mRunning.append(p);
  UseRclonePassword(p);
  p->setProcessChannelMode(QProcess::SeparateChannels);

  QObject::connect(
      p, &QProcess::finished, this,
      [this, p, where, path](int status, QProcess::ExitStatus) {
        mRunning.removeAll(p);
        p->deleteLater();
        QTreeWidgetItem *item = mTree->itemFromIndex(where);
        if (!item) {
          return;
        }
        bool parsed = false;
        const auto entries =
            RcloneOutput::parseLsJson(p->readAllStandardOutput(), &parsed);
        qDeleteAll(item->takeChildren());

        if (status != 0 || !parsed) {
          const QString err = QString::fromUtf8(p->readAllStandardError())
                                  .trimmed()
                                  .section('\n', -1);
          mHint->setText(tr("Could not list this folder. %1").arg(err));
          item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
          return;
        }
        mHint->clear();

        QStringList names;
        for (const auto &e : entries) {
          if (e.isDir) {
            names << e.name;
          }
        }
        std::stable_sort(names.begin(), names.end(),
                         [](const QString &a, const QString &b) {
                           return naturalCompare(a, b) < 0;
                         });
        for (const QString &name : names) {
          item->addChild(makeFolderItem(name, joinPath(path, name)));
        }
        if (names.isEmpty()) {
          item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        }
      });

  p->start(GetRclone(),
           QStringList() << "lsjson" << "--dirs-only" << "--no-mimetype"
                         << GetRcloneConf() << GetShowHidden()
                         << GetDefaultRcloneOptionsList() << remote + path,
           QIODevice::ReadOnly);
}
