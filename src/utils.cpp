#include "utils.h"
#include "legacy_ini.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

static QString gRclone;
static QString gRcloneConf;
static QString gRclonePassword;

// Software versions comparison
// source: https://helloacm.com/how-to-compare-version-numbers-in-c/
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> r;
  int j = 0;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (s[i] == d) {
      r.push_back(s.substr(j, i - j));
      j = i + 1;
    }
  }
  r.push_back(s.substr(j));
  return r;
}

unsigned int compareVersion(std::string version1, std::string version2) {
  auto v1 = split(version1, '.');
  auto v2 = split(version2, '.');
  unsigned int max = v1.size() > v2.size() ? v1.size() : v2.size();
  // pad the shorter version string
  if (v1.size() != max) {
    for (unsigned int i = max - v1.size(); i--;) {
      v1.push_back("0");
    }
  } else {
    for (unsigned int i = max - v2.size(); i--;) {
      v2.push_back("0");
    }
  }
  for (unsigned int i = 0; i < max; i++) {
    unsigned int n1 = stoi(v1[i]);
    unsigned int n2 = stoi(v2[i]);
    if (n1 > n2) {
      // version1 is higher than version2
      return 1;
    } else if (n1 < n2) {
      // version2 is higher than version1
      return 2;
    }
  }
  // the same versions
  return 0;
}

static QString GetIniFilename() {
#ifdef Q_OS_MACOS
  QFileInfo applicationPath = qApp->applicationFilePath();
  //  qDebug() << QString(applicationPath.absolutePath());
  // on macOS excecutable file is located in
  // ./rclone-browser.app/Contents/MasOS/ to get actual bundle folder we have to
  // traverse three levels up
  QFileInfo MacOSPath = applicationPath.dir().path();
  QFileInfo ContentsPath = MacOSPath.dir().path();
  QFileInfo appBundlePath = ContentsPath.dir().path();
  //  qDebug() << QString("utils.cpp appBundle.absolutePath: " +
  //                      appBundlePath.absolutePath());
  //  qDebug() << QString(
  //      "utils.cpp ini file:" +
  //      appBundlePath.dir().filePath(appBundlePath.baseName() + ".ini"));
  return appBundlePath.dir().filePath(appBundlePath.baseName() + ".ini");
#else
#ifdef Q_OS_WIN
  QFileInfo applicationPath(qApp->applicationFilePath());
  return applicationPath.dir().filePath(applicationPath.baseName() + ".ini");
#else
  // Portable mode on Linux: the AppImage runtime sets XDG_CONFIG_HOME to
  // "<AppImage>.config" when that folder exists next to the AppImage.
  const QString xdg_config_home = qEnvironmentVariable("XDG_CONFIG_HOME");
  if (xdg_config_home.isEmpty()) {
    return QString(); // not portable (previously pointed at /rclone-browser/)
  }
  return xdg_config_home + "/rclone-browser/rclone-browser.ini";
#endif
#endif
}

bool IsPortableMode() {
  QString ini = GetIniFilename();
  QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
  //  qDebug() << QString("utils.cpp $XDG_CONFIG_HOME: " + xdg_config_home);
  QString appimage = qgetenv("APPIMAGE");
  //  qDebug() << QString("utils.cpp $APPIMAGE: " + appimage);

  // cat ".config" from $XDG_CONFIG_HOME
  // it should be the same as appimage if run from AppImage
  xdg_config_home = xdg_config_home.left(xdg_config_home.length() - 7);
  //  qDebug() << QString("utils.cpp $XDG_CONFIG_HOME-7: " + xdg_config_home);

  if (!xdg_config_home.isEmpty() && !appimage.isEmpty() &&
      xdg_config_home == appimage) {

    return true;
  }

  if (QFileInfo(ini).exists()) {

    return true;
  } else {
    return false;
  }

  //  return QFileInfo(ini).exists();
}

namespace {

// Portable ini, written in place.
//
// QSettings saves an ini by writing a new file and renaming it over the old
// one. That breaks a hard-linked ini, which is how Scoop keeps settings across
// updates: the app would keep working, but every change after the install
// would be lost on the next update. So QSettings works on a private copy in the
// temp folder, and whenever a settings object is done, the copy is written back
// into the real ini in place (same file, only when something changed). The ini
// format itself is still QSettings' own.

QString ShadowIniPath(const QString &ini) {
  const QByteArray id =
      QCryptographicHash::hash(QFileInfo(ini).absoluteFilePath().toUtf8(), QCryptographicHash::Sha1)
          .toHex()
          .left(16);
  return QDir(QDir::tempPath()).filePath("rclone-explorer-settings-" + QString::fromLatin1(id) + ".ini");
}

QByteArray ReadAll(const QString &path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Overwrites `to` with the bytes of `from` without replacing the file itself.
void WriteInPlace(const QString &from, const QString &to) {
  const QByteArray data = ReadAll(from);
  if (QFileInfo::exists(to) && ReadAll(to) == data) {
    return;
  }
  QFile out(to);
  if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    out.write(data);
  }
}

class InPlaceIniSettings : public QSettings {
public:
  InPlaceIniSettings(const QString &shadow, const QString &ini)
      : QSettings(shadow, QSettings::IniFormat), mIni(ini) {}
  ~InPlaceIniSettings() override {
    sync();
    if (status() == QSettings::NoError) {
      WriteInPlace(fileName(), mIni);
    }
  }

private:
  QString mIni;
};

} // namespace

std::unique_ptr<QSettings> GetSettings() {
  if (IsPortableMode()) {
    const QString ini = GetIniFilename();
    const QString shadow = ShadowIniPath(ini);
    static bool seeded = false; // refresh the copy from the real ini once per run
    if (!seeded) {
      seeded = true;
      QFile::remove(shadow);
      if (QFileInfo::exists(ini)) {
        QFile::copy(ini, shadow);
        QFile(shadow).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
      }
    }
    return std::unique_ptr<QSettings>(new InPlaceIniSettings(shadow, ini));
  }
  return std::unique_ptr<QSettings>(new QSettings);
}

void ReadSettings(QSettings *settings, QObject *widget) {
  QString name = widget->objectName();
  if (!name.isEmpty() && settings->contains(name)) {
    if (QRadioButton *obj = qobject_cast<QRadioButton *>(widget)) {
      obj->setChecked(settings->value(name).toBool());
      return;
    }
    if (QCheckBox *obj = qobject_cast<QCheckBox *>(widget)) {
      obj->setChecked(settings->value(name).toBool());
      return;
    }
    if (QComboBox *obj = qobject_cast<QComboBox *>(widget)) {
      obj->setCurrentIndex(settings->value(name).toInt());
      return;
    }
    if (QSpinBox *obj = qobject_cast<QSpinBox *>(widget)) {
      obj->setValue(settings->value(name).toInt());
      return;
    }
    if (QLineEdit *obj = qobject_cast<QLineEdit *>(widget)) {
      obj->setText(settings->value(name).toString());
      return;
    }
    if (QPlainTextEdit *obj = qobject_cast<QPlainTextEdit *>(widget)) {
      int count = settings->beginReadArray(name);
      QStringList lines;
      lines.reserve(count);
      for (int i = 0; i < count; i++) {
        settings->setArrayIndex(i);
        lines.append(settings->value("value").toString());
      }
      settings->endArray();

      obj->setPlainText(lines.join('\n'));
      return;
    }
  }

  for (auto child : widget->children()) {
    ReadSettings(settings, child);
  }
}

void WriteSettings(QSettings *settings, QObject *widget) {
  QString name = widget->objectName();
  if (QCheckBox *obj = qobject_cast<QCheckBox *>(widget)) {
    settings->setValue(name, obj->isChecked());
    return;
  }
  if (QComboBox *obj = qobject_cast<QComboBox *>(widget)) {
    settings->setValue(name, obj->currentIndex());
    return;
  }
  if (QSpinBox *obj = qobject_cast<QSpinBox *>(widget)) {
    settings->setValue(name, obj->value());
    return;
  }
  if (QLineEdit *obj = qobject_cast<QLineEdit *>(widget)) {
    if (obj->text().isEmpty()) {
      settings->remove(name);
    } else {
      settings->setValue(name, obj->text());
    }
    return;
  }
  if (QPlainTextEdit *obj = qobject_cast<QPlainTextEdit *>(widget)) {
    QString text = obj->toPlainText().trimmed();
    if (!text.isEmpty()) {
      QStringList lines = text.split('\n');
      settings->beginWriteArray(name, lines.size());
      for (int i = 0; i < lines.count(); i++) {
        settings->setArrayIndex(i);
        settings->setValue("value", lines[i]);
      }
      settings->endArray();
    }
    return;
  }

  for (auto child : widget->children()) {
    WriteSettings(settings, child);
  }
}

QStringList GetRcloneConf() {
  if (gRcloneConf.isEmpty()) {
    return QStringList();
  }

  QString conf = gRcloneConf;
  if (IsPortableMode() && QFileInfo(conf).isRelative()) {
#ifdef Q_OS_MACOS
    // on macOS excecutable file is located in
    // ./rclone-browser.app/Contents/MasOS/rclone-browser to get actual bundle
    // folder we have to traverse three levels up
    conf = QDir(qApp->applicationDirPath() + "/../../..").filePath(conf);
#else
#ifdef Q_OS_WIN
    conf = QDir(qApp->applicationDirPath()).filePath(conf);
#else
    QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
    conf = QDir(xdg_config_home + "/..").filePath(conf);
#endif
#endif
    //    qDebug() << QString("utils.cpp conf: " + conf);
  }
  return QStringList() << "--config" << conf;
}

void SetRcloneConf(const QString &rcloneConf) { gRcloneConf = rcloneConf; }

QString GetRclone() {
  QString rclone = gRclone;
  if (IsPortableMode() && QFileInfo(rclone).isRelative()) {
#ifdef Q_OS_MACOS
    // on macOS excecutable file is located in
    // ./rclone-browser.app/Contents/MasOS/rclone-browser to get actual bundle
    // folder we have to traverse three levels up
    rclone = QDir(qApp->applicationDirPath() + "/../../..").filePath(rclone);
#else
#ifdef Q_OS_WIN
    rclone = QDir(qApp->applicationDirPath()).filePath(rclone);
#else
    QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
    rclone = QDir(xdg_config_home + "/..").filePath(rclone);
#endif
#endif
    //    qDebug() << QString("utils.cpp rclone portable: " + rclone);
  }

  return rclone;
}

void SetRclone(const QString &rclone) { gRclone = rclone.trimmed(); }

void UseRclonePassword(QProcess *process) {
  if (!gRclonePassword.isEmpty()) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("RCLONE_CONFIG_PASS", gRclonePassword);
    process->setProcessEnvironment(env);
  }
}

void SetRclonePassword(const QString &rclonePassword) {
  gRclonePassword = rclonePassword;
}

QStringList GetDriveSharedWithMe() {
  auto settings = GetSettings();
  bool driveShared = settings->value("Settings/driveShared", false).toBool();
  QStringList driveSharedOption;
  if (driveShared) {
    driveSharedOption << "--drive-shared-with-me";
  }
  return driveSharedOption;
}

QStringList GetDefaultRcloneOptionsList() {
  auto settings = GetSettings();
  QString defaultRcloneOptions =
      settings->value("Settings/defaultRcloneOptions").toString();
  return splitArguments(defaultRcloneOptions);
}

QStringList GetShowHidden() {
  auto settings = GetSettings();
  bool showHidden = settings->value("Settings/showHidden", true).toBool();
  QStringList showHiddenOption;
  if (!showHidden) {
    showHiddenOption << "--exclude"
                     << ".*/**"
                     << "--exclude"
                     << ".*";
  }
  return showHiddenOption;
}

void MigrateLegacyPortableIni() {
#ifdef Q_OS_WIN
  MigrateLegacyIni(qApp->applicationFilePath());
#endif
}
