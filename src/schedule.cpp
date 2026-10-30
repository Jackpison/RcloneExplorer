#include "schedule.h"

#include <QLocale>
#include <QStringList>

bool Schedule::enabled() const {
  switch (kind) {
  case Interval:
    return intervalMinutes >= 1;
  case Daily:
    return time.isValid();
  case Weekly:
    return time.isValid() && (weekdays & 0x7f) != 0;
  case Off:
    break;
  }
  return false;
}

QDateTime Schedule::next(const QDateTime &after) const {
  if (!enabled()) {
    return QDateTime();
  }
  if (kind == Interval) {
    return after.addSecs(qint64(intervalMinutes) * 60);
  }
  for (int i = 0; i <= 7; ++i) {
    const QDate d = after.date().addDays(i);
    if (kind == Weekly && !(weekdays & dayBit(Qt::DayOfWeek(d.dayOfWeek())))) {
      continue;
    }
    const QDateTime candidate(d, time);
    if (candidate > after) {
      return candidate;
    }
  }
  return QDateTime();
}

QString Schedule::describe() const {
  const QString t = QLocale().toString(time, QLocale::ShortFormat);
  switch (kind) {
  case Interval:
    if (intervalMinutes % 60 == 0) {
      const int h = intervalMinutes / 60;
      return h == 1 ? QStringLiteral("Every hour")
                    : QStringLiteral("Every %1 hours").arg(h);
    }
    return intervalMinutes == 1
               ? QStringLiteral("Every minute")
               : QStringLiteral("Every %1 minutes").arg(intervalMinutes);
  case Daily:
    return QStringLiteral("Daily at %1").arg(t);
  case Weekly: {
    QStringList days;
    for (int d = 1; d <= 7; ++d) {
      if (weekdays & dayBit(Qt::DayOfWeek(d))) {
        days << QLocale().dayName(d, QLocale::ShortFormat);
      }
    }
    return QStringLiteral("%1 at %2").arg(days.join(", "), t);
  }
  case Off:
    break;
  }
  return QStringLiteral("Not scheduled");
}

QString Schedule::serialize() const {
  static const char *names[] = {"off", "interval", "daily", "weekly"};
  return QStringLiteral("%1|%2|%3|%4")
      .arg(QLatin1String(names[kind]))
      .arg(intervalMinutes)
      .arg(time.toString("HH:mm"))
      .arg(weekdays);
}

Schedule Schedule::parse(const QString &text) {
  Schedule s;
  const QStringList p = text.split('|');
  if (p.size() != 4) {
    return s;
  }
  if (p[0] == "interval") {
    s.kind = Interval;
  } else if (p[0] == "daily") {
    s.kind = Daily;
  } else if (p[0] == "weekly") {
    s.kind = Weekly;
  } else {
    return s;
  }
  bool ok = false;
  const int minutes = p[1].toInt(&ok);
  s.intervalMinutes = ok ? qBound(1, minutes, 7 * 24 * 60) : 60;
  const QTime t = QTime::fromString(p[2], "HH:mm");
  if (t.isValid()) {
    s.time = t;
  }
  s.weekdays = p[3].toInt() & 0x7f;
  if (!s.enabled()) {
    s.kind = Off;
  }
  return s;
}
