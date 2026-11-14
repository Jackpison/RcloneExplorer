#include "app_update.h"

#include <QtTest>

using namespace AppUpdate;

namespace {

QByteArray releaseJson(const QString &tag, const QString &assetUrl, bool prerelease = false) {
  return QString(R"({"tag_name":"%1","html_url":"https://github.com/Jackpison/RcloneExplorer/releases/tag/%1",
                     "draft":false,"prerelease":%3,
                     "assets":[{"name":"RcloneExplorer-5.4.2-windows-x64.zip","size":123,
                                "browser_download_url":"%2"}]})")
      .arg(tag, assetUrl, prerelease ? "true" : "false")
      .toUtf8();
}

const QString kZipUrl = "https://github.com/Jackpison/RcloneExplorer/releases/download/v5.4.2/"
                        "RcloneExplorer-5.4.2-windows-x64.zip";

void write(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).path());
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write(data);
}

QByteArray read(const QString &path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestAppUpdate : public QObject {
  Q_OBJECT

private slots:
  void releaseIsParsed() {
    Release r;
    QVERIFY(parseRelease(releaseJson("v5.4.2", kZipUrl), &r));
    QCOMPARE(r.version, QString("5.4.2"));
    QCOMPARE(r.assets.size(), 1);
    QVERIFY(r.asset(legacyZipName("5.4.2"))); // 5.4.2 published the zip under this name
    QVERIFY(!r.asset(zipName("5.4.2")));
    QCOMPARE(zipName("5.4.3"), QString("RcloneExplorer-5.4.3-windows-x64-portable.zip"));
    QCOMPARE(r.page.host(), QString("github.com"));
  }

  void untrustedReleasesAreRefused() {
    Release r;
    QVERIFY(!parseRelease(releaseJson("v5.4.2-beta", kZipUrl), &r)); // not x.y.z
    QVERIFY(!parseRelease(releaseJson("v5.4.2", kZipUrl, true), &r)); // pre-release
    QVERIFY(!parseRelease("not json", &r));
    // downloads from anywhere but this project's releases are dropped
    for (const QString &url :
         {QString("http://github.com/Jackpison/RcloneExplorer/releases/download/v5.4.2/RcloneExplorer-5.4.2-windows-x64.zip"),
          QString("https://evil.example/RcloneExplorer-5.4.2-windows-x64.zip"),
          QString("https://github.com/Someone/RcloneExplorer/releases/download/v5.4.2/RcloneExplorer-5.4.2-windows-x64.zip"),
          QString("https://github.com/Jackpison/RcloneExplorer/releases/download/v5.4.1/RcloneExplorer-5.4.2-windows-x64.zip"),
          QString("https://github.com/Jackpison/RcloneExplorer/releases/download/v5.4.2/../x/RcloneExplorer-5.4.2-windows-x64.zip")}) {
      QVERIFY(parseRelease(releaseJson("v5.4.2", url), &r));
      QVERIFY2(r.assets.isEmpty(), qPrintable(url));
    }
  }

  void versionsCompareAsNumbers() {
    QCOMPARE(compareVersions("5.4.10", "5.4.9"), 1);
    QCOMPARE(compareVersions("5.4.2", "5.4.2"), 0);
    QCOMPARE(compareVersions("5.4.1", "5.5.0"), -1);
    QCOMPARE(compareVersions("6.0.0", "5.99.99"), 1);
  }

  void checksumsAreFound() {
    const QByteArray sums =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef  RcloneExplorer-5.4.2-x86_64.AppImage\n"
        "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789 *RcloneExplorer-5.4.2-windows-x64.zip\r\n";
    QCOMPARE(checksumFor(sums, legacyZipName("5.4.2")),
             QString("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
    QVERIFY(checksumFor(sums, setupName("5.4.2")).isEmpty());
    QVERIFY(checksumFor("short  RcloneExplorer-5.4.2-windows-x64.zip", legacyZipName("5.4.2")).isEmpty());
  }

  void scoopFoldersAreRecognised() {
    QVERIFY(isScoopFolder("C:\\Users\\me\\scoop\\apps\\rclone-explorer\\current"));
    QVERIFY(isScoopFolder("C:/ProgramData/scoop/apps/rclone-explorer/5.4.2"));
    QVERIFY(!isScoopFolder("C:/Tools/Rclone Explorer"));
    QVERIFY(!isScoopFolder("C:/Users/me/AppData/Local/Programs/Rclone Explorer"));
    // Scoop installed somewhere else (SCOOP=D:\\tools): found by its own files
    QTemporaryDir custom;
    QVERIFY(!isScoopFolder(custom.path()));
    write(custom.filePath("install.json"), "{}");
    QVERIFY(!isScoopFolder(custom.path())); // one of the two is not enough
    write(custom.filePath("manifest.json"), "{}");
    QVERIFY(isScoopFolder(custom.path()));
#ifdef Q_OS_WIN
    QCOMPARE(kind(true, "C:/Users/me/scoop/apps/rclone-explorer/current"), Kind::Scoop);
    QCOMPARE(kind(true, "D:/Rclone Explorer"), Kind::PortableZip);
    QCOMPARE(kind(false, "C:/Users/me/AppData/Local/Programs/Rclone Explorer"), Kind::Installer);
#endif
  }

  void userFilesAreKept() {
    QCOMPARE(ruleFor("Rclone Explorer.exe"), Rule::Replace);
    QCOMPARE(ruleFor("qt/plugins/platforms/qwindows.dll"), Rule::Replace);
    QCOMPARE(ruleFor("Rclone Explorer.ini"), Rule::Skip);
    QCOMPARE(ruleFor("tasks.bin"), Rule::Skip);
    QCOMPARE(ruleFor("mirrors.json"), Rule::Skip);
    QCOMPARE(ruleFor("mirror_history.json"), Rule::Skip);
    QCOMPARE(ruleFor("rclone/rclone.exe"), Rule::Skip);
    QCOMPARE(ruleFor("rclone/rclone.conf"), Rule::Skip);
    QCOMPARE(ruleFor("icons/remotes/drive.png"), Rule::KeepExisting);
  }

  void updateIsApplied() {
    QTemporaryDir root;
    const QString app = root.filePath("app"), next = root.filePath("new");
    // the installed version, with the user's files
    write(app + "/Rclone Explorer.exe", "old exe");
    write(app + "/Qt6Core.dll", "old dll");
    write(app + "/Rclone Explorer.ini", "my settings");
    write(app + "/tasks.bin", "my tasks");
    write(app + "/rclone/rclone.exe", "my newer rclone");
    write(app + "/icons/remotes/drive.png", "my own logo");
    // the new version, as unpacked from the zip
    write(next + "/Rclone Explorer.exe", "new exe");
    write(next + "/Qt6Core.dll", "new dll");
    write(next + "/qt/plugins/new.dll", "new plugin");
    write(next + "/Rclone Explorer.ini", "default settings");
    write(next + "/rclone/rclone.exe", "bundled rclone");
    write(next + "/icons/remotes/drive.png", "shipped logo");
    write(next + "/icons/remotes/newcloud.png", "new logo");

    QString error;
    QVERIFY2(apply(next, app, &error), qPrintable(error));
    QCOMPARE(read(app + "/Rclone Explorer.exe"), QByteArray("new exe"));
    QCOMPARE(read(app + "/Qt6Core.dll"), QByteArray("new dll"));
    QCOMPARE(read(app + "/qt/plugins/new.dll"), QByteArray("new plugin"));
    QCOMPARE(read(app + "/Rclone Explorer.ini"), QByteArray("my settings"));
    QCOMPARE(read(app + "/tasks.bin"), QByteArray("my tasks"));
    QCOMPARE(read(app + "/rclone/rclone.exe"), QByteArray("my newer rclone"));
    QCOMPARE(read(app + "/icons/remotes/drive.png"), QByteArray("my own logo"));
    QCOMPARE(read(app + "/icons/remotes/newcloud.png"), QByteArray("new logo"));
    // replaced files wait as .old until the next start removes them
    QCOMPARE(read(app + "/Rclone Explorer.exe.old"), QByteArray("old exe"));
    removeLeftovers(app);
    QVERIFY(!QFileInfo::exists(app + "/Rclone Explorer.exe.old"));
    QVERIFY(!QFileInfo::exists(app + "/Qt6Core.dll.old"));
    QVERIFY(!QFileInfo::exists(leftoversFile(app)));
    QCOMPARE(read(app + "/Rclone Explorer.exe"), QByteArray("new exe"));
  }

  void relativeFoldersWork() {
    // the folders may be given relative to the working folder
    QTemporaryDir root;
    write(root.filePath("app/Rclone Explorer.exe"), "old exe");
    write(root.filePath("app/tasks.bin"), "my tasks");
    write(root.filePath("new/Rclone Explorer.exe"), "new exe");
    write(root.filePath("new/qt/plugins/p.dll"), "plugin");
    const QString cwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(root.path()));
    QString error;
    const bool ok = apply("new", "./app", &error);
    removeLeftovers("app");
    QDir::setCurrent(cwd);
    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(read(root.filePath("app/Rclone Explorer.exe")), QByteArray("new exe"));
    QCOMPARE(read(root.filePath("app/qt/plugins/p.dll")), QByteArray("plugin"));
    QCOMPARE(read(root.filePath("app/tasks.bin")), QByteArray("my tasks"));
    QVERIFY(!QFileInfo::exists(root.filePath("app/Rclone Explorer.exe.old")));
  }

  void failedUpdateIsRolledBack() {
#ifdef Q_OS_WIN
    QSKIP("read-only folders work differently on Windows");
#else
    QTemporaryDir root;
    const QString app = root.filePath("app"), next = root.filePath("new");
    write(app + "/Rclone Explorer.exe", "old exe");
    write(app + "/Qt6Core.dll", "old dll");
    write(next + "/Rclone Explorer.exe", "new exe");
    write(next + "/Qt6Core.dll", "new dll");
    // "zz" sorts last: a file where the new version needs a folder
    write(app + "/zz", "in the way");
    write(next + "/zz/plugin.dll", "new plugin");
    QString error;
    QVERIFY(!apply(next, app, &error));
    QVERIFY(!error.isEmpty());
    // everything as before
    QCOMPARE(read(app + "/Rclone Explorer.exe"), QByteArray("old exe"));
    QCOMPARE(read(app + "/Qt6Core.dll"), QByteArray("old dll"));
    QCOMPARE(read(app + "/zz"), QByteArray("in the way"));
    QVERIFY(!QFileInfo::exists(app + "/Rclone Explorer.exe.old"));
    QVERIFY(!QFileInfo::exists(leftoversFile(app)));
#endif
  }

  void foreignUpdatesAreRefused() {
    QTemporaryDir root;
    write(root.filePath("new/readme.txt"), "not the app");
    write(root.filePath("app/Rclone Explorer.exe"), "old exe");
    QString error;
    QVERIFY(!apply(root.filePath("new"), root.filePath("app"), &error));
    QCOMPARE(read(root.filePath("app/Rclone Explorer.exe")), QByteArray("old exe"));
  }

  void leftoversStayInTheAppFolder() {
    QTemporaryDir root;
    const QString app = root.filePath("app");
    write(app + "/keep.txt", "keep");
    write(root.filePath("outside.old"), "outside");
    write(leftoversFile(app), "../outside.old\nkeep.txt\nC:outside.old\n..\\outside.old\n" +
                                  root.filePath("outside.old").toUtf8());
    removeLeftovers(app);
    QVERIFY(QFileInfo::exists(root.filePath("outside.old")));
    QVERIFY(QFileInfo::exists(app + "/keep.txt"));
  }
};

QTEST_GUILESS_MAIN(TestAppUpdate)
#include "test_app_update.moc"
