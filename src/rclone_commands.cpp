#include "rclone_commands.h"

#include <QRegularExpression>
#include "command_args.h"

namespace RcloneCommands {

QStringList statsArgs() {
  return {"--stats", "1s", "--stats-file-name-length", "0"};
}

QStringList check(const QString &source, const QString &dest,
                  const CheckOptions &o) {
  QStringList l{"check"};
  if (o.oneWay) {
    l << "--one-way";
  }
  if (o.sizeOnly) {
    l << "--size-only";
  }
  l << statsArgs() << source << dest;
  return l;
}

QStringList dedupeModes() {
  return {"newest", "oldest", "largest", "smallest", "first", "rename", "skip"};
}

QStringList dedupe(const QString &path, const QString &mode) {
  QStringList l{"dedupe", "--dedupe-mode",
                dedupeModes().contains(mode) ? mode : QString("skip")};
  l << statsArgs() << path;
  return l;
}

QStringList cleanup(const QString &remote) {
  return {"cleanup", remote};
}

QStringList copy(const QString &source, const QString &dest, bool move) {
  QStringList l{move ? "move" : "copy"};
  l << statsArgs() << source << dest;
  return l;
}

QStringList bisync(const QString &path1, const QString &path2,
                   const BisyncOptions &o) {
  QStringList l{"bisync"};
  if (o.resync) {
    l << "--resync";
  }
  if (o.dryRun) {
    l << "--dry-run";
  }
  l << statsArgs() << path1 << path2;
  return l;
}

QStringList cacheModes() { return {"off", "minimal", "writes", "full"}; }

QStringList mountArgs(const MountOptions &o, const QString &globalOptions) {
  const QStringList global = splitArguments(globalOptions);
  auto globalSets = [&](const QString &name) {
    for (const QString &a : global) {
      if (a == name || a.startsWith(name + "=")) {
        return true;
      }
    }
    return false;
  };

  QStringList l;
  if (!globalSets("--vfs-cache-mode") && cacheModes().contains(o.cacheMode) &&
      o.cacheMode != "off") {
    l << "--vfs-cache-mode" << o.cacheMode;
  }
  if (o.readOnly && !globalSets("--read-only")) {
    l << "--read-only";
  }
  if (!o.volumeName.trimmed().isEmpty() && !globalSets("--volname") &&
      !globalSets("--volume-name")) {
    l << "--volname" << o.volumeName.trimmed();
  }
  l << splitArguments(o.extra);
  l << global;
  return l;
}

QString globEscape(const QString &text) {
  QString out;
  out.reserve(text.size() * 2);
  for (const QChar c : text) {
    if (QStringLiteral("*?[]{}\\").contains(c)) {
      out += QLatin1Char('\\');
    }
    out += c;
  }
  return out;
}

bool validBandwidth(const QString &limit) {
  const QString l = limit.trimmed();
  if (l.isEmpty()) {
    return true;
  }
  static const QString rate = QStringLiteral(R"((?:off|\d+(?:\.\d+)?(?:[BKMGTP](?:iB?)?)?))");
  static const QRegularExpression single(
      QStringLiteral("^%1(?::%1)?$").arg(rate), QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression slot(
      QStringLiteral(R"(^(?:(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)-)?(?:[01]?\d|2[0-3]):[0-5]\d,%1(?::%1)?$)").arg(rate),
      QRegularExpression::CaseInsensitiveOption);
  const QStringList parts = l.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.size() == 1 && single.match(parts.first()).hasMatch()) {
    return true;
  }
  for (const QString &p : parts) {
    if (!slot.match(p).hasMatch()) {
      return false;
    }
  }
  return true;
}

QStringList withBandwidth(const QStringList &args, const QString &limit) {
  const QString l = limit.trimmed();
  if (l.isEmpty() || !validBandwidth(l)) {
    return args;
  }
  for (const QString &a : args) {
    if (a == "--bwlimit" || a.startsWith("--bwlimit=")) {
      return args;
    }
  }
  return QStringList(args) << "--bwlimit" << l;
}

} // namespace RcloneCommands
