/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kitemlistmacdrag.h"

#include <QDrag>

#import <AppKit/AppKit.h>
#include <objc/runtime.h>

namespace
{
using EndedAtPoint = void (*)(id, SEL, NSDraggingSession *, NSPoint, NSDragOperation);

EndedAtPoint s_qtEndedAtPoint = nullptr;
NSDragOperation s_lastOperation = NSDragOperationNone;

void recordEndedAtPoint(id self, SEL cmd, NSDraggingSession *session, NSPoint screenPoint, NSDragOperation operation)
{
    s_lastOperation = operation;
    s_qtEndedAtPoint(self, cmd, session, screenPoint, operation);
}

// QNSView is the drag source of every Qt window. Its drag end is wrapped once to keep the
// operation, and Qt's own implementation still runs, so the drag loop ends as before.
// Remove once Qt reports NSDragOperationDelete to QDrag::exec().
bool installHook()
{
    Class view = NSClassFromString(@"QNSView");
    Method method = view ? class_getInstanceMethod(view, @selector(draggingSession:endedAtPoint:operation:)) : nullptr;
    if (!method) {
        return false;
    }
    s_qtEndedAtPoint = reinterpret_cast<EndedAtPoint>(method_setImplementation(method, reinterpret_cast<IMP>(recordEndedAtPoint)));
    return true;
}
}

bool KItemListMacDrag::execEndsOnTrash(QDrag *drag, Qt::DropActions supportedActions, Qt::DropAction defaultAction)
{
    static const bool hooked = installHook();
    s_lastOperation = NSDragOperationNone;
    drag->exec(supportedActions, defaultAction);
    // Exactly Delete: a target that answered with a wider mask must not get files trashed.
    return hooked && s_lastOperation == NSDragOperationDelete;
}
