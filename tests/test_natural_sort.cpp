#include "natural_sort.h"

#include <QStringList>
#include <QtTest>
#include <algorithm>

class TestNaturalSort : public QObject {
  Q_OBJECT

  static QStringList sorted(QStringList l) {
    std::stable_sort(l.begin(), l.end(), [](const QString &a, const QString &b) {
      return naturalCompare(a, b) < 0;
    });
    return l;
  }

private slots:
  void numbersInNaturalOrder() {
    QCOMPARE(sorted({"Task 10", "Task 2", "Task 1"}),
             QStringList({"Task 1", "Task 2", "Task 10"}));
  }
  void ignoresCase() {
    QCOMPARE(sorted({"task 10", "Task 2", "alpha", "Beta"}),
             QStringList({"alpha", "Beta", "Task 2", "task 10"}));
    QCOMPARE(naturalCompare("Photos", "photos"), 0);
  }
  void leadingZeros() {
    QCOMPARE(naturalCompare("file 007", "file 7"), 0);
    QVERIFY(naturalCompare("file 08", "file 10") < 0);
  }
  void longNumbersDoNotOverflow() {
    QVERIFY(naturalCompare("backup 99999999999999999999", "backup 100000000000000000000") < 0);
  }
  void numbersInsideText() {
    QCOMPARE(sorted({"v1.10", "v1.9", "v1.2"}), QStringList({"v1.2", "v1.9", "v1.10"}));
  }
  void prefixSortsFirst() {
    QVERIFY(naturalCompare("Sync", "Sync 2") < 0);
    QVERIFY(naturalCompare("Sync 2", "Sync") > 0);
    QCOMPARE(naturalCompare("", ""), 0);
    QVERIFY(naturalCompare("", "a") < 0);
  }
  void unicodeNames() { // U/u and A with umlauts, as UTF-8 bytes (ASCII source for MSVC)
    QCOMPARE(naturalCompare(QString::fromUtf8("\xC3\x9C" "ber 2"), QString::fromUtf8("\xC3\xBC" "ber 2")), 0);
    QVERIFY(naturalCompare(QString::fromUtf8("\xC3\x84" "rger 2"), QString::fromUtf8("\xC3\x84" "rger 10")) < 0);
  }
  void symmetric() {
    const QStringList s = {"a", "A 1", "a 01", "b", "Task 2", "task 10", "", "10", "9"};
    for (const QString &x : s)
      for (const QString &y : s)
        QCOMPARE(naturalCompare(x, y), -naturalCompare(y, x));
  }
};

QTEST_APPLESS_MAIN(TestNaturalSort)
#include "test_natural_sort.moc"
