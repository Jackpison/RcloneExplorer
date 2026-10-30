#pragma once

#include <QDateTime>
#include <QString>
#include <QTime>

// When a saved task runs by itself. Evaluated while the app is running
// (including minimised to the tray); runs missed while it was closed are
// skipped, not caught up.
struct Schedule {
  enum Kind { Off, Interval, Daily, Weekly };

  Kind kind = Off;
  int intervalMinutes = 60;
  QTime time = QTime(2, 0);
  int weekdays = 0; // bit 0 = Monday ... bit 6 = Sunday

  bool enabled() const;

  // First run strictly after `after`; invalid when the schedule is off.
  QDateTime next(const QDateTime &after) const;

  QString describe() const;
  QString serialize() const;
  static Schedule parse(const QString &text);

  static int dayBit(Qt::DayOfWeek d) { return 1 << (static_cast<int>(d) - 1); }
};
