#include "job_widget.h"
#include "elided_label.h"
#include "rclone_output.h"
#include "theme.h"
#include "utils.h"


// ---------------------------------------------------------------------------
// One running file: name | progress bar | percent. A plain horizontal layout
// with explicit vertical centring, so the thin bar sits on the text's middle
// line (in a form layout it was pinned to the top of the row).
// ---------------------------------------------------------------------------


class FileProgressRow : public QWidget {
public:
  explicit FileProgressRow(const QString &name, QWidget *parent = nullptr)
      : QWidget(parent), mName(name) {
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 3, 0, 3);
    h->setSpacing(12);

    auto *label = new ElidedLabel(name);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(label, 5, Qt::AlignVCenter);

    mBar = new QProgressBar;
    mBar->setRange(0, 100);
    mBar->setTextVisible(false);
    mBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    h->addWidget(mBar, 4, Qt::AlignVCenter);

    mPercent = new QLabel("0%");
    mPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mPercent->setFixedWidth(mPercent->fontMetrics().horizontalAdvance("100%") + 6);
    h->addWidget(mPercent, 0, Qt::AlignVCenter);
  }

  void setProgress(int percent, const QString &detail) {
    mBar->setValue(percent);
    mPercent->setText(QString::number(percent) + "%");
    setToolTip(mName + "\n" + detail);
  }

private:
  QString mName;
  QProgressBar *mBar;
  QLabel *mPercent;
};

JobWidget::JobWidget(QProcess *process, const QString &info,
                     const QStringList &args, const QString &source,
                     const QString &dest, QWidget *parent)
    : QWidget(parent), mProcess(process) {
  ui.setupUi(this);
  ui.output->setMaximumBlockCount(5000); // the log keeps its newest lines instead of growing forever
  setAttribute(Qt::WA_StyledBackground);
  buildFluentLayout();

  mArgs.append(QDir::toNativeSeparators(GetRclone()));
  mArgs.append(GetRcloneConf());
  mArgs.append(args);

  ui.source->setText(source);
  ui.dest->setText(dest);
  // setText leaves the cursor at the end, which scrolls a long path so that
  // its beginning is cut off: show the start
  ui.source->setCursorPosition(0);
  ui.dest->setCursorPosition(0);
  ui.source->setToolTip(source);
  ui.dest->setToolTip(dest);
  ui.info->setText(info);

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
      if (Theme::confirm(this, tr("Transfer"), tr("Cancel this transfer?"),
                         tr("rclone is still running. Cancelling stops the "
                            "transfer now."),
                         tr("Cancel transfer"), tr("Keep running"))) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  ui.copy->setIcon(
      Theme::icon("copy"));

  QObject::connect(ui.copy, &QToolButton::clicked, this, [=, this]() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(mArgs.join(" "));
  });

  QObject::connect(mProcess, &QProcess::readyRead, this, [=, this]() {
    using RcloneOutput::StatsLine;

    while (mProcess->canReadLine()) {
      QString line = QString::fromUtf8(mProcess->readLine()).trimmed();
      ui.output->appendPlainText(line);

      if (line.isEmpty()) {
        // end of a stats block: drop progress bars of files that finished
        for (auto it = mActive.begin(), eit = mActive.end(); it != eit;
             /* empty */) {
          FileProgressRow *row = it.value();
          if (mUpdated.contains(row)) {
            ++it;
          } else {
            it = mActive.erase(it);
            ui.progress->removeRow(row); // also deletes it
          }
        }
        mUpdated.clear();
        continue;
      }

      const StatsLine s = RcloneOutput::parseStatsLine(line);
      switch (s.kind) {
      case StatsLine::Bytes:
        if (s.total.isEmpty()) { // rclone <= 1.42
          ui.size->setText(s.done);
        } else {
          ui.size->setText(s.done + ", " + s.percent);
          ui.totalsize->setText(s.total);
          ui.eta->setText(s.eta == "-" ? QString() : s.eta); // placeholder shows a dash
        }
        ui.bandwidth->setText(s.speed);
        if (mSummary && !s.total.isEmpty()) {
          QString eta = s.eta == "-" ? QString() : tr(" · %1 left").arg(s.eta);
          mSummary->setText(QString("%1 · %2%3").arg(s.percent, s.speed, eta));
        }
        break;
      case StatsLine::Files:
        ui.transferred->setText(s.text);
        break;
      case StatsLine::Checks:
        ui.checks->setText(s.text);
        break;
      case StatsLine::Errors:
        ui.errors->setText(s.text);
        break;
      case StatsLine::Elapsed:
        ui.elapsed->setText(s.text);
        break;
      case StatsLine::FileProgress: {
        FileProgressRow *row;
        auto it = mActive.find(s.name);
        if (it == mActive.end()) {
          row = new FileProgressRow(s.name);
          ui.progress->addRow(row);
          mActive.insert(s.name, row);
        } else {
          row = it.value();
        }
        row->setProgress(s.filePercent, s.detail);
        mUpdated.insert(row);
        break;
      }
      case StatsLine::None:
        break;
      }
    }
  });

  QObject::connect(mProcess,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=, this](int status, QProcess::ExitStatus) {
                     mProcess->deleteLater();
                     for (FileProgressRow *row : std::as_const(mActive)) {
                       ui.progress->removeRow(row);
                     }
                     mActive.clear();
                     mUpdated.clear();

                     mRunning = false;
                     mPaused = false;
                     if (mPause) {
                       mPause->hide();
                     }
                     if (status == 0) {
                       Theme::setStatus(ui.showDetails, "done");
                       ui.showDetails->setText("Finished");
                     } else {
                       Theme::setStatus(ui.showDetails, "error");
                       ui.showDetails->setText("Error");
                     }
                     // replace the last live reading ("100% · 3.9 MiB/s ·
                     // 0s left") with a result summary
                     if (mSummary) {
                       const QString total =
                           !ui.totalsize->text().isEmpty()
                               ? ui.totalsize->text()
                               : ui.size->text().section(',', 0, 0);
                       const QString took = ui.elapsed->text();
                       QString text;
                       if (status == 0) {
                         text = took.isEmpty() ? tr("Done")
                                               : tr("Done · %1 in %2").arg(total, took);
                       } else {
                         text = took.isEmpty() ? tr("Stopped")
                                               : tr("Stopped after %1").arg(took);
                       }
                       mSummary->setText(text);
                     }

                     ui.cancel->setToolTip("Close");

                     emit finished(ui.info->text(), status == 0);
                   });

  Theme::setStatus(ui.showDetails, "running");
  ui.showDetails->setText("Running");
}

JobWidget::~JobWidget() {}

void JobWidget::showDetails() { ui.showDetails->setChecked(true); }

void JobWidget::cancel() {
  if (!mRunning) {
    return;
  }

  mProcess->kill();
  mProcess->waitForFinished();

  emit closed();
}

// ---------------------------------------------------------------------------
// Windows 11 card layout and pause / resume
// ---------------------------------------------------------------------------

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

void JobWidget::buildFluentLayout() {
  auto *grid = qobject_cast<QGridLayout *>(ui.details->layout());
  QLayout *progress = ui.progress;
  ui.progress->setContentsMargins(0, 0, 0, 0);
  ui.progress->setVerticalSpacing(0);

  // empty the old 8-column grid (widgets stay alive, only re-parented)
  while (QLayoutItem *it = grid->takeAt(0)) {
    if (it->layout()) {
      it->layout()->setParent(nullptr);
    } else {
      delete it;
    }
  }
  delete grid;

  auto *v = new QVBoxLayout(ui.details);
  v->setContentsMargins(0, 6, 0, 0);
  v->setSpacing(8);

  // source / destination as boxed path fields
  auto *paths = new QGridLayout;
  paths->setHorizontalSpacing(10);
  paths->setVerticalSpacing(6);
  ui.label_3->setText(tr("From"));
  ui.label->setText(tr("To"));
  for (QLabel *l : {ui.label_3, ui.label}) {
    l->setObjectName("StatLabel");
  }
  for (QLineEdit *e : {ui.source, ui.dest}) {
    e->setProperty("pathValue", true);
    e->setMinimumWidth(0);
    e->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    e->setCursorPosition(0);
  }
  paths->addWidget(ui.label_3, 0, 0);
  paths->addWidget(ui.source, 0, 1);
  paths->addWidget(ui.label, 1, 0);
  paths->addWidget(ui.dest, 1, 1);
  paths->setColumnStretch(1, 1);
  v->addLayout(paths);

  // statistics as tiles: caption on top, value below
  struct S { QLabel *label; QLineEdit *value; QString caption; };
  const QList<S> stats = {
      {ui.label_5, ui.size, tr("Transferred")},
      {ui.label_4a, ui.totalsize, tr("Total size")},
      {ui.label_2, ui.bandwidth, tr("Speed")},
      {ui.label_4, ui.eta, tr("Time left")},
      {ui.label_6, ui.elapsed, tr("Elapsed")},
      {ui.label_9, ui.transferred, tr("Files")},
      {ui.label_8, ui.checks, tr("Checks")},
      {ui.label_7, ui.errors, tr("Errors")},
  };
  auto *tiles = new QGridLayout;
  tiles->setSpacing(8);
  for (int i = 0; i < stats.size(); ++i) {
    auto *tile = new QWidget;
    tile->setObjectName("StatTile");
    tile->setAttribute(Qt::WA_StyledBackground);
    auto *tl = new QVBoxLayout(tile);
    tl->setContentsMargins(12, 8, 12, 8);
    tl->setSpacing(2);
    stats[i].label->setText(stats[i].caption);
    stats[i].label->setObjectName("StatLabel");
    stats[i].value->setProperty("statValue", true);
    stats[i].value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    stats[i].value->setMinimumWidth(0);
    stats[i].value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    stats[i].value->setPlaceholderText(QStringLiteral("–"));
    tl->addWidget(stats[i].label);
    tl->addWidget(stats[i].value);
    tiles->addWidget(tile, i / 4, i % 4);
  }
  for (int c = 0; c < 4; ++c) {
    tiles->setColumnStretch(c, 1);
  }
  v->addLayout(tiles);

  v->addLayout(progress);
  v->addWidget(ui.showOutput, 0, Qt::AlignLeft);
  v->addWidget(ui.output);

  // the summary line must never force the card wider than the window
  ui.info->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  ui.info->setAutoToolTip(true);

  // pause / resume
  mPause = new QToolButton;
  mPause->setIcon(Theme::icon("pause"));
  mPause->setToolTip(tr("Pause transfer"));
  QObject::connect(mPause, &QToolButton::clicked, this,
                   [this]() { setPaused(!mPaused); });
  auto *header = qobject_cast<QHBoxLayout *>(ui.widget->layout());
  header->insertWidget(header->indexOf(ui.cancel), mPause);
  mSummary = new QLabel;
  mSummary->setObjectName("JobSummary");
  header->insertWidget(header->indexOf(ui.copy), mSummary);
  // the title takes the free room (and is elided in the middle when it is
  // still too long); the result summary keeps only what it needs
  header->setStretchFactor(ui.info, 1);
  ui.cancel->setToolTip(tr("Cancel transfer"));
}

void JobWidget::setPaused(bool paused) {
  if (!mRunning || paused == mPaused) {
    return;
  }
  const qint64 pid = mProcess->processId();
  if (pid <= 0) {
    return;
  }
  bool ok = false;
#ifdef Q_OS_WIN
  // Suspend / resume every thread of the rclone process.
  using NtProcFn = LONG(NTAPI *)(HANDLE);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto fn = reinterpret_cast<NtProcFn>(reinterpret_cast<void *>(GetProcAddress(
      ntdll, paused ? "NtSuspendProcess" : "NtResumeProcess")));
  HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, DWORD(pid));
  if (fn && h) {
    ok = fn(h) >= 0;
  }
  if (h) {
    CloseHandle(h);
  }
#else
  ok = ::kill(pid_t(pid), paused ? SIGSTOP : SIGCONT) == 0;
#endif
  if (!ok) {
    return;
  }
  mPaused = paused;
  mPause->setIcon(Theme::icon(paused ? "resume" : "pause"));
  mPause->setToolTip(paused ? tr("Resume transfer")
                            : tr("Pause transfer"));
  Theme::setStatus(ui.showDetails, paused ? "paused" : "running");
  ui.showDetails->setText(paused ? tr("Paused") : tr("Running"));
  if (paused) {
    ui.bandwidth->setText(QStringLiteral("0 B/s"));
  }
}
