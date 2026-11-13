#include "theme.h"
#include "utils.h"
#include "file_names.h"

#include <QSvgRenderer>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Theme {

namespace {

bool gDark = false;
bool gWatching = false;
QColor gAccentBtn;  // filled accent controls
QColor gAccentText; // accent used for text, indicators, focus

// One definition of the app's text, used for the application font, the
// style sheet, and anything painted by hand: Segoe UI Variable / Segoe UI on
// Windows 10 and 11, with similar sans fonts as fallback elsewhere.
const QStringList kFamilies = {"Segoe UI Variable Text", "Segoe UI", "Selawik", "Noto Sans",
                               "Ubuntu", "DejaVu Sans", "Sans Serif"};
const QStringList kMonoFamilies = {"Cascadia Mono", "Cascadia Code", "Consolas",
                                   "DejaVu Sans Mono", "Liberation Mono", "Monospace"};
constexpr qreal kBasePt = 12.0;
constexpr qreal kListPt = 12.5;  // file names, sizes and dates
constexpr qreal kMonoPt = 10.5;  // log output
constexpr qreal kAboutTitlePt = 18.0; // About dialog heading

QString quoted(const QStringList &families) {
  QStringList q;
  for (const QString &f : families) {
    q << '"' + f + '"';
  }
  return q.join(", ");
}


// ------------------------------------------------------------ accent ----

void computeAccent() {
  // Brand accent: bright gold. Text/indicators use a darker amber on light
  // backgrounds so they stay readable (WCAG AA contrast).
  gAccentBtn = QColor("#F5B70A");
  gAccentText = gDark ? QColor("#FBBF24") : QColor("#A15C07");
}

// ------------------------------------------------------------- icons ----

QByteArray svgSource(const QString &name) {
  static QHash<QString, QByteArray> cache;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it.value();
  }
  QFile f(":/fluent/" + name + ".svg");
  QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  cache.insert(name, data);
  return data;
}

QColor iconColor(const QColor &forced, QIcon::Mode mode) {
  QColor c = forced.isValid() ? forced
             : gDark          ? QColor(255, 255, 255)
                              : QColor(0, 0, 0, 228);
  if (mode == QIcon::Disabled) {
    c.setAlphaF(c.alphaF() * 0.36);
  } else if (mode == QIcon::Selected && !forced.isValid()) {
    c = gAccentText;
  }
  return c;
}

void renderSvg(QPainter *p, const QRectF &rect, const QString &name,
               const QColor &color) {
  QByteArray svg = svgSource(name);
  svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
  QSvgRenderer r(svg);
  p->save();
  p->setOpacity(p->opacity() * color.alphaF());
  r.render(p, rect);
  p->restore();
}

// A glyph is rendered once per (name, colour, pixel size) and then served from
// Qt's pixmap cache. Before, every paint of every icon created a new SVG
// renderer, so large file lists were slow to scroll.
QPixmap glyphPixmap(const QString &name, const QColor &color, const QSize &px) {
  const QString key = QStringLiteral("rbglyph|") + name + QLatin1Char('|') +
                      color.name(QColor::HexArgb) + QLatin1Char('|') +
                      QString::number(px.width()) + QLatin1Char('x') + QString::number(px.height());
  QPixmap pm;
  if (!QPixmapCache::find(key, &pm)) {
    pm = QPixmap(px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(color.alphaF());
    QByteArray svg = svgSource(name);
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
    QSvgRenderer r(svg);
    r.render(&p, QRectF(QPointF(0, 0), QSizeF(px)));
    p.end();
    QPixmapCache::insert(key, pm);
  }
  return pm;
}

void drawGlyph(QPainter *p, const QRectF &rect, const QString &name, const QColor &color) {
  const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
  const QRect target = rect.toRect();
  QPixmap pm = glyphPixmap(name, color,
                           QSize(qMax(1, qRound(target.width() * dpr)),
                                 qMax(1, qRound(target.height() * dpr))));
  pm.setDevicePixelRatio(dpr);
  p->drawPixmap(target.topLeft(), pm);
}

class FluentIconEngine : public QIconEngine {
public:
  FluentIconEngine(const QString &name, const QColor &color,
                   const QColor &hover = QColor())
      : mName(name), mColor(color), mHover(hover) {}

  QColor colorFor(QIcon::Mode mode) const {
    return (mode == QIcon::Active && mHover.isValid()) ? mHover
                                                       : iconColor(mColor, mode);
  }

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
             QIcon::State) override {
    drawGlyph(painter, rect, mName, colorFor(mode));
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                 QIcon::State state) override {
    return scaledPixmap(size, mode, state, 1.0);
  }

  QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State,
                       qreal scale) override {
    QPixmap pm = glyphPixmap(mName, colorFor(mode), size * scale);
    pm.setDevicePixelRatio(scale);
    return pm;
  }

  QIconEngine *clone() const override {
    return new FluentIconEngine(mName, mColor, mHover);
  }
  QString key() const override { return "fluent"; }

private:
  QString mName;
  QColor mColor;
  QColor mHover;
};

// Renders an icon into a PNG for use in the style sheet (check marks,
// chevrons). Files are regenerated whenever the theme changes.
QString iconFile(const QString &name, const QColor &color, int px = 16) {
  static QTemporaryDir dir;
  const QString file =
      dir.filePath(QString("%1-%2-%3.png").arg(name).arg(color.name(QColor::HexArgb).mid(1)).arg(px));
  if (!QFileInfo::exists(file)) {
    const qreal scale = 2.0; // crisp on high-DPI screens
    QImage img(QSize(px, px) * scale, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    img.setDevicePixelRatio(scale);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    renderSvg(&p, QRectF(0, 0, px, px), name, color);
    p.end();
    img.save(file);
  }
  return QDir::fromNativeSeparators(file);
}

// ------------------------------------------------------------ tokens ----


QMap<QString, QString> buildTokens();
QMap<QString, QString> tokens() {
  static QMap<QString, QString> cache;
  static QString cacheKey;
  const QString key = QString("%1%2").arg(gDark).arg(gAccentBtn.name());
  if (key != cacheKey) {
    cache = buildTokens();
    cacheKey = key;
  }
  return cache;
}

QMap<QString, QString> buildTokens() {
  QMap<QString, QString> t;
  auto set = [&](const char *k, const QColor &c) {
    t[k] = c.alpha() == 255 ? c.name()
                            : QString("rgba(%1,%2,%3,%4)")
                                  .arg(c.red()).arg(c.green()).arg(c.blue())
                                  .arg(c.alpha());
  };
  const bool d = gDark;
  const QColor card = d ? QColor("#151515") : QColor("#FFFFFF");
  set("bg", d ? QColor("#0E0E0E") : QColor("#F3F1EC"));        // sidebar, window
  set("layer", d ? QColor("#0A0A0A") : QColor("#FAF9F6"));     // content area
  set("card", card);
  set("cardHover", d ? QColor("#1C1C1C") : QColor("#FBF8F1"));
  set("stroke", d ? QColor("#1F1F1F") : QColor("#E4DFD5"));
  set("cardStroke", d ? QColor("#262626") : QColor("#E4DFD5"));
  set("tile", d ? QColor("#1D1D1D") : QColor("#F5F2EB"));
  set("headerBg", d ? QColor("#191919") : QColor("#F7F4EE"));
  set("divider", d ? QColor("#232323") : QColor("#E7E2D8"));
  set("text", d ? QColor("#FFFFFF") : QColor("#141210"));
  set("text2", d ? QColor("#B5AFA8") : QColor("#57534E"));
  set("text3", d ? QColor("#78716C") : QColor("#A8A29E"));
  set("ctrl", d ? QColor("#171717") : QColor("#FFFFFF"));
  set("ctrlHover", d ? QColor("#1E1E1E") : QColor("#FBF9F5"));
  set("ctrlPressed", d ? QColor("#141414") : QColor("#F4F1EA"));
  set("ctrlDisabled", d ? QColor("#131313") : QColor("#F4F1EA"));
  set("ctrlStroke", d ? QColor("#2A2A2A") : QColor("#DDD7CB"));
  set("ctrlStrokeBottom", d ? QColor("#2A2A2A") : QColor("#C9C2B4"));
  set("inputBottom", d ? QColor("#3A3A3A") : QColor("#B9B1A2"));
  set("inputFocus", d ? QColor("#121212") : QColor("#FFFFFF"));
  set("subtleHover", d ? QColor(255, 255, 255, 14) : QColor(28, 25, 23, 12));
  set("subtlePressed", d ? QColor(255, 255, 255, 9) : QColor(28, 25, 23, 7));
  set("flyout", d ? QColor("#161616") : QColor("#FFFFFF"));
  set("flyoutStroke", d ? QColor("#2A2A2A") : QColor("#DDD7CB"));
  set("scroll", d ? QColor(255, 255, 255, 90) : QColor(28, 25, 23, 100));
  set("track", d ? QColor("#2A2A2A") : QColor("#E4DFD5"));
  set("accentBtn", gAccentBtn);
  set("accentBtnHover", QColor("#FFC93C"));
  set("accentBtnPressed", QColor("#E0A400"));
  set("accentText", gAccentText);
  set("onAccent", QColor("#1A1405"));
  set("sel", d ? QColor(245, 183, 10, 34) : QColor(245, 183, 10, 46));
  set("selHover", d ? QColor(245, 183, 10, 46) : QColor(245, 183, 10, 60));
  set("tabStrip", d ? QColor("#171717") : QColor("#E9E5DC"));
  set("tabHover", d ? QColor("#1F1F1F") : QColor("#F1EEE7"));
  set("navSel", d ? QColor("#2A2312") : QColor("#FBEFC9"));
  set("success", d ? QColor("#4ADE80") : QColor("#15803D"));
  set("critical", d ? QColor("#F87171") : QColor("#B91C1C"));
  set("accentTint", d ? QColor(245, 183, 10, 38) : QColor(245, 183, 10, 40));
  set("successTint", d ? QColor(74, 222, 128, 30) : QColor(21, 128, 61, 22));
  set("criticalTint", d ? QColor(248, 113, 113, 32) : QColor(185, 28, 28, 20));
  set("checkStroke", d ? QColor("#57534E") : QColor("#A8A29E"));

  t["fontFamilies"] = quoted(kFamilies);
  t["monoFamilies"] = quoted(kMonoFamilies);

  const QColor iconC = d ? QColor(255, 255, 255, 200) : QColor(0, 0, 0, 170);
  t["imgCheck"] = iconFile("checkmark", QColor("#1A1405"), 14);
  t["imgRight"] = iconFile("chevron", iconC, 12);
  t["imgDown"] = iconFile("expand", iconC, 12);
  t["imgUp"] = iconFile("collapse", iconC, 12);
  t["imgClose"] = iconFile("close", iconC, 12);
  t["imgDownDisabled"] = iconFile("expand", d ? QColor(255, 255, 255, 80) : QColor(0, 0, 0, 70), 12);
  return t;
}

const char *kStyleSheet = R"QSS(
* { outline: 0; font-family: @fontFamilies; font-size: 12pt; }
/* Sizes that differ from the base: list text, log output. Everything else,
   including every dialog and popup, takes the base size above, so no widget
   depends on what the platform theme hands out. */
QTreeView, QListView#IconGrid { font-size: 12.5pt; }
QPlainTextEdit, QTextEdit { font-family: @monoFamilies; font-size: 10.5pt; }
QWidget { color: @text; }
QMainWindow, QDialog, QMessageBox { background: @bg; }
QWidget#NavPane { background: @bg; }
QWidget#ContentArea {
  background: @layer;
  border-left: 1px solid @stroke;
  border-top: 1px solid @stroke;
  border-top-left-radius: 8px;
}
QTabWidget#tabs::pane { border: none; background: transparent; }
/* pages are painted opaque: transparent pages could show stale content of
   the previous page when the tab strip appears or disappears */
QTabWidget#tabs > QStackedWidget,
QTabWidget#tabs > QStackedWidget > QWidget,
QStackedWidget#Pages, QStackedWidget#Pages > QWidget { background: @layer; border: none; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: @layer; border: none; }

QLabel#PageTitle { font-size: 28pt; font-weight: 700; padding: 0; }
QLabel#AppTitle { color: @accentText; font-size: 14.5pt; font-weight: 700; }
QLabel#NavSection { color: @text2; font-size: 10.5pt; font-weight: 700; padding: 0 0 0 14px; }
QLabel#NavCount { color: @text2; background: @tile; border-radius: 10px; padding: 1px 8px; font-size: 10pt; font-weight: 600; }
QFrame#NavDivider { background: @divider; max-height: 1px; min-height: 1px; border: none; }
QLabel#PageSubtitle, QLabel[secondary="true"] { color: @text2; }
QDialogButtonBox { dialogbuttonbox-buttons-have-icons: 0; }
QMessageBox QLabel#qt_msgbox_label, QMessageBox QLabel#qt_msgbox_informativelabel { min-width: 380px; }
QMessageBox#ConfirmBox QLabel#qt_msgboxex_icon_label { padding: 2px 14px 0 4px; }
QMessageBox#ConfirmBox QLabel#qt_msgbox_label { padding-top: 2px; }
QMessageBox#ConfirmBox QLabel#qt_msgbox_informativelabel { color: @text2; padding: 2px 0 10px 0; }
QLabel#AboutTitle { font-size: 18pt; font-weight: 700; }
QPushButton[destructive="true"] { color: @critical; border-color: @critical; }
QPushButton[destructive="true"]:hover { background: @criticalTint; }
QLabel#noJobsAvailable { color: @text2; }
QLabel#SectionHeader { font-weight: 600; padding-top: 4px; }

QWidget[card="true"], JobWidget, MountWidget, StreamWidget {
  background: @card;
  border: 1px solid @cardStroke;
  border-radius: 12px;
}
JobWidget QLineEdit, MountWidget QLineEdit, StreamWidget QLineEdit { background: transparent; border: none; padding: 1px 0; }
JobWidget QLabel, MountWidget QLabel, StreamWidget QLabel { background: transparent; border: none; }
QToolButton[status] { border: none; font-weight: 600; padding: 4px 12px; border-radius: 6px; }
QToolButton[status="running"] { color: @accentText; background: @accentTint; }
QToolButton[status="paused"] { color: @text2; background: @tile; }
QToolButton[status="done"] { color: @success; background: @successTint; }
QToolButton[status="error"] { color: @critical; background: @criticalTint; }

/* ---------- Mirror page ---------- */
QWidget#MirrorDests { background: @headerBg; border: none; border-top: 1px solid @divider;
  border-bottom-left-radius: 11px; border-bottom-right-radius: 11px; }
QLabel#MirrorTitle { font-size: 13pt; }
QLabel[badge] { border-radius: 9px; padding: 2px 10px; font-weight: 600; font-size: 10.5pt; }
QLabel[badge="ok"] { color: @success; background: @successTint; }
QLabel[badge="warn"] { color: @accentText; background: @accentTint; }
QLabel[badge="new"] { color: @text2; background: @tile; }
QLabel[badge="bad"] { color: @critical; background: @criticalTint; }
QWidget#MirrorBanner { background: @accentTint; border: 1px solid @accentBtn; border-radius: 12px; }
QLabel#MirrorBannerTitle { font-size: 13.5pt; font-weight: 700; background: transparent; }
QWidget#MirrorBanner QLabel { background: transparent; }
QPlainTextEdit#MirrorFiles { background: @layer; border: 1px solid @divider; border-radius: 6px; }
QPushButton#ScheduleChip { border-radius: 13px; padding: 3px 14px; font-size: 10.5pt; }
QPushButton#ScheduleChip[scheduled="true"] { color: @accentText; border-color: @accentText; }

/* ---------- boxed sections ---------- */
QWidget#StatTile { background: @tile; border: none; border-radius: 10px; }
QLabel#JobSummary { color: @text2; background: transparent; padding: 0 8px; }
QLabel#StatLabel { color: @text2; font-size: 10.5pt; background: transparent; border: none; }
QLineEdit[statValue="true"], QLineEdit[statValue="true"]:read-only { background: transparent; border: none; padding: 0; font-weight: 600; font-size: 13.5pt; }
QLineEdit[pathValue="true"], QLineEdit[pathValue="true"]:read-only { background: @tile; border: none; border-radius: 5px; padding: 5px 9px; }
QWidget[card="true"] QHeaderView::section { background: @headerBg; }
QWidget[card="true"] QHeaderView::section:first { border-top-left-radius: 11px; }
QWidget[card="true"] QHeaderView::section:last { border-top-right-radius: 11px; }

/* ---------- remote tabs: Windows 11 File Explorer style ----------
   Tabs sit in a strip in the window colour; the selected tab has the
   content colour and merges into the page below. */
QTabWidget#tabs { background: @tabStrip; }
QTabWidget#tabs > QTabBar { background: @tabStrip; }
QTabWidget#tabs > QTabBar::tab {
  background: transparent; color: @text2; border: none;
  border-right: 1px solid @divider;
  border-top-left-radius: 8px; border-top-right-radius: 8px;
  padding: 9px 10px 9px 12px; margin: 8px 0 0 0; max-width: 260px;
  font-weight: 600;
}
QTabWidget#tabs > QTabBar::tab:first { margin-left: 8px; }
QTabWidget#tabs > QTabBar::tab:next-selected { border-right-color: transparent; }
QTabWidget#tabs > QTabBar::tab:hover:!selected { background: @tabHover; color: @text; }
QTabWidget#tabs > QTabBar::tab:selected { background: @layer; color: @accentText; border-right-color: transparent; }
QToolButton#NewTab { border: none; border-radius: 8px; padding: 0; background: transparent; }
QToolButton#NewTab:hover { background: @tabHover; }
QToolButton#TabClose { border: none; border-radius: 12px; padding: 4px 0 0 0; margin-left: 4px; background: transparent; }
QToolButton#TabClose:hover { background: #C42B1C; }
QToolButton#TabClose:pressed { background: #A1261A; }
QWidget#jobsArea QFrame[frameShape="4"] { border: none; background: transparent; max-height: 2px; }

/* ---------- navigation pane ---------- */
QListWidget#NavList, QListWidget#NavRemotes, QListWidget#NavFooter { background: transparent; border: none; }
QToolButton#ThemeSwitch { padding: 6px; border-radius: 10px; }
QToolButton#NavFooter {
  text-align: left; padding: 10px 12px; margin: 0 6px; border-radius: 10px; color: @text2;
}
QToolButton#NavFooter:hover { background: @subtleHover; color: @text; }
QToolButton#NavAdd { padding: 3px; border-radius: 8px; }

/* ---------- buttons ---------- */
QPushButton {
  background: @ctrl; border: 1px solid @ctrlStroke; border-bottom-color: @ctrlStrokeBottom;
  border-radius: 8px; padding: 7px 16px; min-height: 22px;
}
QPushButton:hover { background: @ctrlHover; }
QPushButton:pressed { background: @ctrlPressed; color: @text2; border-bottom-color: @ctrlStroke; }
QPushButton:disabled { background: @ctrlDisabled; color: @text3; border-color: @ctrlStroke; }
QPushButton:default, QPushButton[accent="true"] {
  background: @accentBtn; color: @onAccent; border: 1px solid @accentBtn;
}
QPushButton:default:hover, QPushButton[accent="true"]:hover { background: @accentBtnHover; border-color: @accentBtnHover; }
QPushButton:default:pressed, QPushButton[accent="true"]:pressed { background: @accentBtnPressed; border-color: @accentBtnPressed; }
QPushButton:default:disabled, QPushButton[accent="true"]:disabled { background: @ctrlDisabled; color: @text3; border-color: @ctrlStroke; }

QToolButton {
  background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 6px 10px;
}
QToolButton:hover { background: @subtleHover; }
QToolButton:pressed { background: @subtlePressed; color: @text2; }
QToolButton:checked { background: @subtleHover; }
QToolButton:disabled { color: @text3; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton[popupMode="1"] { padding-right: 18px; }
QToolButton::menu-button { border: none; width: 16px; }
QToolButton::menu-arrow { image: url(@imgDown); }

/* ---------- inputs ---------- */
QLineEdit, QAbstractSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
  background: @ctrl; border: 1px solid @ctrlStroke; border-bottom: 1px solid @inputBottom;
  border-radius: 8px; padding: 7px 11px;
  selection-background-color: @accentBtn; selection-color: @onAccent;
}
QPlainTextEdit, QTextEdit { padding: 4px; }
QLineEdit:hover, QAbstractSpinBox:hover, QComboBox:hover { background: @ctrlHover; }
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
  background: @inputFocus; border-bottom: 2px solid @accentText; padding-bottom: 4px;
}
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { background: @ctrlDisabled; color: @text3; border-bottom-color: @ctrlStroke; }
QLineEdit:read-only { background: @ctrlDisabled; }
QComboBox { padding-right: 30px; }
QComboBox::drop-down { border: none; width: 30px; subcontrol-origin: padding; subcontrol-position: center right; }
QComboBox::down-arrow { image: url(@imgDown); width: 12px; height: 12px; }
QComboBox::down-arrow:disabled { image: url(@imgDownDisabled); }
QComboBox QAbstractItemView {
  background: @flyout; border: 1px solid @flyoutStroke; padding: 4px;
  selection-background-color: @subtleHover; selection-color: @text;
}
QComboBox QAbstractItemView::item { min-height: 30px; padding-left: 6px; border-radius: 4px; }
QAbstractSpinBox { padding-right: 26px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; width: 22px; background: transparent; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: @subtleHover; border-radius: 3px; }
QAbstractSpinBox::up-arrow { image: url(@imgUp); width: 10px; height: 10px; }
QAbstractSpinBox::down-arrow { image: url(@imgDown); width: 10px; height: 10px; }

/* ---------- check boxes & radio buttons ---------- */
QCheckBox, QRadioButton { spacing: 8px; padding: 2px 0; }
QCheckBox::indicator, QTreeView::indicator, QListView::indicator {
  width: 18px; height: 18px; border-radius: 4px;
  border: 1px solid @checkStroke; background: @ctrl;
}
QCheckBox::indicator:hover { background: @ctrlHover; }
QCheckBox::indicator:checked, QTreeView::indicator:checked, QListView::indicator:checked {
  background: @accentBtn; border-color: @accentBtn; image: url(@imgCheck);
}
QCheckBox::indicator:disabled { border-color: @ctrlStroke; background: @ctrlDisabled; }
QRadioButton::indicator { width: 18px; height: 18px; border-radius: 10px; border: 1px solid @checkStroke; background: @ctrl; }
QRadioButton::indicator:hover { background: @ctrlHover; }
QRadioButton::indicator:checked { border: 5px solid @accentBtn; background: @onAccent; width: 10px; height: 10px; }

/* ---------- lists & trees ---------- */
QTreeView, QListView, QTableView {
  background: transparent; border: none;
  alternate-background-color: @subtlePressed;
  selection-background-color: @sel; selection-color: @text;
}
QTreeView::item, QListView::item { min-height: 34px; padding: 0 4px; border: none; }
QTreeView::item:hover, QListView::item:hover { background: @subtleHover; }
QTreeView::item:selected, QListView::item:selected { background: @sel; color: @text; }
QTreeView::item:selected:hover, QListView::item:selected:hover { background: @selHover; }
QTreeView::branch { background: transparent; }
QTreeView::branch:hover { background: @subtleHover; }
QTreeView::branch:selected { background: @sel; }
QTreeView::branch:has-children:closed { image: url(@imgRight); }
QTreeView::branch:has-children:open { image: url(@imgDown); }
QHeaderView { background: transparent; border: none; }
QHeaderView::section {
  background: transparent; color: @text2; border: none; border-bottom: 1px solid @divider;
  padding: 7px 8px;
}
QHeaderView::section:hover { background: @subtleHover; }
QHeaderView::up-arrow { image: url(@imgUp); width: 10px; height: 10px; }
QHeaderView::down-arrow { image: url(@imgDown); width: 10px; height: 10px; }

QListWidget#remotes { background: transparent; border: none; }
QListView#IconGrid { padding: 14px 10px 10px 12px; }
QListWidget#remotes::item {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px;
  margin: 6px; padding: 10px 6px 8px 6px; color: @text;
}
QListWidget#remotes::item:hover { background: @cardHover; }
QListWidget#remotes::item:selected { background: @cardHover; border: 1px solid @accentText; color: @text; }

QListWidget#tasksListWidget { background: transparent; border: none; }
QListWidget#tasksListWidget::item {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px; margin: 3px 0; padding: 8px 10px;
}
QListWidget#tasksListWidget::item:hover { background: @cardHover; }
QListWidget#tasksListWidget::item:selected { background: @cardHover; border: 1px solid @accentText; color: @text; }

/* ---------- menus, tooltips ---------- */
QMenuBar { background: @bg; }
QMenu { background: @flyout; border: 1px solid @flyoutStroke; padding: 4px; }
QMenu::item { padding: 7px 28px 7px 10px; border-radius: 4px; margin: 1px 0; }
QMenu::item:selected { background: @subtleHover; color: @text; }
QMenu::item:disabled { color: @text3; }
QMenu::separator { height: 1px; background: @divider; margin: 4px 2px; }
QMenu::icon { padding-left: 10px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }
QMenu::indicator:checked { image: url(@imgCheck); background: @accentBtn; border-radius: 3px; }
QToolTip { background: @flyout; color: @text; border: 1px solid @flyoutStroke; padding: 5px 8px; }

/* ---------- containers ---------- */
QGroupBox {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px;
  margin-top: 26px; padding: 14px 10px 10px 10px;
}
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 2px; top: 2px; font-weight: 600; }
QTabWidget::pane { border: 1px solid @cardStroke; border-radius: 8px; background: @card; top: -1px; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
  background: transparent; color: @text2; border: none; border-bottom: 2px solid transparent;
  padding: 7px 14px; margin-right: 2px;
}
QTabBar::tab:hover { color: @text; }
QTabBar::tab:selected { color: @text; border-bottom: 2px solid @accentText; }
QSplitter::handle { background: transparent; }
QStatusBar { background: @bg; color: @text2; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @text2; padding: 0 4px; }

/* ---------- progress & scroll bars ---------- */
QProgressBar {
  background: @track; border: none; border-radius: 2px;
  min-height: 4px; max-height: 4px; color: transparent;
}
QProgressBar::chunk { background: @accentText; border-radius: 2px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px 2px 2px 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0 2px 2px 2px; }
QScrollBar::handle:vertical { background: @scroll; border-radius: 2px; min-height: 32px; margin: 0 4px; }
QScrollBar::handle:horizontal { background: @scroll; border-radius: 2px; min-width: 32px; margin: 4px 0; }
QScrollBar::handle:vertical:hover { margin: 0 2px; border-radius: 3px; }
QScrollBar::handle:horizontal:hover { margin: 2px 0; border-radius: 3px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
)QSS";

QString buildStyleSheet() {
  QString s = QString::fromUtf8(kStyleSheet);
  const auto t = tokens();
  // replace longest names first so "@ctrlHover" isn't eaten by "@ctrl"
  QStringList keys = t.keys();
  std::sort(keys.begin(), keys.end(),
            [](const QString &a, const QString &b) { return a.size() > b.size(); });
  for (const auto &k : keys) {
    s.replace("@" + k, t.value(k));
  }
  return s;
}

QPalette buildPalette() {
  const auto t = tokens();
  auto c = [&](const char *k) { return QColor(t.value(k)); };
  QPalette p;
  p.setColor(QPalette::Window, c("bg"));
  p.setColor(QPalette::WindowText, c("text"));
  p.setColor(QPalette::Base, c("ctrl"));
  p.setColor(QPalette::AlternateBase, c("layer"));
  p.setColor(QPalette::Text, c("text"));
  p.setColor(QPalette::Button, c("ctrl"));
  p.setColor(QPalette::ButtonText, c("text"));
  p.setColor(QPalette::ToolTipBase, c("flyout"));
  p.setColor(QPalette::ToolTipText, c("text"));
  p.setColor(QPalette::PlaceholderText, c("text2")); // readable search hints
  p.setColor(QPalette::Highlight, gAccentBtn);
  p.setColor(QPalette::HighlightedText, c("onAccent"));
  p.setColor(QPalette::Link, gAccentText);
  p.setColor(QPalette::LinkVisited, gAccentText);
  p.setColor(QPalette::Light, c("ctrl"));
  p.setColor(QPalette::Midlight, c("ctrlStroke"));
  p.setColor(QPalette::Mid, c("ctrlStroke"));
  p.setColor(QPalette::Dark, c("ctrlStrokeBottom"));
  p.setColor(QPalette::Shadow, gDark ? Qt::black : QColor(0, 0, 0, 60));
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    p.setColor(QPalette::Disabled, role, c("text3"));
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  p.setColor(QPalette::Accent, gAccentBtn);
#endif
  return p;
}

bool systemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return false;
#endif
}

// ------------------------------------------------- Windows window chrome ----

#ifdef Q_OS_WIN
void styleNativeWindow(QWidget *w) {
  HWND hwnd = reinterpret_cast<HWND>(w->winId());
  if (!hwnd) {
    return;
  }
  // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 2004+ / 11)
  BOOL dark = gDark ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

  const bool popup = (w->windowFlags() & Qt::Popup) == Qt::Popup ||
                     w->windowType() == Qt::ToolTip;
  if (popup) {
    // 33 = DWMWA_WINDOW_CORNER_PREFERENCE, 2 = DWMWCP_ROUND (Windows 11)
    int corner = 3; // DWMWCP_ROUNDSMALL for menus and tooltips
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
  } else {
    // Title bar in the window background colour (Windows 11 22000+):
    // 35 = DWMWA_CAPTION_COLOR, 36 = DWMWA_TEXT_COLOR
    QColor bg = gDark ? QColor("#0E0E0E") : QColor("#F3F1EC");
    QColor fg = gDark ? QColor("#F5F3EE") : QColor("#1C1917");
    COLORREF cap = RGB(bg.red(), bg.green(), bg.blue());
    COLORREF txt = RGB(fg.red(), fg.green(), fg.blue());
    DwmSetWindowAttribute(hwnd, 35, &cap, sizeof(cap));
    DwmSetWindowAttribute(hwnd, 36, &txt, sizeof(txt));
  }
}

class WindowChromeFilter : public QObject {
public:
  using QObject::QObject;
  bool eventFilter(QObject *o, QEvent *e) override {
    if (e->type() == QEvent::Show) {
      if (auto *w = qobject_cast<QWidget *>(o); w && w->isWindow()) {
        styleNativeWindow(w);
      }
    }
    return false;
  }
};
#endif

QFont fluentFont();
void pinClassFonts(const QFont &font);
bool classFontsIntact();

bool gApplying = false;

// Re-pins the fonts if something outside this file resets them: a Windows
// theme or accessibility change makes Qt rebuild its per-class font table.
class FontGuard : public QObject {
public:
  using QObject::QObject;
  bool eventFilter(QObject *o, QEvent *e) override {
    if (o == qApp && !gApplying && !mPending &&
        (e->type() == QEvent::ApplicationFontChange || e->type() == QEvent::ThemeChange)) {
      mPending = true;
      QTimer::singleShot(0, this, [this]() {
        mPending = false;
        if (!gApplying && !classFontsIntact()) {
          gApplying = true;
          pinClassFonts(fluentFont());
          gApplying = false;
        }
      });
    }
    return false;
  }

private:
  bool mPending = false;
};

void installGuards() {
  static bool done = false;
  if (!done) {
    done = true;
    qApp->installEventFilter(new FontGuard(qApp));
  }
}

void applyChromeToAllWindows() {
#ifdef Q_OS_WIN
  static WindowChromeFilter *filter = nullptr;
  if (!filter) {
    filter = new WindowChromeFilter(qApp);
    qApp->installEventFilter(filter);
  }
  for (QWidget *w : QApplication::topLevelWidgets()) {
    if (w->isVisible()) {
      styleNativeWindow(w);
    }
  }
#endif
}

QFont fluentFont() {
  // Built once: it is requested for every item painted by the sidebar.
  static const QFont f = [] {
    QFont font(kFamilies.first());
    font.setFamilies(kFamilies);
    font.setStyleHint(QFont::SansSerif);
    font.setPointSizeF(kBasePt);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
  }();
  return f;
}

// Windows (and Qt, when the system theme changes) keeps a table of fonts per
// widget class: list views, headers, menus, tooltips, message boxes... They
// are normally 9 pt "Segoe UI" and win over the application font. Overriding
// them here is the second line of defence; the style sheet is the first.
void pinClassFonts(const QFont &font) {
  QApplication::setFont(font); // also clears the per-class table
  for (const char *cls : {"QAbstractItemView", "QListView", "QListWidget", "QTreeView",
                          "QTreeWidget", "QTableView", "QHeaderView", "QMenu", "QMenuBar",
                          "QTabBar", "QStatusBar", "QTipLabel", "QToolTip", "QMessageBox",
                          "QMessageBoxLabel", "QMessageBoxDetailsText", "QLabel",
                          "QAbstractButton", "QLineEdit", "QComboBox", "QGroupBox",
                          "QTextEdit", "QPlainTextEdit", "QDialog"}) {
    QApplication::setFont(font, cls);
  }
}

bool classFontsIntact() {
  const QFont want = fluentFont();
  if (QApplication::font() != want) {
    return false;
  }
  for (const char *cls : {"QAbstractItemView", "QListView", "QTreeView", "QHeaderView", "QMenu",
                          "QMessageBox", "QTipLabel", "QStatusBar"}) {
    if (QApplication::font(cls) != want) {
      return false;
    }
  }
  return true;
}

} // namespace

// ------------------------------------------------------------ public ----

Mode load() {
  auto settings = GetSettings();
  if (!settings->contains("Settings/theme")) {
    if (settings->contains("Settings/darkMode")) {
      return settings->value("Settings/darkMode").toBool() ? Dark : System;
    }
    return System;
  }
  int v = settings->value("Settings/theme").toInt();
  return (v >= System && v <= Dark) ? Mode(v) : System;
}

void save(Mode mode) {
  auto settings = GetSettings();
  settings->setValue("Settings/theme", int(mode));
  settings->setValue("Settings/darkMode", mode == Dark);
}

// Colour tokens are looked up on every paint of the sidebar and the home cards.
// Keys are string literals, so a view of the literal is a stable, allocation-free key.
static QHash<QLatin1StringView, QColor> &colorCache() {
  static QHash<QLatin1StringView, QColor> cache;
  return cache;
}

static void applyMode(Mode mode) {
  colorCache().clear();
  gApplying = true;
  gDark = mode == Dark || (mode == System && systemIsDark());
  computeAccent();

  // The style object is created once; switching theme only swaps palette and
  // style sheet (re-creating the style re-polishes every widget twice).
  static bool styleSet = false;
  if (!styleSet) {
    styleSet = true;
    qApp->setStyle(QStyleFactory::create("Fusion"));
  }
  static bool cacheSized = false;
  if (!cacheSized) {
    cacheSized = true;
    QPixmapCache::setCacheLimit(32 * 1024); // KiB: file icons at several sizes and colours
  }
  pinClassFonts(fluentFont());
  qApp->setPalette(buildPalette());
  qApp->setStyleSheet(buildStyleSheet());
  applyChromeToAllWindows();
  gApplying = false;
  installGuards();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (!gWatching) {
    gWatching = true;
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, [] {
      if (load() == System) {
        apply();
      }
    });
  }
#else
  Q_UNUSED(gWatching);
#endif
  emit notifier()->changed();
}

void apply() { applyMode(load()); }

Notifier *notifier() {
  static Notifier *n = new Notifier;
  return n;
}

bool isDark() { return gDark; }

QFont uiFont(qreal scale) {
  QFont f = fluentFont();
  f.setPointSizeF(f.pointSizeF() * scale);
  return f;
}

QFont monoFont() {
  QFont f(kMonoFamilies.first());
  f.setFamilies(kMonoFamilies);
  f.setStyleHint(QFont::Monospace);
  f.setFixedPitch(true);
  f.setPointSizeF(kMonoPt);
  return f;
}

void toggle() {
  save(gDark ? Light : Dark);
  apply();
}
bool usingFusionDark() { return gDark; }
QColor accent() { return gAccentText; }

QIcon icon(const QString &name, const QColor &color) {
  return QIcon(new FluentIconEngine(name, color));
}

QIcon hoverIcon(const QString &name, const QColor &hover) {
  return QIcon(new FluentIconEngine(name, QColor(), hover));
}

// ------------------------------------------------- file type icons ----

QIcon fileIcon(const QString &fileName, bool isFolder) {
  static QHash<QString, QIcon> cache;
  struct T { const char *icon; const char *color; };
  static const QHash<QString, T> byExt = [] {
    QHash<QString, T> m;
    auto add = [&](std::initializer_list<const char *> exts, T t) {
      for (auto e : exts) m.insert(e, t);
    };
    add({"jpg", "jpeg", "png", "gif", "bmp", "webp", "heic", "heif", "tif",
         "tiff", "svg", "raw", "cr2", "nef", "arw", "dng", "ico", "avif"},
        {"f_image", "#0E9F8F"});
    add({"mp4", "mkv", "mov", "avi", "wmv", "webm", "m4v", "mpg", "mpeg",
         "flv", "3gp", "ts"},
        {"f_video", "#8E44C9"});
    add({"mp3", "flac", "wav", "aac", "ogg", "m4a", "wma", "opus", "aiff"},
        {"f_audio", "#E3711B"});
    add({"zip", "7z", "rar", "tar", "gz", "bz2", "xz", "zst", "iso", "cab",
         "tgz"},
        {"f_archive", "#B8862B"});
    add({"pdf"}, {"f_doc", "#D13438"});
    add({"xls", "xlsx", "xlsm", "csv", "ods", "tsv"}, {"f_sheet", "#108A48"});
    add({"ppt", "pptx", "odp", "key"}, {"f_slides", "#D0522B"});
    add({"doc", "docx", "odt", "rtf", "pages"}, {"f_text", "#2B6CD8"});
    add({"txt", "md", "log", "ini", "cfg", "conf", "nfo"}, {"f_text", "#7A8594"});
    add({"c", "cpp", "h", "hpp", "cs", "py", "js", "ts", "tsx", "jsx", "java",
         "go", "rs", "rb", "php", "html", "css", "json", "xml", "yml", "yaml",
         "sh", "ps1", "bat", "cmd", "sql", "kt", "swift", "lua"},
        {"f_code", "#5C6BC0"});
    add({"exe", "msi", "msix", "appx", "apk", "dmg", "deb", "rpm"},
        {"f_app", "#3F6FD8"});
    return m;
  }();

  const QString key = isFolder ? QString("/folder")
                               : fileExtension(fileName).toLower();
  auto it = cache.find(key);
  if (it != cache.end()) {
    return it.value();
  }
  QIcon result;
  if (isFolder) {
    for (int px : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256}) {
      result.addFile(QString(":/folder/folder-%1.png").arg(px), QSize(px, px));
    }
  } else {
    auto t = byExt.value(key, {"f_doc", "#8592A3"});
    result = icon(t.icon, QColor(t.color));
  }
  cache.insert(key, result);
  return result;
}

// ------------------------------------------------- remote type tiles ----

namespace {
class TileIconEngine : public QIconEngine {
public:
  TileIconEngine(const QString &glyph, const QColor &bg) : mGlyph(glyph), mBg(bg) {}
  void paint(QPainter *p, const QRect &r, QIcon::Mode mode, QIcon::State) override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QColor bg = mBg;
    if (mode == QIcon::Disabled) bg.setAlpha(110);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    const QRectF rf = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = rf.width() * 0.24;
    p->drawRoundedRect(rf, radius, radius);
    const qreal inset = rf.width() * 0.2;
    drawGlyph(p, rf.adjusted(inset, inset, -inset, -inset), mGlyph, Qt::white);
    p->restore();
  }
  QPixmap pixmap(const QSize &s, QIcon::Mode m, QIcon::State st) override {
    return scaledPixmap(s, m, st, 1.0);
  }
  QPixmap scaledPixmap(const QSize &s, QIcon::Mode m, QIcon::State st, qreal scale) override {
    QPixmap pm(s * scale);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(scale);
    QPainter p(&pm);
    paint(&p, QRect(QPoint(0, 0), s), m, st);
    return pm;
  }
  QIconEngine *clone() const override { return new TileIconEngine(mGlyph, mBg); }
  QString key() const override { return "fluent-tile"; }
private:
  QString mGlyph;
  QColor mBg;
};
} // namespace

// A logo file. Logos that are themselves almost black (Filen's, for example)
// would vanish on the dark theme, so there they are drawn on a light rounded
// plate. Decided while painting, so it follows the light / dark switch.
class LogoIconEngine : public QIconEngine {
public:
  explicit LogoIconEngine(const QString &file) : mFile(file), mIcon(file) {
    const QImage img = mIcon.pixmap(64, 64).toImage().convertToFormat(QImage::Format_ARGB32);
    double sum = 0;
    int n = 0;
    for (int y = 0; y < img.height(); ++y) {
      for (int x = 0; x < img.width(); ++x) {
        const QColor c = QColor::fromRgba(img.pixel(x, y));
        if (c.alpha() > 200) {
          sum += 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF();
          ++n;
        }
      }
    }
    mNearlyBlack = n > 0 && sum / n < 0.12;
  }

  void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
    const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
    p->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
  }

  void draw(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) {
    if (gDark && mNearlyBlack) {
      p->save();
      p->setRenderHint(QPainter::Antialiasing);
      p->setPen(Qt::NoPen);
      p->setBrush(QColor("#E7E5E4"));
      const qreal r = rect.width() * 0.24;
      p->drawRoundedRect(QRectF(rect), r, r);
      p->restore();
      const int m = qMax(2, rect.width() / 8);
      mIcon.paint(p, rect.adjusted(m, m, -m, -m), Qt::AlignCenter, mode, state);
    } else {
      mIcon.paint(p, rect, Qt::AlignCenter, mode, state);
    }
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
    return scaledPixmap(size, mode, state, 1.0);
  }

  QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state,
                       qreal scale) override {
    // Drawn once per size, mode and theme: lists repaint these a lot.
    const QString key = QString("%1x%2@%3/%4/%5/%6")
                            .arg(size.width()).arg(size.height()).arg(scale)
                            .arg(int(mode)).arg(int(state)).arg(gDark && mNearlyBlack);
    if (const QPixmap *hit = mCache.object(key)) {
      return *hit;
    }
    QPixmap pm(size * scale);
    pm.fill(Qt::transparent);
    {
      QPainter p(&pm);
      draw(&p, QRect(QPoint(0, 0), size * scale), mode, state);
    }
    pm.setDevicePixelRatio(scale);
    mCache.insert(key, new QPixmap(pm), 1);
    return pm;
  }

  QIconEngine *clone() const override { return new LogoIconEngine(mFile, mIcon, mNearlyBlack); }
  QString key() const override { return "logo"; }

private:
  LogoIconEngine(const QString &file, const QIcon &icon, bool nearlyBlack)
      : mFile(file), mIcon(icon), mNearlyBlack(nearlyBlack) {}

  QString mFile;
  QIcon mIcon;
  bool mNearlyBlack = false;
  QCache<QString, QPixmap> mCache{24};
};

static QIcon buildRemoteIcon(const QString &type) {
  // Logos: the user's own folder first, then the logos shipped with the app.
  QStringList dirs;
#ifdef Q_OS_WIN
  dirs << QDir(qApp->applicationDirPath()).filePath("icons/remotes");
#else
  // An AppImage is read-only, so users add logos in a writable folder:
  // next to the AppImage in portable mode, else in the user data folder.
  const QString appImage = qEnvironmentVariable("APPIMAGE");
  if (!appImage.isEmpty() && IsPortableMode()) {
    dirs << QDir(QFileInfo(appImage).absolutePath()).filePath("icons/remotes");
  }
  dirs << QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
              .filePath("icons/remotes");
  dirs << QDir(qApp->applicationDirPath()).filePath("../share/rclone-browser/icons/remotes");
  dirs << QDir(qApp->applicationDirPath()).filePath("icons/remotes");
#endif
  for (const QString &d : dirs) {
    for (const char *ext : {"png", "svg", "ico"}) {
      const QString f = QDir(d).filePath(type + "." + ext);
      if (QFileInfo::exists(f)) {
        return QIcon(new LogoIconEngine(f));
      }
    }
  }

  struct T { const char *glyph; const char *color; };
  // Generated together with docs/remote-icons.md from `rclone help backends`.
  static const QHash<QString, T> known = {
      {"drive", {"t_cloud", "#1E8E3E"}},
      {"onedrive", {"t_cloud", "#0F6CBD"}},
      {"dropbox", {"t_cloud", "#2D5BE3"}},
      {"box", {"t_cloud", "#1A73C7"}},
      {"pcloud", {"t_cloud", "#139C9C"}},
      {"mega", {"t_cloud", "#D9272E"}},
      {"jottacloud", {"t_cloud", "#6A4FB8"}},
      {"koofr", {"t_cloud", "#2F9E6E"}},
      {"yandex", {"t_cloud", "#E0A100"}},
      {"mailru", {"t_cloud", "#1D6FD8"}},
      {"pikpak", {"t_cloud", "#3D7BE0"}},
      {"protondrive", {"t_cloud", "#6D4AFF"}},
      {"iclouddrive", {"t_cloud", "#3A8EE6"}},
      {"seafile", {"t_cloud", "#E06C16"}},
      {"opendrive", {"t_cloud", "#3E82C4"}},
      {"hidrive", {"t_cloud", "#E2001A"}},
      {"zoho", {"t_cloud", "#C8312B"}},
      {"sharefile", {"t_cloud", "#4C6A8E"}},
      {"putio", {"t_cloud", "#E3A72A"}},
      {"premiumizeme", {"t_cloud", "#E0632A"}},
      {"filefabric", {"t_cloud", "#4A5D7A"}},
      {"linkbox", {"t_cloud", "#3BA7C9"}},
      {"fichier", {"t_cloud", "#E65A28"}},
      {"filelu", {"t_cloud", "#2E7DD7"}},
      {"filescom", {"t_cloud", "#2563EB"}},
      {"pixeldrain", {"t_cloud", "#4F8A3C"}},
      {"ulozto", {"t_cloud", "#E5007E"}},
      {"filen", {"t_cloud", "#1F2937"}},
      {"gofile", {"t_cloud", "#3B82F6"}},
      {"drime", {"t_cloud", "#7C3AED"}},
      {"quatrix", {"t_cloud", "#0E7490"}},
      {"shade", {"t_cloud", "#525252"}},
      {"sugarsync", {"t_cloud", "#0A84C6"}},
      {"huaweidrive", {"t_cloud", "#CF0A2C"}},
      {"internxt", {"t_cloud", "#0066FF"}},
      {"gphotos", {"t_photos", "#C2185B"}},
      {"cloudinary", {"t_photos", "#3448C5"}},
      {"imagekit", {"t_photos", "#0450D5"}},
      {"s3", {"t_bucket", "#D9822B"}},
      {"b2", {"t_bucket", "#C8312B"}},
      {"azureblob", {"t_bucket", "#0F78D4"}},
      {"azurefiles", {"t_bucket", "#0F78D4"}},
      {"gcs", {"t_bucket", "#3367D6"}},
      {"swift", {"t_bucket", "#B85C1E"}},
      {"oos", {"t_bucket", "#C74634"}},
      {"storj", {"t_bucket", "#2683FF"}},
      {"tardigrade", {"t_bucket", "#2683FF"}},
      {"qingstor", {"t_bucket", "#2D8CF0"}},
      {"sia", {"t_bucket", "#1ED660"}},
      {"netstorage", {"t_bucket", "#0096D6"}},
      {"sftp", {"t_server", "#0E7C86"}},
      {"ftp", {"t_server", "#3B7E5B"}},
      {"smb", {"t_server", "#4E6A92"}},
      {"webdav", {"t_server", "#7553C0"}},
      {"http", {"t_web", "#4E6A92"}},
      {"hdfs", {"t_server", "#5E6B78"}},
      {"internetarchive", {"t_archive", "#555D66"}},
      {"doi", {"t_archive", "#555D66"}},
      {"local", {"t_drive", "#5E6B78"}},
      {"memory", {"t_drive", "#5E6B78"}},
      {"crypt", {"t_lock", "#5B5FC7"}},
      {"alias", {"t_folder", "#C98B1E"}},
      {"union", {"t_folder", "#C98B1E"}},
      {"combine", {"t_folder", "#C98B1E"}},
      {"chunker", {"t_folder", "#C98B1E"}},
      {"compress", {"t_folder", "#C98B1E"}},
      {"hasher", {"t_folder", "#C98B1E"}},
      {"cache", {"t_folder", "#C98B1E"}},
      {"archive", {"t_folder", "#C98B1E"}}
  };
  T t{"t_cloud", "#5E6B78"};
  if (known.contains(type)) {
    t = known.value(type);
  } else {
    // unknown types: stable colour derived from the name
    static const char *palette[] = {"#2B6CD8", "#108A48", "#8E44C9", "#D0522B",
                                    "#0E9F8F", "#C2185B", "#5C6BC0", "#B8862B"};
    t = {"t_cloud", palette[qHash(type) % 8]};
  }
  return QIcon(new TileIconEngine(t.glyph, QColor(t.color)));
}

QColor color(const char *token) {
  // `token` must be a string literal (see colorCache()).
  auto &cache = colorCache();
  const QLatin1StringView key(token);
  if (auto it = cache.constFind(key); it != cache.constEnd()) {
    return it.value();
  }
  const QString v = tokens().value(QString::fromLatin1(token));
  QColor c;
  if (v.startsWith("rgba(")) {
    const auto p = v.mid(5, v.size() - 6).split(',');
    c = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt(), p[3].toInt());
  } else {
    c = QColor(v);
  }
  cache.insert(key, c);
  return c;
}

bool confirm(QWidget *parent, const QString &title, const QString &question,
             const QString &detail, const QString &acceptText,
             const QString &rejectText) {
  QMessageBox box(parent);
  box.setObjectName("ConfirmBox");
  box.setWindowTitle(title);
  box.setIcon(QMessageBox::Question);
  box.setTextFormat(Qt::RichText);
  box.setText(QString("<b>%1</b>").arg(question.toHtmlEscaped()));
  if (!detail.isEmpty()) {
    box.setInformativeText(detail);
  }
  QPushButton *accept = box.addButton(acceptText, QMessageBox::AcceptRole);
  QPushButton *reject = box.addButton(rejectText, QMessageBox::RejectRole);
  box.setDefaultButton(accept);
  box.setEscapeButton(reject);
  box.exec();
  return box.clickedButton() == accept;
}

void setStatus(QWidget *w, const char *status) {
  w->setProperty("status", status);
  w->style()->unpolish(w);
  w->style()->polish(w);
  w->update();
}

static QHash<QString, QIcon> &remoteIconCache() {
  static QHash<QString, QIcon> cache;
  return cache;
}

QIcon remoteIcon(const QString &type) {
  auto &cache = remoteIconCache();
  auto it = cache.find(type);
  if (it == cache.end()) {
    it = cache.insert(type, buildRemoteIcon(type)); // looks for logo files on disk once
  }
  return it.value();
}

void clearRemoteIconCache() { remoteIconCache().clear(); }

QString displayName(Mode mode) {
  switch (mode) {
  case Light:
    return QObject::tr("Light");
  case Dark:
    return QObject::tr("Dark");
  default:
    return QObject::tr("Same as Windows");
  }
}


// ---------------------------------------------------------------- self test ----

namespace {
struct Probe {
  QString name;
  std::function<QWidget *()> make;
  qreal expect;
};

qreal effectiveSize(QWidget *w) {
  w->ensurePolished();
  if (auto *box = qobject_cast<QMessageBox *>(w)) {
    QLabel *label = box->findChild<QLabel *>("qt_msgbox_label");
    if (label) {
      label->ensurePolished();
      return label->font().pointSizeF();
    }
  }
  return w->font().pointSizeF();
}

QList<Probe> probes() {
  QList<Probe> p;
  p << Probe{"QLabel", [] { return new QLabel("x"); }, kBasePt};
  p << Probe{"QPushButton", [] { return new QPushButton("x"); }, kBasePt};
  p << Probe{"QToolButton", [] { return new QToolButton; }, kBasePt};
  p << Probe{"QLineEdit", [] { return new QLineEdit; }, kBasePt};
  p << Probe{"QComboBox", [] { return new QComboBox; }, kBasePt};
  p << Probe{"QCheckBox", [] { return new QCheckBox("x"); }, kBasePt};
  p << Probe{"QGroupBox", [] { return new QGroupBox("x"); }, kBasePt};
  p << Probe{"QTabBar", [] { return new QTabBar; }, kBasePt};
  p << Probe{"QStatusBar", [] { return new QStatusBar; }, kBasePt};
  p << Probe{"QMenu", [] { return new QMenu; }, kBasePt};
  p << Probe{"QHeaderView", [] { return new QHeaderView(Qt::Horizontal); }, kBasePt};
  p << Probe{"QListWidget", [] { return new QListWidget; }, kBasePt};
  p << Probe{"QDialog", [] { return new QDialog; }, kBasePt};
  p << Probe{"QMessageBox text", [] {
               auto *m = new QMessageBox;
               m->setText("x");
               m->setInformativeText("y");
               return m;
             }, kBasePt};
  p << Probe{"QLabel#AboutTitle", [] {
               auto *l = new QLabel("x");
               l->setObjectName("AboutTitle");
               return l;
             }, kAboutTitlePt};
  p << Probe{"QTreeView (file list)", [] { return new QTreeView; }, kListPt};
  p << Probe{"QListView#IconGrid", [] {
               auto *v = new QListView;
               v->setObjectName("IconGrid");
               return v;
             }, kListPt};
  p << Probe{"QPlainTextEdit (log)", [] { return new QPlainTextEdit; }, kMonoPt};
  return p;
}

void settle() {
  QEventLoop loop;
  QTimer::singleShot(40, &loop, &QEventLoop::quit);
  loop.exec();
}
} // namespace

bool selfTestFonts(QString *report) {
  QStringList problems;
  int checks = 0;
  auto run = [&](const char *phase) {
    for (const Probe &p : probes()) {
      std::unique_ptr<QWidget> w(p.make());
      const qreal got = effectiveSize(w.get());
      ++checks;
      if (!qFuzzyCompare(got + 1, p.expect + 1)) {
        problems << QString("[%1] %2 is %3 pt, expected %4 pt")
                        .arg(phase, p.name).arg(got).arg(p.expect);
      }
    }
  };

  applyMode(Dark);
  run("dark");

  // What Windows does behind the app's back: rebuild the per-class font table
  // from the system theme (9 pt "Segoe UI").
  {
    const QFont system("Sans Serif", 8);
    QApplication::setFont(system);
    for (const char *cls : {"QAbstractItemView", "QListView", "QTreeView", "QHeaderView", "QMenu",
                            "QMessageBox", "QTipLabel", "QStatusBar", "QLabel", "QTabBar"}) {
      QApplication::setFont(system, cls);
    }
    settle();
    run("after font reset");
    if (!classFontsIntact()) {
      problems << "class fonts were not restored after a reset";
    }
  }

  applyMode(Light);
  run("light");
  applyMode(Dark);
  run("dark again");

  if (report) {
    *report = problems.isEmpty()
                  ? QString("fonts ok (%1 checks)").arg(checks)
                  : QString("FONT PROBLEMS (%1 of %2 checks): ").arg(problems.size()).arg(checks) +
                        problems.mid(0, 8).join(" | ");
  }
  return problems.isEmpty();
}

} // namespace Theme
