#include "rclone_output.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace RcloneOutput {

StatsLine parseStatsLine(const QString &rawLine) {
  // All patterns are anchored with ^...$ (QRegularExpression::match is not
  // implicitly exact the way QRegExp::exactMatch was).
  // Size/speed. Formats seen in the wild:
  //  <=1.42 : Transferred:   1.234 MBytes (500 kBytes/s)
  //  1.43+  : Transferred:   10.000M / 100.000 MBytes, 10%, 5.000 MBytes/s, ETA 18s
  //  1.56+  : Transferred:   60.215 MiB / 300.000 MiB, 20%, 0 B/s, ETA -
  // Newer versions may append extra text after the ETA, which we ignore.
  static const QRegularExpression rxBytes(
      R"(^Transferred:\s+(\S+(?: \S+)?) / (\S+(?: \S+)?), ([0-9]+%|-), (\S+(?: \S+)?/s), ETA (\S+).*$)");
  static const QRegularExpression rxBytesOld(
      R"(^Transferred:\s+(\S+ \S+) \(([^)]+)\)$)");

  // File counters.
  //  <=1.42 : Transferred:   3
  //  1.43+  : Transferred:   2 / 3, 67%
  static const QRegularExpression rxFiles(
      R"(^Transferred:\s+(\d+) / (\d+), ([0-9]+%|-)$)");
  static const QRegularExpression rxFilesOld(R"(^Transferred:\s+(\d+)$)");

  //  <=1.42 : Checks:   3
  //  1.43+  : Checks:   1 / 2, 50%
  //  1.6x+  : Checks:   0 / 0, -, Listed 5
  static const QRegularExpression rxChecks(
      R"(^Checks:\s+(\d+) / (\d+), ([0-9]+%|-)(?:, (.+))?$)");
  static const QRegularExpression rxChecksOld(R"(^Checks:\s+(\d+)$)");

  //  Errors:   0
  //  Errors:   1 (retrying may help)
  static const QRegularExpression rxErrors(R"(^Errors:\s+(\d+)\s*(.*)$)");

  static const QRegularExpression rxElapsed(R"(^Elapsed time:\s+(\S+)$)");

  // Per-file progress (lines under "Transferring:").
  //  1.39-1.55 : *  big.bin: 40% /300M, 60M/s, 2s
  //  1.56+     : * big.bin: 40% / 300 MiB, 60.251 MiB/s, 2s
  //              * big.bin: 20% / 300 MiB, 0 B/s, -
  // The name is matched greedily so names that contain ':' work.
  static const QRegularExpression rxFile(
      R"(^\*\s*(.+):\s+(\d+)% /\s*(\S+(?: \S+)?), (\S+(?: \S+)?/s), (\S+)$)");
  //  <=1.38 : * big.bin: 40% done, 60 MBytes/s, ETA: 2s)
  static const QRegularExpression rxFileOld(
      R"(^\*\s*(.+):\s+(\d+)% done.*(ETA: [^)]+)\)?$)");

  StatsLine r;
  const QString line = rawLine.trimmed();
  if (line.isEmpty()) {
    return r;
  }
  // Early exit: stats lines start with "Transferred:", "Checks:", "Errors:",
  // "Elapsed time:" or "* name". The many log lines that start with a
  // timestamp skip all the regular expressions below.
  switch (line.at(0).unicode()) {
  case 'T':
  case 'C':
  case 'E':
  case '*':
    break;
  default:
    return r;
  }

  QRegularExpressionMatch m;

  if ((m = rxBytes.match(line)).hasMatch()) {
    r.kind = StatsLine::Bytes;
    r.done = m.captured(1);
    r.total = m.captured(2);
    r.percent = m.captured(3);
    r.speed = m.captured(4);
    r.eta = m.captured(5);
  } else if ((m = rxBytesOld.match(line)).hasMatch()) {
    r.kind = StatsLine::Bytes;
    r.done = m.captured(1);
    r.speed = m.captured(2);
  } else if ((m = rxFiles.match(line)).hasMatch()) {
    r.kind = StatsLine::Files;
    r.text = m.captured(1) + " / " + m.captured(2) + ", " + m.captured(3);
  } else if ((m = rxFilesOld.match(line)).hasMatch()) {
    r.kind = StatsLine::Files;
    r.text = m.captured(1);
  } else if ((m = rxChecks.match(line)).hasMatch()) {
    r.kind = StatsLine::Checks;
    r.text = m.captured(1) + " / " + m.captured(2) + ", " + m.captured(3);
    if (!m.captured(4).isEmpty()) {
      r.text += ", " + m.captured(4);
    }
  } else if ((m = rxChecksOld.match(line)).hasMatch()) {
    r.kind = StatsLine::Checks;
    r.text = m.captured(1);
  } else if ((m = rxErrors.match(line)).hasMatch()) {
    r.kind = StatsLine::Errors;
    r.text = m.captured(1);
    if (!m.captured(2).isEmpty()) {
      r.text += " " + m.captured(2);
    }
  } else if ((m = rxElapsed.match(line)).hasMatch()) {
    r.kind = StatsLine::Elapsed;
    r.text = m.captured(1);
  } else if ((m = rxFile.match(line)).hasMatch()) {
    r.kind = StatsLine::FileProgress;
    r.name = m.captured(1).trimmed();
    r.filePercent = m.captured(2).toInt();
    r.detail = "Size: " + m.captured(3) + "\nSpeed: " + m.captured(4) +
               "\nETA: " + m.captured(5);
  } else if ((m = rxFileOld.match(line)).hasMatch()) {
    r.kind = StatsLine::FileProgress;
    r.name = m.captured(1).trimmed();
    r.filePercent = m.captured(2).toInt();
    r.detail = m.captured(3);
  }

  return r;
}

QString chomp(const QString &line) {
  QString s = line;
  while (s.endsWith('\n') || s.endsWith('\r')) {
    s.chop(1);
  }
  return s;
}

ListEntry parseLslLine(const QString &rawLine) {
  // "314572800 2026-10-08 15:13:54.287248807 big.bin"
  // "        0 2026-10-08 15:13:54.291248807  name with leading space"
  static const QRegularExpression rx(
      R"(^\s*(\d+) (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)(?:\.\d+)? (.+)$)");
  ListEntry e;
  auto m = rx.match(chomp(rawLine));
  if (m.hasMatch()) {
    e.valid = true;
    e.size = m.captured(1).toULongLong();
    e.modified = m.captured(2);
    e.name = m.captured(3);
  }
  return e;
}

namespace {
ListEntry entryFromJson(const QJsonObject &o) {
  static const QRegularExpression fraction(R"(\.\d+)");
  ListEntry e;
  e.name = o.value(QLatin1String("Name")).toString();
  if (e.name.isEmpty()) {
    return e;
  }
  e.valid = true;
  e.path = o.value(QLatin1String("Path")).toString();
  e.isDir = o.value(QLatin1String("IsDir")).toBool();
  const double size = o.value(QLatin1String("Size")).toDouble();
  e.size = (e.isDir || size < 0) ? 0 : quint64(size);
  // "2026-10-08T18:18:39.142115738Z" -> local "2026-10-08 20:18:39"
  QString t = o.value(QLatin1String("ModTime")).toString();
  t.remove(fraction);
  const QDateTime dt = QDateTime::fromString(t, Qt::ISODate);
  if (dt.isValid()) {
    e.modified = dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
  }
  return e;
}
} // namespace

QList<ListEntry> parseLsJson(const QByteArray &json, bool *ok) {
  QList<ListEntry> out;
  QJsonParseError err{};
  const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
  if (ok) {
    *ok = err.error == QJsonParseError::NoError && doc.isArray();
  }
  if (!doc.isArray()) {
    return out;
  }
  const QJsonArray arr = doc.array();
  out.reserve(arr.size());
  for (const QJsonValue &v : arr) {
    const ListEntry e = entryFromJson(v.toObject());
    if (e.valid) {
      out.append(e);
    }
  }
  return out;
}

ListEntry parseLsJsonLine(const QByteArray &line) {
  QByteArray l = line.trimmed();
  if (l.endsWith(',')) {
    l.chop(1);
  }
  if (!l.startsWith('{')) {
    return ListEntry(); // "[", "]" or a stray message
  }
  const QJsonDocument doc = QJsonDocument::fromJson(l);
  if (!doc.isObject()) {
    return ListEntry();
  }
  ListEntry e = entryFromJson(doc.object());
  if (e.path.isEmpty() || e.path.startsWith('/') ||
      e.path.split('/').contains(QStringLiteral(".."))) {
    e.valid = false; // not a path inside the folder searched
  }
  return e;
}

QStringList LineBuffer::append(const QString &chunk) {
  mPending += chunk;
  QStringList lines;
  int start = 0;
  int nl;
  while ((nl = mPending.indexOf('\n', start)) != -1) {
    lines << chomp(mPending.mid(start, nl - start));
    start = nl + 1;
  }
  mPending.remove(0, start);
  return lines;
}

QString LineBuffer::flush() {
  QString rest = chomp(mPending);
  mPending.clear();
  return rest;
}

} // namespace RcloneOutput
