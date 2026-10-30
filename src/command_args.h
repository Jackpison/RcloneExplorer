#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>

// Splits a user-typed options field ("--vfs-cache-mode full --foo \"a b\"")
// into arguments. Quotes keep spaces together and repeated spaces never
// produce empty arguments (QString::split(' ') did both wrong).
inline QStringList splitArguments(const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }
  return QProcess::splitCommand(trimmed);
}
