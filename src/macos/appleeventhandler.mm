/*
 * SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appleeventhandler.h"

#include <QCoreApplication>
#include <QTimer>
#include <QUrl>

#include <utility>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

// Qt's Cocoa plugin only installs handlers for kAEGetURL and application:openFiles:.
// A Finder-style "reveal this file" request (`open -R <file>`, or a third-party app calling
// NSWorkspace -activateFileViewerSelectingURLs:) arrives as the kAERevealSelection Apple event
// instead, and is silently dropped. Without handling it, revealing a file does nothing at all,
// which matters once Dolphin is registered via NSFileViewer as the file viewer.
//
// The alias list of a kAERevealSelection event holds the URLs to reveal, either as a list of
// strings or as a list of descriptors wrapping strings, depending on the sender.
//
// Requests are queued rather than delivered straight from the event handler: the event can
// arrive before main() has installed the callback and before an event loop exists, and the
// handler must not run application code while AppKit is still dispatching the event.

namespace
{

AppleEventHandler::RevealRequestHandler s_handler;

/// Requests received before setHandler() ran.
QList<QUrl> s_pending;

/// Extracts file URLs from the alias list of a kAERevealSelection event.
QList<QUrl> urlsFromRevealEvent(NSAppleEventDescriptor *event)
{
    QList<QUrl> urls;

    NSAppleEventDescriptor *aliasList = [event paramDescriptorForKeyword:keyDirectObject];
    if (!aliasList) {
        return urls;
    }

    const NSUInteger count = [aliasList numberOfItems];
    for (NSUInteger i = 1; i <= count; ++i) {
        NSAppleEventDescriptor *item = [aliasList descriptorAtIndex:i];
        if (!item) {
            continue;
        }

        NSString *string = [item stringValue];
        if (string.length == 0 && item.data) {
            string = [[NSString alloc] initWithBytes:item.data.bytes length:item.data.length encoding:NSUTF8StringEncoding];
        }
        if (string.length == 0) {
            continue;
        }

        const QUrl url = QUrl::fromUserInput(QString::fromNSString(string));
        if (url.isValid()) {
            urls.append(url);
        }
    }

    return urls;
}

} // namespace

// The handler must be a method on an Objective-C object; a private class keeps it out of the
// global namespace.
@interface KDCocoaRevealHandler : NSObject
@end

@implementation KDCocoaRevealHandler

- (void)handleRevealSelection:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply
{
    Q_UNUSED(reply);

    const QList<QUrl> urls = urlsFromRevealEvent(event);
    if (urls.isEmpty()) {
        return;
    }

    // No event loop exists during launch, so a single-shot timer would never fire; queue instead
    // and let setHandler()/the first event loop turn deliver it.
    if (!QCoreApplication::instance() || !s_handler) {
        s_pending.append(urls);
        return;
    }

    const auto handler = s_handler;
    const QList<QUrl> captured = urls;
    QTimer::singleShot(0, [handler, captured]() {
        handler(captured);
    });
}

@end

namespace AppleEventHandler
{

void install()
{
    // The handler object must outlive this call: NSAppleEventManager keeps only a weak reference.
    static KDCocoaRevealHandler *s_handlerObject = [[KDCocoaRevealHandler alloc] init];

    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:s_handlerObject
          andSelector:@selector(handleRevealSelection:withReplyEvent:)
        forEventClass:kCoreEventClass
           andEventID:kAERevealSelection];
}

void setHandler(RevealRequestHandler handler)
{
    s_handler = std::move(handler);

    // Anything that arrived before now was queued.
    if (!s_pending.isEmpty() && s_handler) {
        const auto handler = s_handler;
        const QList<QUrl> pending = std::move(s_pending);
        s_pending.clear();
        QTimer::singleShot(0, [handler, pending]() {
            handler(pending);
        });
    }
}

} // namespace AppleEventHandler