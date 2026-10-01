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
 * Registers the handler for the kAERevealSelection Apple event ("reveal these files in the file
 * viewer"), which is how `open -R <file>` and third-party "Show in Finder" actions are delivered.
 * Qt's Cocoa plugin does not handle this event.
 *
 * Must be called before QApplication is constructed: Qt installs its own Apple Event handlers
 * during QApplication construction, and a handler registered afterwards is shadowed by them.
 *
 * Events can arrive before setHandler() has supplied a callback - during launch, for instance -
 * so requests are queued until then.
 */
void install();

/// Supplies the callback and immediately delivers anything queued so far.
void setHandler(RevealRequestHandler handler);

} // namespace AppleEventHandler