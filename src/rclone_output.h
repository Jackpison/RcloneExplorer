#pragma once

// Parsers for rclone's human-readable output.
//
// rclone has changed its text output several times (1.39, 1.43, 1.56 and
// later releases). Every regex that interprets rclone output lives here so
// that format changes are fixed in one place and covered by unit tests
// (tests/test_rclone_output.cpp) instead of silently breaking widgets.

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace RcloneOutput {

struct StatsLine {
  enum Kind {
    None,         // not a stats line
    Bytes,        // "Transferred: 10 MiB / 100 MiB, 10%, 5 MiB/s, ETA 18s"
    Files,        // "Transferred: 2 / 3, 67%"
    Checks,       // "Checks: 0 / 0, -, Listed 5"
    Errors,       // "Errors: 1 (retrying may help)"
    Elapsed,      // "Elapsed time: 2.0s"
    FileProgress  // " * name: 40% / 300 MiB, 60 MiB/s, 2s"
  };

  Kind kind = None;

  // Bytes
  QString done;     // "60.215 MiB"
  QString total;    // "300.000 MiB"
  QString percent;  // "20%" or "-"
  QString speed;    // "60.231 MiB/s"
  QString eta;      // "2s" or "-"

  // Files / Checks / Errors / Elapsed: ready-to-display summary
  QString text;

  // FileProgress
  QString name;
  int filePercent = 0;
  QString detail; // size, speed, eta for tooltips
};

// Parse one line of `rclone <cmd> --verbose --stats 1s` output.
// The line may still contain surrounding whitespace.
StatsLine parseStatsLine(const QString &line);

struct ListEntry {
  bool valid = false;
  bool isDir = false;
  quint64 size = 0;
  QString modified; // "YYYY-MM-DD HH:MM:SS"
  QString name;
  QString path; // lsjson only: relative to the folder listed ("a/b.txt")
};

// Parse one line of `rclone lsl` output. Names keep leading/trailing spaces.
ListEntry parseLslLine(const QString &line);

// Parse the complete output of `rclone lsjson` (one directory level).
// Folders and files come back in a single listing, so opening a folder needs
// one rclone call instead of two (lsd + lsl). Times are converted to local
// time and formatted "YYYY-MM-DD HH:MM:SS" like lsl output.
QList<ListEntry> parseLsJson(const QByteArray &json, bool *ok = nullptr);

// One line of `rclone lsjson` output as it streams in: rclone writes one
// entry per line ("{...}," between "[" and "]"), so results can be shown
// before the listing ends. Anything else comes back with valid == false.
ListEntry parseLsJsonLine(const QByteArray &line);

// Strip only the line terminator (\n or \r\n), never other whitespace,
// because file names may legitimately start or end with spaces.
QString chomp(const QString &line);

// Accumulates arbitrary chunks of process output and hands back only
// complete lines, so a line split across two reads is never lost.
class LineBuffer {
public:
  QStringList append(const QString &chunk);
  QString flush(); // whatever is left without a trailing newline

private:
  QString mPending;
};

} // namespace RcloneOutput
