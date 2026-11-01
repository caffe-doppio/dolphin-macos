/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMVIEWSUTILS_H
#define KITEMVIEWSUTILS_H

#include "dolphin_export.h"

#include <QIcon>
#include <qtypes.h>

class QColor;
class QGraphicsItem;

class KItemViewsUtils
{
public:
    static qreal devicePixelRatio(const QGraphicsItem *item);

    /**
     * The theme icon \a name with \a accent for its .ColorScheme-Accent parts, such as the body
     * of a Breeze folder, and the application palette for the rest.
     */
    DOLPHIN_EXPORT static QIcon iconWithAccent(const QString &name, const QColor &accent);
};

#endif // KITEMVIEWSUTILS_H
