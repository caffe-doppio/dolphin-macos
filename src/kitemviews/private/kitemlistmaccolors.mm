/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemlistmaccolors.h"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace
{
QColor toQColor(NSColor *color)
{
    NSColor *srgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    return srgb ? QColor::fromRgbF(srgb.redComponent, srgb.greenComponent, srgb.blueComponent) : QColor();
}

QColor sampleSystemFolderColor()
{
    @autoreleasepool {
        // The settings behind the folder colour (accent, icon style, custom tint) are not
        // public API, so the colour is taken from the folder icon macOS draws.
        constexpr int size = 32;
        NSBitmapImageRep *bitmap = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                                             pixelsWide:size
                                                                             pixelsHigh:size
                                                                          bitsPerSample:8
                                                                        samplesPerPixel:4
                                                                               hasAlpha:YES
                                                                               isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                                            bytesPerRow:0
                                                                           bitsPerPixel:0] autorelease];
        NSImage *icon = [NSWorkspace.sharedWorkspace iconForContentType:UTTypeFolder];
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
        [icon drawInRect:NSMakeRect(0, 0, size, size)];
        [NSGraphicsContext restoreGraphicsState];

        // Average the front of the folder, below the tab, where the icon is shaded the least.
        // ponytail: fixed area of the icon, to revisit if a macOS release redraws the folder.
        qreal red = 0, green = 0, blue = 0;
        int count = 0;
        for (int y = 14; y < 24; ++y) {
            for (int x = 8; x < 24; ++x) {
                NSColor *pixel = [[bitmap colorAtX:x y:y] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
                if (pixel.alphaComponent < 0.9) {
                    continue;
                }
                red += pixel.redComponent;
                green += pixel.greenComponent;
                blue += pixel.blueComponent;
                ++count;
            }
        }
        return count ? QColor::fromRgbF(red / count, green / count, blue / count) : toQColor(NSColor.controlAccentColor);
    }
}
}

QColor KItemListMacColors::systemFolderColor()
{
    // ponytail: read once, so a change in System Settings shows after Dolphin restarts.
    static const QColor color = sampleSystemFolderColor();
    return color;
}
