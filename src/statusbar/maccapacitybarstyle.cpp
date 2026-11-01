/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "maccapacitybarstyle.h"

#include <KCapacityBar>

#include <QApplication>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionProgressBar>

namespace
{
// The hint KStyleExtensions asks with, renaming the widget to the element it wants.
constexpr auto SH_KCustomStyleElement = static_cast<QStyle::StyleHint>(0xff000001);
constexpr auto CE_CapacityBar = static_cast<QStyle::ControlElement>(QStyle::CE_CustomBase + 1);

class CapacityBarStyle : public QProxyStyle
{
    Q_OBJECT
    // KStyleExtensions ignores the hint's answer from a style without this.
    Q_CLASSINFO("X-KDE-CustomElements", "true")

public:
    using QProxyStyle::QProxyStyle;

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const override
    {
        if (hint == SH_KCustomStyleElement && widget && widget->objectName() == QLatin1String("CE_CapacityBar")) {
            return CE_CapacityBar;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        const auto bar = qobject_cast<const KCapacityBar *>(widget);
        const auto progress = qstyleoption_cast<const QStyleOptionProgressBar *>(option);
        if (element != CE_CapacityBar || !bar || !progress) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }

        // The option covers the whole widget. With the text outside, the bar takes the top
        // and the text the bottom, as KWidgetsAddons ad1b701e3c72 draws them.
        QStyleOptionProgressBar barOption(*progress);
        const bool textOutside = bar->drawTextMode() == KCapacityBar::DrawTextOutline;
        if (textOutside) {
            barOption.rect.setHeight(bar->barHeight());
            barOption.textVisible = false;
        }
        QProxyStyle::drawControl(CE_ProgressBar, &barOption, painter, widget);
        if (textOutside) {
            const QString text = bar->fontMetrics().elidedText(progress->text, Qt::ElideRight, option->rect.width());
            painter->drawText(option->rect, Qt::AlignBottom | bar->horizontalTextAlignment(), text);
        }
    }

protected:
    // Bars built outside Dolphin, as in KIO's Properties dialog, get the style when polished.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto bar = qobject_cast<KCapacityBar *>(watched); bar && bar->style() != this) {
                bar->setStyle(this);
            }
        }
        return QProxyStyle::eventFilter(watched, event);
    }
};
}

void installMacCapacityBarStyle()
{
    // A style of its own over the application's: QProxyStyle would delete a base it was handed.
    auto style = new CapacityBarStyle(QApplication::style()->name());
    style->setParent(qApp);
    qApp->installEventFilter(style);
}

#include "maccapacitybarstyle.moc"
