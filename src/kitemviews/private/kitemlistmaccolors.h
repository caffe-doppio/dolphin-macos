/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMLISTMACCOLORS_H
#define KITEMLISTMACCOLORS_H

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
