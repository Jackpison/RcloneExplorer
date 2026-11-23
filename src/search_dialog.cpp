#include "search_dialog.h"
#include "rclone_commands.h"
#include "rclone_output.h"
#include "theme.h"
#include "utils.h"

namespace {
// More matches than anyone reads through: the search stops there and asks
// for a longer name instead of filling memory on a huge remote.
constexpr int kMaxResults = 5000;

constexpr int kPathRole = Qt::UserRole;     // relative path of the match
constexpr int kSizeRole = Qt::UserRole + 1; // bytes, for sorting

enum Column { Name, Folder, Size, Modified };

// Sorts the Size column by bytes, not by its text ("9 KB" after "10 MB").
class ResultItem : public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other) const override {
    const int column = treeWidget() ? treeWidget()->sortColumn() : Name;
    if (column == Size) {
      return data(Size, kSizeRole).toULongLong() <
             other.data(Size, kSizeRole).toULongLong();
    }
    return QString::localeAwareCompare(text(column), other.text(column)) < 0;
  }
};
} // namespace

SearchDialog::SearchDialog(const QString &remote, const QString &folder,
                           bool isLocal, QWidget *parent)
    : QDialog(parent), mRemote(remote), mFolder(folder), mIsLocal(isLocal) {
  setWindowTitle(tr("Search %1").arg(remote));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(920, 540);

  auto *layout = new QVBoxLayout(this);

  auto *where = new QLabel;
  where->setTextFormat(Qt::PlainText); // names may hold < or &
  where->setWordWrap(true);
  const QString shown = mIsLocal ? QDir::toNativeSeparators(folder)
                        : folder.isEmpty() ? remote
                                           : remote + ":" + folder;
  where->setText(tr("Searches %1, including every folder inside it.").arg(shown));
  layout->addWidget(where);

  auto *row = new QHBoxLayout;
  mTerm = new QLineEdit;
  mTerm->setPlaceholderText(tr("Part of a file or folder name"));
  mTerm->setClearButtonEnabled(true);
  mTerm->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
  row->addWidget(mTerm, 1);
  mGo = new QPushButton(tr("Search"));
  // Enter is handled by the name field (starts) and the list (opens): a
  // default button would also get it, and stop the search just started
  mGo->setAutoDefault(false);
  row->addWidget(mGo);
  layout->addLayout(row);

  mResults = new QTreeWidget;
  mResults->setHeaderLabels({tr("Name"), tr("Folder"), tr("Size"), tr("Modified")});
  mResults->setRootIsDecorated(false);
  mResults->setUniformRowHeights(true);
  mResults->setAlternatingRowColors(false);
  mResults->setSelectionMode(QAbstractItemView::SingleSelection);
  mResults->header()->setStretchLastSection(false);
  mResults->header()->setSectionResizeMode(Name, QHeaderView::Interactive);
  mResults->header()->setSectionResizeMode(Folder, QHeaderView::Stretch);
  mResults->setColumnWidth(Name, 260);
  mResults->header()->setSectionResizeMode(Size, QHeaderView::ResizeToContents);
  mResults->header()->setSectionResizeMode(Modified, QHeaderView::ResizeToContents);
  mResults->headerItem()->setTextAlignment(Size, Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(mResults, 1);

  auto *bottom = new QHBoxLayout;
  mStatus = new QLabel;
  mStatus->setTextFormat(Qt::PlainText);
  mStatus->setWordWrap(true);
  bottom->addWidget(mStatus, 1);
  mOpen = new QPushButton(tr("Show in folder"));
  mOpen->setEnabled(false);
  mOpen->setAutoDefault(false);
  mOpen->setToolTip(tr("Opens the folder that holds it, with it selected"));
  bottom->addWidget(mOpen);
  auto *close = new QPushButton(tr("Close"));
  close->setAutoDefault(false);
  bottom->addWidget(close);
  layout->addLayout(bottom);

  QObject::connect(mGo, &QPushButton::clicked, this, [this]() {
    if (mProcess) {
      stop();
    } else {
      start();
    }
  });
  QObject::connect(mTerm, &QLineEdit::returnPressed, this, [this]() {
    if (!mProcess) {
      start();
    }
  });
  QObject::connect(close, &QPushButton::clicked, this, &QDialog::close);
  QObject::connect(mOpen, &QPushButton::clicked, this, &SearchDialog::openCurrent);
  QObject::connect(mResults, &QTreeWidget::itemActivated, this,
                   [this](QTreeWidgetItem *, int) { openCurrent(); });
  QObject::connect(mResults, &QTreeWidget::currentItemChanged, this,
                   [this](QTreeWidgetItem *current) { mOpen->setEnabled(current != nullptr); });

  mTerm->setFocus();
}

SearchDialog::~SearchDialog() {
  if (mProcess) {
    mProcess->disconnect(this);
    mProcess->kill();
  }
}

void SearchDialog::start() {
  const QString term = mTerm->text().trimmed();
  if (term.isEmpty()) {
    mTerm->setFocus();
    return;
  }
  mResults->clear();
  mResults->setSortingEnabled(false); // sorted once, at the end: much faster
  mPending.clear();
  mSearching = term;
  mFound = 0;
  mStopped = false;
  mCapped = false;

  mProcess = new QProcess(this);
  UseRclonePassword(mProcess);
  mProcess->setProcessChannelMode(QProcess::SeparateChannels);
  // Folders come through the filter whatever their name (rclone needs to
  // look inside them), so only files are matched by rclone; folder names
  // are matched here.
  // One ordered list of rules: rclone does not apply --include and
  // --exclude (hidden files) in a fixed order when both are given.
  QStringList filters;
  if (!GetShowHidden().isEmpty()) {
    filters << "--filter" << "- .*/**" << "--filter" << "- .*";
  }
  filters << "--filter" << "+ *" + RcloneCommands::globEscape(term) + "*"
          << "--filter" << "- *";
  const QStringList args =
      QStringList() << "lsjson" << "-R" << "--no-mimetype" << GetRcloneConf()
                    << GetDriveSharedWithMe() << "--ignore-case" << filters
                    << GetDefaultRcloneOptionsList()
                    << mRemote + ":" + mFolder;
  QObject::connect(mProcess, &QProcess::readyReadStandardOutput, this,
                   [this]() { readLines(false); });
  QObject::connect(mProcess,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [this]() { finished(); });
  QObject::connect(mProcess, &QProcess::errorOccurred, this,
                   [this](QProcess::ProcessError e) {
                     if (e == QProcess::FailedToStart) {
                       finished();
                     }
                   });
  mGo->setText(tr("Stop"));
  mTerm->setEnabled(false);
  Theme::setStatus(mStatus, "running");
  mStatus->setText(tr("Searching..."));
  mProcess->start(GetRclone(), args, QIODevice::ReadOnly);
}

void SearchDialog::stop() {
  if (mProcess) {
    mStopped = true;
    mProcess->kill(); // finished() follows
  }
}

void SearchDialog::readLines(bool all) {
  if (!mProcess) {
    return;
  }
  mPending += mProcess->readAllStandardOutput();
  const bool wasEmpty = mFound == 0;
  QList<QTreeWidgetItem *> batch; // added together: one update of the list
  int start = 0;
  int nl;
  while (!mCapped && (nl = mPending.indexOf('\n', start)) != -1) {
    const QByteArray line = mPending.mid(start, nl - start);
    start = nl + 1;
    const RcloneOutput::ListEntry e = RcloneOutput::parseLsJsonLine(line);
    if (!e.valid) {
      continue;
    }
    if (e.isDir && !e.name.contains(mSearching, Qt::CaseInsensitive)) {
      continue; // a folder rclone looked in on the way, not a match
    }
    const int slash = int(e.path.lastIndexOf('/'));
    QString folder = mFolder;
    if (slash > 0) {
      // cleanPath: the top of a local drive is "/" or "C:/" already
      folder = folder.isEmpty() ? e.path.left(slash)
                                : QDir::cleanPath(folder + "/" + e.path.left(slash));
    }
    if (mIsLocal) {
      folder = QDir::toNativeSeparators(folder);
    } else if (folder.isEmpty()) {
      folder = mRemote; // its top, as in the line above the results
    }
    auto *item = new ResultItem;
    item->setText(Name, e.name);
    item->setIcon(Name, Theme::fileIcon(e.name, e.isDir));
    item->setText(Folder, folder);
    item->setToolTip(Folder, folder);
    if (!e.isDir) {
      item->setText(Size, QLocale().formattedDataSize(
                              qint64(e.size), 1, QLocale::DataSizeTraditionalFormat));
    }
    item->setData(Size, kSizeRole, e.size);
    item->setTextAlignment(Size, Qt::AlignRight | Qt::AlignVCenter);
    item->setText(Modified, e.modified);
    item->setData(Name, kPathRole, e.path);
    batch << item;
    if (++mFound >= kMaxResults) {
      mCapped = true;
      mProcess->kill(); // enough: finished() follows
    }
  }
  mPending.remove(0, start);
  mResults->addTopLevelItems(batch);
  if (all && !mCapped && !mPending.trimmed().isEmpty()) {
    mPending += '\n';
    readLines(false);
    return;
  }
  if (mProcess && !mStopped && !mCapped) {
    mStatus->setText(tr("Searching... %n found", nullptr, mFound));
  }
  if (wasEmpty && mFound > 0 && !mResults->currentItem()) {
    // the first match is ready for Enter, as soon as there is one
    mResults->setCurrentItem(mResults->topLevelItem(0));
  }
}

void SearchDialog::finished() {
  if (!mProcess) {
    return;
  }
  readLines(true);
  QProcess *p = mProcess;
  mProcess = nullptr;
  p->deleteLater();

  const bool failed = !mStopped && !mCapped &&
                      (p->exitStatus() != QProcess::NormalExit || p->exitCode() != 0);
  QString error;
  if (failed) {
    const QStringList lines = QString::fromUtf8(p->readAllStandardError())
                                  .split('\n', Qt::SkipEmptyParts);
    static const QRegularExpression stamp(R"(^\d{4}/\d\d/\d\d \d\d:\d\d:\d\d \w+\s*: )");
    for (auto it = lines.crbegin(); it != lines.crend() && error.isEmpty(); ++it) {
      error = it->trimmed().remove(stamp);
    }
    if (error.isEmpty()) {
      error = p->error() == QProcess::FailedToStart
                  ? tr("rclone could not be started.")
                  : tr("rclone could not list it.");
    }
  }

  mResults->setSortingEnabled(true);
  mResults->sortByColumn(Name, Qt::AscendingOrder);
  if (mResults->currentItem()) {
    mResults->scrollToItem(mResults->currentItem());
  }
  mGo->setText(tr("Search"));
  mTerm->setEnabled(true);
  mTerm->setFocus();

  if (mCapped) {
    Theme::setStatus(mStatus, "error");
    mStatus->setText(tr("Stopped at %1 matches. Type more of the name to find "
                        "fewer.").arg(QLocale().toString(kMaxResults)));
  } else if (failed) {
    Theme::setStatus(mStatus, "error");
    mStatus->setText(mFound > 0 ? tr("%n found, then the search failed: %1",
                                     nullptr, mFound).arg(error)
                                : tr("The search failed: %1").arg(error));
  } else if (mStopped) {
    Theme::setStatus(mStatus, "done");
    mStatus->setText(tr("Stopped. %n found.", nullptr, mFound));
  } else {
    Theme::setStatus(mStatus, "done");
    mStatus->setText(mFound > 0 ? tr("%n found.", nullptr, mFound)
                                : tr("Nothing found."));
  }
}

void SearchDialog::openCurrent() {
  QTreeWidgetItem *item = mResults->currentItem();
  if (item) {
    emit openRequested(item->data(Name, kPathRole).toString());
  }
}
