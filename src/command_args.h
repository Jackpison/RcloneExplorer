#pragma once

#include <QProcess>
#include <QRegularExpression>
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

// One argument as it must be typed in this system's terminal: quoted when it
// holds spaces or characters the shell would act on (for "Copy command").
inline QString shellQuote(const QString &arg) {
#ifdef Q_OS_WIN
  // cmd.exe and PowerShell: double quotes, with inner quotes escaped
  if (!arg.isEmpty() && !arg.contains(QRegularExpression(R"([\s"&|<>^%!])"))) {
    return arg;
  }
  QString escaped = arg;
  escaped.replace('"', "\\\"");
  return '"' + escaped + '"';
#else
  // POSIX shells: single quotes, with inner single quotes closed and reopened
  if (!arg.isEmpty() && !arg.contains(QRegularExpression(R"([^A-Za-z0-9_@%+=:,./-])"))) {
    return arg;
  }
  QString escaped = arg;
  escaped.replace('\'', "'\\''");
  return '\'' + escaped + '\'';
#endif
}

inline QString shellCommand(const QStringList &args) {
  QStringList quoted;
  for (const QString &a : args) {
    quoted << shellQuote(a);
  }
  return quoted.join(' ');
}
