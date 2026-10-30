#include "command_args.h"
#include "rclone_commands.h"
#include "schedule.h"

#include <QtTest>

using namespace RcloneCommands;

class TestCommands : public QObject {
  Q_OBJECT

private slots:
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
