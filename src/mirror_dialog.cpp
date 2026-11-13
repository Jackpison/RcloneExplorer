#include "mirror_dialog.h"
#include "elided_label.h"
#include "mirror_tools.h"
#include "natural_sort.h"
#include "rclone_output.h"
#include "remote_folder_dialog.h"
#include "theme.h"

namespace {
enum { PathRole = Qt::UserRole, PlaceholderRole = Qt::UserRole + 1 };

QString joinPath(const QString &parent, const QString &name) {
  return parent.isEmpty() ? name : parent + "/" + name;
}

// The cleaned folder of a destination card: a remote path, or a full path on
// this PC (empty remote).
QString cleanFor(const QString &remote, const QString &text, bool *ok = nullptr) {
  if (remote.isEmpty()) {
    if (ok) {
      *ok = true;
    }
    return Mirror::cleanLocalPath(text);
  }
  return Mirror::cleanPath(text, ok);
}

QTreeWidgetItem *folderItem(const QString &name, const QString &path, const QIcon &icon) {
  auto *item = new QTreeWidgetItem;
  item->setText(0, name);
  item->setIcon(0, icon);
  item->setData(0, PathRole, path);
  auto *placeholder = new QTreeWidgetItem(item); // shows the arrow until listed
  placeholder->setText(0, QObject::tr("Loading…"));
  placeholder->setData(0, PlaceholderRole, true);
  placeholder->setFlags(Qt::NoItemFlags);
  return item;
}
} // namespace

MirrorDialog::MirrorDialog(const QMap<QString, QString> &remotes, const QStringList &order,
                           const QList<Mirror::Plan> &all, const Mirror::Plan *existing,
                           QWidget *parent)
    : QDialog(parent), mRemotes(remotes), mOrder(order), mAll(all) {
  for (auto it = remotes.cbegin(); it != remotes.cend(); ++it) {
    if (!mOrder.contains(it.key())) {
      mOrder << it.key();
    }
  }
  mEditing = existing != nullptr;
  if (existing) {
    mPlan = *existing;
    mSourcePath = mPlan.sourcePath;
    mWalk = !mPlan.sourcePath.isEmpty();
  } else {
    mPlan.id = QUuid::createUuid();
  }
  setWindowTitle(mEditing ? tr("Edit mirror") : tr("New mirror"));
  resize(720, 680);

  auto *v = new QVBoxLayout(this);
  v->setSpacing(10);
  mSteps = new QLabel;
  v->addWidget(mSteps);

  mPages = new QStackedWidget;
  mPages->addWidget(buildFolderStep());
  mPages->addWidget(buildDestinationStep());
  v->addWidget(mPages, 1);

  mError = new QLabel;
  mError->setTextFormat(Qt::PlainText); // messages quote folder names
  mError->setWordWrap(true);
  mError->setStyleSheet(QString("color: %1;").arg(Theme::color("critical").name()));
  mError->hide();
  v->addWidget(mError);

  auto *bar = new QHBoxLayout;
  mBack = new QPushButton(tr("Back"));
  bar->addWidget(mBack);
  bar->addStretch(1);
  auto *cancel = new QPushButton(tr("Cancel"));
  bar->addWidget(cancel);
  mNext = new QPushButton;
  mNext->setProperty("accent", true);
  mNext->setDefault(true);
  bar->addWidget(mNext);
  v->addLayout(bar);

  QObject::connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  QObject::connect(mBack, &QPushButton::clicked, this, [this]() { showStep(0); });
  QObject::connect(mNext, &QPushButton::clicked, this, [this]() {
    if (mPages->currentIndex() == 0) {
      fillDestinations();
      showStep(1);
    } else {
      save();
    }
  });

  if (mEditing) {
    const int i = mSource->findData(mPlan.sourceRemote);
    if (i >= 0) {
      mSource->setCurrentIndex(i);
    }
    fillDestinations();
    showStep(1);
  } else {
    showStep(0);
  }
}

MirrorDialog::~MirrorDialog() { stopLoading(); }

QString MirrorDialog::sourceRemote() const { return mSource->currentData().toString(); }

QString MirrorDialog::selectedPath() const { return mSourcePath; }

void MirrorDialog::showStep(int step) {
  mPages->setCurrentIndex(step);
  const QString on = QString("color: %1; font-weight: 600;").arg(Theme::accent().name());
  const QString off = QString("color: %1;").arg(Theme::color("text3").name());
  mSteps->setText(QString("<span style='%1'>%2</span>&nbsp;&nbsp;&nbsp;<span style='%3'>%4</span>")
                      .arg(step == 0 ? on : off, step == 0 ? tr("① Folder") : tr("✓ Folder"),
                           step == 1 ? on : off, tr("② Destinations")));
  mBack->setVisible(step == 1);
  mNext->setText(step == 0 ? tr("Next  →") : tr("Save mirror"));
  mNext->setIcon(step == 0 ? QIcon() : Theme::icon("checkmark", QColor("#1A1405")));
  mError->hide();
  if (step == 0) {
    folderSelected();
  }
}

// ------------------------------------------------------------ step 1 --

QWidget *MirrorDialog::buildFolderStep() {
  auto *page = new QWidget;
  auto *v = new QVBoxLayout(page);
  v->setContentsMargins(0, 0, 0, 0);
  auto *hint = new QLabel(tr("Choose the folder to mirror: any folder or sub-folder of a remote "
                             "(open folders with the arrow), a whole remote, or a folder on "
                             "this PC."));
  hint->setWordWrap(true);
  hint->setProperty("secondary", true);
  v->addWidget(hint);

  auto *form = new QFormLayout;
  mSource = new QComboBox;
  for (const QString &name : std::as_const(mOrder)) {
    mSource->addItem(Theme::remoteIcon(mRemotes.value(name)), name, name);
  }
  mSource->addItem(Theme::remoteIcon("local"), tr("This PC"), QString());
  form->addRow(tr("Source"), mSource);
  v->addLayout(form);

  // a folder on this PC: typed or picked, no tree
  mLocalRow = new QWidget;
  auto *lv = new QVBoxLayout(mLocalRow);
  lv->setContentsMargins(0, 0, 0, 0);
  auto *localLabel = new QLabel(tr("Folder on this PC"));
  localLabel->setProperty("secondary", true);
  lv->addWidget(localLabel);
  auto *lrow = new QHBoxLayout;
  mLocalPath = new QLineEdit;
#ifdef Q_OS_WIN
  mLocalPath->setPlaceholderText(tr("for example: C:\\Users\\you\\Pictures"));
#else
  mLocalPath->setPlaceholderText(tr("for example: /home/you/Pictures"));
#endif
  lrow->addWidget(mLocalPath, 1);
  auto *localBrowse = new QPushButton(Theme::icon("folder"), tr("Browse…"));
  lrow->addWidget(localBrowse);
  lv->addLayout(lrow);
  lv->addStretch(1);
  v->addWidget(mLocalRow, 1);
  mLocalRow->hide();
  // checked while typing, once the typing pauses
  auto *typing = new QTimer(this);
  typing->setSingleShot(true);
  typing->setInterval(350);
  QObject::connect(typing, &QTimer::timeout, this, [this]() { folderSelected(); });
  QObject::connect(mLocalPath, &QLineEdit::textEdited, typing, qOverload<>(&QTimer::start));
  QObject::connect(mLocalPath, &QLineEdit::editingFinished, this, [this, typing]() {
    typing->stop();
    folderSelected();
  });
  QObject::connect(localBrowse, &QPushButton::clicked, this, [this]() {
    const QString current = Mirror::cleanLocalPath(mLocalPath->text());
    const QString picked = QFileDialog::getExistingDirectory(
        this, tr("Choose the folder to mirror"), current.isEmpty() ? QDir::homePath() : current);
    if (!picked.isEmpty()) {
      mLocalPath->setText(QDir::toNativeSeparators(picked));
      folderSelected();
    }
  });

  mTree = new QTreeWidget;
  mTree->setColumnCount(2);
  mTree->setHeaderHidden(true);
  mTree->header()->setStretchLastSection(false);
  mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  mTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  mTree->setUniformRowHeights(true);
  v->addWidget(mTree, 1);

  mSelected = new QLabel;
  mSelected->setWordWrap(true);
  v->addWidget(mSelected);

  QObject::connect(mSource, &QComboBox::currentIndexChanged, this, [this]() {
    if (sourceRemote() != mPlan.sourceRemote) {
      mWalk = false; // another remote: start from its top
    }
    setSource();
  });
  QObject::connect(mTree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
    if (item->childCount() == 1 && item->child(0)->data(0, PlaceholderRole).toBool()) {
      loadFolder(item);
    }
  });
  QObject::connect(mTree, &QTreeWidget::currentItemChanged, this,
                   [this](QTreeWidgetItem *) { folderSelected(); });
  setSource();
  return page;
}

void MirrorDialog::stopLoading() {
  const auto running = mRunning;
  for (QProcess *p : running) {
    p->disconnect();
    p->kill();
    p->deleteLater();
  }
  mRunning.clear();
}

void MirrorDialog::setSource() {
  stopLoading();
  mTree->clear();
  const QString remote = sourceRemote();
  const bool local = remote.isEmpty();
  mTree->setVisible(!local);
  mLocalRow->setVisible(local);
  if (local) {
    if (mEditing && mPlan.localSource() && mLocalPath->text().isEmpty()) {
      mLocalPath->setText(QDir::toNativeSeparators(mPlan.sourcePath));
    }
    folderSelected();
    return;
  }
  auto *root = folderItem(tr("%1 (whole remote)").arg(remote), QString(),
                          Theme::remoteIcon(mRemotes.value(remote)));
  mTree->addTopLevelItem(root);
  root->setExpanded(true);
  // When editing, the tree opens down to the mirrored folder as it is listed
  // (see loadFolder); until then the saved folder stays chosen.
  if (mWalk && remote == mPlan.sourceRemote) {
    mSourcePath = mPlan.sourcePath;
  } else {
    mSourcePath.clear();
    mTree->setCurrentItem(root);
  }
}

void MirrorDialog::loadFolder(QTreeWidgetItem *item) {
  const QString remote = sourceRemote();
  const QString path = item->data(0, PathRole).toString();
  const QPersistentModelIndex where(mTree->indexFromItem(item));
  QProcess *p = MirrorUtil::runRclone(
      this, {"lsjson", "--dirs-only", "--no-mimetype", Mirror::remotePath(remote, path)},
      [this, where, path, remote](int code, const QByteArray &out, const QString &err) {
        QTreeWidgetItem *item = mTree->itemFromIndex(where);
        if (!item || remote != sourceRemote()) {
          return;
        }
        qDeleteAll(item->takeChildren());
        bool ok = false;
        const auto entries = RcloneOutput::parseLsJson(out, &ok);
        if (code != 0 || !ok) {
          auto *e = new QTreeWidgetItem(item, {tr("Could not list this folder: %1")
                                                   .arg(MirrorUtil::lastError(err))});
          e->setFlags(Qt::NoItemFlags);
          return;
        }
        QStringList names;
        for (const auto &e : entries) {
          if (e.isDir && e.name != Mirror::trashDir()) {
            names << e.name;
          }
        }
        std::stable_sort(names.begin(), names.end(),
                         [](const QString &a, const QString &b) { return naturalCompare(a, b) < 0; });
        for (const QString &name : names) {
          const QString child = joinPath(path, name);
          auto *ci = folderItem(name, child, Theme::icon("folder"));
          ci->setText(1, mSizes.value(Mirror::remotePath(remote, child)));
          item->addChild(ci);
        }
        if (names.isEmpty()) {
          item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        }
        // when editing, walk down to the mirrored folder
        if (mWalk && remote == mPlan.sourceRemote) {
          const QString target = mPlan.sourcePath;
          for (int i = 0; i < item->childCount(); ++i) {
            QTreeWidgetItem *c = item->child(i);
            const QString cp = c->data(0, PathRole).toString();
            if (cp == target) {
              mWalk = false;
              mTree->setCurrentItem(c);
            } else if (target.startsWith(cp + "/")) {
              c->setExpanded(true);
            }
          }
        }
      });
  mRunning << p;
  QObject::connect(p, &QObject::destroyed, this, [this, p]() { mRunning.removeAll(p); });
}

void MirrorDialog::folderSelected() {
  if (sourceRemote().isEmpty()) { // a folder on this PC
    const QString p = Mirror::cleanLocalPath(mLocalPath->text());
    const bool valid = QDir::isAbsolutePath(p) && !QDir(p).isRoot();
    mSourcePath = valid ? p : QString();
    if (mNext) {
      mNext->setEnabled(valid || mPages->currentIndex() == 1);
    }
    if (!valid) {
      mSelected->setText(tr("Choose a folder on this PC."));
      return;
    }
    const QString shown = "<b>" + QDir::toNativeSeparators(p).toHtmlEscaped() + "</b>";
    if (mSizes.contains(p)) {
      mSelected->setText(tr("Mirror %1 · %2").arg(shown, mSizes.value(p)));
      return;
    }
    mSelected->setText(tr("Mirror %1 · counting…").arg(shown));
    QProcess *proc = MirrorUtil::runRclone(
        this, {"size", "--json", p}, [this, p, shown](int code, const QByteArray &out, const QString &err) {
          QString text = err.contains("not found", Qt::CaseInsensitive) ? tr("folder not found")
                                                                        : tr("size unavailable");
          if (code == 0) {
            const QJsonObject o = QJsonDocument::fromJson(out).object();
            text = MirrorUtil::niceSize(o.value("bytes").toInteger()) + " · " +
                   MirrorUtil::countText(o.value("count").toInteger());
            mSizes.insert(p, text);
          }
          if (sourceRemote().isEmpty() && mSourcePath == p) {
            const bool missing = code != 0 && err.contains("not found", Qt::CaseInsensitive);
            mSelected->setText(missing ? tr("%1 was not found on this PC.").arg(shown)
                                       : tr("Mirror %1 · %2").arg(shown, text));
            if (missing && mNext && mPages->currentIndex() == 0) {
              mNext->setEnabled(false);
            }
          }
        });
    mRunning << proc;
    QObject::connect(proc, &QObject::destroyed, this, [this, proc]() { mRunning.removeAll(proc); });
    return;
  }
  QTreeWidgetItem *it = mTree->currentItem();
  const bool valid = it && !it->data(0, PlaceholderRole).toBool();
  if (mNext) { // not created yet while the dialog is being built
    mNext->setEnabled(valid || mPages->currentIndex() == 1);
  }
  if (!valid) {
    mSelected->setText(tr("Select a folder."));
    return;
  }
  const QString remote = sourceRemote();
  const QString path = it->data(0, PathRole).toString();
  mSourcePath = path;
  const QString key = Mirror::remotePath(remote, path);
  const QString shown = path.isEmpty() ? tr("the whole remote") : "<b>" + path.toHtmlEscaped() + "</b>";
  if (mSizes.contains(key)) {
    mSelected->setText(tr("Mirror %1 of %2 · %3").arg(shown, remote.toHtmlEscaped(), mSizes.value(key)));
    return;
  }
  mSelected->setText(tr("Mirror %1 of %2 · counting…").arg(shown, remote.toHtmlEscaped()));
  const QPersistentModelIndex where(mTree->indexFromItem(it));
  QProcess *p = MirrorUtil::runRclone(
      this, {"size", "--json", "--drive-skip-gdocs", key},
      [this, key, where](int code, const QByteArray &out, const QString &) {
        QString text = tr("size unavailable");
        if (code == 0) {
          const QJsonObject o = QJsonDocument::fromJson(out).object();
          text = MirrorUtil::niceSize(o.value("bytes").toInteger()) + " · " +
                 MirrorUtil::countText(o.value("count").toInteger());
          mSizes.insert(key, text);
        }
        if (QTreeWidgetItem *item = mTree->itemFromIndex(where)) {
          item->setText(1, text);
          if (item == mTree->currentItem()) {
            folderSelected();
          }
        }
      });
  mRunning << p;
  QObject::connect(p, &QObject::destroyed, this, [this, p]() { mRunning.removeAll(p); });
}

// ------------------------------------------------------------ step 2 --

QWidget *MirrorDialog::buildDestinationStep() {
  auto *page = new QWidget;
  auto *v = new QVBoxLayout(page);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(10);

  auto *src = new QWidget;
  src->setProperty("card", true);
  src->setAttribute(Qt::WA_StyledBackground);
  auto *sh = new QHBoxLayout(src);
  sh->setContentsMargins(12, 10, 12, 10);
  sh->setSpacing(12);
  mSourceIcon = new QLabel;
  sh->addWidget(mSourceIcon);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(1);
  mSourceText = new QLabel;
  mSourceText->setTextFormat(Qt::PlainText);
  texts->addWidget(mSourceText);
  auto *name = new ElidedLabel; // a long path is shortened in the middle
  name->setAutoToolTip(true);
  name->setStyleSheet("font-weight: 700;");
  mSourceName = name;
  texts->addWidget(mSourceName);
  sh->addLayout(texts, 1);
  auto *change = new QPushButton(tr("Change"));
  QObject::connect(change, &QPushButton::clicked, this, [this]() { showStep(0); });
  sh->addWidget(change);
  v->addWidget(src);

  auto *hint = new QLabel(tr("Switch on the remotes that should get a copy and choose the folder "
                             "on each. It starts as the same path; change it if you like."));
  hint->setWordWrap(true);
  hint->setProperty("secondary", true);
  v->addWidget(hint);

  auto *list = new QWidget;
  mDestLayout = new QVBoxLayout(list);
  mDestLayout->setContentsMargins(0, 0, 4, 0);
  mDestLayout->setSpacing(8);
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setWidget(list);
  v->addWidget(scroll, 1);

  // files to skip
  mSkipToggle = new QToolButton;
  mSkipToggle->setText(tr("Skip some files (optional)"));
  mSkipToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  mSkipToggle->setCheckable(true);
  mSkipToggle->setArrowType(Qt::RightArrow);
  mSkipToggle->setAutoRaise(true);
  v->addWidget(mSkipToggle);
  mSkipBox = new QWidget;
  auto *sv = new QVBoxLayout(mSkipBox);
  sv->setContentsMargins(0, 0, 0, 0);
  auto *skipHint = new QLabel(tr("One pattern per line. <b>*</b> matches any characters: "
                                 "<b>*.tmp</b> skips every .tmp file, <b>node_modules/**</b> a "
                                 "folder. Skipped files are never copied, changed or removed."));
  skipHint->setWordWrap(true);
  skipHint->setProperty("secondary", true);
  sv->addWidget(skipHint);
  auto *chips = new QHBoxLayout;
  for (const QString &c : {QString("Thumbs.db"), QString(".DS_Store"), QString("desktop.ini"),
                           QString("*.tmp"), QString("~$*")}) {
    auto *b = new QPushButton("+ " + c);
    QObject::connect(b, &QPushButton::clicked, this, [this, c]() {
      QStringList lines = mExclude->toPlainText().split('\n');
      for (QString &l : lines) {
        l = l.trimmed();
      }
      if (!lines.contains(c)) {
        QString t = mExclude->toPlainText().trimmed();
        mExclude->setPlainText(t.isEmpty() ? c : t + "\n" + c);
      }
    });
    chips->addWidget(b);
  }
  chips->addStretch(1);
  sv->addLayout(chips);
  mExclude = new QPlainTextEdit;
  mExclude->setPlaceholderText(tr("for example: *.tmp"));
  mExclude->setFixedHeight(90);
  sv->addWidget(mExclude);
  v->addWidget(mSkipBox);
  QObject::connect(mSkipToggle, &QToolButton::toggled, this, [this](bool on) {
    mSkipBox->setVisible(on);
    mSkipToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
  });
  mSkipBox->hide();

  auto *form = new QFormLayout;
  mMode = new QComboBox;
  mMode->addItem(tr("Keep the old copy in a trash folder (safest)"), int(Mirror::DeleteMode::Trash));
  mMode->addItem(tr("Delete it from the destination"), int(Mirror::DeleteMode::Delete));
  mMode->addItem(tr("Leave the destination alone (never remove)"), int(Mirror::DeleteMode::Keep));
  form->addRow(tr("Removed or changed files"), mMode);
  mDocs = new QCheckBox(tr("Copy Google Docs, Sheets and Slides as Office files"));
  mDocs->setToolTip(tr("They are not real files, so copying them is slow and they can look "
                       "changed on every run. Off: they are skipped."));
  form->addRow(QString(), mDocs);
  v->addLayout(form);
  QObject::connect(mMode, &QComboBox::currentIndexChanged, this, [this]() {
    for (int i = 0; i < mDests.size(); ++i) {
      showDestInfo(i);
    }
  });

  // values of the mirror being edited
  mExclude->setPlainText(mPlan.exclude.join('\n'));
  if (!mPlan.exclude.isEmpty()) {
    mSkipToggle->setChecked(true);
  }
  mMode->setCurrentIndex(mMode->findData(int(mPlan.mode)));
  mDocs->setChecked(mPlan.exportGoogleDocs);
  return page;
}

void MirrorDialog::fillDestinations() {
  const QString remote = sourceRemote();
  const QString path = selectedPath();
  const bool localSource = remote.isEmpty();
  mSourceIcon->setPixmap(
      Theme::remoteIcon(localSource ? QStringLiteral("local") : mRemotes.value(remote)).pixmap(36, 36));
  mSourceText->setStyleSheet(QString("color: %1;").arg(Theme::color("text2").name()));
  mSourceText->setText(tr("Source · %1").arg(localSource ? tr("This PC") : remote));
  mSourceName->setText(localSource      ? QDir::toNativeSeparators(path)
                       : path.isEmpty() ? tr("whole remote")
                                        : path);
  mDocs->setVisible(mRemotes.value(remote) == "drive");

  // keep what was typed for remotes that stay in the list
  QHash<QString, QPair<bool, QString>> previous;
  for (const Dest &d : std::as_const(mDests)) {
    previous.insert(d.remote, {d.on->isChecked(), d.path->text()});
  }
  if (previous.isEmpty()) {
    for (const Mirror::Target &t : std::as_const(mPlan.targets)) {
      previous.insert(t.remote, {true, t.local() ? QDir::toNativeSeparators(t.path) : t.path});
    }
  }
  while (QLayoutItem *item = mDestLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  mDests.clear();

  // a cloud copy of C:\\Users\\you\\Pictures goes to "Pictures" by default
  const QString defaultPath = localSource ? QFileInfo(path).fileName() : path.isEmpty() ? remote : path;
  // the other remotes, then a folder on this PC (empty name)
  struct Entry {
    QString name, type, title;
  };
  QList<Entry> entries;
  for (const QString &rname : std::as_const(mOrder)) {
    if (mRemotes.contains(rname) && rname != remote) {
      entries << Entry{rname, mRemotes.value(rname), rname};
    }
  }
  entries << Entry{QString(), QStringLiteral("local"), tr("This PC")};
  for (const Entry &entry : std::as_const(entries)) {
    const bool local = entry.name.isEmpty();
    Dest d;
    d.remote = entry.name;
    auto *card = new QWidget;
    card->setProperty("card", true);
    card->setAttribute(Qt::WA_StyledBackground);
    auto *cv = new QVBoxLayout(card);
    cv->setContentsMargins(12, 10, 12, 10);
    cv->setSpacing(6);
    auto *top = new QHBoxLayout;
    d.on = new QCheckBox;
    top->addWidget(d.on);
    auto *icon = new QLabel;
    icon->setPixmap(Theme::remoteIcon(entry.type).pixmap(30, 30));
    top->addWidget(icon);
    auto *name = new QLabel(QString("<b>%1</b>").arg(entry.title.toHtmlEscaped()));
    top->addWidget(name, 1);
    cv->addLayout(top);

    d.body = new QWidget;
    auto *bv = new QVBoxLayout(d.body);
    bv->setContentsMargins(36, 0, 0, 0);
    bv->setSpacing(4);
    auto *label = new QLabel(local ? tr("Folder on this PC")
                                   : tr("Destination folder on %1").arg(entry.name));
    label->setProperty("secondary", true);
    bv->addWidget(label);
    auto *row = new QHBoxLayout;
    d.path = new QLineEdit;
#ifdef Q_OS_WIN
    d.path->setPlaceholderText(local ? tr("for example: C:\\Backups\\Photos")
                                     : tr("for example: Backups/Photos"));
#else
    d.path->setPlaceholderText(local ? tr("for example: /home/you/Backups/Photos")
                                     : tr("for example: Backups/Photos"));
#endif
    row->addWidget(d.path, 1);
    auto *browse = new QPushButton(local ? tr("Browse…") : tr("Remote…"));
    browse->setIcon(Theme::icon("folder"));
    row->addWidget(browse);
    bv->addLayout(row);
    d.info = new QLabel;
    d.info->setTextFormat(Qt::PlainText);
    d.info->setWordWrap(true);
    bv->addWidget(d.info);
    cv->addWidget(d.body);
    mDestLayout->addWidget(card);

    const auto prev = previous.value(entry.name, {false, QString()});
    d.on->setChecked(prev.first);
    d.path->setText(!prev.second.isEmpty() ? prev.second : local ? QString() : defaultPath);
    d.body->setVisible(prev.first);
    const int i = int(mDests.size());
    mDests << d;

    QObject::connect(d.on, &QCheckBox::toggled, this, [this, i, local, browse](bool on) {
      mDests[i].body->setVisible(on);
      if (on && local && mDests[i].path->text().trimmed().isEmpty()) {
        browse->click(); // nothing chosen yet: open the folder picker straight away
      } else if (on && mDests[i].state == Dest::Unknown) {
        checkDest(i);
      }
    });
    QObject::connect(d.path, &QLineEdit::editingFinished, this, [this, i]() { checkDest(i); });
    QObject::connect(d.path, &QLineEdit::textChanged, mError, &QWidget::hide); // an old complaint
    QObject::connect(browse, &QPushButton::clicked, this, [this, i]() {
      Dest &d = mDests[i];
      if (d.remote.isEmpty()) { // a folder on this PC
        const QString current = Mirror::cleanLocalPath(d.path->text());
        const QString picked = QFileDialog::getExistingDirectory(
            this, tr("Choose a folder on this PC"),
            current.isEmpty() ? QDir::homePath() : current);
        if (!picked.isEmpty()) {
          d.path->setText(QDir::toNativeSeparators(picked));
          checkDest(i);
        }
        return;
      }
      RemoteFolderDialog pick(Mirror::remotePath(d.remote, d.path->text()), this);
      if (pick.exec() != QDialog::Accepted) {
        return;
      }
      const QString sel = pick.selected();
      const QString remote = sel.section(':', 0, 0);
      const QString folder = sel.section(':', 1);
      for (int k = 0; k < mDests.size(); ++k) {
        if (mDests[k].remote == remote) {
          mDests[k].on->setChecked(true);
          mDests[k].path->setText(folder);
          checkDest(k);
        }
      }
    });
    if (prev.first) {
      checkDest(i);
    }
  }
  if (mDests.isEmpty()) {
    auto *none = new QLabel(tr("Add another remote first: a mirror needs somewhere to copy to."));
    none->setProperty("secondary", true);
    mDestLayout->addWidget(none);
  }
  mDestLayout->addStretch(1);
}

void MirrorDialog::checkDest(int i) {
  Dest &d = mDests[i];
  bool ok = true;
  const QString path = cleanFor(d.remote, d.path->text(), &ok);
  const int gen = ++mSerial;
  d.generation = gen;
  if (!ok || path.isEmpty() || (d.remote.isEmpty() && !QDir::isAbsolutePath(path))) {
    d.state = Dest::Unknown;
    showDestInfo(i);
    return;
  }
  d.state = Dest::Loading;
  showDestInfo(i);
  MirrorUtil::runRclone(this, {"size", "--json", Mirror::remotePath(d.remote, path)},
                        [this, i, gen](int code, const QByteArray &out, const QString &err) {
                          if (i >= mDests.size() || mDests[i].generation != gen) {
                            return;
                          }
                          Dest &d = mDests[i];
                          if (code == 0) {
                            const QJsonObject o = QJsonDocument::fromJson(out).object();
                            d.files = o.value("count").toInteger();
                            d.bytes = o.value("bytes").toInteger();
                            d.state = d.files == 0 ? Dest::Empty : Dest::Holds;
                          } else {
                            d.state = err.contains("not found", Qt::CaseInsensitive) ? Dest::Missing
                                                                                       : Dest::Failed;
                          }
                          showDestInfo(i);
                        });
}

void MirrorDialog::showDestInfo(int i) {
  const Dest &d = mDests[i];
  const auto mode = Mirror::DeleteMode(mMode->currentData().toInt());
  QString text, color = Theme::color("text2").name();
  switch (d.state) {
  case Dest::Unknown:
    text = tr("Choose a folder.");
    break;
  case Dest::Loading:
    text = tr("Looking inside…");
    break;
  case Dest::Missing:
    text = tr("✓ New folder, created on the first sync.");
    color = Theme::color("success").name();
    break;
  case Dest::Empty:
    text = tr("Empty folder.");
    break;
  case Dest::Failed:
    text = tr("Could not look inside yet; it is created if needed.");
    break;
  case Dest::Holds: {
    const QString what = MirrorUtil::niceSize(d.bytes) + " · " + MirrorUtil::countText(d.files);
    bool current = false; // the copy this mirror already keeps
    for (const Mirror::Target &t : std::as_const(mPlan.targets)) {
      current = current || (mEditing && t.remote == d.remote &&
                            t.path == cleanFor(d.remote, d.path->text()) &&
                            mPlan.sourceRemote == sourceRemote() && mPlan.sourcePath == mSourcePath);
    }
    if (current) {
      text = tr("The current copy: %1.").arg(what);
    } else if (mode == Mirror::DeleteMode::Keep) {
      text = tr("Already holds %1. Files that aren't in the source are left alone.").arg(what);
    } else {
      text = mode == Mirror::DeleteMode::Trash
                 ? tr("Already holds %1. Files that aren't in the source will be moved to the "
                      "trash folder.").arg(what)
                 : tr("Already holds %1. Files that aren't in the source will be deleted.").arg(what);
      color = Theme::accent().name();
    }
    break;
  }
  }
  if (d.remote.isEmpty()) {
    bool ok = false;
    const QString guarded = Mirror::protectedFolder(cleanFor(d.remote, d.path->text(), &ok));
    if (ok && !guarded.isEmpty()) {
      text = tr("%1 cannot be a mirror destination: everything else in it would be removed. "
                "Choose a folder inside it.").arg(guarded);
      color = Theme::color("critical").name();
    }
  }
  d.info->setStyleSheet(QString("color: %1;").arg(color));
  d.info->setText(text);
}

void MirrorDialog::save() {
  auto fail = [this](const QString &msg) {
    mError->setText(msg);
    mError->show();
  };
  Mirror::Plan p = mPlan;
  p.sourceRemote = sourceRemote();
  p.sourcePath = selectedPath();
  p.targets.clear();
  for (const Dest &d : std::as_const(mDests)) {
    if (!d.on->isChecked()) {
      continue;
    }
    bool ok = true;
    const QString path = cleanFor(d.remote, d.path->text(), &ok);
    if (!ok) {
      return fail(tr("The folder on %1 is not valid.").arg(d.remote));
    }
    p.targets << Mirror::Target{d.remote, path};
  }
  QString err;
  p.exclude = Mirror::cleanExcludes(mExclude->toPlainText().split('\n'), &err);
  if (!err.isEmpty()) {
    return fail(err);
  }
  p.mode = Mirror::DeleteMode(mMode->currentData().toInt());
  p.exportGoogleDocs = mDocs->isVisible() && mDocs->isChecked();
  err = Mirror::validate(p, mAll);
  if (!err.isEmpty()) {
    return fail(err);
  }
  mPlan = p;
  accept();
}
