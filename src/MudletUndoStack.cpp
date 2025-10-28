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

MudletUndoStack::MudletUndoStack(QObject* parent)
    : QUndoStack(parent)
{
    // Connect to indexChanged signal to emit itemsChanged after undo/redo
    connect(this, &QUndoStack::indexChanged, this, [this](int idx) {
        Q_UNUSED(idx);
        // Emit itemsChanged signal for the current command
        if (index() > 0 && index() <= count()) {
            const QUndoCommand* cmd = command(index() - 1);
            if (auto* mudletCmd = dynamic_cast<const MudletCommand*>(cmd)) {
                emit itemsChanged(mudletCmd->viewType(), mudletCmd->affectedItemIDs());
            }
        }
    });
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
