/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MACCAPACITYBARSTYLE_H
#define MACCAPACITYBARSTYLE_H

/**
 * Draws every KCapacityBar of the application as a progress bar of the current style:
 * the status bar's free space and the Properties dialog's.
 *
 * Craft builds Breeze without KStyle, so Breeze lacks KCapacityBar's CE_CapacityBar
 * element, and KWidgetsAddons 6.31 then paints the bar itself with shades of the window
 * colour, near black on a dark scheme. KWidgetsAddons ad1b701e3c72 replaces that painting
 * with CE_ProgressBar, which is what this style does in the meantime.
 *
 * Call it once the application style is set.
 */
void installMacCapacityBarStyle();

#endif
