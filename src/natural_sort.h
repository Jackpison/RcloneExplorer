#pragma once

#include <QString>

// Case-insensitive "natural" order: "Task 2" sorts before "Task 10".
// Own implementation so the order is the same on every platform and locale.
inline int naturalCompare(const QString &a, const QString &b) {
  const QString x = a.toCaseFolded(), y = b.toCaseFolded();
  int i = 0, j = 0;
  while (i < x.size() && j < y.size()) {
    if (x[i].isDigit() && y[j].isDigit()) {
      int si = i, sj = j;
      while (si < x.size() && x[si] == QLatin1Char('0')) ++si;
      while (sj < y.size() && y[sj] == QLatin1Char('0')) ++sj;
      int ei = si, ej = sj;
      while (ei < x.size() && x[ei].isDigit()) ++ei;
      while (ej < y.size() && y[ej].isDigit()) ++ej;
      if (ei - si != ej - sj) {
        return (ei - si) < (ej - sj) ? -1 : 1; // fewer digits = smaller
      }
      const int c = x.mid(si, ei - si).compare(y.mid(sj, ej - sj));
      if (c != 0) {
        return c < 0 ? -1 : 1;
      }
      i = ei;
      j = ej;
    } else {
      if (x[i] != y[j]) {
        return x[i] < y[j] ? -1 : 1;
      }
      ++i;
      ++j;
    }
  }
  if (i < x.size()) return 1;
  if (j < y.size()) return -1;
  return 0;
}
