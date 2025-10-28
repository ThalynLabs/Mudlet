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

#include "MudletUndoStack.h"
#include "MudletCommand.h"
#include "commands/MudletAddItemCommand.h"

#include <QDebug>

MudletUndoStack::MudletUndoStack(QObject* parent)
    : QUndoStack(parent)
{
    // Connect to indexChanged signal to emit itemsChanged after undo/redo
    connect(this, &QUndoStack::indexChanged, this, [this](int newIndex) {
        // Skip emitting itemsChanged during push() operations or macro push operations
        // The action has already been performed before pushing
        if (mInPushOperation || mInMacroPush) {
            mPreviousIndex = newIndex;
            return;
        }

        // Determine which command was affected based on index movement
        int affectedCommandIndex = -1;

        if (newIndex > mPreviousIndex) {
            // Redo: index increased, emit for command that was redone
            affectedCommandIndex = newIndex - 1;
        } else if (newIndex < mPreviousIndex) {
            // Undo: index decreased, emit for command that was undone
            affectedCommandIndex = mPreviousIndex - 1;
        }

        // Emit itemsChanged for the affected command (including all children for macros)
        if (affectedCommandIndex >= 0 && affectedCommandIndex < count()) {
            const QUndoCommand* cmd = command(affectedCommandIndex);
            emitChangesForCommand(cmd);
        }

        // Update previous index for next change
        mPreviousIndex = newIndex;
    });
}

void MudletUndoStack::emitChangesForCommand(const QUndoCommand* cmd)
{
    if (!cmd) {
        return;
    }

    // Collect all affected items by view type from this command and all children
    QMap<EditorViewType, QList<int>> affectedItemsByView;
    collectAffectedItems(cmd, affectedItemsByView);

    // Only emit if we have valid items
    // Emit itemsChanged once per view type with all affected IDs
    for (auto it = affectedItemsByView.constBegin(); it != affectedItemsByView.constEnd(); ++it) {
        const QList<int>& itemIDs = it.value();
        if (!itemIDs.isEmpty()) {
            // Double-check all IDs are valid before emitting
            bool allValid = true;
            for (int id : itemIDs) {
                if (id <= 0) {
                    allValid = false;
                    qWarning() << "MudletUndoStack::emitChangesForCommand() - Invalid item ID" << id << "found, skipping emission";
                    break;
                }
            }
            if (allValid) {
                emit itemsChanged(it.key(), itemIDs);
            }
        }
    }
}

void MudletUndoStack::collectAffectedItems(const QUndoCommand* cmd, QMap<EditorViewType, QList<int>>& affectedItemsByView)
{
    if (!cmd) {
        return;
    }

    // If this is a MudletCommand, collect its affected items
    if (auto* mudletCmd = dynamic_cast<const MudletCommand*>(cmd)) {
        EditorViewType viewType = mudletCmd->viewType();
        QList<int> itemIDs = mudletCmd->affectedItemIDs();

        // Add to the map, avoiding duplicates and invalid IDs
        for (int id : itemIDs) {
            // Skip invalid IDs (0 or negative)
            if (id <= 0) {
                continue;
            }
            if (!affectedItemsByView[viewType].contains(id)) {
                affectedItemsByView[viewType].append(id);
            }
        }
    }

    // Recursively collect from child commands (for macros)
    for (int i = 0; i < cmd->childCount(); ++i) {
        collectAffectedItems(cmd->child(i), affectedItemsByView);
    }
}

void MudletUndoStack::pushCommand(QUndoCommand* cmd)
{
    // Set flag to indicate we're in a push operation
    mInPushOperation = true;
    push(cmd);
    mInPushOperation = false;
}

void MudletUndoStack::beginMacro(const QString& text)
{
    // Set flag to indicate we're starting a macro push operation
    mInMacroPush = true;
    QUndoStack::beginMacro(text);
}

void MudletUndoStack::endMacro()
{
    // Call the base implementation first
    QUndoStack::endMacro();
    // Clear the flag after the macro is complete
    mInMacroPush = false;
}

void MudletUndoStack::redo()
{
    // Get the command that will be redone (if any)
    if (index() < count()) {
        const QUndoCommand* cmd = command(index());

        // Check if this is an AddItemCommand (need to check before redo since ID may change)
        auto* addCmd = dynamic_cast<const MudletAddItemCommand*>(cmd);
        int oldItemID = -1;
        if (addCmd) {
            oldItemID = addCmd->getNewItemID();
        }

        // Call the base class redo
        QUndoStack::redo();

        // Check if the item ID changed during redo
        if (addCmd && addCmd->didItemIDChange()) {
            int newItemID = addCmd->getNewItemID();
            remapItemIDs(oldItemID, newItemID);
        }
    } else {
        // No command to redo
        QUndoStack::redo();
    }
}

void MudletUndoStack::remapItemIDs(int oldID, int newID)
{
    // Iterate through all commands on both undo and redo stacks
    // The stack contains commands from index 0 to count()-1
    for (int i = 0; i < count(); ++i) {
        const QUndoCommand* cmd = command(i);
        // Cast away const to call remapItemID - safe because we own these commands
        if (auto* mudletCmd = dynamic_cast<MudletCommand*>(const_cast<QUndoCommand*>(cmd))) {
            mudletCmd->remapItemID(oldID, newID);
        }
    }
}
