#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

// Updating Rclone Explorer itself: the parts that need no network, so they
// can be tested on their own (see AppUpdater for downloading).
namespace AppUpdate {

struct Asset {
  QString name;
  QUrl url;
  qint64 size = 0;
};

struct Release {
  QString version; // "5.4.2"
  QUrl page;       // the release page on GitHub
  QList<Asset> assets;
  const Asset *asset(const QString &name) const;
};

// How this copy was installed, which decides how it is updated.
enum class Kind {
  PortableZip, // Windows zip with "Rclone Explorer.ini": files are swapped in place
  Installer,   // Windows setup: the new setup runs
  AppImage,    // Linux AppImage: the download page opens
  Flatpak,     // Flathub updates it: no checks at all
  Scoop,       // Windows zip installed by Scoop: `scoop update` updates it
  Other        // built from source: the download page opens
};
Kind kind(bool portableMode, const QString &appDir = QString());
// The app folder is one of Scoop's: it holds Scoop's install.json and
// manifest.json, or lies in …\scoop\apps\<name>\<version or current>.
bool isScoopFolder(const QString &appDir);

// GitHub's "latest release" answer. Only a release of this project with a
// plain x.y.z version and download links on github.com is accepted.
bool parseRelease(const QByteArray &json, Release *out);
bool trustedAssetUrl(const QUrl &url, const QString &version);

// -1, 0 or 1, comparing "5.4.10" with "5.4.9" as numbers.
int compareVersions(const QString &a, const QString &b);

QString zipName(const QString &version);       // RcloneExplorer-5.4.3-windows-x64-portable.zip
QString legacyZipName(const QString &version); // RcloneExplorer-5.4.2-windows-x64.zip (5.4.2 and older)
QString setupName(const QString &version); // RcloneExplorer-5.4.2-windows-x64-setup.exe

// The SHA-256 published for `name` in a sha256sum-style list, lower case,
// or "" when it is not listed.
QString checksumFor(const QByteArray &sums, const QString &name);

// What happens to a file of the new version (path relative to the app folder,
// with '/' separators) when it is copied over the old one.
enum class Rule {
  Replace,      // program files
  KeepExisting, // remote logos: the user may have replaced them
  Skip          // the user's settings, tasks, mirrors and rclone
};
Rule ruleFor(const QString &relativePath);

// Copies the unpacked new version (`newRoot`, the folder holding the exe) over
// `appDir`. Files in use cannot be overwritten on Windows but can be renamed,
// so each replaced file is first renamed to "<name>.old" and listed in
// leftoversFile(), to be deleted on the next start. On any failure everything
// is put back as it was and `error` says why.
bool apply(const QString &newRoot, const QString &appDir, QString *error);
QString leftoversFile(const QString &appDir);
// Deletes the files an earlier update left behind (see apply()).
void removeLeftovers(const QString &appDir);

} // namespace AppUpdate
