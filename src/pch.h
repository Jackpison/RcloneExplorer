#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <functional>
#include <memory>

#include <QtCore>
#include <QtDebug>
#include <QtGui>
#include <QtNetwork>
#include <QtWidgets>

#ifdef Q_OS_WIN
// Win32 API used directly (file icons, COM, console for "rclone config").
// Qt 5 pulled these in through QtWinExtras, which no longer exists in Qt 6.
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
// rpcndr.h defines "small" as "char", which breaks ordinary C++ code.
#ifdef small
#undef small
#endif
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
