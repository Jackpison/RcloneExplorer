#pragma once

#include "pch.h"

// Windows 11 (Fluent / WinUI 3) look & feel for the whole application.
//
// The look is produced by Fusion + a token-driven style sheet rather than the
// native style, so it is identical on every Windows 10/11 build and can be
// switched between light and dark at runtime. Colours follow the WinUI 3
// design tokens and the user's Windows accent colour. On Windows, title bars
// are tinted to match and popups get native Windows 11 rounded corners.
namespace Theme {

// Emits changed() after every theme switch, so widgets that cache a
// theme-coloured pixmap can redraw it.
class Notifier : public QObject {
  Q_OBJECT
signals:
  void changed();
};
Notifier *notifier();

// Checks that every kind of widget gets the right text size, also after a
// simulated Windows font reset and after theme switches. Used by --selftest in
// CI on both platforms. Returns false and fills `report` on any mismatch.
bool selfTestFonts(QString *report);

enum Mode { System = 0, Light = 1, Dark = 2 };

Mode load();
void save(Mode mode);

// Switch between light and dark (the sidebar toggle) and save the choice.
void toggle();

// Apply the saved theme to qApp. Safe to call again after settings change.
void apply();

// True when the dark variant is active right now.
bool isDark();


QString displayName(Mode mode);

// The app's UI font (Segoe UI Variable, 12 pt) times `scale`, and the
// monospace font for log output (Cascadia Mono / Consolas). Widgets that
// paint text themselves use these rather than inheriting a widget font:
// on Windows, list and tree views otherwise start with the smaller system
// "icon title" font until the theme is re-applied.
QFont uiFont(qreal scale = 1.0);
QFont monoFont();

// Accent colour as used for text/indicators in the current mode.
QColor accent();

// A Fluent UI System icon from :/fluent (see src/icons/fluent). The icon is
// re-tinted automatically for light/dark and disabled state. Pass a colour to
// force one (e.g. on accent-coloured buttons).
QIcon icon(const QString &name, const QColor &color = QColor());
// Icon that switches to `hover` while the mouse is over its button.
QIcon hoverIcon(const QString &name, const QColor &hover);

// A design-token colour for custom painting, e.g. "card", "cardHover",
// "cardStroke", "text", "text2", "accentText".
QColor color(const char *token);

// Colour-coded file type icon (folders, images, video, audio, archives...).
QIcon fileIcon(const QString &fileName, bool isFolder);

// Colour-coded tile identifying a remote's storage service. Users can
// override any type with icons\remotes\<type>.png|svg|ico next to the exe.
QIcon remoteIcon(const QString &type);
// Forget which logo files were found (call when the remote list is reloaded).
void clearRemoteIconCache();

// Status text styling for job cards: "running", "done" or "error".
void setStatus(QWidget *w, const char *status);

// Question box with a short bold question, an optional second line and
// named buttons. Returns true when the accept button was pressed.
bool confirm(QWidget *parent, const QString &title, const QString &question,
             const QString &detail, const QString &acceptText,
             const QString &rejectText);

} // namespace Theme
