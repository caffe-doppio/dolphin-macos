/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinmacpagefooter.h"

#include <KPageView>

#include <QApplication>
#include <QEvent>
#include <QStyle>

namespace
{
class PageFooterMargins : public QObject
{
public:
    using QObject::QObject;

protected:
    // KPageView::setPageFooter() sets the margins when the dialog is built, so they are
    // replaced when the footer is polished, before it is first shown.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish) {
            auto footer = qobject_cast<QWidget *>(watched);
            auto view = footer ? qobject_cast<KPageView *>(footer->parentWidget()) : nullptr;
            if (view && view->pageFooter() == footer) {
                const QStyle *style = footer->style();
                const QWidget *window = footer->window();
                footer->setContentsMargins(style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, window),
                                           footer->contentsMargins().top(),
                                           style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, window),
                                           style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, window));
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
}

void installMacPageFooterMargins()
{
    qApp->installEventFilter(new PageFooterMargins(qApp));
}
