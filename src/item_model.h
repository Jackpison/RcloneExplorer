#pragma once

#include "pch.h"

struct Item {
  Item() {}

  ~Item() {
    for (auto child : childs) {
      if (child->isLoading()) {
        child->isDeleted = true;
      } else {
        delete child;
      }
    }
  }

  bool isLoading() const { return state == Loading1 || state == Loading2; }

  // Row of this item in its parent. The last known row is remembered and
  // verified, so repeated lookups are O(1) instead of a linear search (which
  // made big folders quadratic).
  int num() const {
    Q_ASSERT(parent);
    const auto &siblings = parent->childs;
    if (rowHint >= 0 && rowHint < siblings.size() && siblings[rowHint] == this) {
      return rowHint;
    }
    rowHint = int(siblings.indexOf(const_cast<Item *>(this)));
    return rowHint;
  }

  Item *parent = nullptr;

  enum State { Unknown, Loading1, Loading2, Ready, Special };

  State state = Unknown;
  bool isFolder = false;
  bool isDeleted = false;
  QString name;
  QDir path;
  QString modified;
  quint64 size = 0;

  QVector<Item *> childs;

  mutable int rowHint = -1;
  mutable QString typeCache; // "Image", "PDF document", ... filled on first use
};

class ItemSorter;

class ItemModel : public QAbstractItemModel {
  Q_OBJECT
public:
  ItemModel(const QString &remote, QObject *parent);
  ~ItemModel();

  const QDir &path(const QModelIndex &index) const;
  bool isLoading(const QModelIndex &index) const;
  void refresh(const QModelIndex &index);
  void rename(const QModelIndex &index, const QString &name);
  bool isTopLevel(const QModelIndex &index) const;
  bool isFolder(const QModelIndex &index) const;
  // True for a folder whose contents have not been requested yet. rowCount()
  // would start listing it, so code that only wants to look at what is
  // already there (such as the filter) must check this first.
  bool needsLoad(const QModelIndex &index) const;

  QModelIndex addRoot(const QString &name, const QString &path);

  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  bool hasChildren(const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  void sort(int column, Qt::SortOrder order) override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

  bool removeRows(int row, int count, const QModelIndex &parent) override;

  Qt::ItemFlags flags(const QModelIndex &index) const override;

  bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                       int column, const QModelIndex &parent) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;

signals:
  void drop(const QDir &path, const QModelIndex &parent);

private:
  Item *mRoot;

  // At most a few `rclone lsjson` runs at once; the rest wait their turn.
  // A big tree (or a provider with rate limits, such as Google Drive) must
  // not be hit by dozens of listings in parallel.
  static constexpr int kMaxParallelLoads = 4;
  int mActiveLoads = 0;
  QList<std::function<void()>> mPendingLoads;
  void startOrQueueLoad(const std::function<void()> &start);
  void loadFinished();

  QString mRemote;


  bool mFolderIcons;
  bool mFileIcons;

  QIcon mDriveIcon;
  QIcon mFolderIcon;


  int mSortColumn;
  Qt::SortOrder mSortOrder;


  Item *get(const QModelIndex &index) const;
  void load(const QPersistentModelIndex &parentIndex, Item *parent);

  void sortRecursive(Item *item, const ItemSorter &sorter);
  void sort(const QModelIndex &parent, Item *item);
};
