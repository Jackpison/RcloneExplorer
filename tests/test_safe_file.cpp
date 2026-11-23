#include "safe_file.h"

#include <QtTest>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

namespace {
QByteArray read(const QString &path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
}
void write(const QString &path, const QByteArray &data) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(data);
}
} // namespace

class TestSafeFile : public QObject {
  Q_OBJECT

private slots:
  void newFile() {
    QTemporaryDir dir;
    const QString path = dir.filePath("tasks.bin");
    QVERIFY(WriteFileSafely(path, "abc"));
    QCOMPARE(read(path), QByteArray("abc"));
    QVERIFY(!QFileInfo::exists(PendingSaveFile(path))); // gone once written
  }

  void overwriteShorter() {
    QTemporaryDir dir;
    const QString path = dir.filePath("mirrors.json");
    write(path, "a long old content");
    QVERIFY(WriteFileSafely(path, "new"));
    QCOMPARE(read(path), QByteArray("new"));
  }

  void emptyContent() {
    QTemporaryDir dir;
    const QString path = dir.filePath("tasks.bin");
    write(path, "old");
    QVERIFY(WriteFileSafely(path, QByteArray()));
    QCOMPARE(read(path), QByteArray());
  }

  void leftoverPendingIsReplaced() {
    // An earlier save was interrupted: its copy is still there. Saving the
    // same content as the file must still finish and clear it.
    QTemporaryDir dir;
    const QString path = dir.filePath("tasks.bin");
    write(path, "same");
    write(PendingSaveFile(path), "older interrupted save");
    QVERIFY(WriteFileSafely(path, "same"));
    QCOMPARE(read(path), QByteArray("same"));
    QVERIFY(!QFileInfo::exists(PendingSaveFile(path)));
  }

  void unwritableFolder() {
    QTemporaryDir dir;
    const QString path = dir.filePath("missing-folder/tasks.bin");
    QVERIFY(!WriteFileSafely(path, "abc"));
  }

#ifndef Q_OS_WIN
  void hardLinkKept() {
    // Scoop keeps user files through hard links: the link must see the update
    QTemporaryDir dir;
    const QString path = dir.filePath("tasks.bin");
    const QString link = dir.filePath("persist-tasks.bin");
    write(path, "v1");
    QCOMPARE(::link(QFile::encodeName(path).constData(), QFile::encodeName(link).constData()), 0);
    QVERIFY(WriteFileSafely(path, "v2"));
    QCOMPARE(read(link), QByteArray("v2"));
  }
#endif
};

QTEST_GUILESS_MAIN(TestSafeFile)
#include "test_safe_file.moc"
