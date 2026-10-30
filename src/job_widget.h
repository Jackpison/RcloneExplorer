#pragma once

#include "pch.h"
#include "ui_job_widget.h"

class FileProgressRow;

class JobWidget : public QWidget {
  Q_OBJECT

public:
  JobWidget(QProcess *process, const QString &info, const QStringList &args,
            const QString &source, const QString &dest,
            QWidget *parent = nullptr);
  ~JobWidget();

  void showDetails();

public slots:
  void cancel();

signals:
  void finished(const QString &info, bool success);
  void closed();

private:
  Ui::JobWidget ui;

  bool mRunning = true;
  QProcess *mProcess;

  QStringList mArgs;
  QHash<QString, FileProgressRow *> mActive;
  QSet<FileProgressRow *> mUpdated;

  QToolButton *mPause = nullptr;
  QLabel *mSummary = nullptr;
  bool mPaused = false;
  void buildFluentLayout();
  void setPaused(bool paused);
};
