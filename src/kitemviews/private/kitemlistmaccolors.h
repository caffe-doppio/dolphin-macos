/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMLISTMACCOLORS_H
#define KITEMLISTMACCOLORS_H

#include "dolphin_export.h"

#include <QColor>
#include <QObject>
#include <QString>

/**
 * Folder colours as Finder shows them, so that Dolphin and Finder agree.
 * Breeze icons keep their shape: these colours replace .ColorScheme-Accent.
 */
namespace KItemListMacColors
{
/**
 * The colour Finder gives a folder without tags. It follows the accent colour and the
 * icon tint chosen in System Settings, so it is read from the system folder icon.
 */
QColor systemFolderColor();

/**
 * Makes the system folder colour the application's accent colour, as Plasma does with its
 * own, and keeps it there when the palette changes. Icons drawn without colours of their own,
 * such as those of the Places panel, take it. Call once, after the application object exists.
 */
DOLPHIN_EXPORT void useSystemFolderColorAsAccent();

/**
 * Colour index of the most recently added Finder tag of \a localPath that has a colour,
 * from 1 (gray) to 7 (orange), or 0 if there is none. Finder tints the folder with it.
 */
DOLPHIN_EXPORT int tagColorIndex(const QString &localPath);

/**
 * The system colour of the tag colour \a index, in the current light or dark appearance.
 */
DOLPHIN_EXPORT QColor tagColor(int index);

/**
 * Makes \a index, from 1 (gray) to 7 (orange), the colour tag of \a localPath, as if chosen in
 * Finder: the other colour tags (Red, Orange…) are removed, tags with other names are kept.
 * An \a index of 0 removes the colour tags. Emits KItemListMacTagNotifier::tagsChanged().
 */
DOLPHIN_EXPORT bool setTagColorIndex(const QString &localPath, int index);
}

/**
 * Tells this process's folder models that the Finder tags of a path changed. KIO compares
 * neither the change time nor the extended attributes, so it reports no change of its own.
 */
class DOLPHIN_EXPORT KItemListMacTagNotifier : public QObject
{
    Q_OBJECT

public:
    static KItemListMacTagNotifier *instance();

Q_SIGNALS:
    void tagsChanged(const QString &localPath);
};

#endif // KITEMLISTMACCOLORS_H
