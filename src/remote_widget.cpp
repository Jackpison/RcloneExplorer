#include "remote_widget.h"
#include "mount_dialog.h"
#include "rclone_commands.h"
#include "tool_dialog.h"
#include "theme.h"
#include "rclone_output.h"
#include "export_dialog.h"
#include "item_model.h"
#include "list_of_job_options.h"
#include "progress_dialog.h"
#include "transfer_dialog.h"
#include "utils.h"

RemoteWidget::RemoteWidget(const QString &remote,
                           bool isLocal, bool isGoogle, QWidget *parent)
    : QWidget(parent) {
  ui.setupUi(this);

QString root = isLocal ? "/" : QString();

#ifndef Q_OS_WIN
  isLocal = false;
#endif

  auto settings = GetSettings();
  QString rcloneVersion = settings->value("Settings/rcloneVersion").toString();
  settings->setValue("Settings/driveShared", Qt::Unchecked);
  ui.tree->setAlternatingRowColors(
      settings->value("Settings/rowColors", false).toBool());
  ui.checkBoxShared->setChecked(false);
  ui.checkBoxShared->setDisabled(!isGoogle);
  // hide checkBoxShared for non Google remotes
  if (!isGoogle) {
    ui.checkBoxShared->hide();
  }

  ui.buttonRefresh->setDefaultAction(ui.refresh);
  ui.buttonMkdir->setDefaultAction(ui.mkdir);
  ui.buttonRename->setDefaultAction(ui.rename);
  ui.buttonMove->setDefaultAction(ui.move);
  ui.buttonPurge->setDefaultAction(ui.purge);
  ui.buttonMount->setDefaultAction(ui.mount);
  ui.buttonStream->setDefaultAction(ui.stream);
  ui.buttonUpload->setDefaultAction(ui.upload);
  ui.buttonDownload->setDefaultAction(ui.download);
  ui.buttonTree->setDefaultAction(ui.getTree);
  ui.buttonLink->setDefaultAction(ui.link);
  ui.buttonSize->setDefaultAction(ui.getSize);
  ui.buttonExport->setDefaultAction(ui.export_);

  ui.tree->sortByColumn(0, Qt::AscendingOrder);
  ui.tree->header()->setSectionsMovable(false);

  ItemModel *model = new ItemModel(remote, this);
  ui.tree->setModel(model);
  QTimer::singleShot(0, ui.tree, SLOT(setFocus()));
  buildFluentUi(model, remote, isLocal);

  QObject::connect(model, &QAbstractItemModel::layoutChanged, this, [=, this]() {
    ui.tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < model->columnCount(QModelIndex()); ++c) {
      ui.tree->resizeColumnToContents(c);
    }
  });

  QObject::connect(
      ui.tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [=, this](const QItemSelection &selection) {
        // only this widget's own actions, not those inside child widgets
        // (e.g. the filter box's clear button)
        for (auto child :
             findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly)) {
          child->setDisabled(selection.isEmpty());
        }

        if (selection.isEmpty()) {
          ui.path->clear();
          return;
        }

        QModelIndex index = selection.indexes().front();

        bool topLevel = model->isTopLevel(index);
        bool isFolder = model->isFolder(index);

        QDir path;
        if (model->isLoading(index)) {
          ui.refresh->setDisabled(true);
          ui.move->setDisabled(true);
          ui.rename->setDisabled(true);
          ui.purge->setDisabled(true);
          ui.mount->setDisabled(true);
          ui.stream->setDisabled(true);
          ui.upload->setDisabled(true);
          ui.download->setDisabled(true);
          ui.checkBoxShared->setDisabled(true);
          path = model->path(model->parent(index));
        } else {
          ui.refresh->setDisabled(false);
          bool driveShared = ui.checkBoxShared->checkState();
          ui.mkdir->setDisabled(driveShared);
          ui.rename->setDisabled(topLevel || driveShared);
          ui.move->setDisabled(topLevel || driveShared);
          ui.purge->setDisabled(topLevel || driveShared);
          ui.upload->setDisabled(driveShared);

#if defined(Q_OS_WIN32)
          // check if required version
          unsigned int result =
              compareVersion(rcloneVersion.toStdString(), "1.50");
          if (result == 2) {
            ui.mount->setDisabled(true);
          } else {
            ui.mount->setDisabled(!isFolder);
          };
#else
// mount is not supported by rclone on these systems
#if defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
          ui.mount->setDisabled(true);
#else
          ui.mount->setDisabled(!isFolder);
#endif
#endif

          ui.stream->setDisabled(isFolder);
          ui.checkBoxShared->setDisabled(!isGoogle);
          path = model->path(index);
        }

        ui.getSize->setDisabled(!isFolder);
        ui.getTree->setDisabled(!isFolder);
        ui.export_->setDisabled(!isFolder);
        ui.path->setText(isLocal ? QDir::toNativeSeparators(path.path())
                                 : path.path());
      });

  QObject::connect(ui.refresh, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    model->refresh(index);
  });

  QObject::connect(ui.mkdir, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    if (!model->isFolder(index)) {
      index = index.parent();
    }
    QDir path = model->path(index);
    QString pathMsg =
        isLocal ? QDir::toNativeSeparators(path.path()) : path.path();
    QString name = QInputDialog::getText(
        this, "New Folder", QString("Create folder in %1").arg(pathMsg));
    if (!name.isEmpty()) {
      QString folder = path.filePath(name);
      QString folderMsg = isLocal ? QDir::toNativeSeparators(folder) : folder;

      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(QStringList() << "mkdir" << GetRcloneConf()
                                         << GetDriveSharedWithMe()
                                         << GetDefaultRcloneOptionsList()
                                         << remote + ":" + folder);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("New Folder", "Creating...", folderMsg, &process,
                              this);
      if (progress.exec() == QDialog::Accepted) {
        model->refresh(index);
      }
    }
  });

  QObject::connect(ui.rename, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QString name = model->data(index, Qt::DisplayRole).toString();
    name = QInputDialog::getText(this, "Rename",
                                 QString("New name for %1").arg(pathMsg),
                                 QLineEdit::Normal, name);
    if (!name.isEmpty()) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(
          QStringList() << "moveto" << GetRcloneConf() << GetDriveSharedWithMe()
                        << GetDefaultRcloneOptionsList() << remote + ":" + path
                        << remote + ":" +
                               model->path(index.parent()).filePath(name));
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Rename", "Renaming...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        model->rename(index, name);
      }
    }
  });

  QObject::connect(ui.move, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QString name = model->path(index.parent()).path() + "/";
    name = QInputDialog::getText(this, "Move",
                                 QString("New location for %1").arg(pathMsg),
                                 QLineEdit::Normal, name);
    if (!name.isEmpty()) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(
          QStringList() << "move" << GetRcloneConf() << GetDriveSharedWithMe()
                        << GetDefaultRcloneOptionsList() << remote + ":" + path
                        << remote + ":" + name);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Move", "Moving...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        model->refresh(index);
      }
    }
  });

  QObject::connect(ui.purge, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    const bool folder = model->isFolder(index);
    const QString name = model->data(index.siblingAtColumn(0), Qt::DisplayRole).toString();
    QMessageBox confirm(QMessageBox::Warning, tr("Delete"),
                        tr("Delete \u201c%1\u201d?").arg(name), QMessageBox::NoButton, this);
    confirm.setInformativeText(
        folder ? tr("The folder and everything in it will be deleted from %1. "
                    "This cannot be undone.").arg(remote)
               : tr("The file will be deleted from %1. This cannot be undone.").arg(remote));
    QPushButton *del = confirm.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    del->setProperty("destructive", true);
    del->style()->unpolish(del); // re-apply the style sheet with the new property
    del->style()->polish(del);
    QPushButton *cancel = confirm.addButton(QMessageBox::Cancel);
    confirm.setDefaultButton(cancel); // Enter must not delete by accident
    confirm.exec();
    if (confirm.clickedButton() == del) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      // deletefile removes exactly one file with one API call. "delete" is a
      // bulk filter command: it lists the parent folder (recursively with
      // --fast-list) just to find the file, which can take minutes.
      process.setArguments(QStringList()
                           << (folder ? "purge" : "deletefile")
                           << GetRcloneConf() << GetDriveSharedWithMe()
                           << GetDefaultRcloneOptionsList()
                           << remote + ":" + path);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Delete", "Deleting...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        QModelIndex parent = index.parent();
        QModelIndex next = parent.model()->index(index.row() + 1, 0);
        ui.tree->selectionModel()->select(next.isValid() ? next : parent,
                                          QItemSelectionModel::SelectCurrent);
        model->removeRow(index.row(), parent);
      }
    }
  });

  QObject::connect(ui.mount, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    MountDialog dialog(remote + ":" + path, this);
    if (dialog.exec() == QDialog::Accepted) {
      emit addMount(remote + ":" + path, dialog.target(),
                    RcloneCommands::mountArgs(
                        dialog.options(),
                        settings->value("Settings/mount").toString()));
    }
  });

  QObject::connect(ui.stream, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QString path = model->path(index).path();

    bool streamConfirmed =
        settings->value("Settings/streamConfirmed", false).toBool();
    QString stream = settings->value("Settings/stream", "mpv -").toString();
    if (!streamConfirmed) {
      QString result = QInputDialog::getText(
          this, "Stream",
          "Enter stream command (file will be passed in STDIN):",
          QLineEdit::Normal, stream);
      if (result.isEmpty()) {
        return;
      }

      stream = result;

      settings->setValue("Settings/stream", stream);
      settings->setValue("Settings/streamConfirmed", true);
    }

    emit addStream(remote + ":" + path, stream);
  });

  QObject::connect(ui.checkBoxShared, &QCheckBox::toggled, ui.shared,
                   &QAction::toggled);

  QObject::connect(ui.shared, &QAction::toggled, this, [=, this](const bool checked) {
    auto settings = GetSettings();
    settings->setValue("Settings/driveShared", checked);
    ui.checkBoxShared->setChecked(checked);

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QModelIndex top = index;
    while (!model->isTopLevel(top)) {
      top = top.parent();
    }
    ui.tree->selectionModel()->clear();
    ui.tree->selectionModel()->select(top, QItemSelectionModel::Select |
                                               QItemSelectionModel::Rows);
    model->refresh(top);
  });

  QObject::connect(ui.link, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "link" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Fetch Public Link", "Fetching link for...",
                            pathMsg, &process, this, false, true);
    progress.expand();
    progress.allowToClose();
    progress.exec();
  });

  QObject::connect(ui.upload, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    if (!model->isFolder(index)) {
      index = index.parent();
    }
    QDir path = model->path(index);

    TransferDialog t(false, false, remote, path, true, this);
    if (t.exec() == QDialog::Accepted) {
      QString src = t.getSource();
      QString dst = t.getDest();

      QStringList args = t.getOptions();
      emit addTransfer(QString("%1 from %2").arg(t.getMode()).arg(src), src,
                       dst, args);
    }
  });

  QObject::connect(ui.download, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QDir path = model->path(index);

    TransferDialog t(true, false, remote, path, model->isFolder(index), this);
    if (t.exec() == QDialog::Accepted) {
      QString src = t.getSource();
      QString dst = t.getDest();

      QStringList args = t.getOptions();
      emit addTransfer(QString("%1 %2").arg(t.getMode()).arg(src), src, dst,
                       args);
    }
  });

  QObject::connect(ui.getTree, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));
    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "tree"
                      << "-d" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Show directories tree", "Processing...", pathMsg,
                            &process, this, false);
    progress.expand();
    progress.allowToClose();
    progress.resize(1000, 600);
    progress.exec();
  });

  QObject::connect(ui.getSize, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));
    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "size" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Get Size", "Calculating...", pathMsg, &process,
                            this, false);
    progress.expand();
    progress.allowToClose();
    progress.exec();
  });

  QObject::connect(ui.export_, &QAction::triggered, this, [=, this]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QDir path = model->path(index);
    ExportDialog e(remote, path, this);
    if (e.exec() == QDialog::Accepted) {
      QString dst = e.getDestination();
      bool txt = e.onlyFilenames();

      QFile *file = new QFile(dst);
      if (!file->open(QFile::WriteOnly)) {
        QMessageBox::warning(
            this, "Error",
            QString("Cannot open file '%1' for writing!").arg(dst));
        delete file;
        return;
      }

      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(QStringList()
                           << GetRcloneConf() << GetDriveSharedWithMe()
                           << GetDefaultRcloneOptionsList() << e.getOptions());
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Export", "Exporting...", dst, &process, this);
      file->setParent(&progress);

      // Output arrives in arbitrary chunks; buffer partial lines so an entry
      // split across two reads is not dropped from the export.
      auto buffer = std::make_shared<RcloneOutput::LineBuffer>();
      auto out = std::make_shared<QTextStream>(file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
      out->setCodec("UTF-8");
#endif

      auto writeLine = [=](const QString &line) {
        const auto entry = RcloneOutput::parseLslLine(line);
        if (!entry.valid) {
          return;
        }
        if (txt) {
          *out << entry.name << '\n';
        } else {
          QString name = entry.name;
          if (name.contains(' ') || name.contains(',') || name.contains('"')) {
            name = '"' + name.replace("\"", "\"\"") + '"';
          }
          *out << name << ',' << '"' << entry.modified << '"' << ','
               << entry.size << '\n';
        }
      };

      QObject::connect(&progress, &ProgressDialog::outputAvailable, this,
                       [=](const QString &output) {
                         for (const auto &line : buffer->append(output)) {
                           writeLine(line);
                         }
                       });

      progress.exec();
      writeLine(buffer->flush());
      out->flush();
      // QWidget deletes children (the file) before its connections release
      // the stream, so detach now to avoid a flush into a deleted device.
      out->setDevice(nullptr);
    }
  });

  QObject::connect(
      model, &ItemModel::drop, this,
      [=, this](const QDir &path, const QModelIndex &parent) {
        auto settings = GetSettings();
        bool driveShared = ui.checkBoxShared->checkState();
        (driveShared
             ? settings->setValue("Settings/driveShared", Qt::Checked)
             : settings->setValue("Settings/driveShared", Qt::Unchecked));

        qApp->setActiveWindow(this);
        QDir destPath = model->path(parent);
        QString dest = QFileInfo(path.path()).isDir()
                           ? destPath.filePath(path.dirName())
                           : destPath.path();

        TransferDialog t(false, true, remote, dest, true, this);
        t.setSource(path.path());

        if (t.exec() == QDialog::Accepted) {
          QString src = t.getSource();
          QString dst = t.getDest();

          QStringList args = t.getOptions();
          emit addTransfer(QString("%1 from %2").arg(t.getMode()).arg(src), src,
                           dst, args);
        }
      });

  QObject::connect(
      ui.tree, &QWidget::customContextMenuRequested, this,
      [=, this](const QPoint &pos) {
        auto settings = GetSettings();
        bool driveShared = ui.checkBoxShared->checkState();
        (driveShared
             ? settings->setValue("Settings/driveShared", Qt::Checked)
             : settings->setValue("Settings/driveShared", Qt::Unchecked));

        QMenu menu;
        menu.addAction(ui.refresh);
        menu.addAction(ui.getSize);
        menu.addAction(ui.getTree);
        menu.addAction(ui.export_);
        menu.addSeparator();
        menu.addAction(ui.mkdir);
        menu.addAction(ui.rename);
        menu.addAction(ui.move);
        menu.addAction(ui.purge);
        menu.addSeparator();
        menu.addAction(ui.mount);
        menu.addAction(ui.stream);
        menu.addAction(ui.upload);
        menu.addAction(ui.download);
        menu.addAction(ui.link);
        menu.exec(ui.tree->viewport()->mapToGlobal(pos));
      });

  if (isLocal) {
    QHash<QString, QPersistentModelIndex> drives;

    // QDir::drives is fast
    for (const auto &drive : QDir::drives()) {
      QString path = drive.path();
      QModelIndex index = model->addRoot(QDir::toNativeSeparators(path), path);
      drives.insert(path, index);
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)) && !(defined Q_OS_WIN)
    QThread *thread = new QThread(this);
    thread->start();

    QObject *worker = new QObject();
    worker->moveToThread(thread);

    QTimer::singleShot(0, worker, [=, this]() {
      QStorageInfo info;
      info.refresh();

      // QStorageInfo::mountedVolumes is slow :(
      for (const auto &volume : info.mountedVolumes()) {
        QString name = volume.name();
        if (!name.isEmpty()) {
          QString path = volume.rootPath();
          QString item =
              QString("%1 (%2)").arg(QDir::toNativeSeparators(path)).arg(name);
          QTimer::singleShot(0, this,
                             [=]() { model->rename(drives[path], item); });
        }
      }

      thread->quit();
      thread->deleteLater();
      worker->deleteLater();
    });
#endif

    ui.tree->selectionModel()->selectionChanged(QItemSelection(),
                                                QItemSelection());
  } else {
    QModelIndex index = model->addRoot(remote, root);
    ui.tree->selectionModel()->select(
        index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    ui.tree->expand(index);
  }

  QShortcut *close = new QShortcut(QKeySequence::Close, this);
  QObject::connect(close, &QShortcut::activated, this,
                   &RemoteWidget::closeRequested);
}

RemoteWidget::~RemoteWidget() {}

// ---------------------------------------------------------------------------
// Windows 11 style page: header, command bar, breadcrumb bar, filter
// ---------------------------------------------------------------------------

void RemoteWidget::setRemoteType(const QString &type) {
  if (mTypeLabel) {
    mTypeLabel->setText(type);
  }
}

void RemoteWidget::buildFluentUi(ItemModel *model, const QString &remote,
                                 bool isLocal) {
  auto *v = qobject_cast<QVBoxLayout *>(ui.layout->layout());
  v->setContentsMargins(24, 20, 24, 8);
  v->setSpacing(8);
  layout()->setContentsMargins(0, 0, 0, 0);

  // ---- header: name, type, filter box, close
  auto *header = new QWidget;
  auto *hh = new QHBoxLayout(header);
  hh->setContentsMargins(4, 0, 0, 4);
  hh->setSpacing(10);
  auto *titles = new QVBoxLayout;
  titles->setSpacing(0);
  auto *title = new QLabel(remote);
  title->setObjectName("PageTitle");
  titles->addWidget(title);
  mTypeLabel = new QLabel;
  mTypeLabel->setProperty("secondary", true);
  titles->addWidget(mTypeLabel);
  hh->addLayout(titles, 1);

  mFilter = new QLineEdit;
  mFilter->setPlaceholderText(tr("Filter this folder"));
  mFilter->setClearButtonEnabled(true);
  mFilter->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
  mFilter->setFixedWidth(260);
  mFilter->setToolTip(tr("Show only items in the current folder whose name "
                         "contains this text (Ctrl+F)"));
  hh->addWidget(mFilter, 0, Qt::AlignVCenter);

  auto *newTab = new QShortcut(QKeySequence::AddTab, this);
  QObject::connect(newTab, &QShortcut::activated, this,
                   &RemoteWidget::newTabRequested);
  v->insertWidget(0, header);

  // ---- command bar
  struct A { QAction *a; const char *icon; QString text; };
  const QList<A> actions = {
      {ui.mkdir, "new_folder", tr("New folder")},
      {ui.upload, "upload", tr("Upload")},
      {ui.download, "download", tr("Download")},
      {ui.rename, "rename", tr("Rename")},
      {ui.move, "move", tr("Move")},
      {ui.purge, "delete", tr("Delete")},
      {ui.mount, "mount", tr("Mount")},
      {ui.stream, "stream", tr("Stream")},
      {ui.refresh, "refresh", tr("Refresh")},
      {ui.getSize, "size", tr("Folder size")},
      {ui.getTree, "tree", tr("Folder tree")},
      {ui.export_, "export", tr("Export file list")},
      {ui.link, "link", tr("Copy public link")},
  };
  for (const A &x : actions) {
    x.a->setIcon(Theme::icon(x.icon));
    x.a->setText(x.text);
    x.a->setIconText(x.text);
    if (!x.a->shortcut().isEmpty()) {
      x.a->setToolTip(QString("%1 (%2)").arg(x.text, x.a->shortcut().toString(
                                                    QKeySequence::NativeText)));
    } else {
      x.a->setToolTip(x.text);
    }
  }
  ui.upload->setShortcut(QKeySequence(tr("Ctrl+U")));
  ui.download->setShortcut(QKeySequence(tr("Ctrl+D")));

  auto *bar = qobject_cast<QHBoxLayout *>(ui.buttons->layout());
  // take everything out and lay it out again in command-bar order
  while (QLayoutItem *it = bar->takeAt(0)) {
    delete it;
  }
  bar->setContentsMargins(0, 0, 0, 0);
  bar->setSpacing(2);

  auto separator = [&]() {
    auto *f = new QFrame;
    f->setFrameShape(QFrame::NoFrame);
    f->setFixedSize(1, 20);
    f->setAutoFillBackground(true);
    f->setStyleSheet("background: palette(mid);");
    bar->addSpacing(4);
    bar->addWidget(f, 0, Qt::AlignVCenter);
    bar->addSpacing(4);
  };
  auto primary = [&](QToolButton *b) {
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // never squeeze below the natural width (that elides the labels)
    b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    b->setIconSize(QSize(18, 18));
    mPrimaryButtons.append(b); // labels or icons only: see updateCommandBar()
    bar->addWidget(b);
  };
  primary(ui.buttonMkdir);
  primary(ui.buttonUpload);
  primary(ui.buttonDownload);
  separator();
  primary(ui.buttonRename);
  primary(ui.buttonMove);
  primary(ui.buttonPurge);
  separator();
  primary(ui.buttonMount);
  bar->addStretch(1);
  // "Shared with me" (Google Drive only) lives in the More menu
  const bool sharedAvailable = !ui.checkBoxShared->isHidden();
  ui.checkBoxShared->hide();

  ui.buttonRefresh->setToolButtonStyle(Qt::ToolButtonIconOnly);
  ui.buttonRefresh->setIconSize(QSize(18, 18));
  bar->addWidget(ui.buttonRefresh);

  auto *more = new QToolButton;
  more->setIcon(Theme::icon("more"));
  more->setIconSize(QSize(18, 18));
  more->setToolTip(tr("More options"));
  more->setPopupMode(QToolButton::InstantPopup);
  auto *moreMenu = new QMenu(more);
  moreMenu->addAction(ui.stream);
  moreMenu->addSeparator();
  moreMenu->addAction(ui.getSize);
  moreMenu->addAction(ui.getTree);
  moreMenu->addAction(ui.export_);
  moreMenu->addSeparator();
  moreMenu->addAction(ui.link);

  // ---- Tools: compare, remove duplicates, copy to, two-way sync, clean up.
  // The actions belong to the menu (not to this widget) so the selection
  // handler that disables the toolbar actions leaves them alone.
  auto *tools = moreMenu->addMenu(Theme::icon("tasks"), tr("Tools"));
  auto selectedSpec = [this, model, remote]() {
    const auto rows = ui.tree->selectionModel()->selectedRows();
    const QString path =
        rows.isEmpty() ? QString() : model->path(rows.front()).path();
    return remote + ":" + path;
  };
  auto runTool = [this, selectedSpec](ToolDialog::Tool tool) {
    const QString spec = selectedSpec();
    ToolDialog dialog(tool, spec, this);
    if (dialog.exec() == QDialog::Accepted) {
      emit addTransfer(dialog.message(), dialog.source(), dialog.dest(),
                       dialog.args() + GetDefaultRcloneOptionsList());
    }
  };
  auto *compare = tools->addAction(tr("Compare with…"));
  QObject::connect(compare, &QAction::triggered, this,
                   [=]() { runTool(ToolDialog::Compare); });
  auto *copyTo = tools->addAction(tr("Copy to another remote…"));
  QObject::connect(copyTo, &QAction::triggered, this,
                   [=]() { runTool(ToolDialog::CopyTo); });
  auto *bisync = tools->addAction(tr("Two-way sync…"));
  QObject::connect(bisync, &QAction::triggered, this,
                   [=]() { runTool(ToolDialog::Bisync); });
  if (!isLocal) {
    tools->addSeparator();
    auto *dedupe = tools->addAction(tr("Remove duplicates…"));
    QObject::connect(dedupe, &QAction::triggered, this,
                     [=]() { runTool(ToolDialog::Dedupe); });
    auto *cleanup = tools->addAction(tr("Empty trash / clean up…"));
    QObject::connect(cleanup, &QAction::triggered, this, [=, this]() {
      if (Theme::confirm(this, tr("Clean up"),
                         tr("Clean up %1?").arg(remote),
                         tr("Permanently deletes what the provider keeps in "
                            "its trash or as old versions. This cannot be "
                            "undone."),
                         tr("Clean up"), tr("Cancel"))) {
        emit addTransfer(tr("Clean up %1").arg(remote), remote + ":",
                         QString(),
                         RcloneCommands::cleanup(remote + ":") +
                             GetDefaultRcloneOptionsList());
      }
    });
  }
  // compare / copy / sync need something to work on
  QObject::connect(moreMenu, &QMenu::aboutToShow, this,
                   [=, this]() {
                     const bool any =
                         !ui.tree->selectionModel()->selectedRows().isEmpty();
                     const bool folder =
                         any && model->isFolder(
                                    ui.tree->selectionModel()->selectedRows()
                                        .front());
                     for (QAction *a : {compare, copyTo, bisync}) {
                       a->setEnabled(folder);
                     }
                   });
  if (sharedAvailable) {
    moreMenu->addSeparator();
    auto *shared = moreMenu->addAction(tr("Show files shared with me"));
    shared->setCheckable(true);
    shared->setChecked(ui.checkBoxShared->isChecked());
    QObject::connect(shared, &QAction::toggled, ui.checkBoxShared,
                     &QCheckBox::setChecked);
  }
  more->setMenu(moreMenu);
  bar->addWidget(more);

  for (QToolButton *b : {ui.buttonStream, ui.buttonSize, ui.buttonTree,
                         ui.buttonExport, ui.buttonLink}) {
    b->hide();
  }

  // ---- breadcrumb address bar (replaces the plain path text box)
  auto *address = new QWidget;
  address->setProperty("card", true);
  address->setAttribute(Qt::WA_StyledBackground);
  auto *ah = new QHBoxLayout(address);
  ah->setContentsMargins(4, 2, 8, 2);
  ah->setSpacing(2);

  mUp = new QToolButton;
  mUp->setIcon(Theme::icon("up"));
  mUp->setToolTip(tr("Up to parent folder (Alt+Up)"));
  mUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
  ah->addWidget(mUp);

  mCrumbs = new QWidget;
  auto *ch = new QHBoxLayout(mCrumbs);
  ch->setContentsMargins(0, 0, 0, 0);
  ch->setSpacing(0);
  ah->addWidget(mCrumbs, 1);

  auto *copyPath = new QToolButton;
  copyPath->setIcon(Theme::icon("copy"));
  copyPath->setToolTip(tr("Copy path"));
  QObject::connect(copyPath, &QToolButton::clicked, this, [this]() {
    QGuiApplication::clipboard()->setText(ui.path->text());
  });
  ah->addWidget(copyPath);

  ui.path->hide(); // still updated by the existing code; used for "Copy path"
  v->insertWidget(v->indexOf(ui.path), address);

  // boxed sections: command bar and file list sit on their own cards
  ui.buttons->setProperty("card", true);
  ui.buttons->setAttribute(Qt::WA_StyledBackground);
  bar->setContentsMargins(6, 4, 6, 4);

  auto *listCard = new QWidget;
  listCard->setProperty("card", true);
  listCard->setAttribute(Qt::WA_StyledBackground);
  auto *lc = new QVBoxLayout(listCard);
  lc->setContentsMargins(1, 1, 1, 4);
  const int treePos = v->indexOf(ui.tree);
  v->removeWidget(ui.tree);
  lc->addWidget(ui.tree);
  v->insertWidget(treePos, listCard, 1);

  // ---- Icons view (large icons, open folders by double-click)
  mGrid = new QListView;
  mGrid->setObjectName("IconGrid");
  mGrid->setModel(model);
  mGrid->setViewMode(QListView::IconMode);
  mGrid->setSpacing(4);
  mGrid->setResizeMode(QListView::Adjust);
  mGrid->setMovement(QListView::Static);
  mGrid->setWordWrap(true);
  mGrid->setTextElideMode(Qt::ElideRight);
  // names may wrap onto two lines, so items are not uniform in height
  mGrid->setUniformItemSizes(false);
  mGrid->setLayoutMode(QListView::Batched); // stays responsive in huge folders
  mGrid->setBatchSize(200);
  mGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
  mGrid->setFrameShape(QFrame::NoFrame);
  mGrid->setContextMenuPolicy(Qt::ActionsContextMenu);
  mGrid->addActions(ui.tree->actions());
  mGrid->setMouseTracking(true);
  // drag files from File Explorer onto the grid to upload (as in Details)
  mGrid->setAcceptDrops(true);
  mGrid->setDragDropMode(QAbstractItemView::DropOnly);
  mGrid->setDropIndicatorShown(true);
  lc->addWidget(mGrid);

  // mirror the grid selection into the tree: all actions read the tree's
  // selection, so they work the same in both views
  QObject::connect(mGrid->selectionModel(), &QItemSelectionModel::selectionChanged,
                   this, [this, model]() {
                     QItemSelection sel;
                     for (const QModelIndex &i : mGrid->selectionModel()->selectedIndexes()) {
                       sel.select(i, i.siblingAtColumn(model->columnCount(i.parent()) - 1));
                     }
                     mSyncingFromGrid = true;
                     const QModelIndex cur = mGrid->currentIndex();
                     if (cur.isValid()) {
                       ui.tree->selectionModel()->setCurrentIndex(
                           cur, QItemSelectionModel::NoUpdate);
                     }
                     ui.tree->selectionModel()->select(
                         sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                     mSyncingFromGrid = false;
                   });
  mGrid->viewport()->installEventFilter(this); // Ctrl+wheel zoom
  applyIconSize(GetSettings()->value("Settings/gridIconSize", 64).toInt());
  QObject::connect(mGrid, &QListView::activated, this,
                   [this, model](const QModelIndex &index) {
                     if (model->isFolder(index)) {
                       // open the folder: show its contents and keep the
                       // (hidden) tree, breadcrumb and filter in step
                       ui.tree->expand(index);
                       showFolderInGrid(index, model);
                       mSyncingFromGrid = true;
                       ui.tree->setCurrentIndex(index);
                       mSyncingFromGrid = false;
                     }
                   });

  auto viewButton = [&](const char *icon, const QString &tip) {
    auto *b = new QToolButton;
    b->setIcon(Theme::icon(icon));
    b->setIconSize(QSize(18, 18));
    b->setCheckable(true);
    b->setAutoExclusive(true);
    b->setToolTip(tip);
    bar->insertWidget(bar->indexOf(ui.buttonRefresh), b);
    return b;
  };
  mViewIcons = viewButton("view_grid", tr("Icons (Ctrl+Shift+1) - Ctrl+wheel to zoom"));
  // icon sizes, as in File Explorer's View menu
  {
    auto *sizes = new QMenu(mViewIcons);
    mSizeGroup = new QActionGroup(sizes);
    const QList<QPair<QString, int>> options = {
        {tr("Extra large icons"), 96}, {tr("Large icons"), 64},
        {tr("Medium icons"), 48}, {tr("Small icons"), 32}};
    for (const auto &o : options) {
      QAction *a = sizes->addAction(o.first);
      a->setCheckable(true);
      a->setData(o.second);
      mSizeGroup->addAction(a);
      QObject::connect(a, &QAction::triggered, this, [this, model, px = o.second]() {
        applyIconSize(px);
        setIconView(true, model);
      });
    }
    mViewIcons->setMenu(sizes);
    mViewIcons->setPopupMode(QToolButton::MenuButtonPopup);
    applyIconSize(mIconPx); // tick the current size
  }
  mViewDetails = viewButton("view_list", tr("Details (Ctrl+Shift+2)"));
  mViewIcons->setShortcut(QKeySequence(tr("Ctrl+Shift+1")));
  mViewDetails->setShortcut(QKeySequence(tr("Ctrl+Shift+2")));
  QObject::connect(mViewIcons, &QToolButton::clicked, this,
                   [this, model]() { setIconView(true, model); });
  QObject::connect(mViewDetails, &QToolButton::clicked, this,
                   [this, model]() { setIconView(false, model); });

  ui.buttons->installEventFilter(this);
  QTimer::singleShot(0, this, [this]() { updateCommandBar(); });

  ui.tree->setUniformRowHeights(true); // much faster with large folders

  // Details columns: right-click the header to choose (as in File Explorer)
  {
    QHeaderView *hdr = ui.tree->header();
    hdr->setMinimumSectionSize(72);
    hdr->setContextMenuPolicy(Qt::CustomContextMenu);
    const QStringList hidden =
        GetSettings()->value("Settings/hiddenColumns", QStringList{"4", "5"}).toStringList();
    for (int c = 1; c < model->columnCount(QModelIndex()); ++c) {
      hdr->setSectionHidden(c, hidden.contains(QString::number(c)));
    }
    QObject::connect(hdr, &QWidget::customContextMenuRequested, this,
                     [this, hdr, model](const QPoint &pos) {
                       QMenu menu;
                       for (int c = 1; c < model->columnCount(QModelIndex()); ++c) {
                         QAction *a = menu.addAction(
                             model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
                         a->setCheckable(true);
                         a->setChecked(!hdr->isSectionHidden(c));
                         QObject::connect(a, &QAction::toggled, this, [this, hdr, c](bool on) {
                           hdr->setSectionHidden(c, !on);
                           if (on) {
                             ui.tree->resizeColumnToContents(c);
                           }
                           QStringList hiddenNow;
                           for (int i = 1; i < hdr->count(); ++i) {
                             if (hdr->isSectionHidden(i)) {
                               hiddenNow << QString::number(i);
                             }
                           }
                           GetSettings()->setValue("Settings/hiddenColumns", hiddenNow);
                         });
                       }
                       menu.exec(hdr->mapToGlobal(pos));
                     });
  }
  ui.tree->setAnimated(true);
  ui.tree->setIconSize(QSize(22, 22));
  ui.tree->setFrameShape(QFrame::NoFrame);
  ui.tree->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  ui.tree->header()->setHighlightSections(false);

  // ---- behaviour
  QObject::connect(mUp, &QToolButton::clicked, this, [this]() {
    QModelIndex cur = ui.tree->currentIndex();
    if (!cur.isValid()) {
      return;
    }
    QModelIndex parent = cur.parent();
    if (parent.isValid()) {
      ui.tree->setCurrentIndex(parent);
      ui.tree->scrollTo(parent);
    }
  });

  QObject::connect(ui.tree->selectionModel(),
                   &QItemSelectionModel::currentChanged, this,
                   [this, model, remote](const QModelIndex &current) {
                     if (!mSyncingFromGrid && mGrid->isVisible() && current.isValid()) {
                       QModelIndex folder = current.siblingAtColumn(0);
                       if (!model->isFolder(folder)) {
                         folder = folder.parent();
                       }
                       if (folder.isValid() && folder != mGrid->rootIndex()) {
                         showFolderInGrid(folder, model);
                       }
                     }
                     updateBreadcrumbs(model, remote);
                     applyFilter(model);
                   });
  // Typing and model updates (a big folder arriving, sorting) can come in
  // bursts: apply the filter once things settle instead of for each of them.
  mFilterTimer = new QTimer(this);
  mFilterTimer->setSingleShot(true);
  mFilterTimer->setInterval(150);
  QObject::connect(mFilterTimer, &QTimer::timeout, this,
                   [this, model]() { applyFilter(model); });
  QObject::connect(mFilter, &QLineEdit::textChanged, this,
                   [this, model](const QString &text) {
                     if (text.trimmed().isEmpty()) {
                       mFilterTimer->stop();
                       applyFilter(model); // showing everything again is instant
                     } else {
                       mFilterTimer->start();
                     }
                   });
  QObject::connect(model, &QAbstractItemModel::rowsInserted, this,
                   [this](const QModelIndex &parent) {
                     if (!mFilter->text().isEmpty() && parent == mFilterFolder) {
                       mFilterTimer->start();
                     }
                   });
  // (sorting another folder, e.g. one loading in the background, is none of
  // our business)
  QObject::connect(
      model, &QAbstractItemModel::layoutChanged, this,
      [this](const QList<QPersistentModelIndex> &parents,
             QAbstractItemModel::LayoutChangeHint) {
        if (!mFilter->text().isEmpty() &&
            (parents.isEmpty() || parents.contains(mFilterFolder))) {
          mFilterTimer->start();
        }
      });

  auto *find = new QShortcut(QKeySequence::Find, this);
  QObject::connect(find, &QShortcut::activated, this, [this]() {
    mFilter->setFocus();
    mFilter->selectAll();
  });
  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), mFilter);
  escape->setContext(Qt::WidgetShortcut);
  QObject::connect(escape, &QShortcut::activated, this, [this]() {
    mFilter->clear();
    ui.tree->setFocus();
  });

  updateBreadcrumbs(model, remote);
  // the breadcrumb arrows are pixmaps in the theme's colour: redraw on a switch
  QObject::connect(Theme::notifier(), &Theme::Notifier::changed, this,
                   [this, model, remote]() { updateBreadcrumbs(model, remote); });

  // Icons view is the default; the choice is remembered
  const bool icons =
      GetSettings()->value("Settings/remoteView", "icons").toString() == "icons";
  QTimer::singleShot(0, this, [this, model, icons]() { setIconView(icons, model); });
}

bool RemoteWidget::eventFilter(QObject *o, QEvent *e) {
  if (o == ui.buttons && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
    updateCommandBar();
  }
  if (mGrid && o == mGrid->viewport() && e->type() == QEvent::Wheel) {
    auto *we = static_cast<QWheelEvent *>(e);
    if (we->modifiers() & Qt::ControlModifier) {
      static const int steps[] = {32, 48, 64, 96};
      int i = 0;
      while (i < 3 && steps[i] < mIconPx) {
        ++i;
      }
      i += we->angleDelta().y() > 0 ? 1 : -1;
      applyIconSize(steps[qBound(0, i, 3)]);
      return true;
    }
  }
  return QWidget::eventFilter(o, e);
}

void RemoteWidget::updateCommandBar() {
  // Step down gracefully as the window gets narrower so buttons never
  // overlap: all labels -> labels on the main three -> icons only.
  // Labels that are hidden stay available as tooltips.
  auto labelWidth = [](QToolButton *b) {
    return b->fontMetrics().horizontalAdvance(b->text()) + b->iconSize().width() + 32;
  };
  auto iconWidth = [](QToolButton *b) { return b->iconSize().width() + 22; };
  const int fixed = 4 * 44 + 2 * 12 + 16; // view toggles, refresh, more, separators
  const int avail = ui.buttons->width();
  int full = fixed, partial = fixed;
  for (int i = 0; i < mPrimaryButtons.size(); ++i) {
    full += labelWidth(mPrimaryButtons[i]);
    partial += i < 3 ? labelWidth(mPrimaryButtons[i]) : iconWidth(mPrimaryButtons[i]);
  }
  const int labelled = avail >= full ? int(mPrimaryButtons.size()) : avail >= partial ? 3 : 0;
  for (int i = 0; i < mPrimaryButtons.size(); ++i) {
    QToolButton *b = mPrimaryButtons[i];
    const bool text = i < labelled;
    b->setToolButtonStyle(text ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    b->setMinimumWidth(text ? labelWidth(b) : 0);
  }
  mCompactBar = labelled == 0;
}

void RemoteWidget::applyIconSize(int px) {
  px = qBound(32, px, 96);
  mIconPx = px;
  mGrid->setIconSize(QSize(px, px));
  // cell: room for two lines of name under the icon
  const int w = qMax(96, px + 56);
  const int h = px + (px >= 64 ? 52 : 46);
  mGrid->setGridSize(QSize(w, h));
  if (mSizeGroup) {
    for (QAction *a : mSizeGroup->actions()) {
      a->setChecked(a->data().toInt() == px);
    }
  }
  GetSettings()->setValue("Settings/gridIconSize", px);
}

void RemoteWidget::showFolderInGrid(const QModelIndex &folder, ItemModel *model) {
  mGrid->setRootIndex(folder);
  for (int r = 0; r < model->rowCount(folder); ++r) {
    mGrid->setRowHidden(r, false);
  }
  mGrid->scrollToTop();
  mFilterFolder = QPersistentModelIndex();
  applyFilter(model);
}

void RemoteWidget::setIconView(bool icons, ItemModel *model) {
  mViewIcons->setChecked(icons);
  mViewDetails->setChecked(!icons);
  ui.tree->setVisible(!icons);
  mGrid->setVisible(icons);
  GetSettings()->setValue("Settings/remoteView", icons ? "icons" : "details");
  if (icons) {
    QModelIndex folder = ui.tree->currentIndex().siblingAtColumn(0);
    if (folder.isValid() && !model->isFolder(folder)) {
      folder = folder.parent();
    }
    if (!folder.isValid()) {
      folder = model->index(0, 0, QModelIndex()); // the remote's root
      ui.tree->setCurrentIndex(folder);
      ui.tree->expand(folder);
    }
    showFolderInGrid(folder, model);
    mGrid->setFocus();
  } else {
    ui.tree->setFocus();
  }
}

void RemoteWidget::updateBreadcrumbs(ItemModel *model, const QString &remote) {
  QLayout *l = mCrumbs->layout();
  while (QLayoutItem *it = l->takeAt(0)) {
    delete it->widget();
    delete it;
  }

  // folder chain of the current item (a file shows its containing folder)
  QModelIndex cur = ui.tree->currentIndex().siblingAtColumn(0);
  if (cur.isValid() && !model->isFolder(cur)) {
    cur = cur.parent();
  }
  QList<QPersistentModelIndex> chain;
  for (QModelIndex i = cur; i.isValid(); i = i.parent()) {
    chain.prepend(i);
  }
  mUp->setEnabled(ui.tree->currentIndex().parent().isValid());

  auto *h = qobject_cast<QHBoxLayout *>(l);
  if (chain.isEmpty()) {
    auto *b = new QToolButton;
    b->setText(remote);
    h->addWidget(b);
  }
  for (int i = 0; i < chain.size(); ++i) {
    const QPersistentModelIndex idx = chain[i];
    QString text = model->data(idx, Qt::DisplayRole).toString();
    if (i == 0 && (text == "/" || text.isEmpty())) {
      text = remote;
    }
    if (i > 0) {
      auto *sep = new QLabel;
      sep->setPixmap(Theme::icon("chevron").pixmap(12, 12));
      h->addWidget(sep);
    }
    auto *b = new QToolButton;
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    if (i == chain.size() - 1) {
      QFont f = b->font();
      f.setWeight(QFont::DemiBold);
      b->setFont(f);
    }
    QObject::connect(b, &QToolButton::clicked, this, [this, idx]() {
      if (idx.isValid()) {
        ui.tree->setCurrentIndex(idx);
        ui.tree->expand(idx);
        ui.tree->scrollTo(idx);
        ui.tree->setFocus();
      }
    });
    h->addWidget(b);
  }
  h->addStretch(1);
}

void RemoteWidget::applyFilter(ItemModel *model) {
  // Changing which rows are hidden makes the views (and, through the model,
  // sorting) emit signals that lead back here: never run inside ourselves.
  if (mApplyingFilter) {
    return;
  }
  mApplyingFilter = true;
  struct Done {
    bool &flag;
    ~Done() { flag = false; }
  } done{mApplyingFilter};

  // the folder being filtered: the current folder, or a file's parent
  QModelIndex folder = ui.tree->currentIndex().siblingAtColumn(0);
  if (folder.isValid() && !model->isFolder(folder)) {
    folder = folder.parent();
  }

  auto setHidden = [&](const QModelIndex &parent, int row, bool hide) {
    // only touch rows that change: every call makes the view re-layout
    if (ui.tree->isRowHidden(row, parent) != hide) {
      ui.tree->setRowHidden(row, parent, hide);
    }
    if (mGrid && mGrid->rootIndex() == parent && mGrid->isRowHidden(row) != hide) {
      mGrid->setRowHidden(row, hide);
    }
  };

  // un-hide everything in the previously filtered folder
  if (mFilterFolder.isValid() && mFilterFolder != folder &&
      !model->needsLoad(mFilterFolder)) {
    const QModelIndex previous = mFilterFolder;
    for (int r = 0; r < model->rowCount(previous); ++r) {
      setHidden(previous, r, false);
    }
  }
  mFilterFolder = folder;
  if (!folder.isValid()) {
    return;
  }
  // rowCount() would start listing a folder that was never opened. Filtering
  // must only ever look at what is already there; when the folder arrives the
  // model's rowsInserted brings us back.
  if (model->needsLoad(folder)) {
    return;
  }

  const QString text = mFilter->text().trimmed();
  const QModelIndex current = ui.tree->currentIndex().siblingAtColumn(0);
  ui.tree->setUpdatesEnabled(false);
  for (int r = 0; r < model->rowCount(folder); ++r) {
    const QModelIndex child = model->index(r, 0, folder);
    const QString name = model->data(child, Qt::DisplayRole).toString();
    const bool hide = !text.isEmpty() && child != current &&
                      !name.contains(text, Qt::CaseInsensitive) &&
                      !model->isLoading(child);
    setHidden(folder, r, hide);
  }
  if (!text.isEmpty() && !ui.tree->isExpanded(folder)) {
    ui.tree->expand(folder);
  }
  ui.tree->setUpdatesEnabled(true);
}
