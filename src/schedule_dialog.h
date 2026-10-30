#pragma once

#include "pch.h"
#include "schedule.h"

// Edit when a saved task runs by itself.
class ScheduleDialog : public QDialog {
  Q_OBJECT

public:
  ScheduleDialog(const QString &taskName, const Schedule &current,
                 QWidget *parent = nullptr);

  Schedule schedule() const;

private:
  QComboBox *mKind = nullptr;
  QSpinBox *mEvery = nullptr;
  QComboBox *mUnit = nullptr;
  QTimeEdit *mTime = nullptr;
  QList<QCheckBox *> mDays; // Monday .. Sunday
  QWidget *mEveryRow = nullptr;
  QWidget *mTimeRow = nullptr;
  QWidget *mDaysRow = nullptr;
  void updateVisibility();
};
