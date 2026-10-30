// Windows 11 style application shell for MainWindow: navigation pane on the
// left, content pages on the right. The original pages (remotes, jobs, tasks)
// and their logic are kept; this file only rearranges and restyles them.

#include "main_window.h"
#include "remote_widget.h"
#include "theme.h"
#include "utils.h"

namespace {

enum NavRole {
  KindRole = Qt::UserRole + 1, // "page" or "remote"
  TargetRole,                  // page index or remote name
  TypeRole,                    // remote type
  BadgeRole,                   // number shown on the right (0 = none)
  FilledIconRole               // icon used while selected (Windows 11 style)
};

// Sidebar entry: rounded highlight, gold text and icon when selected,
// optional count badge on the right.
class NavDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(200, 42);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QRectF r = QRectF(opt.rect).adjusted(6, 3, -6, -3);
    if (selected || hover) {
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("navSel") : Theme::color("subtleHover"));
      p->drawRoundedRect(r, 10, 10);
    }
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (selected && index.data(FilledIconRole).isValid()) {
      icon = index.data(FilledIconRole).value<QIcon>();
    }
    const QRect iconRect(int(r.left()) + 14, int(r.center().y()) - 11, 22, 22);
    icon.paint(p, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);

    QFont f = Theme::uiFont(1.04);
    if (selected) {
      f.setWeight(QFont::DemiBold);
    }
    p->setFont(f);
    p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));

    const int badge = index.data(BadgeRole).toInt();
    QRectF textRect(iconRect.right() + 14, r.top(), r.right() - iconRect.right() - 24, r.height());
    if (badge > 0) {
      const QString b = QString::number(badge);
      QFont bf = Theme::uiFont(0.85);
      bf.setWeight(QFont::DemiBold);
      const qreal w = qMax<qreal>(22, QFontMetricsF(bf).horizontalAdvance(b) + 12);
      const QRectF pill(r.right() - 12 - w, r.center().y() - 11, w, 22);
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("accentBtn") : Theme::color("tile"));
      p->drawRoundedRect(pill, 11, 11);
      p->setFont(bf);
      p->setPen(selected ? Theme::color("onAccent") : Theme::color("text2"));
      p->drawText(pill, Qt::AlignCenter, b);
      p->setFont(f);
      p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));
      textRect.setRight(pill.left() - 8);
    }
    const QString text = QFontMetrics(f).elidedText(
        index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(textRect.width()));
    p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    p->restore();
  }
};

enum Page { HomePage = 0, TransfersPage = 1, TasksPage = 2, BrowserPage = 3 };

QLabel *makeLabel(const QString &text, const char *objectName) {
  auto *l = new QLabel(text);
  l->setObjectName(objectName);
  return l;
}

// Title + subtitle on the left, optional widgets on the right.
QWidget *makePageHeader(const QString &title, const QString &subtitle,
                        QLabel **subtitleOut, const QList<QWidget *> &right) {
  auto *header = new QWidget;
  auto *h = new QHBoxLayout(header);
  h->setContentsMargins(0, 0, 0, 8);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(2);
  texts->addWidget(makeLabel(title, "PageTitle"));
  auto *sub = makeLabel(subtitle, "PageSubtitle");
  sub->setWordWrap(true);
  texts->addWidget(sub);
  if (subtitleOut) {
    *subtitleOut = sub;
  }
  h->addLayout(texts, 1);
  for (QWidget *w : right) {
    h->addWidget(w, 0, Qt::AlignBottom);
  }
  return header;
}

// Home page remote card: icon tile, name, remote type.
class RemoteCardDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(164, 126);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(opt.rect).adjusted(6.5, 6.5, -6.5, -6.5);
    const bool hover = opt.state & QStyle::State_MouseOver;
    const bool selected = opt.state & QStyle::State_Selected;

    p->setPen(QPen(selected ? Theme::color("accentText") : Theme::color("cardStroke"), 1));
    p->setBrush(hover || selected ? Theme::color("cardHover") : Theme::color("card"));
    p->drawRoundedRect(card, 8, 8);

    // service tile
    const QRectF tile(card.center().x() - 22, card.top() + 14, 44, 44);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(p, tile.toRect());

    // name + type
    QFont f = Theme::uiFont();
    f.setWeight(QFont::DemiBold);
    p->setFont(f);
    p->setPen(Theme::color("text"));
    const QRectF nameRect(card.left() + 8, tile.bottom() + 6, card.width() - 16, 20);
    const QString name = QFontMetrics(f).elidedText(index.data(Qt::DisplayRole).toString(),
                                                    Qt::ElideRight, int(nameRect.width()));
    p->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, name);

    QFont typeFont = Theme::uiFont(0.9);
    p->setFont(typeFont);
    p->setPen(Theme::color("text2"));
    p->drawText(QRectF(nameRect.left(), nameRect.bottom(), nameRect.width(), 18),
                Qt::AlignHCenter | Qt::AlignVCenter,
                index.data(Qt::UserRole).toString());
    p->restore();
  }
};

void setPageMargins(QLayout *l) {
  l->setContentsMargins(32, 28, 32, 16);
  l->setSpacing(10);
}

} // namespace

void MainWindow::buildShell() {
  setWindowTitle(tr("Rclone Explorer"));
  setMinimumSize(960, 600);
  if (!GetSettings()->contains("MainWindow/geometry")) {
    // roomy first-start size, but never larger than the screen
    QSize size(1360, 860);
    if (const QScreen *sc = screen()) {
      size = size.boundedTo(sc->availableGeometry().size() * 0.9);
    }
    resize(size);
  }

  // The classic menu bar is replaced by the navigation pane footer.
  ui.menuBar->hide();

  // ---------------------------------------------------- navigation pane --
  auto *nav = new QWidget;
  nav->setObjectName("NavPane");
  nav->setFixedWidth(292);
  auto *navLayout = new QVBoxLayout(nav);
  navLayout->setContentsMargins(10, 18, 10, 14);
  navLayout->setSpacing(2);

  // brand
  auto *brand = new QWidget;
  auto *bl = new QHBoxLayout(brand);
  bl->setContentsMargins(12, 0, 2, 14);
  bl->setSpacing(10);
  auto *logo = new QLabel;
  logo->setPixmap(qApp->windowIcon().pixmap(32, 32));
  bl->addWidget(logo);
  auto *appName = new QLabel(tr("Rclone Explorer"));
  appName->setObjectName("AppTitle");
  bl->addWidget(appName, 1);
  mThemeButton = new QToolButton;
  mThemeButton->setObjectName("ThemeSwitch");
  mThemeButton->setIconSize(QSize(20, 20));
  mThemeButton->setCursor(Qt::PointingHandCursor);
  QObject::connect(mThemeButton, &QToolButton::clicked, this, [this]() {
    Theme::toggle();
    updateThemeButton();
  });
  bl->addWidget(mThemeButton);
  navLayout->addWidget(brand);

  auto setupList = [&](QListWidget *list, const char *name) {
    list->setObjectName(name);
    list->setItemDelegate(new NavDelegate(list));
    list->setIconSize(QSize(22, 22));
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMouseTracking(true);
    list->setCursor(Qt::PointingHandCursor);
    list->setUniformItemSizes(true);
  };

  // main pages
  mNav = new QListWidget;
  setupList(mNav, "NavList");
  auto addPage = [&](const QString &icon, const QString &text, int page) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, mNav);
    it->setData(KindRole, "page");
    it->setData(TargetRole, page);
    it->setData(FilledIconRole, Theme::icon(icon + "_filled"));
    return it;
  };
  addPage("home", tr("Home"), HomePage);
  mNavTransfers = addPage("transfers", tr("Transfers"), TransfersPage);
  addPage("tasks", tr("Saved tasks"), TasksPage);
  mNav->setFixedHeight(3 * 42 + 4);
  mNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navLayout->addWidget(mNav);

  // remotes section header: title, count, add button
  mNavRemotesHeader = new QWidget;
  auto *rh = new QHBoxLayout(mNavRemotesHeader);
  rh->setContentsMargins(0, 18, 8, 6);
  rh->setSpacing(8);
  auto *rt = new QLabel(tr("Remotes"));
  rt->setObjectName("NavSection");
  rh->addWidget(rt);
  mNavRemoteCount = new QLabel;
  mNavRemoteCount->setObjectName("NavCount");
  rh->addWidget(mNavRemoteCount);
  rh->addStretch(1);
  auto *sortRemotes = new QToolButton;
  sortRemotes->setObjectName("NavAdd");
  sortRemotes->setIcon(Theme::icon("sort"));
  sortRemotes->setToolTip(tr("Sort remotes"));
  sortRemotes->setPopupMode(QToolButton::InstantPopup);
  {
    auto *menu = new QMenu(sortRemotes);
    auto *group = new QActionGroup(menu);
    const QList<QPair<QString, QString>> modes = {
        {"config", tr("Configuration order")}, {"az", tr("Name A–Z")},
        {"za", tr("Name Z–A")}, {"type", tr("Type")},
        {"custom", tr("Custom (drag to reorder)")}};
    const QString current = GetSettings()->value("Settings/remoteSort", "config").toString();
    for (const auto &m : modes) {
      QAction *a = menu->addAction(m.second);
      a->setCheckable(true);
      a->setChecked(m.first == current);
      group->addAction(a);
      QObject::connect(a, &QAction::triggered, this, [this, id = m.first]() {
        GetSettings()->setValue("Settings/remoteSort", id);
        rebuildNavRemotes();
      });
    }
    sortRemotes->setMenu(menu);
  }
  rh->addWidget(sortRemotes);
  auto *addRemote = new QToolButton;
  addRemote->setObjectName("NavAdd");
  addRemote->setIcon(Theme::icon("add"));
  addRemote->setToolTip(tr("New remote"));
  QObject::connect(addRemote, &QToolButton::clicked, ui.config, &QPushButton::click);
  rh->addWidget(addRemote);
  navLayout->addWidget(mNavRemotesHeader);

  mNavRemotes = new QListWidget;
  setupList(mNavRemotes, "NavRemotes");
  mNavRemotes->setContextMenuPolicy(Qt::CustomContextMenu);
  mNavRemotes->setDefaultDropAction(Qt::MoveAction);
  QObject::connect(mNavRemotes->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
    QStringList order;
    for (int i = 0; i < mNavRemotes->count(); ++i) {
      order << mNavRemotes->item(i)->data(TargetRole).toString();
    }
    GetSettings()->setValue("Settings/remoteOrder", order);
  });
  navLayout->addWidget(mNavRemotes, 1);

  // footer
  auto *divider = new QFrame;
  divider->setObjectName("NavDivider");
  navLayout->addSpacing(6);
  navLayout->addWidget(divider);
  navLayout->addSpacing(6);

  // footer: same look as the menu above, but items run commands
  auto *footer = new QListWidget;
  setupList(footer, "NavFooter");
  footer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto addAction = [&](const QString &icon, const QString &text, const char *id) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, footer);
    it->setData(KindRole, "action");
    it->setData(TargetRole, id);
    it->setData(FilledIconRole, Theme::icon(icon + "_filled"));
  };
  addAction("settings", tr("Settings"), "settings");
  addAction("github", tr("GitHub"), "github");
  addAction("help", tr("Help & Update"), "help");
  footer->setFixedHeight(3 * 42 + 4);
  navLayout->addWidget(footer);

  auto *helpMenu = new QMenu(this);
  QObject::connect(helpMenu, &QMenu::aboutToShow, this, [=, this]() {
    helpMenu->clear();
    for (QAction *a : ui.menuHelp->actions()) {
      helpMenu->addAction(a);
    }
    helpMenu->addSeparator();
    helpMenu->addAction(ui.quit);
  });
  QObject::connect(footer, &QListWidget::itemClicked, this,
                   [=, this](QListWidgetItem *item) {
                     const QString id = item->data(TargetRole).toString();
                     const QRect r = footer->visualItemRect(item);
                     footer->clearSelection();
                     footer->setCurrentItem(nullptr);
                     if (id == "settings") {
                       ui.preferences->trigger();
                     } else if (id == "github") {
                       QDesktopServices::openUrl(
                           QUrl("https://github.com/Jackpison/RcloneExplorer"));
                     } else {
                       helpMenu->exec(footer->viewport()->mapToGlobal(r.topRight()));
                     }
                   });
  updateThemeButton();

  ui.about->setIcon(Theme::icon("info"));
  ui.quit->setIcon(Theme::icon("quit"));
  ui.preferences->setIcon(Theme::icon("settings"));

  // ------------------------------------------------------ content area --
  auto *content = new QWidget;
  content->setObjectName("ContentArea");
  content->setAttribute(Qt::WA_StyledBackground);
  auto *cl = new QVBoxLayout(content);
  cl->setContentsMargins(1, 1, 0, 0);
  cl->setSpacing(0);

  // Home, Transfers and Tasks are pages of their own; the tab widget holds
  // only the open remotes (Explorer-style tabs).
  mPages = new QStackedWidget;
  mPages->setObjectName("Pages");
  QWidget *pageWidgets[3] = {ui.tabs->widget(0), ui.tabs->widget(1), ui.tabs->widget(2)};
  for (int i = 2; i >= 0; --i) {
    ui.tabs->removeTab(i);
  }
  for (QWidget *w : pageWidgets) {
    mPages->addWidget(w);
  }
  mPages->addWidget(ui.tabs);
  cl->addWidget(mPages);

  ui.tabs->setAttribute(Qt::WA_StyledBackground);
  ui.tabs->setDocumentMode(true);
  ui.tabs->setTabsClosable(false); // own close buttons, see openRemote()
  ui.tabs->setMovable(true);
  ui.tabs->setElideMode(Qt::ElideRight);
  ui.tabs->setUsesScrollButtons(true);
  ui.tabs->tabBar()->setExpanding(false);
  ui.tabs->tabBar()->setIconSize(QSize(18, 18));

  // "+" right after the last tab, as in File Explorer (Qt only offers a
  // corner position, so it is placed manually; see updateNewTabButton()).
  mNewTab = new QToolButton(ui.tabs);
  mNewTab->setObjectName("NewTab");
  mNewTab->setIcon(Theme::icon("add"));
  mNewTab->setIconSize(QSize(16, 16));
  mNewTab->setFixedSize(32, 32);
  mNewTab->setCursor(Qt::PointingHandCursor);
  mNewTab->setToolTip(tr("New tab (Ctrl+T)"));
  QObject::connect(mNewTab, &QToolButton::clicked, this, [this]() {
    QWidget *w = ui.tabs->currentWidget();
    if (w && w->property("remoteName").isValid()) {
      openRemote(w->property("remoteName").toString(),
                 w->property("remoteType").toString(), true);
    }
  });
  QObject::connect(ui.tabs->tabBar(), &QTabBar::tabMoved, this,
                   [this]() { updateNewTabButton(); });

  auto *central = new QWidget;
  auto *hl = new QHBoxLayout(central);
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(0);
  hl->addWidget(nav);
  hl->addWidget(content, 1);
  QWidget *old = takeCentralWidget();
  setCentralWidget(central);
  if (old) {
    old->deleteLater();
  }

  // ---------------------------------------------------------- Home page --
  {
    QWidget *page = mPages->widget(HomePage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);

    ui.config->setText(tr("New remote"));
    ui.config->setProperty("accent", true);
    ui.config->setToolTip(tr("Add or edit remotes with rclone config"));
    ui.refresh->setText(QString());
    ui.refresh->setIcon(Theme::icon("refresh"));
    ui.refresh->setToolTip(tr("Reload remotes"));
    ui.open->hide();

    QLabel *sub = nullptr;
    v->insertWidget(0, makePageHeader(tr("Remotes"),
                                      tr("Your cloud storage, as configured in rclone."),
                                      &sub, {ui.refresh, ui.config}));
    mHomeSubtitle = sub;

    ui.remotes->setViewMode(QListView::IconMode);
    ui.remotes->setMovement(QListView::Static);
    ui.remotes->setResizeMode(QListView::Adjust);
    ui.remotes->setWrapping(true);
    ui.remotes->setUniformItemSizes(true);
    ui.remotes->setGridSize(QSize(170, 132));
    ui.remotes->setIconSize(QSize(40, 40));
    ui.remotes->setWordWrap(true);
    ui.remotes->setSpacing(0);
    ui.remotes->setFrameShape(QFrame::NoFrame);
    ui.remotes->setSelectionRectVisible(false);
    ui.remotes->setItemDelegate(new RemoteCardDelegate(ui.remotes));
    ui.remotes->setMouseTracking(true);
    ui.remotes->setCursor(Qt::PointingHandCursor);

    mHomeEmpty = new QLabel(
        tr("<b>No remotes yet</b><br><br>Click <b>New remote</b> to connect a "
           "cloud drive, server or folder with rclone's setup assistant."));
    mHomeEmpty->setAlignment(Qt::AlignCenter);
    mHomeEmpty->setProperty("secondary", true);
    mHomeEmpty->hide();
    v->insertWidget(2, mHomeEmpty, 1);
  }

  // ----------------------------------------------------- Transfers page --
  {
    QWidget *page = mPages->widget(TransfersPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Transfers"),
                                      tr("Running and finished copies, syncs, "
                                         "mounts and streams."),
                                      nullptr, {}));
    ui.jobsArea->setFrameShape(QFrame::NoFrame);
    ui.jobsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.jobs->setSpacing(8);
    ui.noJobsAvailable->setText(tr("No transfers yet.\n\nUploads, downloads, "
                                   "mounts and tasks you run will appear here."));
    ui.noJobsAvailable->setAlignment(Qt::AlignCenter);
    ui.noJobsAvailable->setMinimumHeight(160);
  }

  // --------------------------------------------------------- Tasks page --
  {
    QWidget *page = mPages->widget(TasksPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    // Sort control (view only: tasks.bin keeps the order tasks were saved in)
    auto *sortBox = new QWidget;
    auto *sortLayout = new QHBoxLayout(sortBox);
    sortLayout->setContentsMargins(0, 0, 0, 0);
    sortLayout->setSpacing(8);
    auto *sortLabel = new QLabel(tr("Sort by"));
    sortLabel->setProperty("secondary", true);
    sortLayout->addWidget(sortLabel);
    mTasksSort = new QComboBox;
    mTasksSort->addItem(tr("Order saved"), "saved");
    mTasksSort->addItem(tr("Name (A to Z)"), "name_asc");
    mTasksSort->addItem(tr("Name (Z to A)"), "name_desc");
    mTasksSort->addItem(tr("Type (downloads first)"), "type");
    {
      const int ix = mTasksSort->findData(
          GetSettings()->value("Settings/tasksSort", "saved").toString());
      mTasksSort->setCurrentIndex(ix >= 0 ? ix : 0);
    }
    sortLayout->addWidget(mTasksSort);
    QObject::connect(mTasksSort, &QComboBox::currentIndexChanged, this,
                     [this](int) {
                       GetSettings()->setValue("Settings/tasksSort",
                                               mTasksSort->currentData());
                       listTasks();
                     });
    v->insertWidget(0, makePageHeader(tr("Saved tasks"),
                                      tr("Saved transfers you can run again "
                                         "with one click."),
                                      nullptr, {sortBox}));
    ui.tasksArea->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setSpacing(0);
    ui.tasksListWidget->setIconSize(QSize(20, 20));

    // Action bar sits right under the title, above the list. It used to be
    // inside the scroll area with a 32 px height cap from the .ui file,
    // which cut the buttons off at the bottom of the page.
    v->insertWidget(1, ui.tasksActionBar);
    ui.tasksActionBar->setMinimumSize(0, 0);
    ui.tasksActionBar->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    ui.tasksActionBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    if (auto *bar = qobject_cast<QHBoxLayout *>(ui.tasksActionBar->layout())) {
      bar->setContentsMargins(0, 0, 0, 8);
      bar->setSpacing(8);
      // Run is the main action, so it goes first.
      bar->removeWidget(ui.buttonRunTask);
      bar->insertWidget(0, ui.buttonRunTask);
      bar->addStretch(1);
    }
    ui.buttonDryrunTask->setText(tr("Dry run"));
    mButtonSchedule = new QPushButton(tr("Schedule…"));
    mButtonSchedule->setIcon(Theme::icon("tasks"));
    mButtonSchedule->setEnabled(false);
    mButtonSchedule->setToolTip(
        tr("Run the selected task by itself on a schedule"));
    if (auto *bar = qobject_cast<QHBoxLayout *>(ui.tasksActionBar->layout())) {
      bar->insertWidget(bar->indexOf(ui.buttonEditTask) + 1, mButtonSchedule);
    }
    QObject::connect(mButtonSchedule, &QPushButton::clicked, this,
                     [this]() { editSelectedSchedule(); });

    ui.buttonRunTask->setProperty("accent", true);
    // already polished by the time we get here, so restyle it
    ui.buttonRunTask->style()->unpolish(ui.buttonRunTask);
    ui.buttonRunTask->style()->polish(ui.buttonRunTask);
    ui.buttonDryrunTask->setIcon(Theme::icon("dryrun"));
    ui.buttonEditTask->setIcon(Theme::icon("edit"));
    ui.buttonDeleteTask->setIcon(Theme::icon("delete"));
  }

  mUploadIcon = Theme::icon("upload");
  mDownloadIcon = Theme::icon("download");

  // ------------------------------------------------------- navigation ----
  auto onNav = [this](QListWidgetItem *item) {
    if (!item || mSyncingNav) {
      return;
    }
    if (item->data(KindRole).toString() == "page") {
      showPage(item->data(TargetRole).toInt());
    } else {
      openRemote(item->data(TargetRole).toString(),
                 item->data(TypeRole).toString());
    }
  };
  QObject::connect(mNav, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNav, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(ui.tabs, &QTabWidget::currentChanged, this, [this](int) {
    syncNavToCurrentPage();
    updateNewTabButton();
  });
  QObject::connect(mPages, &QStackedWidget::currentChanged, this,
                   [this](int) { syncNavToCurrentPage(); });
  QObject::connect(ui.tabs, &QTabWidget::tabCloseRequested, this,
                   [this](int i) { closeRemoteWidget(ui.tabs->widget(i)); });
  // middle-click closes a tab, as in File Explorer and browsers
  ui.tabs->tabBar()->installEventFilter(this);

  QObject::connect(mNavRemotes, &QWidget::customContextMenuRequested, this,
                   [this](const QPoint &pos) {
                     QListWidgetItem *item = mNavRemotes->itemAt(pos);
                     if (!item || item->data(KindRole).toString() != "remote") {
                       return;
                     }
                     const QString name = item->data(TargetRole).toString();
                     QMenu menu;
                     menu.addAction(Theme::icon("open"), tr("Open"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString());
                     });
                     menu.addAction(Theme::icon("new_tab"), tr("Open in new tab"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString(), true);
                     });
                     QAction *close = menu.addAction(Theme::icon("close"), tr("Close all tabs"), this,
                                                     [=, this]() { closeRemote(name); });
                     close->setEnabled(remoteTab(name) >= 0);
                     menu.addSeparator();
                     menu.addAction(Theme::icon("refresh"), tr("Reload remotes"), this,
                                    &MainWindow::rcloneListRemotes);
                     menu.exec(mNavRemotes->viewport()->mapToGlobal(pos));
                   });

  mNav->setCurrentRow(0);
}

int MainWindow::remoteTab(const QString &name) const {
  for (int i = 0; i < ui.tabs->count(); ++i) {
    if (ui.tabs->widget(i)->property("remoteName").toString() == name) {
      return i;
    }
  }
  return -1;
}

void MainWindow::openRemote(const QString &name, const QString &type,
                            bool newTab) {
  int index = newTab ? -1 : remoteTab(name);
  if (index < 0) {
    const bool isLocal = type == "local";
    const bool isGoogle = type == "drive";
    auto *remote = new RemoteWidget(name, isLocal, isGoogle, ui.tabs);
    remote->setProperty("remoteName", name);
    remote->setProperty("remoteType", type);
    remote->setRemoteType(type);
    QObject::connect(remote, &RemoteWidget::addMount, this, &MainWindow::addMount);
    QObject::connect(remote, &RemoteWidget::addStream, this, &MainWindow::addStream);
    QObject::connect(remote, &RemoteWidget::addTransfer, this, &MainWindow::addTransfer);
    QObject::connect(remote, &RemoteWidget::closeRequested, this,
                     [this, remote]() { closeRemoteWidget(remote); });
    QObject::connect(remote, &RemoteWidget::newTabRequested, this,
                     [this, name, type]() { openRemote(name, type, true); });
    index = ui.tabs->addTab(remote, Theme::remoteIcon(type), name);
    ui.tabs->setTabToolTip(index, QString("%1 · %2").arg(name, type));

    // Own close button: Qt's built-in one is positioned against the tab's
    // outer margin box and ends up above the text line.
    auto *close = new QToolButton;
    close->setObjectName("TabClose");
    close->setIcon(Theme::hoverIcon("close", Qt::white)); // white on red hover
    close->setIconSize(QSize(14, 14));
    close->setFixedSize(24, 24);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Close tab (Ctrl+W)"));
    close->setFocusPolicy(Qt::NoFocus);
    QObject::connect(close, &QToolButton::clicked, this,
                     [this, remote]() { closeRemoteWidget(remote); });
    ui.tabs->tabBar()->setTabButton(index, QTabBar::RightSide, close);
  }
  ui.tabs->setCurrentIndex(index);
  showPage(BrowserPage);
  QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
}

void MainWindow::updateNewTabButton() {
  if (!mNewTab) {
    return;
  }
  QTabBar *bar = ui.tabs->tabBar();
  if (bar->count() == 0) {
    mNewTab->hide();
    return;
  }
  const QRect last = bar->tabRect(bar->count() - 1);
  QPoint pos = bar->mapTo(ui.tabs, QPoint(last.right() + 6,
                                          last.center().y() - mNewTab->height() / 2));
  // with many tabs (scroll buttons visible) keep it inside the widget
  pos.setX(qMin(pos.x(), ui.tabs->width() - mNewTab->width() - 4));
  mNewTab->move(pos);
  mNewTab->show();
  mNewTab->raise();
}

void MainWindow::showPage(int page) {
  if (!mPages) {
    return;
  }
  if (page == BrowserPage && ui.tabs->count() == 0) {
    page = HomePage;
  }
  mPages->setCurrentIndex(page);
  syncNavToCurrentPage();
}

void MainWindow::closeRemoteWidget(QWidget *w) {
  const int index = ui.tabs->indexOf(w);
  if (index < 0) {
    return;
  }
  ui.tabs->removeTab(index); // Qt selects the neighbouring tab
  w->deleteLater();
  QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
  if (ui.tabs->count() == 0 && mPages->currentIndex() == BrowserPage) {
    showPage(HomePage);
  }
  syncNavToCurrentPage();
}

void MainWindow::closeRemote(const QString &name) {
  int index;
  while ((index = remoteTab(name)) >= 0) {
    closeRemoteWidget(ui.tabs->widget(index));
  }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e) {
  if (o == ui.tabs->tabBar() &&
      (e->type() == QEvent::Resize || e->type() == QEvent::LayoutRequest ||
       e->type() == QEvent::Move || e->type() == QEvent::Show)) {
    QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
  }
  if (o == ui.tabs->tabBar() && e->type() == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(e);
    if (me->button() == Qt::MiddleButton) {
      const int i = ui.tabs->tabBar()->tabAt(me->position().toPoint());
      if (i >= 0) {
        closeRemoteWidget(ui.tabs->widget(i));
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(o, e);
}

void MainWindow::syncNavToCurrentPage() {
  if (!mNav) {
    return;
  }
  const int index = mPages->currentIndex();
  QWidget *tab = ui.tabs->currentWidget();
  const QString remote = (index == BrowserPage && tab)
                             ? tab->property("remoteName").toString()
                             : QString();
  mSyncingNav = true;
  QListWidget *active = remote.isEmpty() ? mNav : mNavRemotes;
  QListWidget *other = remote.isEmpty() ? mNavRemotes : mNav;
  other->clearSelection();
  other->setCurrentItem(nullptr);
  for (int i = 0; i < active->count(); ++i) {
    QListWidgetItem *it = active->item(i);
    const bool match = remote.isEmpty() ? it->data(TargetRole).toInt() == index
                                        : it->data(TargetRole).toString() == remote;
    if (match) {
      active->setCurrentItem(it);
      break;
    }
  }
  mSyncingNav = false;
}

void MainWindow::rebuildNavRemotes() {
  if (!mNavRemotes) {
    return;
  }
  mSyncingNav = true;
  mNavRemotes->clear();
  mSyncingNav = false;
  const int n = ui.remotes->count();
  QList<QListWidgetItem *> sources;
  for (int i = 0; i < n; ++i) {
    sources << ui.remotes->item(i);
  }
  const QString sortMode = GetSettings()->value("Settings/remoteSort", "config").toString();
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  if (sortMode == "az" || sortMode == "za") {
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      const int c = collator.compare(a->text(), b->text());
      return sortMode == "az" ? c < 0 : c > 0;
    });
  } else if (sortMode == "type") {
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      const int c = collator.compare(a->data(Qt::UserRole).toString(),
                                     b->data(Qt::UserRole).toString());
      return c != 0 ? c < 0 : collator.compare(a->text(), b->text()) < 0;
    });
  } else if (sortMode == "custom") {
    const QStringList order = GetSettings()->value("Settings/remoteOrder").toStringList();
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      auto rank = [&](QListWidgetItem *x) {
        const int i = order.indexOf(x->text());
        return i < 0 ? int(order.size()) : i; // new remotes go last
      };
      return rank(a) < rank(b);
    });
  }
  mNavRemotes->setDragDropMode(sortMode == "custom" ? QAbstractItemView::InternalMove
                                                    : QAbstractItemView::NoDragDrop);
  for (QListWidgetItem *src : sources) {
    auto *it = new QListWidgetItem(src->icon(), src->text(), mNavRemotes);
    it->setData(KindRole, "remote");
    it->setData(TargetRole, src->text());
    it->setData(TypeRole, src->data(Qt::UserRole));
    it->setToolTip(src->text() + " · " + src->data(Qt::UserRole).toString());
  }
  mNavRemoteCount->setText(QString::number(n));
  mNavRemoteCount->setVisible(n > 0);
  ui.remotes->setVisible(n > 0);
  mHomeEmpty->setVisible(n == 0);
  mHomeSubtitle->setText(
      n == 0 ? tr("Your cloud storage, as configured in rclone.")
             : (n == 1 ? tr("1 remote configured in rclone. Double-click to browse.")
                       : tr("%1 remotes configured in rclone. Double-click to "
                            "browse.").arg(n)));
  syncNavToCurrentPage();
}

void MainWindow::updateThemeButton() {
  if (mThemeButton) {
    mThemeButton->setIcon(Theme::icon(Theme::isDark() ? "sun" : "moon"));
    mThemeButton->setToolTip(Theme::isDark() ? tr("Switch to light mode")
                                             : tr("Switch to dark mode"));
  }
  if (mNav) {
    mNav->viewport()->update();
    mNavRemotes->viewport()->update();
  }
}

void MainWindow::setJobsTabText(const QString &text) {
  if (mNavTransfers) {
    static const QRegularExpression rx(R"(\((\d+)\))");
    const auto m = rx.match(text);
    mNavTransfers->setData(BadgeRole, m.hasMatch() ? m.captured(1).toInt() : 0);
  }
}
