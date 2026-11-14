#include "app_update.h"
#include <cstdio>
#include "main_window.h"
#include "theme.h"
#include "utils.h"

int main(int argc, char *argv[]) {

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // Qt 6 always enables per-monitor high-DPI scaling.
  static const char ENV_VAR_QT_DEVICE_PIXEL_RATIO[] = "QT_DEVICE_PIXEL_RATIO";
  if (!qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO) &&
      !qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR") &&
      !qEnvironmentVariableIsSet("QT_SCALE_FACTOR") &&
      !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  }
#endif

#if defined(Q_OS_LINUX)
  // The desktop portal gives the GNOME or KDE file dialogs (the only way to
  // reach files from inside a Flatpak) and tells Qt whether the system is
  // light or dark, which the "System" theme follows. Without a running portal
  // Qt quietly uses its own dialogs. A theme the user chose is kept.
  if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME")) {
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
  }
#endif

  QApplication app(argc, argv);

  //  app.setApplicationDisplayName("Rclone Explorer");
  app.setApplicationName("rclone-browser");
  app.setOrganizationName("rclone-browser");
  // Settings keep their old internal name ("rclone-browser") on purpose, so
  // everyone's settings carry over from Rclone Browser. Only the visible name
  // changed.
  // the window's app id under Wayland: the name of the installed desktop file
  app.setDesktopFileName(IsFlatpak() ? qEnvironmentVariable("FLATPAK_ID")
                                     : QStringLiteral("rclone-explorer"));
  MigrateLegacyPortableIni();
  app.setWindowIcon(QIcon(":/icons/icon.png"));

  // Build check used by CI: verifies that the deployed folder layout works
  // (platform plugin found via qt.conf, SVG/ICO image formats, TLS backend
  // for the updater) and exits without opening a window.
  if (app.arguments().contains(QStringLiteral("--selftest"))) {
    const auto formats = QImageReader::supportedImageFormats();
    const bool images = formats.contains("svg") && formats.contains("ico");
    const bool tls = !QSslSocket::availableBackends().isEmpty() &&
                     QSslSocket::supportsSsl();
    const bool platform = !QGuiApplication::platformName().isEmpty();
    std::fprintf(stdout, "platform=%s images=%d tls=%d plugins=%s\n",
                 qPrintable(QGuiApplication::platformName()), images, tls,
                 qPrintable(QCoreApplication::libraryPaths().join(';')));
    std::fflush(stdout);
    QString fontReport;
    const bool fonts = Theme::selfTestFonts(&fontReport);
    std::fprintf(stdout, "%s\n", qPrintable(fontReport));
    std::fflush(stdout);
    return (platform && images && tls && fonts) ? 0 : 1;
  }

// initialize SSL libraries
// see: https://github.com/linuxdeploy/linuxdeploy-plugin-qt/issues/57
#if defined(Q_OS_LINUX)
  QString currentDir = QDir::currentPath();
  QDir::setCurrent(QCoreApplication::applicationDirPath());
  QSslSocket::supportsSsl();
  QDir::setCurrent(currentDir);
#endif

  auto settings = GetSettings();

  // initialize proxy settings
  if (!(settings->contains("Settings/useProxy"))) {
    settings->setValue("Settings/useProxy", "false");
  };
  if (!(settings->contains("Settings/http_proxy"))) {
    settings->setValue("Settings/http_proxy", "");
  };
  if (!(settings->contains("Settings/https_proxy"))) {
    settings->setValue("Settings/https_proxy", "");
  };
  if (!(settings->contains("Settings/no_proxy"))) {
    settings->setValue("Settings/no_proxy", "");
  };

  if (settings->value("Settings/useProxy").toBool()) {
    qputenv("HTTP_PROXY", settings->value("Settings/http_proxy").toByteArray());
    qputenv("http_proxy", settings->value("Settings/http_proxy").toByteArray());
    qputenv("HTTPS_PROXY",
            settings->value("Settings/https_proxy").toByteArray());
    qputenv("https_proxy",
            settings->value("Settings/https_proxy").toByteArray());
    qputenv("NO_PROXY", settings->value("Settings/no_proxy").toByteArray());
    qputenv("no_proxy", settings->value("Settings/no_proxy").toByteArray());
  }

  // during first run the iconSize key might not exist
  if (!(settings->contains("Settings/iconSize"))) {
    // if iconSize does not exist create new key
    settings->setValue("Settings/iconSize", "medium");
  };

  // enforce one instance of Rclone Explorer per user
  QString tmpDir;
  QString applicationNameBase;
  QFileInfo applicationPath;
  QFileInfo applicationUserPath;

  // QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
  // qDebug() << QString("main.cpp $XDG_CONFIG_HOME: " + xdg_config_home);

  // QString APPIMAGE = qgetenv("APPIMAGE");
  // qDebug() << QString("main.cpp $APPIMAGE: " + APPIMAGE);

  QFileInfo appBundlePath;

  if (IsPortableMode()) {

    //  qDebug() << QString("isPortable is true");
    //  applicationPath = qApp->applicationFilePath();
#ifdef Q_OS_MACOS
    // on macOS excecutable file is located in
    // ./rclone-browser.app/Contents/MasOS/
    // to get actual bundle folder we have
    // to traverse three levels up
    applicationPath = qApp->applicationFilePath();
    tmpDir = applicationPath.absolutePath() + "/../../..";

    // get bundle name
    QFileInfo MacOSPath = applicationPath.dir().path();
    QFileInfo ContentsPath = MacOSPath.dir().path();
    appBundlePath = ContentsPath.dir().path();

#else
    // not macOS
#ifdef Q_OS_WIN
    applicationPath = QFileInfo(qApp->applicationFilePath());
    tmpDir = applicationPath.absolutePath();
#else
    QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
    tmpDir = xdg_config_home + "/rclone-browser";
    // create ./rclone-browser folder
    if (!QDir(tmpDir).exists()) {
      QDir().mkdir(tmpDir);
    }
#endif
#endif
  } else {
    // not portable mode
    // get tmp folder from Qt  - OS dependend
    tmpDir = QDir::tempPath();
  }

  // check if tmpDir writable
  // as isWritable does weird things on Windows
  // we do this old fashioned way by creating temp file
  QTemporaryFile tempfile(tmpDir + "/rclone-browserXXXXXX.test");

  if (tempfile.open()) {
    tempfile.close();
    tempfile.remove();
  } else {
    // folder has no write access
    if (IsPortableMode()) {
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setText("You need write "
                     "access to this folder:\n\n"
#ifdef Q_OS_MACOS
                     + appBundlePath.absolutePath() +
#else
#ifdef Q_OS_WIN
                     + tmpDir +
#else
                     + tmpDir.left(tmpDir.length() - 15) +
#endif
#endif
                     "\n\n"
#ifdef Q_OS_MACOS
                     "Or remove file:\n\n" +
                     appBundlePath.baseName() +
                     ".ini \n\nfrom the above folder "
#else
#ifdef Q_OS_WIN
                     "Or remove file:\n\n" +
                     applicationPath.baseName() +
                     ".ini \n\nfrom the above folder "
#else
                     "Or remove folder:\n\n" +
                     tmpDir.left(tmpDir.length() - 15) +
                     "\n\n"
#endif
#endif
                     "to disable portable mode.");
      msgBox.exec();
    } else {
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setText("You need write "
                     "access to this folder: \n\n"
#ifdef Q_OS_MACOS
                     + tmpDir
#else

#ifdef Q_OS_WIN
                     + tmpDir
#else

                     + tmpDir.left(tmpDir.length() - 15)
#endif
#endif
      );
      msgBox.exec();
    }
    return static_cast<int>(
        0x80004004); // exit immediately if folder not writable
  }

  // qDebug() << QString("main.cpp tmpDir:  " + tmpDir);

  // not most elegant as fixed name but in reality not big deal
  // The single-instance lock lives in the temp folder (not next to the exe,
  // where it cluttered the portable folder). The name includes a hash of the
  // install location, so separate portable copies can still run side by side.
  Q_UNUSED(tmpDir);
  const QByteArray installId =
      QCryptographicHash::hash(QCoreApplication::applicationDirPath().toUtf8(),
                               QCryptographicHash::Sha256)
          .toHex()
          .left(16);
  QLockFile lockFile(QDir::temp().filePath("RcloneExplorer_" +
                                           QString::fromLatin1(installId) + ".lock"));

  // Started by an update: the old version is still closing, so wait for it.
  const bool afterUpdate = QCoreApplication::arguments().contains("--after-update");
  if (!lockFile.tryLock(afterUpdate ? 20000 : 100)) {
    // if already running display warning and quit
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Rclone Explorer is already running."
                   "\r\n\nOnly one instance is allowed.");
    msgBox.exec();
    return static_cast<int>(
        0x80004004); // exit immediately if another instance is running
  }

  // files an update replaced while they were in use (see AppUpdate::apply)
  AppUpdate::removeLeftovers(QCoreApplication::applicationDirPath());

  // Apply fonts, colours and the style sheet before any widget exists, so
  // nothing is created with Windows' default font and styled from it.
  Theme::apply();

  MainWindow w;
  w.show();

  return app.exec();
}
