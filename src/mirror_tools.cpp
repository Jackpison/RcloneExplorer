#include "mirror_tools.h"
#include "elided_label.h"
#include "rclone_output.h"
#include "theme.h"
#include "utils.h"

namespace MirrorStore {

QString dataDir() {
  QDir dir;
  if (IsPortableMode()) {
#ifdef Q_OS_WIN
    dir = QDir(qApp->applicationDirPath());
#else
    dir = QDir(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME")) + "/rclone-browser");
#endif
  } else {
    dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
  }
  if (!dir.exists()) {
    dir.mkpath(".");
  }
  return dir.absolutePath();
}

namespace {
QString plansFile() { return QDir(dataDir()).filePath("mirrors.json"); }
QString historyFile() { return QDir(dataDir()).filePath("mirror_history.json"); }

// A save in progress: the complete new content, kept until the real file has
// been written (see writeJson).
QString pendingFile(const QString &path) { return path + ".saving"; }

QJsonDocument readJson(const QString &path) {
  for (const QString &file : {path, pendingFile(path)}) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
      continue;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error == QJsonParseError::NoError) {
      return doc; // the file, or what an interrupted save left behind
    }
  }
  return {};
}

void writeJson(const QString &path, const QJsonDocument &doc) {
  // Written into the existing file rather than replacing it: Scoop keeps
  // these files across updates through hard links, which a replaced file
  // would cut (the same reason the ini and tasks.bin are written in place).
  // The new content is first saved completely beside it, so a crash or a
  // full disk halfway through loses nothing (readJson falls back to it).
  const QByteArray data = doc.toJson(QJsonDocument::Indented);
  QFile f(path);
  if (f.open(QIODevice::ReadOnly) && f.readAll() == data) {
    return;
  }
  f.close();
  QSaveFile pending(pendingFile(path));
  if (!pending.open(QIODevice::WriteOnly) || pending.write(data) != data.size() ||
      !pending.commit()) {
    return; // nothing changed
  }
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size() &&
      f.flush()) {
    f.close();
    QFile::remove(pendingFile(path));
  }
}

QJsonArray capped(const QStringList &l) {
  return QJsonArray::fromStringList(l.mid(0, 100));
}
QStringList strings(const QJsonValue &v) {
  QStringList l;
  for (const QJsonValue &x : v.toArray()) {
    l << x.toString();
  }
  return l;
}
} // namespace

QList<Mirror::Plan> loadPlans() { return Mirror::fromJson(readJson(plansFile()).array()); }

void savePlans(const QList<Mirror::Plan> &plans) {
  writeJson(plansFile(), QJsonDocument(Mirror::toJson(plans)));
}

QList<HistEntry> loadHistory() {
  QList<HistEntry> list;
  for (const QJsonValue &v : readJson(historyFile()).array()) {
    const QJsonObject o = v.toObject();
    HistEntry e;
    e.sync = o.value("kind").toString() != "check";
    e.started = QDateTime::fromString(o.value("started").toString(), Qt::ISODate);
    for (const QJsonValue &pv : o.value("pairs").toArray()) {
      const QJsonObject p = pv.toObject();
      HistPair h;
      h.source = p.value("source").toString();
      h.dest = p.value("dest").toString();
      h.status = p.value("status").toString();
      h.added = p.value("added").toInt();
      h.updated = p.value("updated").toInt();
      h.removed = p.value("removed").toInt();
      h.changes.added = strings(p.value("addedFiles"));
      h.changes.updated = strings(p.value("updatedFiles"));
      h.changes.removed = strings(p.value("removedFiles"));
      h.errors = strings(p.value("errors"));
      h.trash = p.value("trash").toString();
      e.pairs << h;
    }
    list << e;
  }
  return list;
}

void addHistory(const HistEntry &entry) {
  QJsonArray all = readJson(historyFile()).array();
  QJsonArray pairs;
  for (const HistPair &h : entry.pairs) {
    pairs.append(QJsonObject{{"source", h.source},
                             {"dest", h.dest},
                             {"status", h.status},
                             {"added", h.added},
                             {"updated", h.updated},
                             {"removed", h.removed},
                             {"addedFiles", capped(h.changes.added)},
                             {"updatedFiles", capped(h.changes.updated)},
                             {"removedFiles", capped(h.changes.removed)},
                             {"errors", capped(h.errors.mid(0, 5))},
                             {"trash", h.trash}});
  }
  all.prepend(QJsonObject{{"kind", entry.sync ? "sync" : "check"},
                          {"started", entry.started.toString(Qt::ISODate)},
                          {"pairs", pairs}});
  while (all.size() > 100) {
    all.removeLast();
  }
  writeJson(historyFile(), QJsonDocument(all));
}

void clearHistory() { writeJson(historyFile(), QJsonDocument(QJsonArray())); }

} // namespace MirrorStore

namespace MirrorUtil {

QString niceSize(qint64 bytes) {
  return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString countText(qint64 files) {
  return files == 1 ? QObject::tr("1 file")
                    : QObject::tr("%1 files").arg(QLocale().toString(files));
}

QString plural(qint64 n, const QString &one, const QString &many) {
  return (n == 1 ? one : many).arg(QLocale().toString(n));
}

QString lastError(const QString &stderrText) {
  const QStringList lines = stderrText.split('\n', Qt::SkipEmptyParts);
  for (int i = int(lines.size()) - 1; i >= 0; --i) {
    QString l = lines[i].trimmed();
    // "2026/11/13 09:46:00 ERROR : message" -> "message"
    static const QRegularExpression stamp(R"(^\d{4}/\d\d/\d\d \d\d:\d\d:\d\d \w+\s*: )");
    l.remove(stamp);
    if (!l.isEmpty() && !l.startsWith("Transferred:") && !l.startsWith("Elapsed")) {
      return l;
    }
  }
  return QString();
}

QProcess *runRclone(QObject *owner, const QStringList &args,
                    std::function<void(int, const QByteArray &, const QString &)> done) {
  auto *p = new QProcess(owner);
  UseRclonePassword(p);
  p->setProcessChannelMode(QProcess::SeparateChannels);
  QObject::connect(p, &QProcess::finished, owner,
                   [p, done](int code, QProcess::ExitStatus status) {
                     const QByteArray out = p->readAllStandardOutput();
                     const QString err = QString::fromUtf8(p->readAllStandardError());
                     p->deleteLater();
                     done(status == QProcess::NormalExit ? code : -1, out, err);
                   });
  QObject::connect(p, &QProcess::errorOccurred, owner, [p, done](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      p->deleteLater();
      done(-1, QByteArray(), QObject::tr("rclone could not be started."));
    }
  });
  p->start(GetRclone(), GetRcloneConf() + GetDefaultRcloneOptionsList() + args,
           QIODevice::ReadOnly);
  return p;
}

} // namespace MirrorUtil

// ------------------------------------------------------------- History --

MirrorHistoryDialog::MirrorHistoryDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Mirror history"));
  resize(760, 560);
  auto *v = new QVBoxLayout(this);
  auto *hint = new QLabel(tr("Every check and sync of your mirrors, newest first. "
                             "Open an entry to see what changed."));
  hint->setProperty("secondary", true);
  hint->setWordWrap(true);
  v->addWidget(hint);

  mTree = new QTreeWidget;
  mTree->setColumnCount(2);
  mTree->setHeaderLabels({tr("Run"), tr("Changes")});
  mTree->header()->setStretchLastSection(false);
  mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  mTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  mTree->setUniformRowHeights(true);
  v->addWidget(mTree, 1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  auto *clear = buttons->addButton(tr("Clear history"), QDialogButtonBox::ResetRole);
  clear->setProperty("destructive", true);
  clear->setAutoDefault(false); // Enter closes; clearing needs a deliberate click
  buttons->button(QDialogButtonBox::Close)->setDefault(true);
  QObject::connect(clear, &QPushButton::clicked, this, [this]() {
    if (QMessageBox::question(this, tr("Clear history"),
                              tr("Remove the whole history? Your files are not touched.")) ==
        QMessageBox::Yes) {
      MirrorStore::clearHistory();
      fill();
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  v->addWidget(buttons);
  fill();
}

void MirrorHistoryDialog::fill() {
  mTree->clear();
  const auto list = MirrorStore::loadHistory();
  if (list.isEmpty()) {
    auto *it = new QTreeWidgetItem(mTree, {tr("Nothing yet. Checks and syncs you run appear here.")});
    it->setFlags(Qt::NoItemFlags);
    return;
  }
  bool first = true;
  for (const auto &e : list) {
    int total = 0, problems = 0;
    for (const auto &p : e.pairs) {
      total += p.added + p.updated + p.removed;
      problems += p.status == "error" ? 1 : 0;
    }
    const QString when = QLocale().toString(e.started, QLocale::ShortFormat);
    const QString what = e.sync ? tr("Sync") : tr("Check");
    QString summary = total == 0 ? tr("no changes")
                      : e.sync   ? MirrorUtil::plural(total, tr("%1 change applied"), tr("%1 changes applied"))
                                 : MirrorUtil::plural(total, tr("%1 change found"), tr("%1 changes found"));
    if (problems) {
      summary += " · " + MirrorUtil::plural(problems, tr("%1 problem"), tr("%1 problems"));
    }
    auto *top = new QTreeWidgetItem(mTree, {what + " · " + when, summary});
    top->setIcon(0, Theme::icon(e.sync ? "update" : "dryrun"));
    if (problems) {
      top->setForeground(1, Theme::color("critical"));
    }
    for (const auto &p : e.pairs) {
      const QString counts = tr("%1 added · %2 updated · %3 removed")
                                 .arg(p.added).arg(p.updated).arg(p.removed);
      auto *pi = new QTreeWidgetItem(top, {p.source + "  →  " + p.dest, counts});
      pi->setIcon(0, Theme::icon(p.status == "error" ? "close" : "checkmark"));
      auto addFiles = [&](const QStringList &files, const QString &mark) {
        for (const QString &f : files) {
          new QTreeWidgetItem(pi, {mark + " " + f});
        }
      };
      addFiles(p.changes.added, "+");
      addFiles(p.changes.updated, "~");
      addFiles(p.changes.removed, "−");
      const int listed = int(p.changes.total());
      const int all = p.added + p.updated + p.removed;
      if (all > listed) {
        new QTreeWidgetItem(pi, {tr("… and %1 more").arg(all - listed)});
      }
      for (const QString &err : p.errors) {
        auto *ei = new QTreeWidgetItem(pi, {err});
        ei->setForeground(0, Theme::color("critical"));
      }
      if (!p.trash.isEmpty() && (p.updated || p.removed)) {
        new QTreeWidgetItem(pi, {tr("Old versions kept in %1").arg(p.trash)});
      }
    }
    top->setExpanded(first);
    first = false;
  }
}

// ------------------------------------------------------- Clean up trash --

MirrorTrashDialog::MirrorTrashDialog(const QList<Place> &places, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Clean up trash"));
  resize(600, 460);
  auto *v = new QVBoxLayout(this);
  auto *hint = new QLabel(
      tr("When a mirror removes or replaces a file, the old version is kept in a "
         "<b>%1</b> folder on the destination. Delete the old ones to free space. "
         "Your copies are not touched.").arg(Mirror::trashDir()));
  hint->setWordWrap(true);
  hint->setProperty("secondary", true);
  v->addWidget(hint);

  auto *form = new QFormLayout;
  mAge = new QComboBox;
  mAge->addItem(tr("7 days"), 7);
  mAge->addItem(tr("14 days"), 14);
  mAge->addItem(tr("30 days"), 30);
  mAge->addItem(tr("90 days"), 90);
  mAge->addItem(tr("any age (everything)"), 0);
  mAge->setCurrentIndex(2);
  form->addRow(tr("Delete trash older than"), mAge);
  v->addLayout(form);

  auto *list = new QWidget;
  auto *lv = new QVBoxLayout(list);
  lv->setContentsMargins(0, 6, 0, 0);
  lv->setSpacing(10);
  for (const Place &place : places) {
    Row r;
    r.root = place.root;
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    auto *icon = new QLabel;
    icon->setPixmap(Theme::remoteIcon(place.type).pixmap(32, 32));
    h->addWidget(icon);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(0);
    auto *name = new ElidedLabel(place.title); // long folder paths shortened in the middle
    name->setAutoToolTip(true);
    name->setStyleSheet("font-weight: 600;");
    texts->addWidget(name);
    r.text = new QLabel;
    r.text->setTextFormat(Qt::PlainText); // may hold an rclone error
    r.text->setProperty("secondary", true);
    r.text->setWordWrap(true);
    texts->addWidget(r.text);
    h->addLayout(texts, 1);
    lv->addWidget(row);
    mRows << r;
  }
  if (mRows.isEmpty()) {
    auto *none = new QLabel(tr("No mirror destinations yet."));
    none->setProperty("secondary", true);
    lv->addWidget(none);
  }
  lv->addStretch(1);
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setWidget(list);
  v->addWidget(scroll, 1);

  auto *bottom = new QHBoxLayout;
  mTotal = new QLabel;
  mTotal->setProperty("secondary", true);
  bottom->addWidget(mTotal, 1);
  auto *close = new QPushButton(tr("Close"));
  close->setDefault(true);
  QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);
  bottom->addWidget(close);
  mDelete = new QPushButton(tr("Delete permanently"));
  mDelete->setAutoDefault(false);
  mDelete->setProperty("destructive", true);
  mDelete->setIcon(Theme::icon("delete"));
  QObject::connect(mDelete, &QPushButton::clicked, this, [this]() { clean(); });
  bottom->addWidget(mDelete);
  v->addLayout(bottom);

  QObject::connect(mAge, &QComboBox::currentIndexChanged, this, [this]() { scan(); });
  scan();
}

QDateTime MirrorTrashDialog::cutoff() const {
  const int days = mAge->currentData().toInt();
  return days == 0 ? QDateTime::currentDateTime().addSecs(60)
                   : QDateTime::currentDateTime().addDays(-days);
}

void MirrorTrashDialog::scan() {
  const int gen = ++mGeneration;
  for (int i = 0; i < mRows.size(); ++i) {
    Row &r = mRows[i];
    r.snapshots.clear();
    r.old.clear();
    r.files = r.bytes = 0;
    r.cleaned = 0;
    r.error.clear();
    r.busy = true;
    scanRow(i, gen);
  }
  update();
}

void MirrorTrashDialog::scanRow(int i, int gen) {
  const QString root = mRows[i].root;
  MirrorUtil::runRclone(
      this, {"lsjson", "--dirs-only", "--no-mimetype", root},
      [this, i, gen](int code, const QByteArray &out, const QString &err) {
        if (gen != mGeneration) {
          return;
        }
        Row &r = mRows[i];
        if (code != 0) {
          if (!err.contains("not found", Qt::CaseInsensitive)) {
            r.error = MirrorUtil::lastError(err);
          }
          r.busy = false;
          update();
          return;
        }
        bool ok = false;
        for (const auto &e : RcloneOutput::parseLsJson(out, &ok)) {
          if (e.isDir) {
            r.snapshots << e.name;
          }
        }
        r.old = Mirror::oldSnapshots(r.snapshots, cutoff());
        // keep only real snapshot names in the total too
        r.snapshots = Mirror::oldSnapshots(r.snapshots, QDateTime::currentDateTime().addYears(100));
        sizeNext(i, 0, gen);
      });
}

void MirrorTrashDialog::sizeNext(int i, int k, int gen) {
  Row &r = mRows[i];
  if (k >= r.old.size()) {
    r.busy = false;
    update();
    return;
  }
  const QString path = r.root + "/" + r.old[k];
  MirrorUtil::runRclone(this, {"size", "--json", path},
                        [this, i, k, gen](int code, const QByteArray &out, const QString &) {
                          if (gen != mGeneration) {
                            return;
                          }
                          if (code == 0) {
                            const QJsonObject o = QJsonDocument::fromJson(out).object();
                            mRows[i].files += o.value("count").toInteger();
                            mRows[i].bytes += o.value("bytes").toInteger();
                          }
                          sizeNext(i, k + 1, gen);
                        });
}

void MirrorTrashDialog::clean() {
  int old = 0;
  qint64 bytes = 0;
  for (const Row &r : mRows) {
    old += int(r.old.size());
    bytes += r.bytes;
  }
  if (QMessageBox::warning(this, tr("Delete permanently"),
                           tr("Permanently delete %1 (%2)? This cannot be undone.")
                               .arg(MirrorUtil::plural(old, tr("%1 trash snapshot"), tr("%1 trash snapshots")),
                                    MirrorUtil::niceSize(bytes)),
                           QMessageBox::Yes | QMessageBox::Cancel,
                           QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }
  mCleaning = true;
  ++mGeneration; // a scan still running must not overwrite the results
  for (int i = 0; i < mRows.size(); ++i) {
    if (!mRows[i].old.isEmpty()) {
      mRows[i].busy = true;
      cleanNext(i, 0);
    }
  }
  update();
}

void MirrorTrashDialog::cleanNext(int i, int k) {
  Row &r = mRows[i];
  if (k >= r.old.size()) {
    r.busy = false;
    update();
    return;
  }
  const QString path = r.root + "/" + r.old[k];
  MirrorUtil::runRclone(this, {"purge", path},
                        [this, i, k](int code, const QByteArray &, const QString &err) {
                          if (code != 0) {
                            mRows[i].error = MirrorUtil::lastError(err);
                            mRows[i].busy = false;
                            update();
                            return;
                          }
                          mRows[i].cleaned++;
                          cleanNext(i, k + 1);
                        });
}

void MirrorTrashDialog::update() {
  int old = 0;
  qint64 files = 0, bytes = 0;
  bool busy = false;
  for (Row &r : mRows) {
    old += int(r.old.size());
    files += r.files;
    bytes += r.bytes;
    busy = busy || r.busy;
    QString t;
    if (r.busy) {
      t = mCleaning ? tr("Deleting…") : tr("Looking…");
    } else if (!r.error.isEmpty()) {
      t = r.error;
    } else if (r.snapshots.isEmpty()) {
      t = tr("No trash");
    } else if (mCleaning) {
      t = MirrorUtil::plural(r.cleaned, tr("%1 snapshot deleted"), tr("%1 snapshots deleted"));
    } else if (r.old.isEmpty()) {
      t = MirrorUtil::plural(r.snapshots.size(), tr("%1 snapshot, none that old"), tr("%1 snapshots, none that old"));
    } else {
      t = tr("%1 of %2 can go · %3 · %4")
              .arg(r.old.size())
              .arg(MirrorUtil::plural(r.snapshots.size(), tr("%1 snapshot"), tr("%1 snapshots")))
              .arg(MirrorUtil::countText(r.files), MirrorUtil::niceSize(r.bytes));
    }
    r.text->setText(t);
  }
  mAge->setEnabled(!busy && !mCleaning);
  mDelete->setVisible(!mCleaning);
  mDelete->setEnabled(!busy && old > 0);
  mTotal->setText(busy || mCleaning || old == 0
                      ? QString()
                      : tr("Frees %1 · %2").arg(MirrorUtil::niceSize(bytes),
                                                MirrorUtil::countText(files)));
}
