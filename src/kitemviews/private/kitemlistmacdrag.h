/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KITEMLISTMACDRAG_H
#define KITEMLISTMACDRAG_H

#include <qnamespace.h>

class QDrag;

namespace KItemListMacDrag
{
/**
 * Runs QDrag::exec() and returns true if the items were dropped on the Trash in the Dock.
 *
 * The Dock answers NSDragOperationDelete and leaves trashing the files to the drag source.
 * Qt maps that operation to Qt::MoveAction and does not report it to exec() for a drop
 * outside the application, so it is read from Qt's view instead.
 */
bool execEndsOnTrash(QDrag *drag, Qt::DropActions supportedActions, Qt::DropAction defaultAction);
}

#endif // KITEMLISTMACDRAG_H
