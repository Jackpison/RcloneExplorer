#pragma once

#include "safe_file.h"

#include "pch.h"
#include "command_args.h"

std::unique_ptr<QSettings> GetSettings();

// Running as a Flatpak: rclone and the app are updated by Flathub, and mounts
// (FUSE) are not available in the sandbox.
bool IsFlatpak();

// A path or file name for a message box: a stretch too long for one line is
// shortened in the middle instead of being cut off at the edge (message
// boxes only wrap at spaces). Short names are returned unchanged.
QString ForMessage(const QString &text);


// Windows portable mode: releases up to 4.x were called "Rclone Browser" and
// kept their settings in RcloneBrowser.ini next to the exe. Copy such a file
// to the new name (RcloneExplorer.ini) so portable mode and all settings carry
// over. A fresh, never-used RcloneExplorer.ini (the one shipped in the zip) is
// replaced; one that has been used is never touched.
void MigrateLegacyPortableIni();

void ReadSettings(QSettings *settings, QObject *widget);
void WriteSettings(QSettings *settings, QObject *widget);

bool IsPortableMode();

QString GetRclone();
void SetRclone(const QString &rclone);

QStringList GetRcloneConf();
void SetRcloneConf(const QString &rcloneConf);

void UseRclonePassword(QProcess *process);
void SetRclonePassword(const QString &rclonePassword);

QStringList GetDriveSharedWithMe();
QStringList GetDefaultRcloneOptionsList();
QStringList GetShowHidden();

unsigned int compareVersion(std::string, std::string);

// Names of the remotes in the rclone configuration, each with its trailing
// colon ("drive:"), as `rclone listremotes` prints them. Uses the list the
// main window keeps up to date and then calls `done` right away; before that
// list exists it asks rclone and calls `done` later (an empty list if rclone
// cannot be run, with `error` saying why). Nothing is called once `context`
// is gone.
void ListRemoteNames(
    QObject *context,
    const std::function<void(const QStringList &names, const QString &error)> &done);
void SetKnownRemoteNames(const QStringList &names); // plain names, no ':'

// A tooltip that shows `text` exactly: Qt would read a file name such as
// "<b>x" as formatting.
QString PlainToolTip(const QString &text);
