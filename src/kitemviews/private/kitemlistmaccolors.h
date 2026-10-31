/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMLISTMACCOLORS_H
#define KITEMLISTMACCOLORS_H

#include "dolphin_export.h"

#include <QColor>
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
int tagColorIndex(const QString &localPath);

/**
 * The system colour of the tag colour \a index, in the current light or dark appearance.
 */
QColor tagColor(int index);
}

#endif // KITEMLISTMACCOLORS_H
