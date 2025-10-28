#ifndef MUDLET_EDITORUNDOSYSTEM_H
#define MUDLET_EDITORUNDOSYSTEM_H

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

// TODO: Consider evaluating Qt's QUndoStack/QUndoCommand framework as an alternative
// to the custom EditorUndoSystem implementation. Qt's framework provides:
// - QUndoView for visualizing undo history
// - Built-in macro commands (QUndoCommand::mergeWith)
// - Undo group support for multiple document interfaces
// - Cleaner integration with Qt's action system
// Current custom implementation works well but Qt's framework may offer better
// maintainability and additional features for future enhancements.

#include "EditorViewType.h"

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include <vector>

class Host;
class QTreeWidgetItem;

// Forward declarations
class TTrigger;
class TAlias;
class TTimer;
class TScript;
class TKey;
class TAction;

// Base command class
class EditorCommand {
public:
    explicit EditorCommand(Host* host) : mpHost(host) {}
    virtual ~EditorCommand() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual QString text() const = 0;
    virtual EditorViewType viewType() const = 0;
    virtual QList<int> affectedItemIDs() const = 0; // Return IDs of items affected by this command
    virtual void remapItemID(int oldID, int newID) = 0; // Update stored IDs when items are recreated

protected:
    Host* mpHost;
};

// Command for adding items (triggers, aliases, etc.)
class AddItemCommand : public EditorCommand {
public:
    AddItemCommand(EditorViewType viewType, int itemID, int parentID,
                   int positionInParent, bool isFolder, const QString& itemName, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }
    void remapItemID(int oldID, int newID) override;

    // Check if item ID changed during redo (when item was recreated)
    bool didItemIDChange() const { return mOldItemID != -1 && mOldItemID != mItemID; }
    int getOldItemID() const { return mOldItemID; }
    int getNewItemID() const { return mItemID; }

private:
    EditorViewType mViewType;
    int mItemID;
    int mOldItemID = -1; // Tracks old ID before redo, for ID remapping
    int mParentID;
    int mPositionInParent;
    bool mIsFolder;
    QString mItemName;
    QString mItemSnapshot;
};

// Command for deleting items
// NOTE: Uses built-in batching (QList<DeletedItemInfo>) rather than BatchCommand
// for efficiency - delete operations naturally come as sets with ordering constraints
class DeleteItemCommand : public EditorCommand {
public:
    struct DeletedItemInfo {
        int itemID;
        int parentID;
        int positionInParent;
        QString xmlSnapshot;
        QString itemName;
    };

    DeleteItemCommand(EditorViewType viewType, const QList<DeletedItemInfo>& deletedItems, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override;
    void remapItemID(int oldID, int newID) override;

private:
    EditorViewType mViewType;
    QList<DeletedItemInfo> mDeletedItems;
};

// Command for modifying item properties
class ModifyPropertyCommand : public EditorCommand {
public:
    ModifyPropertyCommand(EditorViewType viewType, int itemID,
                          const QString& itemName,
                          const QString& oldStateXML, const QString& newStateXML,
                          Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }
    void remapItemID(int oldID, int newID) override;

private:
    EditorViewType mViewType;
    int mItemID;
    QString mItemName;
    QString mOldStateXML;
    QString mNewStateXML;
};

// Command for moving items between parents
// NOTE: Single-item design - use BatchCommand to group multiple moves triggered by drag-and-drop
class MoveItemCommand : public EditorCommand {
public:
    MoveItemCommand(EditorViewType viewType, int itemID,
                    int oldParentID, int newParentID,
                    int oldPosition, int newPosition,
                    const QString& itemName,
                    Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }
    void remapItemID(int oldID, int newID) override;

private:
    EditorViewType mViewType;
    int mItemID;
    int mOldParentID;
    int mNewParentID;
    int mOldPosition;
    int mNewPosition;
    QString mItemName;
};

// Command for toggling active/inactive state
class ToggleActiveCommand : public EditorCommand {
public:
    ToggleActiveCommand(EditorViewType viewType, int itemID,
                        bool oldState, bool newState,
                        const QString& itemName,
                        Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }
    void remapItemID(int oldID, int newID) override;

private:
    EditorViewType mViewType;
    int mItemID;
    bool mOldActiveState;
    bool mNewActiveState;
    QString mItemName;
};

// Compound command that contains multiple sub-commands for batch operations
// Use BatchCommand when: operations are triggered individually but should undo/redo as one atomic action
// Use built-in batching (e.g. DeleteItemCommand) when: operations naturally come as sets from a single UI action
class BatchCommand : public EditorCommand {
public:
    explicit BatchCommand(const QString& description, Host* host)
        : EditorCommand(host), mDescription(description) {}

    void addCommand(std::unique_ptr<EditorCommand> cmd) {
        mCommands.push_back(std::move(cmd));
    }

    void undo() override {
        // Undo in reverse order
        for (auto it = mCommands.rbegin(); it != mCommands.rend(); ++it) {
            (*it)->undo();
        }
    }

    void redo() override {
        // Redo in forward order
        for (auto& cmd : mCommands) {
            cmd->redo();
        }
    }

    QString text() const override {
        return mDescription;
    }

    EditorViewType viewType() const override {
        // Return the view type of the first command if available
        return mCommands.empty() ? EditorViewType::cmTriggerView : mCommands[0]->viewType();
    }

    QList<int> affectedItemIDs() const override {
        QList<int> allIDs;
        for (const auto& cmd : mCommands) {
            allIDs.append(cmd->affectedItemIDs());
        }
        return allIDs;
    }

    void remapItemID(int oldID, int newID) override {
        for (auto& cmd : mCommands) {
            cmd->remapItemID(oldID, newID);
        }
    }

private:
    std::vector<std::unique_ptr<EditorCommand>> mCommands;
    QString mDescription;
};

// Main undo system class
class EditorUndoSystem : public QObject {
    Q_OBJECT

public:
    explicit EditorUndoSystem(Host* host, QObject* parent = nullptr);
    ~EditorUndoSystem();

    void pushCommand(std::unique_ptr<EditorCommand> cmd);
    void undo();
    void redo();

    void beginBatch(const QString& description = QString());
    void endBatch();

    bool canUndo() const { return !mUndoStack.empty(); }
    bool canRedo() const { return !mRedoStack.empty(); }

    QString undoText() const;
    QString redoText() const;

    void clear();
    void setUndoLimit(int limit);
    int undoLimit() const { return mUndoLimit; }

signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void undoTextChanged(const QString& text);
    void redoTextChanged(const QString& text);
    void itemsChanged(EditorViewType viewType, QList<int> affectedItemIDs); // Emitted when items are added/deleted/modified

private:
    void clearRedoStack();
    void enforceUndoLimit();
    void emitSignals();
    void remapItemIDs(int oldID, int newID); // Update all commands when an item ID changes

    std::vector<std::unique_ptr<EditorCommand>> mUndoStack;
    std::vector<std::unique_ptr<EditorCommand>> mRedoStack;
    int mUndoLimit = 50;
    Host* mpHost;

    // Batch operation support
    bool mInBatch = false;
    std::vector<std::unique_ptr<EditorCommand>> mBatchCommands;
    QString mBatchDescription;
};

// XML Helper Functions for item export/import
// These functions are used by both EditorUndoSystem and MudletAddItemCommand
// to preserve item state during undo/redo operations

QString exportTriggerToXML(TTrigger* trigger);
TTrigger* importTriggerFromXML(const QString& xmlSnapshot, TTrigger* pParent, Host* host, int position = -1);
bool updateTriggerFromXML(TTrigger* trigger, const QString& xmlSnapshot);

QString exportAliasToXML(TAlias* alias);
TAlias* importAliasFromXML(const QString& xmlSnapshot, TAlias* pParent, Host* host, int position = -1);
bool updateAliasFromXML(TAlias* alias, const QString& xmlSnapshot);

QString exportTimerToXML(TTimer* timer);
TTimer* importTimerFromXML(const QString& xmlSnapshot, TTimer* pParent, Host* host, int position = -1);
bool updateTimerFromXML(TTimer* timer, const QString& xmlSnapshot);

QString exportScriptToXML(TScript* script);
TScript* importScriptFromXML(const QString& xmlSnapshot, TScript* pParent, Host* host, int position = -1);
bool updateScriptFromXML(TScript* script, const QString& xmlSnapshot);

QString exportKeyToXML(TKey* key);
TKey* importKeyFromXML(const QString& xmlSnapshot, TKey* pParent, Host* host, int position = -1);
bool updateKeyFromXML(TKey* key, const QString& xmlSnapshot);

QString exportActionToXML(TAction* action);
TAction* importActionFromXML(const QString& xmlSnapshot, TAction* pParent, Host* host, int position = -1);
bool updateActionFromXML(TAction* action, const QString& xmlSnapshot);

#endif // MUDLET_EDITORUNDOSYSTEM_H
