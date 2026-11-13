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

// A full path on this PC: "/data/x" on Linux, "C:/data/x" on Windows
static QString P(const char *path) {
#ifdef Q_OS_WIN
  return QStringLiteral("C:") + QString::fromUtf8(path);
#else
  return QString::fromUtf8(path);
#endif
}

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

  void localFolderPaths() {
#ifdef Q_OS_WIN // a backslash is only a separator on Windows
    QCOMPARE(cleanLocalPath(" C:\\Backups\\Photos\\ "), QString("C:/Backups/Photos"));
#endif
    QCOMPARE(cleanLocalPath("/home/me//Backups/"), QString("/home/me/Backups"));
    QCOMPARE(cleanLocalPath(""), QString());
    const Target t{QString(), P("/data/Backups/Paris")};
    QVERIFY(t.local());
    QCOMPARE(targetPath(t), P("/data/Backups/Paris"));
    QCOMPARE(trashRoot(t), P("/data/Backups/_SyncTrash"));
    QCOMPARE(trashFor(t, "2026-11-13_094600"), P("/data/Backups/_SyncTrash/2026-11-13_094600/Paris"));
    const Target r{"pCloud", "Backups/Paris"};
    QCOMPARE(trashRoot(r), QString("pCloud:_SyncTrash"));
  }

  void localFolderArgs() {
    Plan p = plan("gdrive", "Gallery", {{QString(), P("/data/Backups/Gallery")}});
    RunOptions o;
    o.stamp = "2026-11-13_094600";
    const QStringList a = syncArgs(p, p.targets[0], o);
    QCOMPARE(a.mid(0, 3), QStringList({"sync", "gdrive:Gallery", P("/data/Backups/Gallery")}));
    QCOMPARE(a.at(a.indexOf("--backup-dir") + 1),
             P("/data/Backups/_SyncTrash/2026-11-13_094600/Gallery"));
  }

  void localFolderValidation() {
    QVERIFY(validate(plan("g", "A", {{QString(), P("/data/Backups/A")}}), {}).isEmpty());
    QVERIFY(!validate(plan("g", "A", {{QString(), ""}}), {}).isEmpty());              // none chosen
    QVERIFY(!validate(plan("g", "A", {{QString(), "Backups/A"}}), {}).isEmpty());     // not a full path
    QVERIFY(!validate(plan("g", "A", {{QString(), "/"}}), {}).isEmpty());             // a whole drive
    QVERIFY(!validate(plan("g", "A", {{QString(), P("/x/_SyncTrash/A")}}), {}).isEmpty()); // into trash
    const Plan a = plan("g", "A", {{QString(), P("/data/Backups")}});
    QVERIFY(!validate(plan("g", "B", {{QString(), P("/data/Backups/B")}}), {a}).isEmpty()); // overlap
    QVERIFY(validate(plan("g", "B", {{QString(), P("/data/Other/B")}}), {a}).isEmpty());
    // a local folder and a remote folder with the same name do not clash
    QVERIFY(validate(plan("g", "B", {{"p", "data/Backups"}}), {a}).isEmpty());
  }

  void localFolderSource() {
    const Plan up = plan(QString(), P("/data/Pictures"), {{"pCloud", "Pictures"}});
    QVERIFY(up.localSource());
    QVERIFY(validate(up, {}).isEmpty());
    QCOMPARE(syncArgs(up, up.targets[0], {}).mid(0, 3),
             QStringList({"sync", P("/data/Pictures"), "pCloud:Pictures"}));
    QVERIFY(!validate(plan(QString(), "Pictures", {{"p", "X"}}), {}).isEmpty());      // not a full path
    QVERIFY(!validate(plan(QString(), "", {{"p", "X"}}), {}).isEmpty());              // none chosen
    // from one folder on this PC to another: fine apart, refused when nested
    QVERIFY(validate(plan(QString(), P("/data/Pictures"), {{QString(), P("/backup/Pictures")}}), {}).isEmpty());
    QVERIFY(!validate(plan(QString(), P("/data/Pictures"), {{QString(), P("/data/Pictures/copy")}}), {}).isEmpty());
    QVERIFY(!validate(plan(QString(), P("/data"), {{QString(), P("/data/copy")}}), {}).isEmpty());
    const QList<Plan> back = fromJson(toJson({up}));
    QCOMPARE(back.size(), 1);
    QVERIFY(back[0].localSource());
    QCOMPARE(back[0].sourcePath, P("/data/Pictures"));
  }

  void localFolderSafety() {
    // a mirror empties its destination of everything else: never the home
    // folder, the usual personal folders, a system folder, or a folder above home
    const QString home = QDir::fromNativeSeparators(QDir::homePath());
    QVERIFY(!protectedFolder(home).isEmpty());
    QVERIFY(!protectedFolder(QFileInfo(home).path()).isEmpty()); // e.g. C:/Users
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!docs.isEmpty()) {
      QVERIFY(!protectedFolder(QDir::fromNativeSeparators(docs)).isEmpty());
      QVERIFY(protectedFolder(QDir::fromNativeSeparators(docs) + "/Backup").isEmpty());
    }
    QVERIFY(protectedFolder(home + "/Mirrors/Photos").isEmpty());
    QVERIFY(!validate(plan("g", "A", {{QString(), home}}), {}).isEmpty());
    QVERIFY(validate(plan("g", "A", {{QString(), home + "/Mirrors/A"}}), {}).isEmpty());
#ifndef Q_OS_WIN
    QVERIFY(!protectedFolder("/etc").isEmpty());
    QVERIFY(!protectedFolder("/usr").isEmpty());
    QVERIFY(protectedFolder("/srv/backups").isEmpty());
#endif
    // a remote name that rclone would read as an option is refused
    QVERIFY(!validate(plan("--config", "A", {{"p", "B"}}), {}).isEmpty());
    QVERIFY(!validate(plan("g", "A", {{"-x", "B"}}), {}).isEmpty());
    // ":" paths are Qt resources or rclone backends, not folders on this PC
    QVERIFY(!isLocalPath(":s3,provider=AWS:bucket"));
    QVERIFY(!validate(plan("g", "A", {{QString(), ":local:/tmp/x"}}), {}).isEmpty());
    QVERIFY(!validate(plan(QString(), ":memory:x", {{"p", "B"}}), {}).isEmpty());
    QVERIFY(!isLocalPath("-x"));
    QVERIFY(!isLocalPath("relative/path"));
#ifdef Q_OS_WIN
    QVERIFY(isLocalPath("C:/Backups"));
    QVERIFY(isLocalPath("//nas/share/Backups"));
#else
    QVERIFY(isLocalPath("/srv/Backups"));
#endif
    // and so is a hand-edited relative path for this PC
    const QList<Plan> edited = fromJson(QJsonDocument::fromJson(
        R"([{"sourceRemote":"g","sourcePath":"A","targets":[{"remote":"","path":"--delete-before"}]}])").array());
    QCOMPARE(edited.size(), 1);
    QVERIFY(!validate(edited[0], {}).isEmpty());
  }

  void localFolderLinks() {
#ifdef Q_OS_WIN
    QSKIP("symbolic links need extra rights on Windows");
#else
    // a destination that is a link to the mirrored folder is the same folder
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath("src"), link = dir.filePath("link");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QFile::link(src, link));
    QVERIFY(!validate(plan(QString(), src, {{QString(), link}}), {}).isEmpty());
    QVERIFY(!validate(plan(QString(), src, {{QString(), link + "/inside"}}), {}).isEmpty());
    // and two mirrors cannot reach one folder through a link either
    const Plan a = plan("g", "A", {{QString(), src}});
    QVERIFY(!validate(plan("g", "B", {{QString(), link}}), {a}).isEmpty());
    QVERIFY(validate(plan("g", "B", {{QString(), dir.filePath("other")}}), {a}).isEmpty());
#endif
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
    // folders on this computer keep their full path
    const Plan l = plan("gdrive", "A", {{QString(), P("/data/Backups/A")}});
    QCOMPARE(fromJson(toJson({l}))[0].targets[0].path, P("/data/Backups/A"));
    QVERIFY(fromJson(toJson({l}))[0].targets[0].local());
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
    const Target pc{QString(), cleanLocalPath(root.filePath("pc/Backups/Gallery"))};
    auto runTo = [&](const Target &target, bool dry, const QString &stamp) {
      RunOptions o;
      o.dryRun = dry;
      o.stamp = stamp;
      o.reportFile = root.filePath("report.txt");
      QProcess proc;
      proc.start(rclone, QStringList{"--config", conf} + syncArgs(p, target, o));
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
    auto run = [&](bool dry, const QString &stamp) { return runTo(p.targets[0], dry, stamp); };
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

    // the same mirror into a folder on this computer
    c = runTo(pc, false, "2026-01-03_000000");
    QCOMPARE(c.added.size(), 1);
    QVERIFY(QFileInfo::exists(pc.path + "/a.jpg"));
    QVERIFY(!QFileInfo::exists(pc.path + "/scratch.tmp"));
    write("a.jpg", "changed again");
    c = runTo(pc, false, "2026-01-04_000000");
    QCOMPARE(c.updated, QStringList({"a.jpg"}));
    QVERIFY(QFileInfo::exists(root.filePath("pc/Backups/_SyncTrash/2026-01-04_000000/Gallery/a.jpg")));

    // a folder on this PC as the source, mirrored to a remote
    Plan up = plan(QString(), cleanLocalPath(root.filePath("src/Gallery")), {{"dst", "FromPC"}});
    up.exclude = {"*.tmp"};
    RunOptions o;
    o.stamp = "2026-01-05_000000";
    QProcess proc;
    proc.start(rclone, QStringList{"--config", conf} + syncArgs(up, up.targets[0], o));
    QVERIFY(proc.waitForFinished(60000));
    QCOMPARE(proc.exitCode(), 0);
    QVERIFY(QFileInfo::exists(root.filePath("dst/FromPC/a.jpg")));
    QVERIFY(!QFileInfo::exists(root.filePath("dst/FromPC/scratch.tmp")));
  }
};

QTEST_MAIN(TestMirror)
#include "test_mirror.moc"
