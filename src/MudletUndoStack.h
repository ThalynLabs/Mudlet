#ifndef MUDLET_MUDLETUNDOSTACK_H
#define MUDLET_MUDLETUNDOSTACK_H

/***************************************************************************
 *   Copyright (C) 2025 by Mudlet Development Team                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "EditorViewType.h"

#include <QList>
#include <QUndoStack>

/*!
 * \brief Wrapper around Qt's QUndoStack with Mudlet-specific features
 *
 * Extends QUndoStack to add:
 * - itemsChanged signal for targeted UI updates
 * - remapItemIDs() method for ID tracking when items are recreated
 * - Overridden undo()/redo() to emit custom signals
 *
 * This class replaces the custom EditorUndoSystem as part of the migration
 * to Qt's built-in undo framework.
 */
class MudletUndoStack : public QUndoStack
{
    Q_OBJECT

public:
    explicit MudletUndoStack(QObject* parent = nullptr);

    /*!
     * \brief Remaps item IDs in all commands on the stack
     *
     * When an item is deleted and restored via undo, it gets a new ID.
     * This method updates all commands on the undo/redo stacks to reflect
     * the ID change.
     *
     * \param oldID The original item ID
     * \param newID The new item ID after recreation
     */
    void remapItemIDs(int oldID, int newID);

signals:
    /*!
     * \brief Emitted when items are modified by undo/redo operations
     *
     * This signal allows the UI to refresh only the affected items,
     * rather than refreshing the entire tree.
     *
     * \param viewType The editor view type (triggers, aliases, etc.)
     * \param affectedItemIDs List of item IDs that were modified
     */
    void itemsChanged(EditorViewType viewType, QList<int> affectedItemIDs);

protected:
    // Expose protected command() method for accessing commands on the stack
    using QUndoStack::command;
};

#endif // MUDLET_MUDLETUNDOSTACK_H
