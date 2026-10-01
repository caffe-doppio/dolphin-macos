/*
 * SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QUrl>

#include <functional>

namespace AppleEventHandler
{

/// Called on the event loop thread with the file URLs the system asked to reveal.
using RevealRequestHandler = std::function<void(const QList<QUrl> &urls)>;

/**
 * Installs the handler for the kAERevealSelection Apple event ("reveal these files in the file
 * viewer"), which is how `open -R <file>` and third-party "Show in Finder" actions are delivered.
 *
 * Qt's Cocoa plugin does not handle this event, so without this a reveal request is dropped.
 * Must be called after QApplication has been constructed, while the event loop is not yet running.
 */
void install(RevealRequestHandler handler);

} // namespace AppleEventHandler