#include "main_window.h"
#include "natural_sort.h"
#include <algorithm>
#include "theme.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "rclone_updater.h"
#include "job_options.h"
#include "job_widget.h"
#include "list_of_job_options.h"
#include "mount_widget.h"
#include "preferences_dialog.h"
#include "remote_widget.h"
#include "stream_widget.h"
#include "transfer_dialog.h"
#include "utils.h"

MainWindow::MainWindow() {
  ui.setupUi(this);

  if (IsPortableMode()) {
    // (portable mode is indicated in About and Settings, not the title)
  } else {
    this->setWindowTitle("Rclone Explorer");
  }

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // disable "?" WindowContextHelpButton (Qt 6 never shows it)
  QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

  buildShell(); // theme already applied in main()

  mSystemTray.setIcon(qApp->windowIcon());
  {
    auto settings = GetSettings();
    if (settings->contains("MainWindow/geometry")) {
      restoreGeometry(settings->value("MainWindow/geometry").toByteArray());
    }
    SetRclone(settings->value("Settings/rclone").toString());
    SetRcloneConf(settings->value("Settings/rcloneConf").toString());
#ifdef Q_OS_WIN
    // Portable: if no config location is set and an rclone.conf sits in the
    // bundled rclone folder, use it so remotes travel with the folder.
    if (IsPortableMode() &&
        settings->value("Settings/rcloneConf").toString().isEmpty() &&
        QFileInfo::exists(QDir(qApp->applicationDirPath())
                              .filePath("rclone/rclone.conf"))) {
      settings->setValue("Settings/rcloneConf", "rclone/rclone.conf");
      SetRcloneConf("rclone/rclone.conf");
    }
#endif

    mAlwaysShowInTray =
        settings->value("Settings/alwaysShowInTray", false).toBool();
    mCloseToTray = settings->value("Settings/closeToTray", false).toBool();
    mNotifyFinishedTransfers =
        settings->value("Settings/notifyFinishedTransfers", true).toBool();

    mSystemTray.setVisible(mAlwaysShowInTray);

    // during first run the lastUsed keys might not exist
    if (!(settings->contains("Settings/lastUsedSourceFolder"))) {
      // if lastUsedSourceFolder does not exist create new empty key
      settings->setValue("Settings/lastUsedSourceFolder", "");
    };
    if (!(settings->contains("Settings/lastUsedDestFolder"))) {
      // if lastUsedDestFolder does not exist create new empty key
      settings->setValue("Settings/lastUsedDestFolder", "");
    };
    if (!(settings->contains("Settings/defaultDownloadOptions"))) {
      // if defaultDownloadOptions does not exist create new empty key
      settings->setValue("Settings/defaultDownloadOptions", "");
    };
#ifdef Q_OS_MACOS
    // for macOS by default exclude .DS_Store files from uploads
    if (!(settings->contains("Settings/defaultUploadOptions"))) {
      // if defaultDownloadOptions does not exist create new empty key
      settings->setValue("Settings/defaultUploadOptions",
                         "--exclude .DS_Store");
    };
#else
    if (!(settings->contains("Settings/defaultUploadOptions"))) {
      // if defaultDownloadOptions does not exist create new empty key
      settings->setValue("Settings/defaultUploadOptions", "");
    };
#endif
    if (!(settings->contains("Settings/defaultRcloneOptions"))) {
      // if defaultRcloneOptions does not exist create new empty key
      settings->setValue("Settings/defaultRcloneOptions", "--fast-list");
    };
  }

  QObject::connect(ui.preferences, &QAction::triggered, this, [=, this]() {
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
      auto settings = GetSettings();
      settings->setValue("Settings/rclone", dialog.getRclone().trimmed());
      settings->setValue("Settings/rcloneConf",
                         dialog.getRcloneConf().trimmed());
      settings->setValue("Settings/stream", dialog.getStream());
      settings->setValue("Settings/mount", dialog.getMount());
      settings->setValue("Settings/defaultDownloadDir",
                         dialog.getDefaultDownloadDir().trimmed());
      settings->setValue("Settings/defaultUploadDir",
                         dialog.getDefaultUploadDir().trimmed());
      settings->setValue("Settings/defaultDownloadOptions",
                         dialog.getDefaultDownloadOptions().trimmed());
      settings->setValue("Settings/defaultUploadOptions",
                         dialog.getDefaultUploadOptions().trimmed());
      settings->setValue("Settings/defaultRcloneOptions",
                         dialog.getDefaultRcloneOptions().trimmed());

      settings->setValue("Settings/checkRcloneBrowserUpdates",
                         dialog.getCheckRcloneBrowserUpdates());
      settings->setValue("Settings/checkRcloneUpdates",
                         dialog.getCheckRcloneUpdates());

      settings->setValue("Settings/alwaysShowInTray",
                         dialog.getAlwaysShowInTray());
      settings->setValue("Settings/closeToTray", dialog.getCloseToTray());
      settings->setValue("Settings/notifyFinishedTransfers",
                         dialog.getNotifyFinishedTransfers());

      settings->setValue("Settings/showFolderIcons",
                         dialog.getShowFolderIcons());
      settings->setValue("Settings/showFileIcons", dialog.getShowFileIcons());
      settings->setValue("Settings/rowColors", dialog.getRowColors());
      settings->setValue("Settings/showHidden", dialog.getShowHidden());
      Theme::save(dialog.getTheme());
      Theme::apply();
      updateThemeButton();
      settings->setValue("Settings/iconSize", dialog.getIconSize().trimmed());

      settings->setValue("Settings/useProxy", dialog.getUseProxy());
      settings->setValue("Settings/http_proxy",
                         dialog.getHttpProxy().trimmed());
      settings->setValue("Settings/https_proxy",
                         dialog.getHttpsProxy().trimmed());
      settings->setValue("Settings/no_proxy", dialog.getNoProxy().trimmed());

      SetRclone(dialog.getRclone());
      SetRcloneConf(dialog.getRcloneConf());
      mFirstTime = true;
      rcloneGetVersion();

      mAlwaysShowInTray = dialog.getAlwaysShowInTray();
      mCloseToTray = dialog.getCloseToTray();
      mNotifyFinishedTransfers = dialog.getNotifyFinishedTransfers();

      mSystemTray.setVisible(mAlwaysShowInTray);
    }
  });

  QObject::connect(ui.quit, &QAction::triggered, this, [=, this]() {
    mCloseToTray = false;
    close();
  });

  QObject::connect(ui.about, &QAction::triggered, this, [=, this]() {
    QDialog dlg(this);
    dlg.setObjectName("AboutDialog");
    dlg.setWindowTitle(tr("About Rclone Explorer"));
    auto *grid = new QHBoxLayout(&dlg);
    grid->setContentsMargins(24, 24, 24, 20);
    grid->setSpacing(20);

    auto *logo = new QLabel;
    logo->setPixmap(qApp->windowIcon().pixmap(64, 64));
    logo->setAlignment(Qt::AlignTop);
    grid->addWidget(logo, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout;
    col->setSpacing(4);
    grid->addLayout(col, 1);

    auto *title = new QLabel(tr("Rclone Explorer"));
    title->setObjectName("AboutTitle");
    col->addWidget(title);
    auto *version = new QLabel(tr("Version %1").arg(RCLONE_BROWSER_VERSION));
    version->setProperty("secondary", true);
    col->addWidget(version);
    col->addSpacing(12);

    auto addText = [col](const QString &html) {
      auto *l = new QLabel(html);
      l->setTextFormat(Qt::RichText);
      l->setWordWrap(true);
      l->setOpenExternalLinks(true);
      l->setTextInteractionFlags(Qt::TextBrowserInteraction);
      col->addWidget(l);
      return l;
    };
    addText(tr("Browse, transfer and sync your cloud storage with rclone."));
    col->addSpacing(12);
    addText(tr("Website: <a href=\"https://jackpison.github.io/RcloneExplorer/\">"
               "jackpison.github.io/RcloneExplorer</a><br>"
               "Source: <a href=\"https://github.com/Jackpison/RcloneExplorer\">"
               "github.com/Jackpison/RcloneExplorer</a>"));
    col->addSpacing(12);
    addText(tr("Based on Rclone Browser, originally by "
               "<a href=\"https://github.com/mmozeiko/RcloneBrowser\">Martins Mozeiko</a>."));
    auto *legal = addText(tr("Copyright &copy; 2026 Jackpison. Released under the MIT License."));
    legal->setProperty("secondary", true);

    col->addSpacing(16);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setProperty("accent", true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    col->addWidget(buttons);

    dlg.setMinimumWidth(500);
    dlg.exec();
  });
  QObject::connect(ui.aboutQt, &QAction::triggered, qApp,
                   &QApplication::aboutQt);

  QObject::connect(
      ui.remotes, &QListWidget::currentItemChanged, this,
      [=, this](QListWidgetItem *current) { ui.open->setEnabled(current != NULL); });
  QObject::connect(ui.remotes, &QListWidget::itemActivated, ui.open,
                   &QPushButton::clicked);

  QObject::connect(ui.config, &QPushButton::clicked, this,
                   &MainWindow::rcloneConfig);
  QObject::connect(ui.refresh, &QPushButton::clicked, this,
                   &MainWindow::rcloneListRemotes);

  QObject::connect(ui.open, &QPushButton::clicked, this, [=, this]() {
    auto items = ui.remotes->selectedItems();
    if (items.isEmpty()) {
      return;
    }
    openRemote(items.front()->text(), items.front()->data(Qt::UserRole).toString());
  });


  QObject::connect(ui.tasksListWidget, &QListWidget::currentItemChanged, this,
                   [=, this](QListWidgetItem *current) {
                     ui.buttonDeleteTask->setEnabled(current != nullptr);
                     ui.buttonEditTask->setEnabled(current != nullptr);
                     ui.buttonRunTask->setEnabled(current != nullptr);
                     ui.buttonDryrunTask->setEnabled(current != nullptr);
                   });

  QObject::connect(ui.buttonRunTask, &QPushButton::clicked, this, [=, this]() {
    JobOptionsListWidgetItem *item = static_cast<JobOptionsListWidgetItem *>(
        ui.tasksListWidget->currentItem());
    runItem(item);
  });
  QObject::connect(ui.buttonDryrunTask, &QPushButton::clicked, this, [=, this]() {
    JobOptionsListWidgetItem *item = static_cast<JobOptionsListWidgetItem *>(
        ui.tasksListWidget->currentItem());
    runItem(item, true);
  });

  //    QObject::connect(ui.tasksListWidget, &QListWidget::itemDoubleClicked,
  //    this, [=]()
  //    {
  //        editSelectedTask();
  //    });

  QObject::connect(ui.buttonEditTask, &QPushButton::clicked, this,
                   [=, this]() { editSelectedTask(); });

  QObject::connect(ui.buttonDeleteTask, &QPushButton::clicked, this, [=, this]() {
    JobOptionsListWidgetItem *item = static_cast<JobOptionsListWidgetItem *>(
        ui.tasksListWidget->currentItem());
    JobOptions *jo = item->GetData();
    ListOfJobOptions::getInstance()->Forget(jo);
  });

  QObject::connect(ListOfJobOptions::getInstance(),
                   &ListOfJobOptions::tasksListUpdated, this,
                   &MainWindow::listTasks);


  showPage(0);

  listTasks();

  QObject::connect(&mSystemTray, &QSystemTrayIcon::activated, this,
                   [=, this](QSystemTrayIcon::ActivationReason reason) {
                     if (reason == QSystemTrayIcon::DoubleClick ||
                         reason == QSystemTrayIcon::Trigger) {
                       showNormal();
                       mSystemTray.setVisible(mAlwaysShowInTray);
                     }
                   });

  QObject::connect(&mSystemTray, &QSystemTrayIcon::messageClicked, this, [=, this]() {
    showNormal();
    mSystemTray.setVisible(mAlwaysShowInTray);

    showPage(1);
    if (mLastFinished) {
      mLastFinished->showDetails();
      ui.jobsArea->ensureWidgetVisible(mLastFinished);
    }
  });

  QMenu *trayMenu = new QMenu(this);
  QObject::connect(
      trayMenu->addAction("&Show"), &QAction::triggered, this, [=, this]() {
        MainWindow::setWindowState((windowState() & ~Qt::WindowMinimized) |
                                   Qt::WindowActive);
        MainWindow::show();  // bring window to top on macOS
        MainWindow::raise(); // bring window from minimized state on macOS
        MainWindow::activateWindow(); // bring window to front/unminimize on
                                      // windows
        mSystemTray.setVisible(mAlwaysShowInTray);
      });
  QObject::connect(trayMenu->addAction("&Quit"), &QAction::triggered, this,
                   &QWidget::close);
  mSystemTray.setContextMenu(trayMenu);

  mStatusMessage = new QLabel();
  ui.statusBar->addWidget(mStatusMessage);
  ui.statusBar->setStyleSheet("QStatusBar::item { border: 0; }");

  QTimer::singleShot(0, ui.remotes, SLOT(setFocus()));

  RcloneUpdater::cleanup();
  setupRcloneUpdater();

  QString rclone = GetRclone();
  // A saved path that no longer exists (e.g. an old AppImage mount point)
  // is treated like no setting at all.
  if (!rclone.isEmpty() && !QFileInfo::exists(rclone) &&
      QStandardPaths::findExecutable(rclone).isEmpty()) {
    rclone.clear();
  }
  if (rclone.isEmpty()) {
    // Prefer our own updated copy, then rclone bundled in the AppImage
    // (used for this session only: its path changes on every start), then
    // anything installed on the system.
    const QString bundled = RcloneUpdater::bundledPath();
    if (QFileInfo::exists(RcloneUpdater::managedPath())) {
      rclone = RcloneUpdater::managedSettingValue();
    } else if (!bundled.isEmpty()) {
      SetRclone(bundled);
      rcloneGetVersion();
      return;
    } else {
      rclone = QStandardPaths::findExecutable("rclone");
    }
    if (rclone.isEmpty()) {
      QTimer::singleShot(0, this, [this]() { offerRcloneDownload(); });
    } else {
      auto settings = GetSettings();
      settings->setValue("Settings/rclone", rclone);
      SetRclone(rclone);
      rcloneGetVersion();
    }
  } else {
    rcloneGetVersion();
  }
}

MainWindow::~MainWindow() {
  auto settings = GetSettings();
  settings->setValue("MainWindow/geometry", saveGeometry());
}

void MainWindow::rcloneGetVersion() {
  bool firstTime = mFirstTime;
  mFirstTime = false;

  QProcess *p = new QProcess();

  QObject::connect(
      p,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=, this](int code, QProcess::ExitStatus) {
        if (code == 0) {
          QString version = p->readAllStandardOutput().trimmed();

          // extract rclone version - numbers only
          QString rclone_info1 = version;
          QString rclone_version_no;
          int lineBreak = rclone_info1.indexOf('\n');
          if (lineBreak != -1) {
            rclone_info1.remove(lineBreak, rclone_info1.length() - lineBreak);
            rclone_version_no = rclone_info1;
            rclone_version_no.replace("rclone v", "");
            rclone_version_no.replace("-DEV", "");
          } else {
            // for very old rclone versions format was one line only
            rclone_version_no = rclone_info1.trimmed();
            rclone_version_no.replace("rclone v", "");
            rclone_version_no.replace("-DEV", "");
          }
          // save current version no in settings
          auto settings = GetSettings();
          settings->setValue("Settings/rcloneVersion", rclone_version_no);
          mRcloneVersion = rclone_version_no;

#if defined(Q_OS_WIN32)
          // check if required version
          unsigned int result =
              compareVersion(rclone_version_no.toStdString(), "1.50");

          if (result == 2) {
            QMessageBox::warning(
                this, "",
                "For mount functionality to work you need "
                "rclone version at least v1.50 "
                "and your current version is v" +
                    rclone_version_no +
                    ". Mount will be disabled. \n\nPlease consider upgrading.");
          };
#endif

          QStringList lines = version.split("\n", Qt::SkipEmptyParts);
          QString rclone_info2;
          QString rclone_info3;

          int counter = 0;
          foreach (QString line, lines) {
            line = line.trimmed();
            if (counter == 1)
              rclone_info2 = line.replace("- ", "");
            if (counter == 2)
              rclone_info3 = line.replace("- ", "");
            counter++;
          };

          QFileInfo appBundlePath;
#ifdef Q_OS_MACOS
          if (IsPortableMode()) {

            QFileInfo applicationPath = qApp->applicationFilePath();
            QFileInfo MacOSPath = applicationPath.dir().path();
            QFileInfo ContentsPath = MacOSPath.dir().path();
            appBundlePath = ContentsPath.dir().path();

            mStatusMessage->setText(
                rclone_info1 + " in " +
                QDir::toNativeSeparators(GetRclone().replace(
                    appBundlePath.fileName() + "/Contents/MacOS/../../../",
                    "")) +
                ", " + rclone_info2 + ", " + rclone_info3);

          } else {

            mStatusMessage->setText(rclone_info1 + " in " +
                                    QDir::toNativeSeparators(GetRclone()) +
                                    ", " + rclone_info2 + ", " + rclone_info3);
          }
#else
#ifdef Q_OS_WIN
          mStatusMessage->setText(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone()) + ", " +
                                  rclone_info2 + ", " + rclone_info3);
#else
          if (IsPortableMode()) {
            QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
            QString appImageConfigFolder = xdg_config_home.right(xdg_config_home.length()-xdg_config_home.lastIndexOf("/"));

            mStatusMessage->setText(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone().replace(appImageConfigFolder + "/..",  "")) + ", " +
                                  rclone_info2 + ", " + rclone_info3);
          } else {
            mStatusMessage->setText(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone()) + ", " +
                                  rclone_info2 + ", " + rclone_info3);
         }
#endif
#endif

          rcloneListRemotes();
        } else {
          if (p->error() != QProcess::FailedToStart) {
            if (getConfigPassword(p)) {
              rcloneGetVersion();
            } else {
              close();
            }
            p->deleteLater();
            return;
          }

          if (firstTime) {
            if (p->error() == QProcess::FailedToStart) {
              p->deleteLater();
              offerRcloneDownload();
              return;
            } else {
              QMessageBox::information(this, "Error",
                                       "Cannot check rclone version!\nPlease "
                                       "verify rclone location.");
            }
            emit ui.preferences->trigger();
          }
        }

        if (code == 0) {
          updateRcloneStatus();
          // the full version details go into the tooltip, not the status bar
          if (mRcloneStatus && !mStatusMessage->text().isEmpty()) {
            mRcloneStatus->setToolTip(mStatusMessage->text());
            mStatusMessage->clear();
          }
          maybeAutoCheckRclone();
        }

        p->deleteLater();
      });

  UseRclonePassword(p);
  p->start(GetRclone(),
           QStringList() << "version"
                         << "--ask-password=false",
           QIODevice::ReadOnly);
}

void MainWindow::rcloneConfig() {

  // for macOS and Linux we have to take care of possible spaces in rclone and
  // rclone.conf paths by using "" around them
  QString terminalRcloneCmd;
  if (!GetRcloneConf().isEmpty()) {
    terminalRcloneCmd = "\"" + GetRclone() + "\"" + " config" + " --config " +
                        "\"" + GetRcloneConf().at(1) + "\"";
  } else {
    terminalRcloneCmd = "\"" + GetRclone() + "\"" + " config";
  }

#if defined(Q_OS_WIN32) && (QT_VERSION < QT_VERSION_CHECK(5, 7, 0))
  QProcess::startDetached(GetRclone(), QStringList()
                                           << "config" << GetRcloneConf());
  return;
#else

  QProcess *p = new QProcess(this);

  QObject::connect(p,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=, this](int code, QProcess::ExitStatus) {
                     if (code == 0) {
                       emit rcloneListRemotes();
                     }
                     p->deleteLater();
                   });
#endif

#if defined(Q_OS_WIN32)
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
  p->setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
      });
  p->setProgram(GetRclone());
  p->setArguments(QStringList() << "config" << GetRcloneConf());
#endif

#elif defined(Q_OS_MACOS)
  auto tmp = new QFile("/tmp/rclone_config.command");
  tmp->open(QIODevice::WriteOnly);
  QTextStream(tmp) << "#!/bin/sh\n" << terminalRcloneCmd << "\n";
  tmp->close();
  tmp->setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser |
                      QFileDevice::ExeUser | QFileDevice::ReadGroup |
                      QFileDevice::ExeGroup | QFileDevice::ReadOther |
                      QFileDevice::ExeOther);
  p->setProgram("open");
  p->setArguments(QStringList() << tmp->fileName());
#else
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  QString terminal = env.value("TERMINAL");
  if (terminal.isEmpty()) {
    terminal = QStandardPaths::findExecutable("gnome-terminal");
    if (terminal.isEmpty()) {
      terminal = QStandardPaths::findExecutable("xfce4-terminal");
      if (terminal.isEmpty()) {
        terminal = QStandardPaths::findExecutable("xterm");
        if (terminal.isEmpty()) {
          terminal = QStandardPaths::findExecutable("x-terminal-emulator");
          if (terminal.isEmpty()) {
            terminal = QStandardPaths::findExecutable("konsole");
            if (terminal.isEmpty()) {
              QMessageBox::critical(this, "Error",
                                    "Not sure how to launch terminal!\n"
                                    "Please set path to terminal executable in "
                                    "$TERMINAL environment variable.",
                                    QMessageBox::Ok);
              return;
            }
          }
        }
      }
    }
  }

  p->setArguments(QStringList() << "-e" << terminalRcloneCmd);
  p->setProgram(terminal);
#endif

#if !defined(Q_OS_WIN32) || (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  UseRclonePassword(p);
  p->start(QIODevice::NotOpen);
#endif
}

void MainWindow::rcloneListRemotes() {
  Theme::clearRemoteIconCache(); // pick up logos added to the folder
  ui.remotes->clear();

  QProcess *p = new QProcess();

  QObject::connect(
      p,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=, this](int code, QProcess::ExitStatus) {
        if (code == 0) {
          QString bytes = p->readAllStandardOutput().trimmed();
          QStringList items = bytes.split('\n');

          for (const QString &line : items) {
            if (line.isEmpty()) {
              continue;
            }

            QStringList parts = line.split(':');
            if (parts.count() != 2) {
              continue;
            }

            QString name = parts[0].trimmed();
            QString type = parts[1].trimmed();
            QString tooltip = type;

            QIcon icon = Theme::remoteIcon(type);

            QListWidgetItem *item = new QListWidgetItem(icon, name);
            item->setData(Qt::UserRole, type);
            item->setToolTip(tooltip);
            ui.remotes->addItem(item);
          }
          rebuildNavRemotes();
        } else {
          if (p->error() != QProcess::FailedToStart) {
            if (getConfigPassword(p)) {
              rcloneListRemotes();
            }
          }
        }
        p->deleteLater();
      });

  UseRclonePassword(p);
  p->start(GetRclone(),
           QStringList() << "listremotes" << GetRcloneConf() << "--long"
                         << "--ask-password=false",
           QIODevice::ReadOnly);
}

bool MainWindow::getConfigPassword(QProcess *p) {
  QString output = p->readAllStandardError().trimmed();
  if (output.indexOf("RCLONE_CONFIG_PASS") > 0) {
    bool ok;
    QString password = QInputDialog::getText(
        this, tr("Rclone Explorer"),
        "Enter password for .rclone.conf configuration file:",
        QLineEdit::Password, QString(), &ok);
    if (ok) {
      SetRclonePassword(password);
      return true;
    }
  } else if (output.indexOf("unknown command \"listremotes\"") > 0) {
    QMessageBox::critical(this, tr("Rclone Explorer"),
                          "It seems rclone version you are using is too "
                          "old.\nPlease upgrade to the latest version");
    return false;
  }
  return false;
}

bool MainWindow::canClose() {
  if (mJobCount == 0) {
    return true;
  }

  bool wasVisible = isVisible();

  showPage(1);
  showNormal();

  const bool stop = Theme::confirm(
      this, tr("Rclone Explorer"),
      mJobCount == 1 ? tr("Stop the running job and quit?")
                     : tr("Stop %1 running jobs and quit?").arg(mJobCount),
      tr("Transfers, mounts and streams that are still running will be "
         "stopped."),
      tr("Stop and quit"), tr("Keep running"));

  if (!wasVisible) {
    hide();
  }

  if (stop) {
    for (int i = 0; i < ui.jobs->count(); i++) {
      QWidget *widget = ui.jobs->itemAt(i)->widget();
      if (auto mount = qobject_cast<MountWidget *>(widget)) {
        mount->cancel();
      } else if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        transfer->cancel();
      } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
        stream->cancel();
      }
    }
    return true;
  }

  return false;
}

void MainWindow::closeEvent(QCloseEvent *ev) {
  if (mCloseToTray && isVisible()) {
    mSystemTray.show();
    hide();
    ev->ignore();
    return;
  }

  if (canClose()) {
    QApplication::quit();
  } else {
    ev->ignore();
  }
}

void MainWindow::listTasks() {
  // keep the selection across a re-sort
  QUuid selected;
  if (auto *cur = static_cast<JobOptionsListWidgetItem *>(
          ui.tasksListWidget->currentItem())) {
    selected = cur->GetData()->uniqueId;
  }

  ui.tasksListWidget->clear();

  ListOfJobOptions *ljo = ListOfJobOptions::getInstance();
  QList<JobOptions *> tasks = ljo->getTasks();

  const QString mode =
      mTasksSort ? mTasksSort->currentData().toString() : QString("saved");
  if (mode != "saved") {
    auto byName = [](const JobOptions *a, const JobOptions *b) {
      return naturalCompare(a->description, b->description) < 0;
    };
    if (mode == "name_asc") {
      std::stable_sort(tasks.begin(), tasks.end(), byName);
    } else if (mode == "name_desc") {
      std::stable_sort(tasks.begin(), tasks.end(),
                       [&](const JobOptions *a, const JobOptions *b) {
                         return byName(b, a);
                       });
    } else if (mode == "type") {
      std::stable_sort(tasks.begin(), tasks.end(),
                       [&](const JobOptions *a, const JobOptions *b) {
                         if (a->jobType != b->jobType) {
                           return a->jobType == JobOptions::JobType::Download;
                         }
                         return byName(a, b);
                       });
    }
  }

  for (JobOptions *jo : tasks) {
    JobOptionsListWidgetItem *item = new JobOptionsListWidgetItem(
        jo,
        jo->jobType == JobOptions::JobType::Download ? mDownloadIcon
                                                     : mUploadIcon,
        jo->description);
    ui.tasksListWidget->addItem(item);
    if (!selected.isNull() && jo->uniqueId == selected) {
      ui.tasksListWidget->setCurrentItem(item);
    }
  }
}

void MainWindow::runItem(JobOptionsListWidgetItem *item, bool dryrun) {
  if (item == nullptr)
    return;
  JobOptions *jo = item->GetData();
  jo->dryRun = dryrun;
  QStringList args = jo->getOptions();
  // same wording as transfers started from the browser ("Copy /path")
  const QString mode = jo->operation == JobOptions::Move   ? QString("Move")
                       : jo->operation == JobOptions::Sync ? QString("Sync")
                                                           : QString("Copy");
  addTransfer(QString("%1 %2").arg(mode).arg(jo->source), jo->source,
              jo->dest, args);
}

void MainWindow::editSelectedTask() {
  auto selection = ui.tasksListWidget->selectionModel()->currentIndex();
  JobOptionsListWidgetItem *item = static_cast<JobOptionsListWidgetItem *>(
      ui.tasksListWidget->currentItem());
  JobOptions *jo = item->GetData();
  bool isDownload = (jo->jobType == JobOptions::Download);
  QString remote = isDownload ? jo->source : jo->dest;
  QString path = isDownload ? jo->dest : jo->source;
  // qDebug() << "remote:" + remote;
  // qDebug() << "path:" + path;
  TransferDialog td(isDownload, false, remote, path, jo->isFolder, this, jo,
                    true);
  td.exec();
  // restore the selection to help user keep track of what s/he was doing
  ui.tasksListWidget->selectionModel()->select(selection,
                                               QItemSelectionModel::Select);
  // edit mode on the TransferDialog suppresses the usual Accept buttons
  // and the Save Task button closes it... so there is nothing more to do here
}

void MainWindow::addTransfer(const QString &message, const QString &source,
                             const QString &dest, const QStringList &args) {
  QProcess *transfer = new QProcess(this);
  transfer->setProcessChannelMode(QProcess::MergedChannels);

  auto widget = new JobWidget(transfer, message, args, source, dest);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QObject::connect(
      widget, &JobWidget::finished, this, [=, this](const QString &info) {
        if (mNotifyFinishedTransfers) {
          qApp->alert(this);
          mLastFinished = widget;
          mSystemTray.showMessage("Transfer finished", info);
        }

        if (--mJobCount == 0) {
          setJobsTabText("Jobs");
        } else {
          setJobsTabText(QString("Jobs (%1)").arg(mJobCount));
        }
      });

  QObject::connect(widget, &JobWidget::closed, this, [=, this]() {
    if (widget == mLastFinished) {
      mLastFinished = nullptr;
    }
    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;
    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }
  });

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);
  setJobsTabText(QString("Jobs (%1)").arg(++mJobCount));

  UseRclonePassword(transfer);
  transfer->start(GetRclone(), GetRcloneConf() + args, QIODevice::ReadOnly);
}

void MainWindow::addMount(const QString &remote, const QString &folder) {
  QProcess *mount = new QProcess(this);
  mount->setProcessChannelMode(QProcess::MergedChannels);

  auto widget = new MountWidget(mount, remote, folder);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QObject::connect(widget, &MountWidget::finished, this, [=, this]() {
    if (--mJobCount == 0) {
      setJobsTabText("Jobs");
    } else {
      setJobsTabText(QString("Jobs (%1)").arg(mJobCount));
    }
  });

  QObject::connect(widget, &MountWidget::closed, this, [=, this]() {
    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;
    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }
  });

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);
  setJobsTabText(QString("Jobs (%1)").arg(++mJobCount));

  auto settings = GetSettings();
  QString opt = settings->value("Settings/mount").toString();
  bool driveShared = settings->value("Settings/driveShared").toBool();

  QStringList args;
  args << "mount";

#if defined(Q_OS_WIN32)
  args << "--rc";
  args << "--rc-addr";

  // calculate remote control interface port based on mount drive letter
  // this way every mount will have unique port assigned
  int port_offset = folder[0].toLatin1();
  unsigned short int rclone_rc_port_base = 19000;
  unsigned short int rclone_rc_port = rclone_rc_port_base + port_offset;
  args << "localhost:" + QVariant(rclone_rc_port).toString();
#endif

  // for google drive "shared with me" without --read-only writes go created in
  // main google drive it is more logical to mount it as read only so there is
  // no confusion
  if (driveShared) {
    args << "--drive-shared-with-me";
    args << "--read-only";
  };

  //	 default mount is now more generic. all options can be passed via
  // preferences mount field
  //       args << "--vfs-cache-mode";
  //       args << "writes";

  args.append(GetRcloneConf());
  if (!opt.isEmpty()) {
    args.append(opt.split(' '));
  }
  args << remote << folder;

  UseRclonePassword(mount);
  mount->start(GetRclone(), args, QIODevice::ReadOnly);
}

void MainWindow::addStream(const QString &remote, const QString &stream) {
  auto player = new QProcess();
  auto rclone = new QProcess();
  rclone->setStandardOutputProcess(player);

  QObject::connect(
      player,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=, this](int status, QProcess::ExitStatus) {
        player->deleteLater();
        if (status != 0 && player->error() == QProcess::FailedToStart) {
          QMessageBox::critical(
              this, "Error",
              QString("Failed to start '%1' player process").arg(stream));
          auto settings = GetSettings();
          settings->remove("Settings/streamConfirmed");
        }
      });

  auto widget = new StreamWidget(rclone, player, remote, stream);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QObject::connect(widget, &StreamWidget::finished, this, [=, this]() {
    if (--mJobCount == 0) {
      setJobsTabText("Jobs");
    } else {
      setJobsTabText(QString("Jobs (%1)").arg(mJobCount));
    }
  });

  QObject::connect(widget, &StreamWidget::closed, this, [=, this]() {
    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;
    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }
  });

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);
  setJobsTabText(QString("Jobs (%1)").arg(++mJobCount));

  {
    // "stream" is a full command line, e.g. "C:\Program Files\VLC\vlc.exe" -
    QStringList cmd = QProcess::splitCommand(stream);
    const QString program = cmd.isEmpty() ? QString() : cmd.takeFirst();
    player->start(program, cmd, QProcess::ReadOnly);
  }
  UseRclonePassword(rclone);
  rclone->start(GetRclone(),
                QStringList() << "cat" << GetRcloneConf() << remote,
                QProcess::WriteOnly);
}

// ------------------------------------------------------------------------
// rclone download / update
// ------------------------------------------------------------------------

void MainWindow::setupRcloneUpdater() {
  mUpdater = new RcloneUpdater(this);

  // Permanent status-bar indicator (right side): "rclone v1.75.1" or an update link.
  mRcloneStatus = new QLabel(this);
  mRcloneStatus->setTextFormat(Qt::RichText);
  mRcloneStatus->setContentsMargins(6, 0, 6, 0);
  // App version on the left (before messages), rclone version on the right.
  auto *appVersion = new QLabel(QString("Rclone Explorer %1").arg(RCLONE_BROWSER_VERSION), this);
  appVersion->setContentsMargins(6, 0, 6, 0);
  ui.statusBar->insertWidget(0, appVersion);
  ui.statusBar->addPermanentWidget(mRcloneStatus);
  QObject::connect(mRcloneStatus, &QLabel::linkActivated, this,
                   [this](const QString &link) {
                     if (link == "update" && !mLatestRclone.isEmpty()) {
                       installRclone(mLatestRclone);
                     } else if (link == "download") {
                       offerRcloneDownload();
                     }
                   });

  // Help menu entries.
  ui.menuHelp->insertSeparator(ui.menuHelp->actions().value(0));
  auto *checkNow = new QAction(tr("Check for rclone &updates…"), this);
  ui.menuHelp->insertAction(ui.menuHelp->actions().value(0), checkNow);
  QObject::connect(checkNow, &QAction::triggered, this,
                   [this]() { checkRcloneUpdate(true); });

  mAutoCheckAction =
      new QAction(tr("Check for rclone updates &automatically"), this);
  mAutoCheckAction->setCheckable(true);
  mAutoCheckAction->setChecked(
      GetSettings()->value("Settings/checkRcloneUpdates", true).toBool());
  ui.menuHelp->insertAction(ui.menuHelp->actions().value(1), mAutoCheckAction);
  QObject::connect(mAutoCheckAction, &QAction::toggled, this, [](bool on) {
    GetSettings()->setValue("Settings/checkRcloneUpdates", on);
  });

  updateRcloneStatus();
}

void MainWindow::updateRcloneStatus() {
  if (!mRcloneStatus) {
    return;
  }
  if (mRcloneVersion.isEmpty()) {
    mRcloneStatus->setText(
        tr("rclone not found — <a href=\"download\">download</a>"));
    mRcloneStatus->setToolTip(QString());
    return;
  }
  const bool newer =
      !mLatestRclone.isEmpty() &&
      RcloneUpdater::compareVersions(mLatestRclone, mRcloneVersion) > 0;
  if (newer) {
    mRcloneStatus->setText(tr("rclone v%1 — <a href=\"update\">update to %2</a>")
                               .arg(mRcloneVersion, mLatestRclone));
  } else {
    mRcloneStatus->setText(tr("rclone v%1").arg(mRcloneVersion));
  }
  mRcloneStatus->setToolTip(QDir::toNativeSeparators(GetRclone()));
}

void MainWindow::maybeAutoCheckRclone() {
  auto settings = GetSettings();
  if (!settings->value("Settings/checkRcloneUpdates", true).toBool()) {
    return;
  }
  const QDateTime last =
      settings->value("Settings/lastRcloneUpdateCheckTime").toDateTime();
  if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 20 * 3600) {
    // Still show the cached result from the last check.
    mLatestRclone = settings->value("Settings/latestRcloneVersion").toString();
    updateRcloneStatus();
    return;
  }
  checkRcloneUpdate(false);
}

void MainWindow::checkRcloneUpdate(bool interactive) {
  if (mUpdater->isBusy()) {
    return;
  }
  if (interactive) {
    mStatusMessage->setText(tr("Checking for rclone updates…"));
  }

  // One-shot connections for this check only.
  auto *ctx = new QObject(this);
  QObject::connect(
      mUpdater, &RcloneUpdater::latestVersionFound, ctx,
      [this, ctx, interactive](const QString &latest) {
        ctx->deleteLater();
        mStatusMessage->clear();
        mLatestRclone = latest;
        auto settings = GetSettings();
        settings->setValue("Settings/lastRcloneUpdateCheckTime",
                           QDateTime::currentDateTimeUtc());
        settings->setValue("Settings/latestRcloneVersion", latest);
        updateRcloneStatus();

        const bool newer =
            RcloneUpdater::compareVersions(latest, mRcloneVersion) > 0;
        if (!newer) {
          if (interactive) {
            QMessageBox::information(
                this, tr("rclone is up to date"),
                tr("You have the latest rclone (v%1).").arg(mRcloneVersion));
          }
          return;
        }
        // Background checks only nag once per version.
        if (!interactive &&
            settings->value("Settings/skippedRcloneVersion").toString() ==
                latest) {
          return;
        }

        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("rclone update available"));
        box.setText(tr("<b>rclone %1 is available.</b><br>You have v%2.")
                        .arg(latest, mRcloneVersion));
        QString info = tr("It will be downloaded from rclone.org, its "
                          "checksum verified, and installed to:<br>%1")
                           .arg(QDir::toNativeSeparators(
                               RcloneUpdater::managedPath()));
        if (QDir::cleanPath(GetRclone()) !=
            QDir::cleanPath(RcloneUpdater::managedPath())) {
          const bool bundled = !RcloneUpdater::bundledPath().isEmpty() &&
                               GetRclone() == RcloneUpdater::bundledPath();
          info += bundled
                      ? tr("<br><br>Rclone Explorer will use this copy instead of the "
                           "rclone included in the app. Your remotes and settings "
                           "are not affected.")
                      : tr("<br><br>Rclone Explorer will switch from your current "
                           "rclone (%1) to this copy. Your remotes and settings are "
                           "not affected.")
                            .arg(QDir::toNativeSeparators(GetRclone()));
        }
        info += tr("<br><br><a href=\"https://rclone.org/changelog/\">What's "
                   "new in rclone</a>");
        box.setInformativeText(info);
        box.setTextFormat(Qt::RichText);
        auto *update = box.addButton(tr("Update now"), QMessageBox::AcceptRole);
        auto *skip = box.addButton(tr("Skip this version"), QMessageBox::RejectRole);
        box.addButton(tr("Later"), QMessageBox::RejectRole);
        box.setDefaultButton(update);
        box.exec();
        if (box.clickedButton() == update) {
          installRclone(latest);
        } else if (box.clickedButton() == skip) {
          settings->setValue("Settings/skippedRcloneVersion", latest);
        }
      });
  QObject::connect(mUpdater, &RcloneUpdater::failed, ctx,
                   [this, ctx, interactive](const QString &error) {
                     ctx->deleteLater();
                     mStatusMessage->clear();
                     if (interactive) {
                       QMessageBox::warning(this, tr("Update check failed"),
                                            error);
                     }
                   });
  mUpdater->checkLatest();
}

void MainWindow::offerRcloneDownload() {
  QMessageBox box(this);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(tr("rclone not found"));
  box.setText(tr("<b>Rclone Explorer needs rclone to work.</b>"));
  box.setInformativeText(
      IsPortableMode()
          ? tr("It can download the latest official rclone into the "
               "<i>rclone</i> folder next to Rclone Explorer, so the whole "
               "folder stays portable.")
          : tr("It can download the latest official rclone for you, or you "
               "can point it at an rclone.exe you already have."));
  auto *download = box.addButton(tr("Download rclone"), QMessageBox::AcceptRole);
  auto *locate = box.addButton(tr("Locate rclone…"), QMessageBox::ActionRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(download);
  box.exec();

  if (box.clickedButton() == locate) {
    emit ui.preferences->trigger();
    return;
  }
  if (box.clickedButton() != download) {
    updateRcloneStatus();
    return;
  }

  // Find the latest version, then install it.
  auto *ctx = new QObject(this);
  mStatusMessage->setText(tr("Looking up the latest rclone…"));
  QObject::connect(mUpdater, &RcloneUpdater::latestVersionFound, ctx,
                   [this, ctx](const QString &latest) {
                     ctx->deleteLater();
                     mStatusMessage->clear();
                     mLatestRclone = latest;
                     installRclone(latest);
                   });
  QObject::connect(mUpdater, &RcloneUpdater::failed, ctx,
                   [this, ctx](const QString &error) {
                     ctx->deleteLater();
                     mStatusMessage->clear();
                     QMessageBox::warning(this, tr("Download failed"), error);
                     updateRcloneStatus();
                   });
  mUpdater->checkLatest();
}

void MainWindow::installRclone(const QString &version) {
  if (mUpdater->isBusy()) {
    return;
  }

  auto *dlg = new QProgressDialog(this);
  dlg->setWindowTitle(tr("Installing rclone %1").arg(version));
  dlg->setLabelText(tr("Starting…"));
  dlg->setMinimumWidth(420);
  dlg->setMinimumDuration(0);
  dlg->setRange(0, 0);
  dlg->setAutoClose(false);
  dlg->setAutoReset(false);
  dlg->setWindowModality(Qt::WindowModal);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();

  auto *ctx = new QObject(dlg);
  QObject::connect(dlg, &QProgressDialog::canceled, mUpdater,
                   &RcloneUpdater::cancel);
  QObject::connect(mUpdater, &RcloneUpdater::status, ctx,
                   [dlg](const QString &msg) { dlg->setLabelText(msg); });
  QObject::connect(mUpdater, &RcloneUpdater::progress, ctx,
                   [dlg](qint64 got, qint64 total) {
                     if (total > 0) {
                       dlg->setRange(0, 1000);
                       dlg->setValue(int(got * 1000 / total));
                     }
                   });
  QObject::connect(
      mUpdater, &RcloneUpdater::installed, ctx,
      [this, dlg](const QString &v, const QString &) {
        dlg->close();
        const QString setting = RcloneUpdater::managedSettingValue();
        GetSettings()->setValue("Settings/rclone", setting);
        SetRclone(setting);
        mLatestRclone = v;
        mStatusMessage->setText(
            tr("rclone %1 installed. Running jobs keep their current "
               "version until they finish.").arg(v));
        mFirstTime = true;
        rcloneGetVersion(); // refreshes version, status bar and remotes
      });
  QObject::connect(mUpdater, &RcloneUpdater::failed, ctx,
                   [this, dlg](const QString &error) {
                     dlg->close();
                     QMessageBox::warning(this, tr("rclone update failed"),
                                          error);
                     updateRcloneStatus();
                   });

  mUpdater->install(version);
}
