#include "mirror_page.h"
#include "job_widget.h"
#include "mirror_dialog.h"
#include "mirror_tools.h"
#include "schedule_dialog.h"
#include "theme.h"
#include "utils.h"

namespace {

QLabel *badge(const QString &text, const char *kind) {
  auto *l = new QLabel(text);
  l->setProperty("badge", kind);
  return l;
}

// "Gallery › Trips › Paris" with the last part bold, for rich-text labels.
QString crumbs(const QString &path, const QString &emptyText) {
  if (path.isEmpty()) {
    return "<b>" + emptyText.toHtmlEscaped() + "</b>";
  }
  const QStringList parts = path.split('/');
  const QString dim = Theme::color("text2").name();
  QString out;
  for (int i = 0; i < parts.size(); ++i) {
    if (i == parts.size() - 1) {
      out += "<b>" + parts[i].toHtmlEscaped() + "</b>";
    } else {
      out += QString("<span style='color:%1'>%2 › </span>").arg(dim, parts[i].toHtmlEscaped());
    }
  }
  return out;
}

qsizetype lines_shown(const Mirror::Changes &c) {
  return qMin<qsizetype>(c.added.size() + c.updated.size() + c.removed.size(), 1001);
}

QToolButton *cardButton(const QString &icon, const QString &tip) {
  auto *b = new QToolButton;
  b->setIcon(Theme::icon(icon));
  b->setIconSize(QSize(18, 18));
  b->setToolTip(tip);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  return b;
}

QString scheduleKey(const QUuid &id) {
  return "MirrorSchedules/" + id.toString(QUuid::WithoutBraces);
}

} // namespace

MirrorPage::MirrorPage(AddTransfer addTransfer, std::function<void()> showTransfers,
                       QWidget *parent)
    : QWidget(parent), mAddTransfer(std::move(addTransfer)),
      mShowTransfers(std::move(showTransfers)) {
  mPlans = MirrorStore::loadPlans();
  loadSchedules();

  auto *v = new QVBoxLayout(this);
  v->setContentsMargins(32, 28, 32, 16);
  v->setSpacing(10);

  auto *title = new QLabel(tr("Mirror"));
  title->setObjectName("PageTitle");
  auto *subtitle = new QLabel(tr("Keep a folder, or any sub-folder, identical on your other remotes."));
  subtitle->setObjectName("PageSubtitle");
  subtitle->setWordWrap(true);
  v->addWidget(title);
  v->addWidget(subtitle);

  auto *bar = new QHBoxLayout;
  bar->setSpacing(8);
  mNew = new QPushButton(Theme::icon("add", QColor("#1A1405")), tr("New mirror"));
  mNew->setProperty("accent", true);
  mCheckAll = new QPushButton(Theme::icon("dryrun"), tr("Check all"));
  mCheckAll->setToolTip(tr("List what a sync would change, without changing anything"));
  mSyncAll = new QPushButton(Theme::icon("update"), tr("Sync all"));
  mHistory = new QPushButton(Theme::icon("tasks"), tr("History"));
  mTrash = new QPushButton(Theme::icon("delete"), tr("Clean up trash…"));
  auto *refresh = new QPushButton(Theme::icon("refresh"), QString());
  refresh->setToolTip(tr("Count sizes and files again"));
  QObject::connect(refresh, &QPushButton::clicked, this, [this]() {
    for (auto it = mSizes.begin(); it != mSizes.end();) {
      it = it.value().state == Size::Loading ? std::next(it) : mSizes.erase(it);
    }
    rebuild();
  });
  bar->addWidget(mNew);
  bar->addWidget(mCheckAll);
  bar->addWidget(mSyncAll);
  bar->addStretch(1);
  bar->addWidget(refresh);
  bar->addWidget(mHistory);
  bar->addWidget(mTrash);
  v->addSpacing(6);
  v->addLayout(bar);

  // banner: progress and results of the current / last run
  mBanner = new QWidget;
  mBanner->setObjectName("MirrorBanner");
  mBanner->setAttribute(Qt::WA_StyledBackground);
  auto *bh = new QHBoxLayout(mBanner);
  bh->setContentsMargins(16, 12, 10, 12);
  auto *bt = new QVBoxLayout;
  bt->setSpacing(2);
  mBannerTitle = new QLabel;
  mBannerTitle->setObjectName("MirrorBannerTitle");
  mBannerSub = new QLabel;
  mBannerSub->setProperty("secondary", true);
  bt->addWidget(mBannerTitle);
  bt->addWidget(mBannerSub);
  bh->addLayout(bt, 1);
  mGoTransfers = new QPushButton(Theme::icon("transfers"), tr("Show transfers"));
  mStop = new QPushButton(Theme::icon("stop"), tr("Stop"));
  mApply = new QPushButton(Theme::icon("update", QColor("#1A1405")), tr("Apply these changes"));
  mApply->setProperty("accent", true);
  mDismiss = new QToolButton;
  mDismiss->setIcon(Theme::icon("close"));
  mDismiss->setAutoRaise(true);
  mDismiss->setToolTip(tr("Hide"));
  bh->addWidget(mGoTransfers);
  bh->addWidget(mStop);
  bh->addWidget(mApply);
  bh->addWidget(mDismiss);
  mBanner->hide();
  v->addWidget(mBanner);

  mScroll = new QScrollArea;
  mScroll->setWidgetResizable(true);
  mScroll->setFrameShape(QFrame::NoFrame);
  mScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  v->addWidget(mScroll, 1);

  QObject::connect(mNew, &QPushButton::clicked, this, [this]() { editPlan(QUuid()); });
  QObject::connect(mCheckAll, &QPushButton::clicked, this, [this]() { start(false, {}); });
  QObject::connect(mSyncAll, &QPushButton::clicked, this, [this]() { start(true, {}); });
  QObject::connect(mHistory, &QPushButton::clicked, this, [this]() {
    MirrorHistoryDialog d(this);
    d.exec();
  });
  QObject::connect(mTrash, &QPushButton::clicked, this, [this]() {
    QMap<QString, QString> targets;
    for (const auto &p : std::as_const(mPlans)) {
      for (const auto &t : p.targets) {
        targets.insert(t.remote, mRemotes.value(t.remote));
      }
    }
    MirrorTrashDialog d(targets, this);
    d.exec();
  });
  QObject::connect(mGoTransfers, &QPushButton::clicked, this, [this]() { mShowTransfers(); });
  QObject::connect(mStop, &QPushButton::clicked, this, [this]() { stop(); });
  QObject::connect(mApply, &QPushButton::clicked, this, [this]() {
    QList<QUuid> ids;
    for (const Pair &p : std::as_const(mPairs)) {
      if (!ids.contains(p.plan)) {
        ids << p.plan;
      }
    }
    start(true, ids);
  });
  QObject::connect(mDismiss, &QToolButton::clicked, this, [this]() {
    mDismissed = true;
    updateBanner();
    rebuild();
  });

  mRebuild.setSingleShot(true);
  mRebuild.setInterval(60);
  QObject::connect(&mRebuild, &QTimer::timeout, this, [this]() { rebuild(); });
  mTimer.setInterval(30 * 1000);
  QObject::connect(&mTimer, &QTimer::timeout, this, [this]() { tick(); });
  mTimer.start();
  QObject::connect(Theme::notifier(), &Theme::Notifier::changed, this, [this]() { rebuild(); });
  rebuild();
}

MirrorPage::~MirrorPage() {
  for (Pair &p : mPairs) {
    if (p.proc) {
      p.proc->disconnect();
      p.proc->kill();
    }
  }
}

void MirrorPage::setRemotes(const QMap<QString, QString> &remotes, const QStringList &order) {
  mRemotes = remotes;
  mOrder = order;
  rebuild();
}

const Mirror::Plan *MirrorPage::find(const QUuid &id) const {
  for (const auto &p : mPlans) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

void MirrorPage::save() { MirrorStore::savePlans(mPlans); }

// ------------------------------------------------------------- sizes --

MirrorPage::Size MirrorPage::wantSize(const QString &remote, const QString &path,
                                      const Mirror::Plan &plan) {
  const QStringList filters = Mirror::filterArgs(plan);
  const QString target = Mirror::remotePath(remote, path);
  const QString key = target + "|" + filters.join(' ');
  auto it = mSizes.find(key);
  if (it != mSizes.end()) {
    return it.value();
  }
  if (!mRemotes.contains(remote)) {
    Size s;
    s.state = Size::Failed;
    return s;
  }
  Size s;
  s.state = Size::Loading;
  mSizes.insert(key, s);
  mSizeQueue.append({key, QStringList{"size", "--json", target} + filters});
  pumpSizes();
  return s;
}

void MirrorPage::pumpSizes() {
  while (mSizeRunning < 3 && !mSizeQueue.isEmpty()) {
    const auto job = mSizeQueue.takeFirst();
    ++mSizeRunning;
    MirrorUtil::runRclone(this, job.second,
                          [this, key = job.first](int code, const QByteArray &out, const QString &err) {
                            --mSizeRunning;
                            Size s;
                            if (code == 0) {
                              const QJsonObject o = QJsonDocument::fromJson(out).object();
                              s.state = Size::Ready;
                              s.files = o.value("count").toInteger();
                              s.bytes = o.value("bytes").toInteger();
                            } else {
                              s.state = err.contains("not found", Qt::CaseInsensitive) ? Size::Missing
                                                                                         : Size::Failed;
                            }
                            if (mSizes.contains(key)) {
                              mSizes.insert(key, s);
                            }
                            pumpSizes();
                            scheduleRebuild();
                          });
  }
}

void MirrorPage::dropSizes(const QString &remotePath) {
  for (auto it = mSizes.begin(); it != mSizes.end();) {
    if (it.key().startsWith(remotePath + "|") && it.value().state != Size::Loading) {
      it = mSizes.erase(it);
    } else {
      ++it;
    }
  }
}

QString MirrorPage::sizeText(const Size &s) const {
  switch (s.state) {
  case Size::Loading:
  case Size::Unknown:
    return tr("counting…");
  case Size::Missing:
    return tr("not created yet");
  case Size::Failed:
    return tr("size unavailable");
  case Size::Ready:
    break;
  }
  return MirrorUtil::niceSize(s.bytes) + " · " + MirrorUtil::countText(s.files);
}

// ------------------------------------------------------------ editing --

void MirrorPage::editPlan(const QUuid &id) {
  const Mirror::Plan *existing = id.isNull() ? nullptr : find(id);
  MirrorDialog dialog(mRemotes, mOrder, mPlans, existing, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  const Mirror::Plan p = dialog.plan();
  bool replaced = false;
  for (auto &x : mPlans) {
    if (x.id == p.id) {
      x = p;
      replaced = true;
    }
  }
  if (!replaced) {
    mPlans << p;
  }
  save();
  rebuild();
}

void MirrorPage::removePlan(const QUuid &id) {
  const Mirror::Plan *p = find(id);
  if (!p ||
      QMessageBox::question(this, tr("Remove mirror"),
                            tr("Remove the mirror of %1? Files already copied stay where they are.")
                                .arg(p->title())) != QMessageBox::Yes) {
    return;
  }
  mPlans.removeIf([&](const Mirror::Plan &x) { return x.id == id; });
  GetSettings()->remove(scheduleKey(id));
  mSchedules.remove(id);
  mNextRun.remove(id);
  save();
  rebuild();
}

// ---------------------------------------------------------- schedules --

void MirrorPage::loadSchedules() {
  auto settings = GetSettings();
  settings->beginGroup("MirrorSchedules");
  for (const QString &key : settings->childKeys()) {
    const QUuid id(key);
    const Schedule s = Schedule::parse(settings->value(key).toString());
    if (!id.isNull() && s.enabled()) {
      mSchedules.insert(id, s);
    }
  }
  settings->endGroup();
}

void MirrorPage::editSchedule(const QUuid &id) {
  const Mirror::Plan *p = find(id);
  if (!p) {
    return;
  }
  ScheduleDialog dialog(tr("Mirror of %1").arg(p->title()), mSchedules.value(id), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  const Schedule s = dialog.schedule();
  if (s.enabled()) {
    GetSettings()->setValue(scheduleKey(id), s.serialize());
    mSchedules.insert(id, s);
  } else {
    GetSettings()->remove(scheduleKey(id));
    mSchedules.remove(id);
  }
  mNextRun.remove(id);
  tick(); // works out the next run
  rebuild();
}

void MirrorPage::tick() {
  const QDateTime now = QDateTime::currentDateTime();
  for (const auto &p : std::as_const(mPlans)) {
    const auto s = mSchedules.constFind(p.id);
    if (s == mSchedules.constEnd()) {
      continue;
    }
    auto next = mNextRun.find(p.id);
    if (next == mNextRun.end()) {
      mNextRun.insert(p.id, s->next(now));
      continue;
    }
    if (now < next.value() || mActive) {
      continue; // not due, or wait for the run in progress
    }
    mNextRun[p.id] = s->next(now);
    start(true, {p.id}, false);
    return; // one at a time; the others follow on the next ticks
  }
}

// ---------------------------------------------------------------- runs --

void MirrorPage::start(bool sync, QList<QUuid> ids, bool interactive) {
  if (mActive) {
    return;
  }
  if (ids.isEmpty()) {
    for (const auto &p : std::as_const(mPlans)) {
      ids << p.id;
    }
  }
  QList<Pair> pairs;
  bool deletes = false;
  for (const QUuid &id : ids) {
    const Mirror::Plan *p = find(id);
    if (!p) {
      continue;
    }
    deletes = deletes || p->mode == Mirror::DeleteMode::Delete;
    for (int t = 0; t < p->targets.size(); ++t) {
      Pair pair;
      pair.plan = p->id;
      pair.target = t;
      pair.source = Mirror::remotePath(p->sourceRemote, p->sourcePath);
      pair.dest = Mirror::remotePath(p->targets[t].remote, p->targets[t].path);
      pair.destRemote = p->targets[t].remote;
      pair.status = "waiting";
      pairs << pair;
    }
  }
  if (pairs.isEmpty()) {
    return;
  }
  if (sync && deletes && interactive &&
      QMessageBox::warning(this, tr("Sync"),
                           tr("Files removed from the source will be permanently deleted from "
                              "the destinations of at least one of these mirrors. Continue?"),
                           QMessageBox::Yes | QMessageBox::Cancel,
                           QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }
  mPairs = pairs;
  mSync = sync;
  mActive = true;
  mStopping = false;
  mDismissed = false;
  mOpenFiles.clear();
  mBusy.clear();
  mStarted = QDateTime::currentDateTime();
  mStamp = mStarted.toString(Mirror::stampFormat());
  mTmp = std::make_unique<QTemporaryDir>();
  pump();
  updateBanner();
  rebuild();
}

void MirrorPage::pump() {
  for (int i = 0; i < mPairs.size(); ++i) {
    if (mPairs[i].status == "waiting" && !mBusy.contains(mPairs[i].destRemote)) {
      startPair(i);
    }
  }
}

void MirrorPage::startPair(int i) {
  Pair &pair = mPairs[i];
  const Mirror::Plan *plan = find(pair.plan);
  if (!plan || pair.target >= plan->targets.size()) {
    pair.status = "error";
    pair.errors << tr("This mirror was changed or removed.");
    return;
  }
  const Mirror::Target target = plan->targets[pair.target];
  pair.status = "running";
  mBusy.insert(pair.destRemote);
  Mirror::RunOptions o;
  o.dryRun = !mSync;
  o.stamp = mStamp;
  o.reportFile = mTmp->filePath(QString("report-%1.txt").arg(i));
  pair.report = o.reportFile;
  if (mSync && plan->mode == Mirror::DeleteMode::Trash) {
    pair.trash = Mirror::remotePath(target.remote, Mirror::trashDir() + "/" + mStamp + "/" + target.path);
  }
  const QStringList args = Mirror::syncArgs(*plan, target, o);

  if (mSync) {
    JobWidget *job = mAddTransfer(tr("Mirror %1 → %2").arg(pair.source, pair.dest), pair.source,
                                  pair.dest, GetDefaultRcloneOptionsList() + args);
    pair.job = job;
    if (!job) {
      pairDone(i, -1, tr("The transfer could not be started."));
      return;
    }
    QObject::connect(job, &JobWidget::finished, this, [this, i](const QString &, bool ok) {
      pairDone(i, ok ? 0 : 1, ok ? QString() : tr("See the transfer card on the Transfers page for details."));
    });
    QObject::connect(job, &QObject::destroyed, this,
                     [this, i]() { pairDone(i, -2, tr("The transfer was closed.")); });
  } else {
    pair.proc = MirrorUtil::runRclone(this, args, [this, i](int code, const QByteArray &, const QString &err) {
      pairDone(i, code, code == 0 ? QString() : MirrorUtil::lastError(err));
    });
  }
}

void MirrorPage::pairDone(int i, int code, const QString &error) {
  if (i >= mPairs.size() || mPairs[i].status != "running") {
    return;
  }
  Pair &pair = mPairs[i];
  if (const Mirror::Plan *plan = find(pair.plan)) {
    QFile report(pair.report);
    if (report.open(QIODevice::ReadOnly)) {
      pair.changes = Mirror::parseReport(report.readAll(), plan->mode);
    }
  }
  pair.status = code == 0 ? "done" : mStopping ? "cancelled" : "error";
  if (code != 0 && !mStopping && !error.isEmpty()) {
    pair.errors << error;
  }
  mBusy.remove(pair.destRemote);
  if (mSync) {
    dropSizes(pair.dest);
  }
  if (!mStopping) {
    pump();
  }
  bool open = false;
  for (const Pair &p : std::as_const(mPairs)) {
    open = open || p.status == "waiting" || p.status == "running";
  }
  if (!open) {
    finishRun();
  } else {
    updateBanner();
    scheduleRebuild();
  }
}

void MirrorPage::stop() {
  mStopping = true;
  for (Pair &p : mPairs) {
    if (p.status == "waiting") {
      p.status = "cancelled";
    } else if (p.status == "running") {
      if (p.job) {
        p.job->cancel();
      }
      if (p.proc) {
        p.proc->kill();
      }
    }
  }
  bool open = false;
  for (const Pair &p : std::as_const(mPairs)) {
    open = open || p.status == "running";
  }
  if (!open) {
    finishRun();
  }
}

void MirrorPage::finishRun() {
  mActive = false;
  for (const Pair &p : std::as_const(mPairs)) {
    dropSizes(p.source);
    dropSizes(p.dest);
  }
  MirrorStore::HistEntry e;
  e.sync = mSync;
  e.started = mStarted;
  for (const Pair &p : std::as_const(mPairs)) {
    MirrorStore::HistPair h;
    h.source = p.source;
    h.dest = p.dest;
    h.status = p.status;
    h.changes = p.changes;
    h.added = int(p.changes.added.size());
    h.updated = int(p.changes.updated.size());
    h.removed = int(p.changes.removed.size());
    h.errors = p.errors;
    h.trash = p.trash;
    e.pairs << h;
  }
  MirrorStore::addHistory(e);
  updateBanner();
  rebuild();
  QTimer::singleShot(0, this, [this]() { tick(); }); // a schedule may have waited for us
}

void MirrorPage::updateBanner() {
  const bool show = mActive || (!mPairs.isEmpty() && !mDismissed);
  mBanner->setVisible(show);
  if (!show) {
    return;
  }
  int total = 0, problems = 0, stopped = 0;
  for (const Pair &p : std::as_const(mPairs)) {
    total += p.changes.total();
    problems += p.status == "error";
    stopped += p.status == "cancelled";
  }
  QString title, sub;
  if (mActive) {
    title = mSync ? tr("Syncing…") : tr("Checking for changes…");
    sub = mSync ? tr("Progress is shown on the Transfers page.") : tr("Nothing is changed by a check.");
  } else if (problems) {
    title = MirrorUtil::plural(problems, tr("Finished with problems in %1 copy"), tr("Finished with problems in %1 copies"));
  } else if (stopped) {
    title = tr("Stopped");
  } else if (!mSync) {
    title = total ? MirrorUtil::plural(total, tr("%1 change waiting"), tr("%1 changes waiting")) : tr("Everything is already up to date");
    sub = total ? tr("Nothing has been changed yet.") : QString();
  } else {
    title = total ? MirrorUtil::plural(total, tr("Sync complete: %1 change applied"), tr("Sync complete: %1 changes applied"))
                  : tr("Sync complete: everything was already up to date");
  }
  mBannerTitle->setText(title);
  mBannerSub->setText(sub);
  mBannerSub->setVisible(!sub.isEmpty());
  mGoTransfers->setVisible(mSync);
  mStop->setVisible(mActive);
  mApply->setVisible(!mActive && !mSync && total > 0 && !problems && !stopped);
  mDismiss->setVisible(!mActive);
}

// ---------------------------------------------------------- interface --

QWidget *MirrorPage::emptyState() {
  auto *w = new QWidget;
  auto *v = new QVBoxLayout(w);
  v->addStretch(1);
  auto *icon = new QLabel;
  icon->setPixmap(Theme::icon("update", Theme::accent()).pixmap(56, 56));
  icon->setAlignment(Qt::AlignCenter);
  v->addWidget(icon);
  const bool enough = mRemotes.size() >= 2;
  auto *text = new QLabel(
      enough ? tr("<b>No mirrors yet</b><br><br>Pick a folder, or any sub-folder, of one remote "
                  "and choose where its copies should live.")
             : tr("<b>Add at least two remotes</b><br><br>A mirror copies a folder of one remote "
                  "to others. Click <b>New remote</b> on the Home page."));
  text->setAlignment(Qt::AlignCenter);
  text->setProperty("secondary", true);
  text->setWordWrap(true);
  v->addWidget(text);
  if (enough) {
    auto *b = new QPushButton(Theme::icon("add", QColor("#1A1405")), tr("New mirror"));
    b->setProperty("accent", true);
    QObject::connect(b, &QPushButton::clicked, this, [this]() { editPlan(QUuid()); });
    auto *h = new QHBoxLayout;
    h->addStretch(1);
    h->addWidget(b);
    h->addStretch(1);
    v->addLayout(h);
  }
  v->addStretch(2);
  return w;
}

void MirrorPage::rebuild() {
  const int scroll = mScroll->verticalScrollBar()->value();
  auto *host = new QWidget;
  auto *v = new QVBoxLayout(host);
  v->setContentsMargins(0, 4, 6, 4);
  v->setSpacing(14);
  if (mPlans.isEmpty()) {
    v->addWidget(emptyState(), 1);
  } else {
    for (const auto &p : std::as_const(mPlans)) {
      v->addWidget(planCard(p));
    }
    v->addStretch(1);
  }
  mScroll->setWidget(host); // deletes the previous list
  mListHost = host;
  QTimer::singleShot(0, this, [this, scroll]() { mScroll->verticalScrollBar()->setValue(scroll); });

  const bool idle = !mActive;
  mNew->setEnabled(idle && mRemotes.size() >= 2);
  mCheckAll->setEnabled(idle && !mPlans.isEmpty());
  mSyncAll->setEnabled(idle && !mPlans.isEmpty());
  mTrash->setEnabled(idle && !mPlans.isEmpty());
}

QWidget *MirrorPage::planCard(const Mirror::Plan &p) {
  auto *card = new QWidget;
  card->setProperty("card", true);
  card->setAttribute(Qt::WA_StyledBackground);
  auto *v = new QVBoxLayout(card);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);

  auto *head = new QWidget;
  auto *h = new QHBoxLayout(head);
  h->setContentsMargins(16, 12, 12, 12);
  h->setSpacing(12);
  auto *icon = new QLabel;
  icon->setPixmap(Theme::remoteIcon(mRemotes.value(p.sourceRemote)).pixmap(40, 40));
  h->addWidget(icon);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(1);
  auto *title = new QLabel(crumbs(p.sourcePath, tr("%1 (whole remote)").arg(p.sourceRemote)));
  title->setObjectName("MirrorTitle");
  texts->addWidget(title);
  const Size src = wantSize(p.sourceRemote, p.sourcePath, p);
  QString meta = p.sourceRemote + " · " +
                 (mRemotes.contains(p.sourceRemote) ? sizeText(src) : tr("remote not found"));
  if (!p.exclude.isEmpty()) {
    meta += " · " + MirrorUtil::plural(p.exclude.size(), tr("skipping %1 pattern"), tr("skipping %1 patterns"));
  }
  auto *metaLabel = new QLabel(meta);
  metaLabel->setProperty("secondary", true);
  if (!p.exclude.isEmpty()) {
    metaLabel->setToolTip(p.exclude.join(", "));
  }
  texts->addWidget(metaLabel);
  h->addLayout(texts, 1);

  // Manual, or when it runs by itself
  const auto sched = mSchedules.constFind(p.id);
  const bool scheduled = sched != mSchedules.constEnd();
  auto *chip = new QPushButton(scheduled ? sched->describe() : tr("Manual"));
  chip->setObjectName("ScheduleChip");
  chip->setProperty("scheduled", scheduled);
  chip->setCursor(Qt::PointingHandCursor);
  QString tip = tr("Runs only when you press Sync. Click to schedule it.");
  if (scheduled) {
    const QDateTime next = mNextRun.value(p.id);
    tip = next.isValid() ? tr("Next run: %1. Click to change.").arg(QLocale().toString(next, QLocale::ShortFormat))
                         : tr("Click to change the schedule.");
  }
  chip->setToolTip(tip);
  QObject::connect(chip, &QPushButton::clicked, this, [this, id = p.id]() { editSchedule(id); });
  h->addWidget(chip);

  auto *check = cardButton("dryrun", tr("Check this mirror for changes"));
  auto *sync = cardButton("update", tr("Sync this mirror now"));
  auto *edit = cardButton("edit", tr("Edit"));
  auto *remove = cardButton("delete", tr("Remove"));
  for (QToolButton *b : {check, sync, edit, remove}) {
    b->setEnabled(!mActive);
    h->addWidget(b);
  }
  QObject::connect(check, &QToolButton::clicked, this, [this, id = p.id]() { start(false, {id}); });
  QObject::connect(sync, &QToolButton::clicked, this, [this, id = p.id]() { start(true, {id}); });
  QObject::connect(edit, &QToolButton::clicked, this, [this, id = p.id]() { editPlan(id); });
  QObject::connect(remove, &QToolButton::clicked, this, [this, id = p.id]() { removePlan(id); });
  v->addWidget(head);

  auto *dests = new QWidget;
  dests->setObjectName("MirrorDests");
  dests->setAttribute(Qt::WA_StyledBackground);
  auto *dv = new QVBoxLayout(dests);
  dv->setContentsMargins(0, 0, 0, 0);
  dv->setSpacing(0);
  for (int t = 0; t < p.targets.size(); ++t) {
    if (t > 0) {
      auto *line = new QFrame;
      line->setObjectName("NavDivider");
      dv->addWidget(line);
    }
    dv->addWidget(destRow(p, t, src));
  }
  v->addWidget(dests);
  return card;
}

QWidget *MirrorPage::destRow(const Mirror::Plan &p, int t, const Size &source) {
  const Mirror::Target &target = p.targets[t];
  auto *row = new QWidget;
  auto *v = new QVBoxLayout(row);
  v->setContentsMargins(16, 10, 16, 10);
  v->setSpacing(6);
  auto *h = new QHBoxLayout;
  h->setSpacing(12);
  auto *arrow = new QLabel;
  arrow->setPixmap(Theme::icon("update", Theme::color("text3")).pixmap(16, 16));
  h->addWidget(arrow);
  auto *icon = new QLabel;
  icon->setPixmap(Theme::remoteIcon(mRemotes.value(target.remote)).pixmap(30, 30));
  h->addWidget(icon);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(1);
  auto *name = new QLabel(QString("<b>%1</b> <span style='color:%2'>:</span> %3")
                              .arg(target.remote.toHtmlEscaped(), Theme::color("text3").name(),
                                   crumbs(target.path, QString())));
  texts->addWidget(name);
  const Size dest = wantSize(target.remote, target.path, p);
  auto *meta = new QLabel(mRemotes.contains(target.remote) ? sizeText(dest) : tr("remote not found"));
  meta->setProperty("secondary", true);
  texts->addWidget(meta);
  h->addLayout(texts, 1);

  if (source.state == Size::Ready && dest.state == Size::Missing) {
    h->addWidget(badge(tr("New"), "new"));
  } else if (source.state == Size::Ready && dest.state == Size::Ready) {
    const bool same = source.files == dest.files && source.bytes == dest.bytes;
    h->addWidget(same ? badge(tr("✓ Matches"), "ok") : badge(tr("Differs"), "warn"));
  }

  // results of the current / last run for this destination
  const Pair *pair = nullptr;
  if (!mDismissed || mActive) {
    for (const Pair &x : std::as_const(mPairs)) {
      if (x.plan == p.id && x.target == t) {
        pair = &x;
      }
    }
  }
  if (pair) {
    if (pair->status == "waiting") {
      h->addWidget(badge(tr("Waiting"), "new"));
    } else if (pair->status == "running") {
      h->addWidget(badge(mSync ? tr("Syncing…") : tr("Checking…"), "warn"));
    } else if (pair->status == "done") {
      h->addWidget(badge(mSync ? tr("Done") : tr("Checked"), "ok"));
    } else if (pair->status == "error") {
      h->addWidget(badge(tr("Problem"), "bad"));
    } else {
      h->addWidget(badge(tr("Stopped"), "new"));
    }
  }
  v->addLayout(h);

  if (pair && pair->status != "waiting" && pair->status != "running") {
    const Mirror::Changes &c = pair->changes;
    const bool dry = !mSync;
    if (c.total() > 0) {
      auto *counts = new QHBoxLayout;
      counts->setContentsMargins(70, 0, 0, 0);
      counts->addWidget(badge(dry ? tr("%1 to add").arg(c.added.size()) : tr("%1 added").arg(c.added.size()), "ok"));
      counts->addWidget(badge(dry ? tr("%1 to update").arg(c.updated.size()) : tr("%1 updated").arg(c.updated.size()), "warn"));
      if (p.mode != Mirror::DeleteMode::Keep) {
        const QString what = p.mode == Mirror::DeleteMode::Trash
                                 ? (dry ? tr("%1 to move to trash") : tr("%1 moved to trash"))
                                 : (dry ? tr("%1 to delete") : tr("%1 deleted"));
        counts->addWidget(badge(what.arg(c.removed.size()), c.removed.isEmpty() ? "new" : "bad"));
      }
      counts->addStretch(1);
      v->addLayout(counts);

      const QString key = p.id.toString() + "/" + QString::number(t);
      auto *toggle = new QToolButton;
      toggle->setText(tr("Show files"));
      toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      toggle->setAutoRaise(true);
      toggle->setCheckable(true);
      toggle->setChecked(mOpenFiles.contains(key));
      toggle->setArrowType(toggle->isChecked() ? Qt::DownArrow : Qt::RightArrow);
      auto *th = new QHBoxLayout;
      th->setContentsMargins(64, 0, 0, 0);
      th->addWidget(toggle);
      th->addStretch(1);
      v->addLayout(th);
      auto *files = new QPlainTextEdit;
      files->setObjectName("MirrorFiles");
      files->setReadOnly(true);
      // room for up to 10 lines, so a short list is not squeezed
      files->setFixedHeight(qBound(2, int(lines_shown(c)), 10) * QFontMetrics(Theme::monoFont()).lineSpacing() + 18);
      QStringList lines;
      for (const QString &f : c.added) lines << "+ " + f;
      for (const QString &f : c.updated) lines << "~ " + f;
      for (const QString &f : c.removed) lines << "− " + f;
      files->setPlainText(lines.mid(0, 1000).join('\n') +
                          (lines.size() > 1000 ? tr("\n… and %1 more").arg(lines.size() - 1000) : QString()));
      files->setVisible(toggle->isChecked());
      auto *fh = new QHBoxLayout;
      fh->setContentsMargins(70, 0, 0, 0);
      fh->addWidget(files);
      v->addLayout(fh);
      QObject::connect(toggle, &QToolButton::toggled, this, [this, toggle, files, key](bool on) {
        files->setVisible(on);
        toggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        if (on) {
          mOpenFiles.insert(key);
        } else {
          mOpenFiles.remove(key);
        }
      });
      if (!dry && !pair->trash.isEmpty() && (c.updated.size() || c.removed.size())) {
        auto *note = new QLabel(tr("Old versions kept in %1").arg(pair->trash));
        note->setProperty("secondary", true);
        note->setContentsMargins(70, 0, 0, 0);
        note->setWordWrap(true);
        v->addWidget(note);
      }
    } else if (pair->status == "done") {
      auto *ok = new QLabel(tr("Up to date"));
      ok->setProperty("secondary", true);
      ok->setContentsMargins(70, 0, 0, 0);
      v->addWidget(ok);
    }
    for (const QString &err : pair->errors) {
      auto *e = new QLabel(err);
      e->setWordWrap(true);
      e->setProperty("badge", "bad");
      v->addWidget(e);
    }
  }
  return row;
}
