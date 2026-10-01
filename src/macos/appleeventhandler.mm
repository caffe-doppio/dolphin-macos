/*
 * SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appleeventhandler.h"

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

namespace
{

/// Set by install(); read on the event loop thread when a reveal event arrives.
AppleEventHandler::RevealRequestHandler s_revealRequestHandler;

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

// The handler must be a method on an Objective-C object; a category-free private class keeps it
// out of the global namespace without needing an instance of our own beyond the one held below.
@interface KDCocoaRevealHandler : NSObject
@end

@implementation KDCocoaRevealHandler
@end

namespace AppleEventHandler
{

void install(RevealRequestHandler handler)
{
    s_revealRequestHandler = std::move(handler);

    // The handler object must outlive this call: NSAppleEventManager keeps only a weak reference.
    static KDCocoaRevealHandler *s_handlerObject = [[KDCocoaRevealHandler alloc] init];

    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:s_handlerObject
          andSelector:@selector(handleRevealSelection:withReplyEvent:)
        forEventClass:kCoreEventClass
           andEventID:kAERevealSelection];


}

} // namespace AppleEventHandler