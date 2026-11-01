/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinmacpagefooter.h"

#include <KPageDialog>

#include <QDialogButtonBox>
#include <QStyle>
#include <QTest>

class DolphinMacPageFooterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMargins();
};

// The buttons keep the style's distance from the window edge, not KPageView's 4 pixels.
void DolphinMacPageFooterTest::testMargins()
{
    installMacPageFooterMargins();

    KPageDialog dialog;
    auto buttonBox = dialog.findChild<QDialogButtonBox *>();
    QVERIFY(buttonBox);
    buttonBox->ensurePolished();

    const QMargins margins = buttonBox->contentsMargins();
    QCOMPARE(margins.right(), dialog.style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, &dialog));
    QCOMPARE(margins.bottom(), dialog.style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, &dialog));
    QCOMPARE(margins.top(), 4);
}

QTEST_MAIN(DolphinMacPageFooterTest)

#include "dolphinmacpagefootertest.moc"
