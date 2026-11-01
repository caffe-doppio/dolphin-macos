/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHINMACPAGEFOOTER_H
#define DOLPHINMACPAGEFOOTER_H

/**
 * Moves the buttons of every page dialog (Properties, Configure Dolphin) away from the
 * window edge, by the style's margin for a window instead of KPageView's fixed 4 pixels.
 * With the large corners of macOS windows, those 4 pixels put the last button in the corner.
 *
 * Call it once the application style is set.
 */
void installMacPageFooterMargins();

#endif
