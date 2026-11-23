#include "command_args.h"
#include "rclone_commands.h"
#include "schedule.h"

#include <QtTest>

using namespace RcloneCommands;

class TestCommands : public QObject {
  Q_OBJECT

private slots:
  void globEscaping() {
    QCOMPARE(RcloneCommands::globEscape("beach"), QString("beach"));
    QCOMPARE(RcloneCommands::globEscape("a[b]*?{c}\\"),
             QString("a\\[b\\]\\*\\?\\{c\\}\\\\"));
  }
  void bandwidthLimits() {
    for (const char *ok : {"", "10M", "512k", "1.5M", "off", "10M:2M", "08:00,2M 19:00,off",
                           "Mon-08:00,1M Sat-00:00,off", "  4M  ", "1MiB", "1Mi"}) {
      QVERIFY2(RcloneCommands::validBandwidth(ok), ok);
    }
    for (const char *bad : {"fast", "2MB", "10 M", "25:00,1M", "08:00 2M", "10M; rm -rf", "-1M", "08:00,2M off"}) {
      QVERIFY2(!RcloneCommands::validBandwidth(bad), bad);
    }
    const QStringList copy{"copy", "a:", "b:"};
    QCOMPARE(RcloneCommands::withBandwidth(copy, " 4M "), QStringList({"copy", "a:", "b:", "--bwlimit", "4M"}));
    QCOMPARE(RcloneCommands::withBandwidth(copy, ""), copy);
    QCOMPARE(RcloneCommands::withBandwidth(copy, "fast"), copy);
    const QStringList own{"copy", "a:", "b:", "--bwlimit", "1M"};
    QCOMPARE(RcloneCommands::withBandwidth(own, "4M"), own); // the task's own limit wins
    const QStringList ownEq{"copy", "--bwlimit=1M"};
    QCOMPARE(RcloneCommands::withBandwidth(ownEq, "4M"), ownEq);
  }

  void shellQuoting() {
    QCOMPARE(shellQuote("--fast-list"), QString("--fast-list"));
    QCOMPARE(shellCommand({"rclone", "copy", "gdrive:Photos"}), QString("rclone copy gdrive:Photos"));
#ifdef Q_OS_WIN
    QCOMPARE(shellQuote("C:\\My Files"), QString("\"C:\\My Files\""));
    QCOMPARE(shellQuote("a\"b c"), QString("\"a\\\"b c\""));
#else
    QCOMPARE(shellQuote("/home/me/My Files"), QString("'/home/me/My Files'"));
    QCOMPARE(shellQuote("it's"), QString("'it'\\''s'"));
    QCOMPARE(shellQuote("$HOME"), QString("'$HOME'"));
    QCOMPARE(shellQuote(""), QString("''"));
#endif
  }

  void splitKeepsQuotedSpaces() {
    QCOMPARE(splitArguments("--a 1 --name \"my drive\""),
             QStringList({"--a", "1", "--name", "my drive"}));
  }
  void splitKeepsWindowsPaths() {
    QCOMPARE(splitArguments("--log-file \"C:\\Users\\me\\rclone log.txt\""),
             QStringList({"--log-file", "C:\\Users\\me\\rclone log.txt"}));
  }
  void splitIgnoresExtraSpaces() {
    QCOMPARE(splitArguments("  --a   --b  "), QStringList({"--a", "--b"}));
    QVERIFY(splitArguments("   ").isEmpty());
    QVERIFY(splitArguments("").isEmpty());
  }

  void checkOptions() {
    CheckOptions o;
    o.oneWay = true;
    o.sizeOnly = true;
    const QStringList a = check("a:x", "b:y", o);
    QCOMPARE(a.first(), QString("check"));
    QVERIFY(a.contains("--one-way"));
    QVERIFY(a.contains("--size-only"));
    QCOMPARE(a.mid(a.size() - 2), QStringList({"a:x", "b:y"}));
    QVERIFY(!check("a:", "b:", {}).contains("--one-way"));
  }
  void dedupeRejectsUnknownMode() {
    QCOMPARE(dedupe("a:", "newest").mid(0, 3),
             QStringList({"dedupe", "--dedupe-mode", "newest"}));
    // "interactive" would wait for input that never comes
    QCOMPARE(dedupe("a:", "interactive")[2], QString("skip"));
  }
  void copyAndMove() {
    QCOMPARE(copy("a:", "b:", false).first(), QString("copy"));
    QCOMPARE(copy("a:", "b:", true).first(), QString("move"));
  }
  void bisyncFlags() {
    BisyncOptions o;
    o.resync = true;
    o.dryRun = true;
    const QStringList a = bisync("a:", "b:", o);
    QVERIFY(a.contains("--resync") && a.contains("--dry-run"));
    QCOMPARE(bisync("a:", "b:", {}).contains("--resync"), false);
  }

  void mountDefaults() {
    QCOMPARE(mountArgs({}, ""), QStringList({"--vfs-cache-mode", "writes"}));
  }
  void mountOff() {
    MountOptions o;
    o.cacheMode = "off";
    QVERIFY(mountArgs(o, "").isEmpty());
  }
  void mountPreferencesWin() {
    MountOptions o;
    o.cacheMode = "full";
    o.readOnly = true;
    const QStringList a =
        mountArgs(o, "--vfs-cache-mode=minimal --read-only --dir-cache-time 5m");
    QVERIFY(!a.contains("full"));
    QCOMPARE(a.count("--read-only"), 1);
    QVERIFY(a.contains("--dir-cache-time"));
  }
  void mountExtraAndVolume() {
    MountOptions o;
    o.volumeName = " Photos ";
    o.extra = "--buffer-size \"64 M\"";
    const QStringList a = mountArgs(o, "");
    QVERIFY(a.contains("--volname"));
    QVERIFY(a.contains("Photos"));
    QVERIFY(a.contains("64 M"));
  }

  void scheduleRoundTrip() {
    Schedule s;
    s.kind = Schedule::Weekly;
    s.time = QTime(3, 30);
    s.weekdays = Schedule::dayBit(Qt::Monday) | Schedule::dayBit(Qt::Friday);
    const Schedule p = Schedule::parse(s.serialize());
    QCOMPARE(p.kind, Schedule::Weekly);
    QCOMPARE(p.time, QTime(3, 30));
    QCOMPARE(p.weekdays, s.weekdays);
  }
  void scheduleParseGarbage() {
    QCOMPARE(Schedule::parse("").kind, Schedule::Off);
    QCOMPARE(Schedule::parse("daily|x").kind, Schedule::Off);
    QCOMPARE(Schedule::parse("weekly|60|02:00|0").kind, Schedule::Off);
  }
  void scheduleInterval() {
    Schedule s;
    s.kind = Schedule::Interval;
    s.intervalMinutes = 90;
    const QDateTime t(QDate(2026, 10, 30), QTime(10, 0));
    QCOMPARE(s.next(t), t.addSecs(90 * 60));
  }
  void scheduleDaily() {
    Schedule s;
    s.kind = Schedule::Daily;
    s.time = QTime(2, 0);
    QCOMPARE(s.next(QDateTime(QDate(2026, 10, 30), QTime(1, 0))),
             QDateTime(QDate(2026, 10, 30), QTime(2, 0)));
    // exactly at the time counts as already run
    QCOMPARE(s.next(QDateTime(QDate(2026, 10, 30), QTime(2, 0))),
             QDateTime(QDate(2026, 10, 31), QTime(2, 0)));
  }
  void scheduleWeekly() {
    Schedule s;
    s.kind = Schedule::Weekly;
    s.time = QTime(8, 0);
    s.weekdays = Schedule::dayBit(Qt::Monday);
    // 2026-10-30 is a Friday: next Monday is 2026-11-02
    QCOMPARE(s.next(QDateTime(QDate(2026, 10, 30), QTime(9, 0))),
             QDateTime(QDate(2026, 11, 2), QTime(8, 0)));
    // same weekday, time still ahead
    QCOMPARE(s.next(QDateTime(QDate(2026, 11, 2), QTime(7, 0))),
             QDateTime(QDate(2026, 11, 2), QTime(8, 0)));
    // same weekday, time passed: a week later
    QCOMPARE(s.next(QDateTime(QDate(2026, 11, 2), QTime(8, 0))),
             QDateTime(QDate(2026, 11, 9), QTime(8, 0)));
  }
  void scheduleDescribeSingularAndPlural() {
    Schedule s;
    s.kind = Schedule::Interval;
    s.intervalMinutes = 1;
    QCOMPARE(s.describe(), QString("Every minute"));
    s.intervalMinutes = 5;
    QCOMPARE(s.describe(), QString("Every 5 minutes"));
    s.intervalMinutes = 60;
    QCOMPARE(s.describe(), QString("Every hour"));
    s.intervalMinutes = 180;
    QCOMPARE(s.describe(), QString("Every 3 hours"));
  }
  void scheduleOffHasNoNext() {
    QVERIFY(!Schedule().next(QDateTime::currentDateTime()).isValid());
  }
};

QTEST_MAIN(TestCommands)
#include "test_commands.moc"
