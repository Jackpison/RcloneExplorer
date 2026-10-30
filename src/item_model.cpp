#include "item_model.h"
#include "file_names.h"
#include "theme.h"
#include "rclone_output.h"
#include "utils.h"
#include <algorithm>

namespace {
static void advanceSpinner(QString &text) {
  int spinnerPos = (int)((size_t)text.length() - 2);
  QChar current = text[spinnerPos];
  static const QChar spinner[] = {'-', '\\', '|', '/'};
  size_t spinnerCount = sizeof(spinner) / sizeof(*spinner);
  const QChar *found = std::find(spinner, spinner + spinnerCount, current);
  size_t idx = found - spinner;
  size_t next = idx == spinnerCount - 1 ? 0 : idx + 1;
  text[spinnerPos] = spinner[next];
}

QString getNiceSize(quint64 size) {
  // "6.7 MB" style, as in Windows 11 File Explorer details view
  return QLocale().formattedDataSize(qint64(size), 1,
                                     QLocale::DataSizeTraditionalFormat);
}
} // namespace

// Friendly type name, as in File Explorer's "Type" column.
QString typeName(const Item *item) {
  if (item->isFolder) {
    return QObject::tr("Folder");
  }
  if (!item->typeCache.isEmpty()) {
    return item->typeCache;
  }
  static const QHash<QString, const char *> names = [] {
    QHash<QString, const char *> m;
    auto add = [&](std::initializer_list<const char *> exts, const char *name) {
      for (auto e : exts) m.insert(e, name);
    };
    add({"jpg", "jpeg", "png", "gif", "bmp", "webp", "heic", "heif", "tif", "tiff",
         "avif", "ico", "svg", "raw", "cr2", "nef", "arw", "dng"}, "Image");
    add({"mp4", "mkv", "mov", "avi", "wmv", "webm", "m4v", "mpg", "mpeg", "flv",
         "3gp", "ts"}, "Video");
    add({"mp3", "flac", "wav", "aac", "ogg", "m4a", "wma", "opus", "aiff"}, "Audio");
    add({"zip", "7z", "rar", "tar", "gz", "bz2", "xz", "zst", "tgz", "cab"}, "Archive");
    add({"iso", "img", "vhd", "vhdx", "vmdk"}, "Disk image");
    add({"pdf"}, "PDF document");
    add({"doc", "docx", "odt", "rtf", "pages"}, "Document");
    add({"xls", "xlsx", "xlsm", "ods", "csv", "tsv"}, "Spreadsheet");
    add({"ppt", "pptx", "odp", "key"}, "Presentation");
    add({"txt", "md", "log", "nfo"}, "Text");
    add({"ini", "cfg", "conf", "json", "xml", "yml", "yaml", "toml"}, "Settings / data");
    add({"c", "cpp", "h", "hpp", "cs", "py", "js", "ts", "tsx", "jsx", "java", "go",
         "rs", "rb", "php", "html", "css", "sh", "ps1", "bat", "cmd", "sql", "kt",
         "swift", "lua"}, "Source code");
    add({"exe", "msi", "msix", "appx", "apk", "dmg", "deb", "rpm", "appimage"}, "Program / installer");
    add({"ttf", "otf", "woff", "woff2"}, "Font");
    return m;
  }();
  const QString ext = fileExtension(item->name).toLower();
  QString result;
  if (ext.isEmpty()) {
    result = QObject::tr("File");
  } else {
    auto it = names.find(ext);
    result = it != names.end() ? QObject::tr(it.value())
                               : QObject::tr("%1 file").arg(ext.toUpper());
  }
  item->typeCache = result;
  return result;
}

QString extensionOf(const Item *item) {
  return item->isFolder ? QString() : fileExtension(item->name).toLower();
}

class ItemSorter {
public:
  inline ItemSorter(int column, Qt::SortOrder order)
      : mColumn(column), mOrder(order) {
    mCompare.setNumericMode(true);
  }

  bool operator()(const Item *a, const Item *b) const {
    switch (mColumn) {
    case 0:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      return mOrder == Qt::AscendingOrder
                 ? mCompare.compare(a->name, b->name) < 0
                 : mCompare.compare(b->name, a->name) < 0;

    case 1:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      if (a->size == b->size) {
        return mOrder == Qt::AscendingOrder
                   ? mCompare.compare(a->name, b->name) < 0
                   : mCompare.compare(b->name, a->name) < 0;
      }
      return mOrder == Qt::AscendingOrder ? a->size < b->size
                                          : b->size < a->size;

    case 2:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      if (a->modified == b->modified) {
        return mOrder == Qt::AscendingOrder
                   ? mCompare.compare(a->name, b->name) < 0
                   : mCompare.compare(b->name, a->name) < 0;
      }
      return mOrder == Qt::AscendingOrder ? a->modified < b->modified
                                          : b->modified < a->modified;

    case 3: // type
    case 4: // extension
    case 5: { // path
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      const QString ka = mColumn == 3 ? typeName(a) : mColumn == 4 ? extensionOf(a) : (a->parent ? a->parent->path.path() : QString());
      const QString kb = mColumn == 3 ? typeName(b) : mColumn == 4 ? extensionOf(b) : (b->parent ? b->parent->path.path() : QString());
      if (ka == kb) {
        return mOrder == Qt::AscendingOrder ? mCompare.compare(a->name, b->name) < 0
                                            : mCompare.compare(b->name, a->name) < 0;
      }
      return mOrder == Qt::AscendingOrder ? mCompare.compare(ka, kb) < 0
                                          : mCompare.compare(kb, ka) < 0;
    }
    }
    return false;
  }

private:
  QCollator mCompare;
  int mColumn;
  Qt::SortOrder mOrder;
};

ItemModel::ItemModel(const QString &remote, QObject *parent)
    : QAbstractItemModel(parent), mRemote(remote) {
  for (int px : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256}) {
    mDriveIcon.addFile(QString(":/drive/drive-%1.png").arg(px), QSize(px, px));
  }
  mFolderIcon = Theme::fileIcon(QString(), true);

  auto settings = GetSettings();
  mFolderIcons = settings->value("Settings/showFolderIcons", true).toBool();
  mFileIcons = settings->value("Settings/showFileIcons", true).toBool();

  mRoot = new Item();
  mRoot->isFolder = true;
  mRoot->state = Item::Ready;
}

ItemModel::~ItemModel() { delete mRoot; }

const QDir &ItemModel::path(const QModelIndex &index) const {
  return get(index)->path;
}

bool ItemModel::isLoading(const QModelIndex &index) const {
  return get(index)->parent->isLoading();
}

void ItemModel::refresh(const QModelIndex &index) {
  Item *item = get(index);
  Item *folderItem = item->isFolder ? item : item->parent;
  if (folderItem->isLoading()) {
    return;
  }
  load(item->isFolder ? index : index.parent(), folderItem);
}

void ItemModel::rename(const QModelIndex &index, const QString &name) {
  Item *item = get(index);
  item->name = name;
  item->typeCache.clear();
  item->path.setPath(item->parent->path.filePath(item->name));
  emit dataChanged(index, index, QVector<int>{Qt::DisplayRole});
}

bool ItemModel::isTopLevel(const QModelIndex &index) const {
  return get(index)->parent == mRoot;
}

bool ItemModel::isFolder(const QModelIndex &index) const {
  return get(index)->isFolder;
}

bool ItemModel::needsLoad(const QModelIndex &index) const {
  const Item *item = get(index);
  return item->isFolder && item->state == Item::Unknown;
}

void ItemModel::startOrQueueLoad(const std::function<void()> &start) {
  if (mActiveLoads < kMaxParallelLoads) {
    ++mActiveLoads;
    start();
  } else {
    mPendingLoads.append(start);
  }
}

void ItemModel::loadFinished() {
  if (mActiveLoads > 0) {
    --mActiveLoads;
  }
  while (mActiveLoads < kMaxParallelLoads && !mPendingLoads.isEmpty()) {
    ++mActiveLoads;
    mPendingLoads.takeFirst()();
  }
}

QModelIndex ItemModel::addRoot(const QString &name, const QString &path) {
  emit layoutAboutToBeChanged();

  Item *item = new Item();
  item->isFolder = true;
  item->name = name;
  item->path.setPath(path);
  item->parent = mRoot;
  mRoot->childs.append(item);

  emit layoutChanged();

  return createIndex(item->num(), 0, item);
}

QModelIndex ItemModel::index(int row, int column,
                             const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  Item *item = get(parent);
  return createIndex(row, column, item->childs[row]);
}

QModelIndex ItemModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) {
    return QModelIndex();
  }

  Item *child = get(index);
  if (child->parent == mRoot) {
    return QModelIndex();
  }

  return createIndex(child->parent->num(), 0, child->parent);
}

bool ItemModel::hasChildren(const QModelIndex &parent) const {
  Item *item = get(parent);
  if (item->isFolder) {
    if (item->state == Item::Ready) {
      return !item->childs.isEmpty();
    }
    return true;
  }
  return false;
}

int ItemModel::rowCount(const QModelIndex &parent) const {
  Item *item = get(parent);
  if (item->isFolder) {
    if (item->state == Item::Unknown) {
      const_cast<ItemModel *>(this)->load(parent, item);
    }
  }
  return item->childs.count();
}

int ItemModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 6; // Name, Size, Modified, Type, Extension, Path
}

void ItemModel::sort(int column, Qt::SortOrder order) {
  mSortColumn = column;
  mSortOrder = order;
  sort(QModelIndex(), mRoot);
}

QVariant ItemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  const Item *item = get(index);

  if (role == Qt::DecorationRole && index.column() == 0) {
    if (item->state == Item::Special) {
      return QIcon();
    }

    if (item->isFolder) {
      if (mFolderIcons) {
        return item->parent == mRoot ? mDriveIcon : mFolderIcon;
      }
      return QIcon();
    }

    if (mFileIcons) {
      // Icons are drawn from bundled SVGs and cached per file type, so they
      // are available immediately: no background loading needed.
      return Theme::fileIcon(item->name, false);
    }

    return QIcon();
  }

  if (role == Qt::TextAlignmentRole) {
    if (index.column() == 1) {
      return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case 0:
      return item->name;
    case 1:
      if (item->isFolder || item->state == Item::Special) {
        return QString();
      } else {
        return getNiceSize(item->size);
      }
    case 2:
      return item->modified;
    case 3:
      return item->state == Item::Special ? QString() : typeName(item);
    case 4:
      return item->state == Item::Special ? QString() : extensionOf(item);
    case 5: {
      if (item->state == Item::Special) {
        return QString();
      }
      QString folder = item->parent ? item->parent->path.path() : QString();
      return folder.isEmpty() ? QStringLiteral("/") : folder;
    }
    }
  }
  return QVariant();
}

QVariant ItemModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return tr("Name");
    case 1:
      return tr("Size");
    case 2:
      return tr("Modified");
    case 3:
      return tr("Type");
    case 4:
      return tr("Extension");
    case 5:
      return tr("Path");
    }
  }

  return QVariant();
}

bool ItemModel::removeRows(int row, int count, const QModelIndex &parent) {
  if (!hasIndex(row, 0, parent)) {
    return false;
  }

  Item *item = get(parent);
  if (row + count > item->childs.count()) {
    return false;
  }

  emit beginRemoveRows(parent, row, row + count - 1);

  for (int i = row; i < row + count; i++) {
    Item *node = item->childs.at(i);
    if (node->isLoading()) {
      node->isDeleted = true;
    } else {
      delete node;
    }
  }
  item->childs.remove(row, count);

  emit endRemoveRows();

  return true;
}

Qt::ItemFlags ItemModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

  if (!index.isValid()) {
    return defaultFlags;
  }

  return Qt::ItemIsDropEnabled | defaultFlags;
}

bool ItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column,
                                const QModelIndex &parent) const {
  Q_UNUSED(row);
  Q_UNUSED(column);
  Q_UNUSED(parent);

  if (action != Qt::CopyAction && action != Qt::MoveAction) {
    return false;
  }

  if (!data->hasUrls()) {
    return false;
  }

  auto urls = data->urls();
  if (urls.count() == 1) {
    return urls.front().isLocalFile();
  }

  return false;
}

bool ItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                             int row, int column, const QModelIndex &parent) {
  if (!canDropMimeData(data, action, row, column, parent)) {
    return false;
  }

  QDir path = QDir(data->urls().front().toLocalFile());
  Item *item = get(parent);

  emit drop(path, item->isFolder ? parent : parent.parent());

  return false;
}

Item *ItemModel::get(const QModelIndex &index) const {
  return index.isValid() ? static_cast<Item *>(index.internalPointer()) : mRoot;
}

void ItemModel::load(const QPersistentModelIndex &parentIndex, Item *parent) {
  // One `rclone lsjson` call lists folders and files together (previously
  // two separate rclone runs, lsd + lsl, each listing the folder again).
  auto ls = new QProcess(this);

  auto cache = std::make_shared<QVector<Item *>>();

  Item *loading = new Item();
  loading->state = Item::Special;
  loading->name = "... loading [-]";
  loading->parent = parent;

  QTimer *timer = new QTimer(this);

  QObject::connect(timer, &QTimer::timeout, this, [=, this]() {
    advanceSpinner(loading->name);
    auto loadingIndex = createIndex(loading->num(), 0, loading);
    emit dataChanged(loadingIndex, loadingIndex, QVector<int>{Qt::DisplayRole});
  });

  auto accounted = std::make_shared<bool>(false);
  auto release = [=, this]() {
    if (!*accounted) {
      *accounted = true;
      loadFinished();
    }
  };

  auto rcloneFinished = [=, this]() {
    release();
    ls->deleteLater();
    bool parsed = false;
    const auto entries = RcloneOutput::parseLsJson(ls->readAllStandardOutput(), &parsed);
    for (const auto &e : entries) {
      Item *child = new Item();
      child->parent = parent;
      child->isFolder = e.isDir;
      child->name = e.name;
      child->modified = e.modified;
      child->size = e.size;
      cache->append(child);
    }
    parent->state = Item::Ready;

    timer->stop();
    timer->deleteLater();

    if (parent->isDeleted) {
      qDeleteAll(*cache);
      delete parent;
      return;
    }

    QHash<QString, int> existing;
    int shown = 0;
    for (int i = 0; i < parent->childs.count(); i++) {
      if (parent->childs[i] != loading) {
        existing.insert(parent->childs[i]->name, i);
        ++shown;
      }
    }

    // Some providers (Google Drive) allow several items with the same name in
    // one folder. Matching by name cannot tell them apart, and every reload
    // used to add another copy of the extra ones. When names are not unique,
    // start from scratch for this folder instead.
    bool ambiguous = existing.size() != shown;
    QSet<QString> names;
    for (const Item *item : std::as_const(*cache)) {
      if (names.contains(item->name)) {
        ambiguous = true;
        break;
      }
      names.insert(item->name);
    }
    if (ambiguous) {
      for (int i = 0; i < parent->childs.count(); i++) {
        if (parent->childs[i] != loading) {
          emit beginRemoveRows(parentIndex, i, i);
          delete parent->childs.takeAt(i);
          emit endRemoveRows();
          i--;
        }
      }
      existing.clear();
    }

    QVector<Item *> todo;

    bool modified = false;
    for (auto &item : *cache) {
      auto it = existing.find(item->name);
      if (it == existing.end()) {
        item->path.setPath(parent->path.filePath(item->name));
        todo.append(item);
        item = nullptr;
      } else {
        Item *old = parent->childs[it.value()];
        if (old->isFolder != item->isFolder ||
            old->modified != item->modified || old->size != item->size) {
          old->state = Item::Unknown;
          old->isFolder = item->isFolder;
          old->modified = item->modified;
          old->size = item->size;
          modified = true;
          emit dataChanged(createIndex(it.value(), 0, parent),
                           createIndex(it.value(), 2, parent),
                           QVector<int>{Qt::DisplayRole});
        }
        existing.erase(it);
      }
    }

    qDeleteAll(*cache);

    for (int i = 0; i < parent->childs.count(); i++) {
      if (parent->childs[i] == loading ||
          existing.contains(parent->childs[i]->name)) {
        emit beginRemoveRows(parentIndex, i, i);
        delete parent->childs.takeAt(i);
        emit endRemoveRows();
        i--;
      }
    }

    if (!todo.isEmpty()) {
      modified = true;
      emit beginInsertRows(parentIndex, parent->childs.count(),
                           parent->childs.count() + todo.count() - 1);
      parent->childs += todo;
      emit endInsertRows();
    }

    if (modified) {
      sort(parentIndex, parent);
    }
  };

  QObject::connect(ls,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, rcloneFinished);

  parent->state = Item::Loading2;

  emit beginInsertRows(parentIndex, 0, 0);
  parent->childs.prepend(loading);
  emit endInsertRows();

  timer->start(100);
  // a process that cannot even start never reports "finished"
  QObject::connect(ls, &QProcess::errorOccurred, this,
                   [=, this](QProcess::ProcessError e) {
                     if (e == QProcess::FailedToStart) {
                       release();
                     }
                   });
  startOrQueueLoad([=, this]() {
    UseRclonePassword(ls);
    ls->setProcessChannelMode(QProcess::SeparateChannels);
    ls->start(GetRclone(),
              QStringList() << "lsjson" << "--no-mimetype"
                            << GetRcloneConf() << GetDriveSharedWithMe()
                            << GetShowHidden() << GetDefaultRcloneOptionsList()
                            << mRemote + ":" + parent->path.path(),
              QIODevice::ReadOnly);
  });
}

void ItemModel::sortRecursive(Item *item, const ItemSorter &sorter) {
  std::sort(item->childs.begin(), item->childs.end(), sorter);

  for (auto child : item->childs) {
    sortRecursive(child, sorter);
  }
}

void ItemModel::sort(const QModelIndex &parent, Item *item) {
  if (item->childs.isEmpty()) {
    return;
  }

  QList<QPersistentModelIndex> parents;
  parents << parent;
  emit layoutAboutToBeChanged(parents, QAbstractItemModel::VerticalSortHint);

  QModelIndexList oldList = persistentIndexList();
  QVector<QPair<Item *, int>> oldNodes;
  oldNodes.reserve(oldList.count());
  for (const auto &index : oldList) {
    oldNodes.append(qMakePair(get(index), index.column()));
  }

  ItemSorter sorter(mSortColumn, mSortOrder);
  sortRecursive(item, sorter);

  QModelIndexList newList;
  newList.reserve(oldNodes.size());
  for (const auto &node : oldNodes) {
    Item *child = node.first;
    int column = node.second;
    int row = child->num();
    newList.append(createIndex(row, column, child));
  }

  changePersistentIndexList(oldList, newList);

  emit layoutChanged(parents, QAbstractItemModel::VerticalSortHint);
}
