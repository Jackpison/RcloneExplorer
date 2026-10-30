#pragma once

#include "pch.h"
#include "ui_remote_widget.h"


class RemoteWidget : public QWidget {
  Q_OBJECT

public:
  RemoteWidget(const QString &remote, bool isLocal,
               bool isGoogle, QWidget *parent = nullptr);
  ~RemoteWidget();

  // Shown under the remote name in the page header (e.g. "drive").
  void setRemoteType(const QString &type);

signals:
  void addTransfer(const QString &message, const QString &source,
                   const QString &remote, const QStringList &args);
  void addMount(const QString &remote, const QString &folder,
                const QStringList &mountOptions);
  void addStream(const QString &remote, const QString &stream);
  void closeRequested();
  void newTabRequested();

private:
  Ui::RemoteWidget ui;
  QLabel *mTypeLabel = nullptr;
  QLineEdit *mFilter = nullptr;
  QToolButton *mUp = nullptr;
  QWidget *mCrumbs = nullptr;
  QPersistentModelIndex mFilterFolder;
  class QListView *mGrid = nullptr;
  QToolButton *mViewIcons = nullptr;
  QToolButton *mViewDetails = nullptr;
  bool mSyncingFromGrid = false;
  QList<QToolButton *> mPrimaryButtons;
  bool mCompactBar = false;
  int mIconPx = 64;
  QActionGroup *mSizeGroup = nullptr;
  void applyIconSize(int px);
  void updateCommandBar();
  bool eventFilter(QObject *o, QEvent *e) override;
  void setIconView(bool icons, class ItemModel *model);
  void showFolderInGrid(const QModelIndex &folder, class ItemModel *model);

  void buildFluentUi(class ItemModel *model, const QString &remote,
                     bool isLocal);
  void updateBreadcrumbs(class ItemModel *model, const QString &remote);
  void applyFilter(class ItemModel *model);
};
