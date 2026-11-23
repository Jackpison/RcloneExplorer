#pragma once

#include <QByteArray>
#include <QString>

// Saving a user file (tasks, mirrors) so that a crash or a full disk halfway
// through loses nothing. The file is written in place, not replaced: Scoop
// keeps these files across updates through hard links, which a replaced file
// would cut. The complete new content is first saved beside it as
// PendingSaveFile(), which stays there until the real file has been written,
// so a reader should try PendingSaveFile() first.
QString PendingSaveFile(const QString &path);
// False if nothing could be saved.
bool WriteFileSafely(const QString &path, const QByteArray &data);
