#pragma once

#include "pch.h"
#include "rclone_commands.h"

// Asks where to mount a remote and how: cache mode (the setting that decides
// whether apps can write to the mount reliably), read-only, volume name.
// The choices are remembered between mounts.
class MountDialog : public QDialog {
  Q_OBJECT

public:
  MountDialog(const QString &remote, QWidget *parent = nullptr);

  QString target() const { return mTarget; }
  RcloneCommands::MountOptions options() const;

private:
  QLineEdit *mPath = nullptr;
  QComboBox *mCache = nullptr;
  QCheckBox *mReadOnly = nullptr;
  QLineEdit *mVolume = nullptr;
  QLineEdit *mExtra = nullptr;
  QString mTarget;

  void accept() override;
};
