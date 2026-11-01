/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHINMACOPENWITH_H
#define DOLPHINMACOPENWITH_H

#include "dolphin_export.h"

class KPropertiesDialog;

/**
 * The "Open with" row of the General tab, as Finder shows it. KIO reads the default
 * application from freedesktop .desktop files, which macOS lacks, and its "Change…"
 * button runs keditfiletype, which is not shipped: both are given to LaunchServices.
 */
namespace DolphinMacOpenWith
{
/**
 * Call right after creating \a dialog, before it is shown.
 * Does nothing if KIO's widgets are not found.
 */
DOLPHIN_EXPORT void adopt(KPropertiesDialog *dialog);
}

#endif // DOLPHINMACOPENWITH_H
