/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemviewsutils.h"

#include <KIconColors>
#include <KIconLoader>

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

qreal KItemViewsUtils::devicePixelRatio(const QGraphicsItem *item)
{
    qreal dpr = qApp->devicePixelRatio();
    if (item->scene()) {
        if (const auto views = item->scene()->views(); !views.isEmpty()) {
            dpr = views.first()->devicePixelRatioF();
        }
    }
    return dpr;
}

QIcon KItemViewsUtils::iconWithAccent(const QString &name, const QColor &accent)
{
    KIconColors colors(QApplication::palette());
    colors.setAccent(accent);
    // KIconLoader's pixmap cache key holds text, highlight, highlightedText and background,
    // not the accent (paletteId() in KIconThemes 6.31), so the first accent loaded would be
    // returned for every other. Icon stylesheets use QColor::name(), which drops the alpha:
    // carrying the accent in the alpha of these colours changes the key, not the icon.
    // Remove once KIconThemes adds the accent to the key.
    const auto withAlpha = [](QColor color, int alpha) {
        color.setAlpha(alpha);
        return color;
    };
    colors.setText(withAlpha(colors.text(), accent.red()));
    colors.setHighlight(withAlpha(colors.highlight(), accent.green()));
    colors.setHighlightedText(withAlpha(colors.highlightedText(), accent.blue()));
    return KDE::icon(name, colors);
}
