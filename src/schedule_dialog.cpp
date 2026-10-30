#include "schedule_dialog.h"

ScheduleDialog::ScheduleDialog(const QString &taskName, const Schedule &s,
                               QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Schedule “%1”").arg(taskName));
  setMinimumWidth(420);

  auto *layout = new QVBoxLayout(this);

  mKind = new QComboBox;
  mKind->addItem(tr("Not scheduled"), Schedule::Off);
  mKind->addItem(tr("Repeat every…"), Schedule::Interval);
  mKind->addItem(tr("Every day"), Schedule::Daily);
  mKind->addItem(tr("On selected days"), Schedule::Weekly);
  layout->addWidget(mKind);

  // repeat every N minutes / hours
  mEveryRow = new QWidget;
  auto *every = new QHBoxLayout(mEveryRow);
  every->setContentsMargins(0, 0, 0, 0);
  mEvery = new QSpinBox;
  mEvery->setRange(1, 999);
  mUnit = new QComboBox;
  mUnit->addItem(tr("minutes"), 1);
  mUnit->addItem(tr("hours"), 60);
  every->addWidget(mEvery);
  every->addWidget(mUnit);
  every->addStretch(1);
  layout->addWidget(mEveryRow);

  // time of day
  mTimeRow = new QWidget;
  auto *time = new QHBoxLayout(mTimeRow);
  time->setContentsMargins(0, 0, 0, 0);
  time->addWidget(new QLabel(tr("At")));
  mTime = new QTimeEdit;
  mTime->setDisplayFormat("HH:mm");
  time->addWidget(mTime);
  time->addStretch(1);
  layout->addWidget(mTimeRow);

  // weekdays
  mDaysRow = new QWidget;
  auto *days = new QHBoxLayout(mDaysRow);
  days->setContentsMargins(0, 0, 0, 0);
  for (int d = 1; d <= 7; ++d) {
    auto *box = new QCheckBox(QLocale().dayName(d, QLocale::ShortFormat));
    mDays << box;
    days->addWidget(box);
  }
  layout->addWidget(mDaysRow);

  auto *note = new QLabel(
      tr("Runs while Rclone Explorer is open, including in the tray. "
         "Times missed while it was closed are skipped."));
  note->setWordWrap(true);
  note->setProperty("secondary", true);
  layout->addWidget(note);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);
  layout->addWidget(buttons);

  // current values
  mKind->setCurrentIndex(qMax(0, mKind->findData(s.kind)));
  const bool hours = s.intervalMinutes % 60 == 0;
  mUnit->setCurrentIndex(hours ? 1 : 0);
  mEvery->setValue(hours ? s.intervalMinutes / 60 : s.intervalMinutes);
  mTime->setTime(s.time);
  for (int i = 0; i < 7; ++i) {
    mDays[i]->setChecked(s.weekdays & (1 << i));
  }
  if (s.kind != Schedule::Weekly && s.weekdays == 0) {
    for (auto *box : mDays) { // sensible starting point: every day
      box->setChecked(true);
    }
  }

  QObject::connect(mKind, &QComboBox::currentIndexChanged, this,
                   [this](int) { updateVisibility(); });
  updateVisibility();
}

void ScheduleDialog::updateVisibility() {
  const auto kind = Schedule::Kind(mKind->currentData().toInt());
  mEveryRow->setVisible(kind == Schedule::Interval);
  mTimeRow->setVisible(kind == Schedule::Daily || kind == Schedule::Weekly);
  mDaysRow->setVisible(kind == Schedule::Weekly);
  adjustSize();
}

Schedule ScheduleDialog::schedule() const {
  Schedule s;
  s.kind = Schedule::Kind(mKind->currentData().toInt());
  s.intervalMinutes = mEvery->value() * mUnit->currentData().toInt();
  s.time = mTime->time();
  s.weekdays = 0;
  for (int i = 0; i < 7; ++i) {
    if (mDays[i]->isChecked()) {
      s.weekdays |= 1 << i;
    }
  }
  if (!s.enabled()) {
    s.kind = Schedule::Off;
  }
  return s;
}
