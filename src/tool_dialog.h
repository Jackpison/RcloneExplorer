#pragma once

#include "pch.h"

// Small dialogs behind the "Tools" menu of a remote: compare two places,
// remove duplicates, copy to another remote, two-way sync. Each one only
// collects choices; the caller runs the resulting rclone command as a job.
class ToolDialog : public QDialog {
  Q_OBJECT

public:
  enum Tool { Compare, Dedupe, CopyTo, Bisync };

  // `source` is a full rclone path such as "drive:Photos".
  ToolDialog(Tool tool, const QString &source, QWidget *parent = nullptr);

  QString message() const { return mMessage; }
  QString source() const { return mSource; }
  QString dest() const { return mDest; }
  QStringList args() const { return mArgs; }

private:
  Tool mTool;
  QString mSource;
  QString mMessage;
  QString mDest;
  QStringList mArgs;

  QLineEdit *mOther = nullptr;
  QCheckBox *mOptionA = nullptr;
  QCheckBox *mOptionB = nullptr;
  QComboBox *mMode = nullptr;

  void accept() override;
};
