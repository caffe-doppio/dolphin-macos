/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMLISTMACCOLORS_H
#define KITEMLISTMACCOLORS_H

#include <QColor>

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

}

#endif // KITEMLISTMACCOLORS_H
