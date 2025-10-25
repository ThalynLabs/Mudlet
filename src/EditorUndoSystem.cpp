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

#include "EditorUndoSystem.h"

#include "Host.h"
#include "TTrigger.h"
#include "TAlias.h"
#include "TTimer.h"
#include "TScript.h"
#include "TKey.h"
#include "TAction.h"
#include "TriggerUnit.h"
#include "AliasUnit.h"
#include "TimerUnit.h"
#include "ScriptUnit.h"
#include "KeyUnit.h"
#include "ActionUnit.h"
#include "XMLexport.h"
#include "XMLimport.h"

#include <QDebug>
#include <sstream>

// =============================================================================
// Helper functions for XML serialization
// =============================================================================

namespace {

// Export a single trigger to XML string
QString exportTriggerToXML(TTrigger* trigger) {
    if (!trigger) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("TriggerSnapshot");

    XMLexport exporter(trigger);
    exporter.writeTrigger(trigger, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Export a single alias to XML string
QString exportAliasToXML(TAlias* alias) {
    if (!alias) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("AliasSnapshot");

    XMLexport exporter(alias);
    exporter.writeAlias(alias, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Export a single timer to XML string
QString exportTimerToXML(TTimer* timer) {
    if (!timer) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("TimerSnapshot");

    XMLexport exporter(timer);
    exporter.writeTimer(timer, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Export a single script to XML string
QString exportScriptToXML(TScript* script) {
    if (!script) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("ScriptSnapshot");

    XMLexport exporter(script);
    exporter.writeScript(script, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Export a single key to XML string
QString exportKeyToXML(TKey* key) {
    if (!key) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("KeySnapshot");

    XMLexport exporter(key);
    exporter.writeKey(key, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Export a single action to XML string
QString exportActionToXML(TAction* action) {
    if (!action) {
        return QString();
    }

    pugi::xml_document doc;
    auto root = doc.append_child("ActionSnapshot");

    XMLexport exporter(action);
    exporter.writeAction(action, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

// Helper to get item name based on view type and ID
QString getItemName(EditorViewType viewType, int itemID, Host* host) {
    switch (viewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* trigger = host->getTriggerUnit()->getTrigger(itemID);
        return trigger ? trigger->getName() : QString();
    }
    case EditorViewType::cmAliasView: {
        TAlias* alias = host->getAliasUnit()->getAlias(itemID);
        return alias ? alias->getName() : QString();
    }
    case EditorViewType::cmTimerView: {
        TTimer* timer = host->getTimerUnit()->getTimer(itemID);
        return timer ? timer->getName() : QString();
    }
    case EditorViewType::cmScriptView: {
        TScript* script = host->getScriptUnit()->getScript(itemID);
        return script ? script->getName() : QString();
    }
    case EditorViewType::cmKeysView: {
        TKey* key = host->getKeyUnit()->getKey(itemID);
        return key ? key->getName() : QString();
    }
    case EditorViewType::cmActionView: {
        TAction* action = host->getActionUnit()->getAction(itemID);
        return action ? action->getName() : QString();
    }
    default:
        return QString();
    }
}

// Helper to get view type name
QString getViewTypeName(EditorViewType viewType) {
    switch (viewType) {
    case EditorViewType::cmTriggerView:
        return QObject::tr("Trigger");
    case EditorViewType::cmAliasView:
        return QObject::tr("Alias");
    case EditorViewType::cmTimerView:
        return QObject::tr("Timer");
    case EditorViewType::cmScriptView:
        return QObject::tr("Script");
    case EditorViewType::cmKeysView:
        return QObject::tr("Key");
    case EditorViewType::cmActionView:
        return QObject::tr("Action");
    default:
        return QObject::tr("Item");
    }
}

} // anonymous namespace

// =============================================================================
// AddItemCommand implementation
// =============================================================================

AddItemCommand::AddItemCommand(EditorViewType viewType, int itemID, int parentID,
                               bool isFolder, const QString& itemName, Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mParentID(parentID)
, mIsFolder(isFolder)
, mItemName(itemName)
{
}

void AddItemCommand::undo() {
    // Export the item to XML before deleting (for redo)
    if (mItemSnapshot.isEmpty()) {
        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            TTrigger* trigger = mpHost->getTriggerUnit()->getTrigger(mItemID);
            if (trigger) {
                mItemSnapshot = exportTriggerToXML(trigger);
            }
            break;
        }
        case EditorViewType::cmAliasView: {
            TAlias* alias = mpHost->getAliasUnit()->getAlias(mItemID);
            if (alias) {
                mItemSnapshot = exportAliasToXML(alias);
            }
            break;
        }
        case EditorViewType::cmTimerView: {
            TTimer* timer = mpHost->getTimerUnit()->getTimer(mItemID);
            if (timer) {
                mItemSnapshot = exportTimerToXML(timer);
            }
            break;
        }
        case EditorViewType::cmScriptView: {
            TScript* script = mpHost->getScriptUnit()->getScript(mItemID);
            if (script) {
                mItemSnapshot = exportScriptToXML(script);
            }
            break;
        }
        case EditorViewType::cmKeysView: {
            TKey* key = mpHost->getKeyUnit()->getKey(mItemID);
            if (key) {
                mItemSnapshot = exportKeyToXML(key);
            }
            break;
        }
        case EditorViewType::cmActionView: {
            TAction* action = mpHost->getActionUnit()->getAction(mItemID);
            if (action) {
                mItemSnapshot = exportActionToXML(action);
            }
            break;
        }
        default:
            break;
        }
    }

    // Delete the item
    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* trigger = mpHost->getTriggerUnit()->getTrigger(mItemID);
        if (trigger) {
            delete trigger;
        }
        break;
    }
    case EditorViewType::cmAliasView: {
        TAlias* alias = mpHost->getAliasUnit()->getAlias(mItemID);
        if (alias) {
            delete alias;
        }
        break;
    }
    case EditorViewType::cmTimerView: {
        TTimer* timer = mpHost->getTimerUnit()->getTimer(mItemID);
        if (timer) {
            delete timer;
        }
        break;
    }
    case EditorViewType::cmScriptView: {
        TScript* script = mpHost->getScriptUnit()->getScript(mItemID);
        if (script) {
            delete script;
        }
        break;
    }
    case EditorViewType::cmKeysView: {
        TKey* key = mpHost->getKeyUnit()->getKey(mItemID);
        if (key) {
            delete key;
        }
        break;
    }
    case EditorViewType::cmActionView: {
        TAction* action = mpHost->getActionUnit()->getAction(mItemID);
        if (action) {
            delete action;
        }
        break;
    }
    default:
        break;
    }
}

void AddItemCommand::redo() {
    // On first redo, the item already exists (it was just created)
    // On subsequent redos, we need to recreate from snapshot
    if (!mFirstRedo && !mItemSnapshot.isEmpty()) {
        // TODO: Implement item recreation from XML snapshot
        // This requires extending XMLimport to support single-item import
        qWarning() << "AddItemCommand::redo() - Item recreation from snapshot not yet implemented";
    }
    mFirstRedo = false;
}

QString AddItemCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    if (mIsFolder) {
        return QObject::tr("Add %1 Group \"%2\"").arg(typeName, mItemName);
    } else {
        return QObject::tr("Add %1 \"%2\"").arg(typeName, mItemName);
    }
}

// =============================================================================
// DeleteItemCommand implementation
// =============================================================================

DeleteItemCommand::DeleteItemCommand(EditorViewType viewType, const QList<DeletedItemInfo>& deletedItems, Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mDeletedItems(deletedItems)
{
}

void DeleteItemCommand::undo() {
    // Restore all deleted items from their XML snapshots
    // TODO: Implement item restoration from XML snapshots
    // This requires extending XMLimport to support single-item import with specific parent/position
    qWarning() << "DeleteItemCommand::undo() - Item restoration from snapshot not yet implemented";
}

void DeleteItemCommand::redo() {
    // On first redo, items are already deleted
    // On subsequent redos, we need to delete them again
    if (!mFirstRedo) {
        for (const auto& info : mDeletedItems) {
            switch (mViewType) {
            case EditorViewType::cmTriggerView: {
                TTrigger* trigger = mpHost->getTriggerUnit()->getTrigger(info.itemID);
                if (trigger) {
                    delete trigger;
                }
                break;
            }
            case EditorViewType::cmAliasView: {
                TAlias* alias = mpHost->getAliasUnit()->getAlias(info.itemID);
                if (alias) {
                    delete alias;
                }
                break;
            }
            case EditorViewType::cmTimerView: {
                TTimer* timer = mpHost->getTimerUnit()->getTimer(info.itemID);
                if (timer) {
                    delete timer;
                }
                break;
            }
            case EditorViewType::cmScriptView: {
                TScript* script = mpHost->getScriptUnit()->getScript(info.itemID);
                if (script) {
                    delete script;
                }
                break;
            }
            case EditorViewType::cmKeysView: {
                TKey* key = mpHost->getKeyUnit()->getKey(info.itemID);
                if (key) {
                    delete key;
                }
                break;
            }
            case EditorViewType::cmActionView: {
                TAction* action = mpHost->getActionUnit()->getAction(info.itemID);
                if (action) {
                    delete action;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    mFirstRedo = false;
}

QString DeleteItemCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    if (mDeletedItems.size() == 1) {
        return QObject::tr("Delete %1 \"%2\"").arg(typeName, mDeletedItems.first().itemName);
    } else {
        return QObject::tr("Delete %1 %2(s)").arg(QString::number(mDeletedItems.size()), typeName);
    }
}

// =============================================================================
// ModifyPropertyCommand implementation
// =============================================================================

ModifyPropertyCommand::ModifyPropertyCommand(EditorViewType viewType, int itemID,
                                            const QString& itemName,
                                            const QString& oldStateXML, const QString& newStateXML,
                                            Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mItemName(itemName)
, mOldStateXML(oldStateXML)
, mNewStateXML(newStateXML)
{
}

void ModifyPropertyCommand::undo() {
    // Restore item to old state from XML
    // TODO: Implement property restoration from XML snapshot
    // This requires being able to update an existing item's properties from XML
    qWarning() << "ModifyPropertyCommand::undo() - Property restoration not yet implemented";
}

void ModifyPropertyCommand::redo() {
    // On first redo, changes are already applied
    // On subsequent redos, apply the new state
    if (!mFirstRedo) {
        // TODO: Implement property update from XML snapshot
        qWarning() << "ModifyPropertyCommand::redo() - Property update not yet implemented";
    }
    mFirstRedo = false;
}

QString ModifyPropertyCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    return QObject::tr("Modify %1 \"%2\"").arg(typeName, mItemName);
}

// =============================================================================
// EditorUndoSystem implementation
// =============================================================================

EditorUndoSystem::EditorUndoSystem(Host* host, QObject* parent)
: QObject(parent)
, mpHost(host)
{
}

EditorUndoSystem::~EditorUndoSystem()
{
    clear();
}

void EditorUndoSystem::pushCommand(std::unique_ptr<EditorCommand> cmd)
{
    if (!cmd) {
        return;
    }

    mUndoStack.push_back(std::move(cmd));
    clearRedoStack();
    enforceUndoLimit();
    emitSignals();
}

void EditorUndoSystem::undo()
{
    if (mUndoStack.empty()) {
        return;
    }

    auto cmd = std::move(mUndoStack.back());
    mUndoStack.pop_back();

    cmd->undo();

    mRedoStack.push_back(std::move(cmd));
    emitSignals();
}

void EditorUndoSystem::redo()
{
    if (mRedoStack.empty()) {
        return;
    }

    auto cmd = std::move(mRedoStack.back());
    mRedoStack.pop_back();

    cmd->redo();

    mUndoStack.push_back(std::move(cmd));
    emitSignals();
}

QString EditorUndoSystem::undoText() const
{
    if (mUndoStack.empty()) {
        return QObject::tr("Nothing to undo");
    }
    return mUndoStack.back()->text();
}

QString EditorUndoSystem::redoText() const
{
    if (mRedoStack.empty()) {
        return QObject::tr("Nothing to redo");
    }
    return mRedoStack.back()->text();
}

void EditorUndoSystem::clear()
{
    mUndoStack.clear();
    mRedoStack.clear();
    emitSignals();
}

void EditorUndoSystem::setUndoLimit(int limit)
{
    mUndoLimit = limit;
    enforceUndoLimit();
}

void EditorUndoSystem::clearRedoStack()
{
    mRedoStack.clear();
}

void EditorUndoSystem::enforceUndoLimit()
{
    while (static_cast<int>(mUndoStack.size()) > mUndoLimit) {
        mUndoStack.erase(mUndoStack.begin());
    }
}

void EditorUndoSystem::emitSignals()
{
    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
    emit undoTextChanged(undoText());
    emit redoTextChanged(redoText());
}
