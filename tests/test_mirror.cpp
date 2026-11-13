#include "mirror.h"

#include <QtTest>

using namespace Mirror;

namespace {
Plan plan(const QString &src, const QString &path, QList<Target> targets) {
  Plan p;
  p.id = QUuid::createUuid();
  p.sourceRemote = src;
  p.sourcePath = path;
  p.targets = targets;
  return p;
}
} // namespace

class TestMirror : public QObject {
  Q_OBJECT

private slots:
  void cleanPathTidies() {
    QCOMPARE(cleanPath(" /Gallery\\Trips//Paris/ "), QString("Gallery/Trips/Paris"));
    QCOMPARE(cleanPath(""), QString());
    QCOMPARE(cleanPath("./a/./b"), QString("a/b"));
    bool ok = true;
    QCOMPARE(cleanPath("a/../b", &ok), QString());
    QVERIFY(!ok);
  }

  void excludesAreCleaned() {
    QString err;
    QCOMPARE(cleanExcludes({" *.tmp ", "", "*.tmp", "# note", "Thumbs.db"}, &err),
             QStringList({"*.tmp", "Thumbs.db"}));
    QVERIFY(err.isEmpty());
    cleanExcludes({"[abc"}, &err);
    QVERIFY(!err.isEmpty());
    QStringList many;
    for (int i = 0; i < 51; ++i) {
      many << QString("*.x%1").arg(i);
    }
    cleanExcludes(many, &err);
    QVERIFY(!err.isEmpty());
  }

  void validPlanPasses() {
    const Plan p = plan("gdrive", "Gallery/Trips", {{"pcloud", "Backups/Trips"}, {"koofr", "Trips"}});
    QVERIFY(validate(p, {}).isEmpty());
    // the same folder name on different remotes is fine
    const Plan q = plan("gdrive", "Videos", {{"pcloud", "Videos"}, {"koofr", "Videos"}});
    QVERIFY(validate(q, {p}).isEmpty());
  }

  void overlappingDestinationsAreRefused() {
    const Plan a = plan("gdrive", "Videos", {{"pcloud", "Media"}});
    const Plan nested = plan("gdrive", "Music", {{"pcloud", "media/Music"}}); // case differs
    QVERIFY(!validate(nested, {a}).isEmpty());
    const Plan twice = plan("gdrive", "Music", {{"pcloud", "X"}, {"pcloud", "X/Y"}});
    QVERIFY(!validate(twice, {}).isEmpty());
    // editing a plan does not clash with its own saved version
    QVERIFY(validate(a, {a}).isEmpty());
  }

  void badPlansAreRefused() {
    QVERIFY(!validate(plan("", "", {{"p", "X"}}), {}).isEmpty());                  // no source
    QVERIFY(!validate(plan("g", "A", {}), {}).isEmpty());                          // no target
    QVERIFY(!validate(plan("g", "A", {{"p", ""}}), {}).isEmpty());                 // target root
    QVERIFY(!validate(plan("g", "A", {{"p", "_SyncTrash/x"}}), {}).isEmpty());     // into trash
    QVERIFY(!validate(plan("g", "_synctrash", {{"p", "X"}}), {}).isEmpty());       // from trash
    QVERIFY(!validate(plan("g", "Photos", {{"g", "Photos/Backup"}}), {}).isEmpty()); // into itself
    QVERIFY(validate(plan("g", "Photos", {{"g", "Backup/Photos"}}), {}).isEmpty());  // same remote, apart
    QVERIFY(!validate(plan("g", "", {{"g", "Backup"}}), {}).isEmpty());           // whole remote into itself
  }

  void syncArgsForTrashMode() {
    Plan p = plan("gdrive", "Gallery", {{"pcloud", "Backups/Gallery"}});
    p.exclude = {"*.tmp"};
    RunOptions o;
    o.stamp = "2026-11-13_094600";
    o.reportFile = "/tmp/r.txt";
    const QStringList a = syncArgs(p, p.targets[0], o);
    QCOMPARE(a.mid(0, 3), QStringList({"sync", "gdrive:Gallery", "pcloud:Backups/Gallery"}));
    const int b = int(a.indexOf("--backup-dir"));
    QVERIFY(b > 0);
    QCOMPARE(a.at(b + 1), QString("pcloud:_SyncTrash/2026-11-13_094600/Backups/Gallery"));
    QVERIFY(a.contains("--drive-skip-gdocs"));
    QVERIFY(a.contains("--exclude=*.tmp"));
    QVERIFY(a.contains("--exclude=/_SyncTrash/**"));
    QCOMPARE(a.at(a.indexOf("--combined") + 1), QString("/tmp/r.txt"));
    QVERIFY(!a.contains("--dry-run"));
    QVERIFY(a.contains("--stats"));
  }

  void syncArgsForOtherModes() {
    Plan p = plan("gdrive", "", {{"pcloud", "All"}});
    p.mode = DeleteMode::Keep;
    p.exportGoogleDocs = true;
    RunOptions o;
    o.dryRun = true;
    const QStringList a = syncArgs(p, p.targets[0], o);
    QCOMPARE(a.mid(0, 3), QStringList({"copy", "gdrive:", "pcloud:All"}));
    QVERIFY(!a.contains("--backup-dir"));
    QVERIFY(!a.contains("--drive-skip-gdocs"));
    QVERIFY(a.contains("--dry-run"));
    p.mode = DeleteMode::Delete;
    const QStringList d = syncArgs(p, p.targets[0], {});
    QCOMPARE(d.first(), QString("sync"));
    QVERIFY(!d.contains("--backup-dir"));
  }

  void reportIsParsed() {
    const QByteArray r = "+ new.jpg\n* changed.jpg\r\n- gone.jpg\n= same.jpg\n! broken.jpg\n"
                         "+ sub folder/b.jpg\n\nnonsense\n";
    Changes c = parseReport(r, DeleteMode::Trash);
    QCOMPARE(c.added, QStringList({"new.jpg", "sub folder/b.jpg"}));
    QCOMPARE(c.updated, QStringList({"changed.jpg"}));
    QCOMPARE(c.removed, QStringList({"gone.jpg"}));
    QCOMPARE(c.total(), 4);
    // with "never remove", files only on the destination are not changes
    QVERIFY(parseReport(r, DeleteMode::Keep).removed.isEmpty());
  }

  void oldSnapshotsByAge() {
    const QDateTime now = QDateTime::currentDateTime();
    auto stamp = [&](int days) { return now.addDays(-days).toString(stampFormat()); };
    const QStringList names{stamp(40), stamp(10), stamp(0), "notes", "2026-13-99_000000x"};
    QCOMPARE(oldSnapshots(names, now.addDays(-30)), QStringList({stamp(40)}));
    QCOMPARE(oldSnapshots(names, now.addSecs(60)).size(), 3);
  }

  void jsonRoundTrip() {
    Plan p = plan("gdrive", "Gallery/Trips", {{"pcloud", "Backups/Trips"}});
    p.exclude = {"*.tmp"};
    p.mode = DeleteMode::Keep;
    p.exportGoogleDocs = true;
    const QList<Plan> back = fromJson(toJson({p}));
    QCOMPARE(back.size(), 1);
    QCOMPARE(back[0].id, p.id);
    QCOMPARE(back[0].sourcePath, p.sourcePath);
    QCOMPARE(back[0].targets[0].path, QString("Backups/Trips"));
    QCOMPARE(back[0].exclude, p.exclude);
    QVERIFY(back[0].mode == DeleteMode::Keep);
    QVERIFY(back[0].exportGoogleDocs);
  }

  // Runs the generated command lines through a real rclone against local
  // folders: a check changes nothing, a sync copies, honours the patterns and
  // keeps old versions in the trash. Skipped when rclone is not installed.
  void endToEndWithRclone() {
    const QString rclone = QStandardPaths::findExecutable("rclone");
    if (rclone.isEmpty()) {
      QSKIP("rclone is not installed");
    }
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString conf = root.filePath("rclone.conf");
    QFile cf(conf);
    QVERIFY(cf.open(QIODevice::WriteOnly));
    for (const char *n : {"src", "dst"}) {
      QDir(root.path()).mkpath(n);
      cf.write(QString("[%1]\ntype = alias\nremote = %2\n\n")
                   .arg(n, root.filePath(n)).toUtf8());
    }
    cf.close();
    auto write = [&](const QString &rel, const QByteArray &data) {
      const QString f = root.filePath("src/Gallery/" + rel);
      QDir().mkpath(QFileInfo(f).path());
      QFile file(f);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write(data);
    };
    write("a.jpg", "aaa");
    write("Paris/b.jpg", "bbb");
    write("scratch.tmp", "junk");

    Plan p = plan("src", "Gallery", {{"dst", "Backups/Gallery"}});
    p.exclude = {"*.tmp"};
    auto run = [&](bool dry, const QString &stamp) {
      RunOptions o;
      o.dryRun = dry;
      o.stamp = stamp;
      o.reportFile = root.filePath("report.txt");
      QProcess proc;
      proc.start(rclone, QStringList{"--config", conf} + syncArgs(p, p.targets[0], o));
      if (!proc.waitForFinished(60000) || proc.exitCode() != 0) {
        qWarning() << proc.readAllStandardError();
        return Changes{};
      }
      QFile rep(o.reportFile);
      if (!rep.open(QIODevice::ReadOnly)) {
        return Changes{};
      }
      return parseReport(rep.readAll(), p.mode);
    };
    const QString dest = root.filePath("dst/Backups/Gallery");

    Changes c = run(true, "2026-01-01_000000");
    QCOMPARE(c.added.size(), 2);
    QVERIFY(!QFileInfo::exists(dest));

    c = run(false, "2026-01-01_000000");
    QCOMPARE(c.added.size(), 2);
    QVERIFY(QFileInfo::exists(dest + "/Paris/b.jpg"));
    QVERIFY(!QFileInfo::exists(dest + "/scratch.tmp"));

    QFile::remove(root.filePath("src/Gallery/Paris/b.jpg"));
    write("a.jpg", "changed");
    c = run(false, "2026-01-02_000000");
    QCOMPARE(c.updated, QStringList({"a.jpg"}));
    QCOMPARE(c.removed, QStringList({"Paris/b.jpg"}));
    QVERIFY(!QFileInfo::exists(dest + "/Paris/b.jpg"));
    const QString trash = root.filePath("dst/_SyncTrash/2026-01-02_000000/Backups/Gallery");
    QVERIFY(QFileInfo::exists(trash + "/Paris/b.jpg"));
    QVERIFY(QFileInfo::exists(trash + "/a.jpg"));
  }
};

QTEST_MAIN(TestMirror)
#include "test_mirror.moc"
