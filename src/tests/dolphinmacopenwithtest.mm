/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "views/dolphinmacopenwith.h"

#include <KPropertiesDialog>

#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#import <AppKit/AppKit.h>

// Reads the row only: a test never changes a system association.
class DolphinMacOpenWithTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTextFileShowsDefaultApp();
    void testDynamicTypeHidesButton();
    void testFolderHidesRow();

private:
    QString createFile(const QString &name);
    QTemporaryDir m_dir;
};

QString DolphinMacOpenWithTest::createFile(const QString &name)
{
    const QString path = m_dir.filePath(name);
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write("text\n");
    return path;
}

void DolphinMacOpenWithTest::testTextFileShowsDefaultApp()
{
    const QString path = createFile(QStringLiteral("note.txt"));
    QString expected;
    @autoreleasepool {
        NSURL *app = [NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:[NSURL fileURLWithPath:path.toNSString()]];
        QVERIFY(app);
        expected = QString::fromNSString([NSFileManager.defaultManager displayNameAtPath:app.path]);
    }

    KPropertiesDialog dialog(QUrl::fromLocalFile(path));
    DolphinMacOpenWith::adopt(&dialog);

    const auto *label = dialog.findChild<QLabel *>(QStringLiteral("defaultHandlerLabel"));
    QVERIFY(label);
    QCOMPARE(label->text(), expected);
    QVERIFY(label->isEnabled());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("configureMimeBtn"))->isHidden());
}

void DolphinMacOpenWithTest::testDynamicTypeHidesButton()
{
    const QString path = createFile(QStringLiteral("unknown.zq9xk"));

    KPropertiesDialog dialog(QUrl::fromLocalFile(path));
    DolphinMacOpenWith::adopt(&dialog);

    const auto *button = dialog.findChild<QPushButton *>(QStringLiteral("configureMimeBtn"));
    QVERIFY(button);
    QVERIFY(button->isHidden());
}

void DolphinMacOpenWithTest::testFolderHidesRow()
{
    KPropertiesDialog dialog(QUrl::fromLocalFile(m_dir.path()));
    DolphinMacOpenWith::adopt(&dialog);

    const auto *label = dialog.findChild<QLabel *>(QStringLiteral("defaultHandlerLabel"));
    QVERIFY(label);
    QVERIFY(label->isHidden());
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("defaultHandlerLabel_Left"))->isHidden());
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("configureMimeBtn"))->isHidden());
}

QTEST_MAIN(DolphinMacOpenWithTest)

#include "dolphinmacopenwithtest.moc"
