/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemlistmaccolors.h"

#include <QApplication>
#include <QFile>
#include <QPalette>

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <sys/xattr.h>

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

namespace
{
void setApplicationAccent()
{
    const QColor color = KItemListMacColors::systemFolderColor();
    QPalette palette = QApplication::palette();
    // setPalette() sends ApplicationPaletteChange again: stop once the accent is in place.
    if (palette.color(QPalette::Accent) == color) {
        return;
    }
    palette.setColor(QPalette::Accent, color);
    QApplication::setPalette(palette);
}

// A colour scheme change, such as dark mode, replaces the whole application palette.
class ApplicationPaletteFollower : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange) {
            setApplicationAccent();
        }
        return false;
    }
};
}

void KItemListMacColors::useSystemFolderColorAsAccent()
{
    setApplicationAccent();
    qApp->installEventFilter(new ApplicationPaletteFollower(qApp));
}

int KItemListMacColors::tagColorIndex(const QString &localPath)
{
    // A binary plist array of "Name\nindex" strings, in the order the tags were added.
    // A tag without a colour, such as a custom one, has no index or index 0.
    static const char attribute[] = "com.apple.metadata:_kMDItemUserTags";
    const QByteArray path = QFile::encodeName(localPath);
    const ssize_t size = getxattr(path.constData(), attribute, nullptr, 0, 0, XATTR_NOFOLLOW);
    if (size <= 0) {
        return 0;
    }
    QByteArray value(size, Qt::Uninitialized);
    if (getxattr(path.constData(), attribute, value.data(), value.size(), 0, XATTR_NOFOLLOW) != size) {
        return 0;
    }

    @autoreleasepool {
        NSData *data = [NSData dataWithBytesNoCopy:value.data() length:value.size() freeWhenDone:NO];
        id tags = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nullptr error:nullptr];
        if (![tags isKindOfClass:NSArray.class]) {
            return 0;
        }
        for (id tag in [tags reverseObjectEnumerator]) {
            if (![tag isKindOfClass:NSString.class]) {
                continue;
            }
            const NSRange newline = [tag rangeOfString:@"\n" options:NSBackwardsSearch];
            if (newline.location == NSNotFound) {
                continue;
            }
            const int index = [[tag substringFromIndex:newline.location + 1] intValue];
            if (index >= 1 && index <= 7) {
                return index;
            }
        }
    }
    return 0;
}

QColor KItemListMacColors::tagColor(int index)
{
    @autoreleasepool {
        NSColor *colors[] = {nil,
                             NSColor.systemGrayColor,
                             NSColor.systemGreenColor,
                             NSColor.systemPurpleColor,
                             NSColor.systemBlueColor,
                             NSColor.systemYellowColor,
                             NSColor.systemRedColor,
                             NSColor.systemOrangeColor};
        if (index < 1 || index > 7) {
            return QColor();
        }
        // System colours are resolved for an appearance: use the application's, so that
        // the colour follows dark mode and increased contrast like Finder's.
        NSColor *systemColor = colors[index];
        __block QColor color;
        NSAppearance *appearance = NSApp ? NSApp.effectiveAppearance : NSAppearance.currentDrawingAppearance;
        [appearance performAsCurrentDrawingAppearance:^{
            color = toQColor(systemColor);
        }];
        return color;
    }
}
