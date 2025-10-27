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

// Editor view type enum (should match dlgTriggerEditor's enum)
enum class EditorViewType {
    cmTriggerView,
    cmTimerView,
    cmAliasView,
    cmScriptView,
    cmActionView,
    cmKeysView,
    cmVarsView,
    cmUnknownView
};

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

protected:
    Host* mpHost;
};

// Command for adding items (triggers, aliases, etc.)
class AddItemCommand : public EditorCommand {
public:
    AddItemCommand(EditorViewType viewType, int itemID, int parentID,
                   bool isFolder, const QString& itemName, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;
    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }

private:
    EditorViewType mViewType;
    int mItemID;
    int mParentID;
    bool mIsFolder;
    QString mItemName;
    QString mItemSnapshot;
};

// Command for deleting items
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

private:
    EditorViewType mViewType;
    int mItemID;
    QString mItemName;
    QString mOldStateXML;
    QString mNewStateXML;
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

    std::vector<std::unique_ptr<EditorCommand>> mUndoStack;
    std::vector<std::unique_ptr<EditorCommand>> mRedoStack;
    int mUndoLimit = 50;
    Host* mpHost;
};

#endif // MUDLET_EDITORUNDOSYSTEM_H
