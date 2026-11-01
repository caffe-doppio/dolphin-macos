/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemviews/private/kitemlistmaccolors.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#import <AppKit/AppKit.h>

class KItemListMacColorsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testApplicationPalette();
    void testSetTagColorIndex();
};

namespace
{
QStringList tagNames(const QString &path)
{
    QStringList names;
    @autoreleasepool {
        NSArray<NSString *> *tags = nil;
        [[NSURL fileURLWithPath:path.toNSString()] getResourceValue:&tags forKey:NSURLTagNamesKey error:nil];
        for (NSString *tag in tags) {
            names << QString::fromNSString(tag);
        }
    }
    return names;
}

QColor systemAccentColor()
{
    NSColor *srgb = [NSColor.controlAccentColor colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    return QColor::fromRgbF(srgb.redComponent, srgb.greenComponent, srgb.blueComponent);
}
}

void KItemListMacColorsTest::testApplicationPalette()
{
    KItemListMacColors::useSystemFolderColorAsAccent();
    // Widgets take the system accent colour, folder icons keep the folder colour.
    QCOMPARE(QApplication::palette().color(QPalette::Highlight), systemAccentColor());
    QCOMPARE(QApplication::palette().color(QPalette::Accent), KItemListMacColors::systemFolderColor());

    // A colour scheme change, as KColorSchemeManager makes, replaces the whole palette.
    QApplication::setPalette(QPalette(Qt::darkGray));
    QCOMPARE(QApplication::palette().color(QPalette::Inactive, QPalette::Highlight), systemAccentColor());
    QCOMPARE(QApplication::palette().color(QPalette::Accent), KItemListMacColors::systemFolderColor());
}

void KItemListMacColorsTest::testSetTagColorIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path();
    QSignalSpy spy(KItemListMacTagNotifier::instance(), &KItemListMacTagNotifier::tagsChanged);

    // A tag without a colour, as Finder's custom tags.
    @autoreleasepool {
        QVERIFY([[NSURL fileURLWithPath:path.toNSString()] setResourceValue:@[ @"Dolphin test" ] forKey:NSURLTagNamesKey error:nil]);
    }
    QCOMPARE(KItemListMacColors::tagColorIndex(path), 0);

    // Orange is added after the custom tag.
    QVERIFY(KItemListMacColors::setTagColorIndex(path, 7));
    QCOMPARE(KItemListMacColors::tagColorIndex(path), 7);
    QCOMPARE(tagNames(path).size(), 2);
    QCOMPARE(tagNames(path).first(), QStringLiteral("Dolphin test"));

    // Green replaces Orange: one colour tag at a time, as the menu offers one colour.
    QVERIFY(KItemListMacColors::setTagColorIndex(path, 2));
    QCOMPARE(KItemListMacColors::tagColorIndex(path), 2);
    QCOMPARE(tagNames(path).size(), 2);

    // Default removes the colour tags and keeps the others.
    QVERIFY(KItemListMacColors::setTagColorIndex(path, 0));
    QCOMPARE(KItemListMacColors::tagColorIndex(path), 0);
    QCOMPARE(tagNames(path), QStringList{QStringLiteral("Dolphin test")});

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).at(0).toString(), path);
}

QTEST_MAIN(KItemListMacColorsTest)

#include "kitemlistmaccolorstest.moc"
