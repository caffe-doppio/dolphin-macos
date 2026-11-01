/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "statusbar/maccapacitybarstyle.h"

#include <KCapacityBar>
#include <KStyleExtensions>

#include <QTest>

class MacCapacityBarStyleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCustomElement();
};

// A bar created anywhere, as KIO does for the Properties dialog, must stop
// falling back to KCapacityBar's own painting once polished.
void MacCapacityBarStyleTest::testCustomElement()
{
    installMacCapacityBarStyle();

    KCapacityBar bar;
    bar.ensurePolished();

    QVERIFY(KStyleExtensions::customControlElement(QStringLiteral("CE_CapacityBar"), &bar));
}

QTEST_MAIN(MacCapacityBarStyleTest)

#include "maccapacitybarstyletest.moc"
