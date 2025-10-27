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

// Import a single trigger from XML string
TTrigger* importTriggerFromXML(const QString& xmlSnapshot, TTrigger* pParent, Host* host) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importTriggerFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("TriggerSnapshot");
    if (!root) {
        qWarning() << "importTriggerFromXML: No TriggerSnapshot root element found";
        return nullptr;
    }

    // Find the Trigger or TriggerGroup element
    auto triggerNode = root.child("Trigger");
    if (!triggerNode) {
        triggerNode = root.child("TriggerGroup");
    }
    if (!triggerNode) {
        qWarning() << "importTriggerFromXML: No Trigger/TriggerGroup element found";
        return nullptr;
    }

    // Create new trigger
    auto pT = new TTrigger(pParent, host);
    host->getTriggerUnit()->registerTrigger(pT);

    // Read attributes
    pT->setIsActive(QString::fromStdString(triggerNode.attribute("isActive").value()) == "yes");
    pT->setIsFolder(QString::fromStdString(triggerNode.attribute("isFolder").value()) == "yes");
    pT->setTemporary(QString::fromStdString(triggerNode.attribute("isTempTrigger").value()) == "yes");
    pT->setIsMultiline(QString::fromStdString(triggerNode.attribute("isMultiline").value()) == "yes");
    pT->mPerlSlashGOption = QString::fromStdString(triggerNode.attribute("isPerlSlashGOption").value()) == "yes";
    pT->setIsColorizerTrigger(QString::fromStdString(triggerNode.attribute("isColorizerTrigger").value()) == "yes");
    pT->mFilterTrigger = QString::fromStdString(triggerNode.attribute("isFilterTrigger").value()) == "yes";
    pT->mSoundTrigger = QString::fromStdString(triggerNode.attribute("isSoundTrigger").value()) == "yes";
    pT->mColorTrigger = QString::fromStdString(triggerNode.attribute("isColorTrigger").value()) == "yes";

    // Temporary storage for pattern data
    QStringList patterns;
    QList<int> patternKinds;

    // Read child elements
    for (auto node : triggerNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pT->setName(nodeValue);
        } else if (nodeName == "script") {
            pT->setScript(nodeValue);
        } else if (nodeName == "packageName") {
            pT->mPackageName = nodeValue;
        } else if (nodeName == "triggerType") {
            pT->setTriggerType(nodeValue.toInt());
        } else if (nodeName == "conditonLineDelta") {
            pT->setConditionLineDelta(nodeValue.toInt());
        } else if (nodeName == "mStayOpen") {
            pT->mStayOpen = nodeValue.toInt();
        } else if (nodeName == "mCommand") {
            pT->setCommand(nodeValue);
        } else if (nodeName == "mFgColor") {
            pT->setColorizerFgColor(QColor::fromString(nodeValue));
        } else if (nodeName == "mBgColor") {
            pT->setColorizerBgColor(QColor::fromString(nodeValue));
        } else if (nodeName == "colorTriggerFgColor") {
            pT->mColorTriggerFgColor = QColor::fromString(nodeValue);
        } else if (nodeName == "colorTriggerBgColor") {
            pT->mColorTriggerBgColor = QColor::fromString(nodeValue);
        } else if (nodeName == "mSoundFile") {
            pT->setSound(nodeValue);
        } else if (nodeName == "regexCodeList") {
            // Read pattern list
            for (auto patternNode : node.children("string")) {
                patterns << QString::fromStdString(patternNode.child_value());
            }
        } else if (nodeName == "regexCodePropertyList") {
            // Read pattern property list
            for (auto propertyNode : node.children("integer")) {
                patternKinds << QString::fromStdString(propertyNode.child_value()).toInt();
            }
        }
    }

    // Set the regex patterns
    if (!patterns.isEmpty()) {
        pT->setRegexCodeList(patterns, patternKinds);
    }

    // Compile and validate the trigger to ensure error states are computed
    pT->compileAll();

    return pT;
}

// Update an existing trigger from XML string
bool updateTriggerFromXML(TTrigger* pT, const QString& xmlSnapshot) {
    if (!pT || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateTriggerFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("TriggerSnapshot");
    if (!root) {
        return false;
    }

    auto triggerNode = root.child("Trigger");
    if (!triggerNode) {
        triggerNode = root.child("TriggerGroup");
    }
    if (!triggerNode) {
        return false;
    }

    // Update attributes
    pT->setIsActive(QString::fromStdString(triggerNode.attribute("isActive").value()) == "yes");
    pT->setIsFolder(QString::fromStdString(triggerNode.attribute("isFolder").value()) == "yes");
    pT->setTemporary(QString::fromStdString(triggerNode.attribute("isTempTrigger").value()) == "yes");
    pT->setIsMultiline(QString::fromStdString(triggerNode.attribute("isMultiline").value()) == "yes");
    pT->mPerlSlashGOption = QString::fromStdString(triggerNode.attribute("isPerlSlashGOption").value()) == "yes";
    pT->setIsColorizerTrigger(QString::fromStdString(triggerNode.attribute("isColorizerTrigger").value()) == "yes");
    pT->mFilterTrigger = QString::fromStdString(triggerNode.attribute("isFilterTrigger").value()) == "yes";
    pT->mSoundTrigger = QString::fromStdString(triggerNode.attribute("isSoundTrigger").value()) == "yes";
    pT->mColorTrigger = QString::fromStdString(triggerNode.attribute("isColorTrigger").value()) == "yes";

    // Temporary storage for pattern data
    QStringList patterns;
    QList<int> patternKinds;

    // Update child elements
    for (auto node : triggerNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pT->setName(nodeValue);
        } else if (nodeName == "script") {
            pT->setScript(nodeValue);
        } else if (nodeName == "packageName") {
            pT->mPackageName = nodeValue;
        } else if (nodeName == "triggerType") {
            pT->setTriggerType(nodeValue.toInt());
        } else if (nodeName == "conditonLineDelta") {
            pT->setConditionLineDelta(nodeValue.toInt());
        } else if (nodeName == "mStayOpen") {
            pT->mStayOpen = nodeValue.toInt();
        } else if (nodeName == "mCommand") {
            pT->setCommand(nodeValue);
        } else if (nodeName == "mFgColor") {
            pT->setColorizerFgColor(QColor::fromString(nodeValue));
        } else if (nodeName == "mBgColor") {
            pT->setColorizerBgColor(QColor::fromString(nodeValue));
        } else if (nodeName == "colorTriggerFgColor") {
            pT->mColorTriggerFgColor = QColor::fromString(nodeValue);
        } else if (nodeName == "colorTriggerBgColor") {
            pT->mColorTriggerBgColor = QColor::fromString(nodeValue);
        } else if (nodeName == "mSoundFile") {
            pT->setSound(nodeValue);
        } else if (nodeName == "regexCodeList") {
            for (auto patternNode : node.children("string")) {
                patterns << QString::fromStdString(patternNode.child_value());
            }
        } else if (nodeName == "regexCodePropertyList") {
            for (auto propertyNode : node.children("integer")) {
                patternKinds << QString::fromStdString(propertyNode.child_value()).toInt();
            }
        }
    }

    // Update the regex patterns
    if (!patterns.isEmpty()) {
        pT->setRegexCodeList(patterns, patternKinds);
    }

    // Compile and validate the trigger to ensure error states are computed
    pT->compileAll();

    return true;
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
    // Recreate the item from XML snapshot
    // Note: When the command is first created, the item already exists (it was just added),
    // but we never call redo() at that point. The first time redo() is called is after
    // undo() has deleted the item, so we always need to recreate it.
    if (!mItemSnapshot.isEmpty()) {
        // Track old ID for remapping purposes
        mOldItemID = mItemID;
        qDebug() << "[AddItemCommand::redo] Recreating item, oldID:" << mOldItemID;

        // Get parent trigger
        TTrigger* pParent = nullptr;
        if (mParentID != -1) {
            pParent = mpHost->getTriggerUnit()->getTrigger(mParentID);
        }

        // Recreate the trigger from XML snapshot
        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            TTrigger* pNewTrigger = importTriggerFromXML(mItemSnapshot, pParent, mpHost);
            if (pNewTrigger) {
                // Store the new ID (it may be different from original)
                mItemID = pNewTrigger->getID();
                qDebug() << "[AddItemCommand::redo] Item recreated, newID:" << mItemID;
                if (mOldItemID != mItemID) {
                    qDebug() << "[AddItemCommand::redo] ID CHANGED! oldID:" << mOldItemID << "-> newID:" << mItemID;
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate trigger from snapshot";
            }
            break;
        }
        // TODO: Implement for other item types (aliases, timers, etc.)
        default:
            qWarning() << "AddItemCommand::redo() - Not yet implemented for this item type";
            break;
        }
    }
}

void AddItemCommand::remapItemID(int oldID, int newID) {
    qDebug() << "[AddItemCommand::remapItemID] Called with oldID:" << oldID << "newID:" << newID << "| my mItemID:" << mItemID << "mParentID:" << mParentID;
    if (mItemID == oldID) {
        qDebug() << "[AddItemCommand::remapItemID] Remapping mItemID from" << oldID << "to" << newID;
        mItemID = newID;
    }
    if (mParentID == oldID) {
        qDebug() << "[AddItemCommand::remapItemID] Remapping mParentID from" << oldID << "to" << newID;
        mParentID = newID;
    }
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
    // Process in reverse order to maintain tree structure (parents before children)
    for (int i = mDeletedItems.size() - 1; i >= 0; --i) {
        const auto& info = mDeletedItems[i];

        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            // Get parent trigger
            TTrigger* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getTriggerUnit()->getTrigger(info.parentID);
            }

            // Restore the trigger from XML snapshot
            TTrigger* pRestoredTrigger = importTriggerFromXML(info.xmlSnapshot, pParent, mpHost);
            if (!pRestoredTrigger) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore trigger" << info.itemName;
            }
            break;
        }
        // TODO: Implement for other item types (aliases, timers, etc.)
        default:
            qWarning() << "DeleteItemCommand::undo() - Not yet implemented for this item type";
            break;
        }
    }
}

void DeleteItemCommand::redo() {
    // Delete items again
    // Note: When the command is first created, items are already deleted,
    // but we never call redo() at that point. The first time redo() is called is after
    // undo() has restored the items, so we need to delete them again.
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

QString DeleteItemCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    if (mDeletedItems.size() == 1) {
        return QObject::tr("Delete %1 \"%2\"").arg(typeName, mDeletedItems.first().itemName);
    } else {
        return QObject::tr("Delete %1 %2(s)").arg(QString::number(mDeletedItems.size()), typeName);
    }
}

QList<int> DeleteItemCommand::affectedItemIDs() const {
    QList<int> ids;
    for (const auto& info : mDeletedItems) {
        ids.append(info.itemID);
    }
    return ids;
}

void DeleteItemCommand::remapItemID(int oldID, int newID) {
    qDebug() << "[DeleteItemCommand::remapItemID] Called with oldID:" << oldID << "newID:" << newID;
    for (auto& info : mDeletedItems) {
        if (info.itemID == oldID) {
            qDebug() << "[DeleteItemCommand::remapItemID] Remapping deleted item ID from" << oldID << "to" << newID;
            info.itemID = newID;
        }
        if (info.parentID == oldID) {
            qDebug() << "[DeleteItemCommand::remapItemID] Remapping deleted item parent ID from" << oldID << "to" << newID;
            info.parentID = newID;
        }
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
    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* pTrigger = mpHost->getTriggerUnit()->getTrigger(mItemID);
        if (pTrigger) {
            if (!updateTriggerFromXML(pTrigger, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore trigger" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Trigger" << mItemName << "not found";
        }
        break;
    }
    // TODO: Implement for other item types (aliases, timers, etc.)
    default:
        qWarning() << "ModifyPropertyCommand::undo() - Not yet implemented for this item type";
        break;
    }
}

void ModifyPropertyCommand::redo() {
    // Apply the new state
    // Note: When the command is first created, changes are already applied,
    // but we never call redo() at that point. The first time redo() is called is after
    // undo() has restored the old state, so we need to apply the new state again.
    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* pTrigger = mpHost->getTriggerUnit()->getTrigger(mItemID);
        if (pTrigger) {
            if (!updateTriggerFromXML(pTrigger, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to trigger" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Trigger" << mItemName << "not found";
        }
        break;
    }
    // TODO: Implement for other item types (aliases, timers, etc.)
    default:
        qWarning() << "ModifyPropertyCommand::redo() - Not yet implemented for this item type";
        break;
    }
}

QString ModifyPropertyCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    return QObject::tr("Modify %1 \"%2\"").arg(typeName, mItemName);
}

void ModifyPropertyCommand::remapItemID(int oldID, int newID) {
    qDebug() << "[ModifyPropertyCommand::remapItemID] Called with oldID:" << oldID << "newID:" << newID << "| my mItemID:" << mItemID;
    if (mItemID == oldID) {
        qDebug() << "[ModifyPropertyCommand::remapItemID] Remapping mItemID from" << oldID << "to" << newID;
        mItemID = newID;
    }
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

    EditorViewType affectedView = cmd->viewType();
    QList<int> affectedIDs = cmd->affectedItemIDs();
    cmd->undo();

    mRedoStack.push_back(std::move(cmd));
    emitSignals();
    emit itemsChanged(affectedView, affectedIDs);
}

void EditorUndoSystem::redo()
{
    if (mRedoStack.empty()) {
        return;
    }

    auto cmd = std::move(mRedoStack.back());
    mRedoStack.pop_back();

    EditorViewType affectedView = cmd->viewType();
    QList<int> affectedIDs = cmd->affectedItemIDs();
    cmd->redo();

    // Check if this was an AddItemCommand that changed the item ID
    // This happens when an item is deleted then recreated - the ID may change
    if (auto* addCmd = dynamic_cast<AddItemCommand*>(cmd.get())) {
        if (addCmd->didItemIDChange()) {
            int oldID = addCmd->getOldItemID();
            int newID = addCmd->getNewItemID();
            qDebug() << "[EditorUndoSystem::redo] AddItemCommand changed ID, remapping from" << oldID << "to" << newID;
            remapItemIDs(oldID, newID);
        }
    }

    mUndoStack.push_back(std::move(cmd));
    emitSignals();
    emit itemsChanged(affectedView, affectedIDs);
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

void EditorUndoSystem::remapItemIDs(int oldID, int newID)
{
    qDebug() << "[EditorUndoSystem::remapItemIDs] Remapping all commands from oldID:" << oldID << "to newID:" << newID;
    qDebug() << "[EditorUndoSystem::remapItemIDs] Undo stack size:" << mUndoStack.size() << "Redo stack size:" << mRedoStack.size();

    // Update all commands in undo stack
    int undoCount = 0;
    for (auto& cmd : mUndoStack) {
        qDebug() << "[EditorUndoSystem::remapItemIDs] Remapping command in undo stack #" << undoCount++;
        cmd->remapItemID(oldID, newID);
    }

    // Update all commands in redo stack
    int redoCount = 0;
    for (auto& cmd : mRedoStack) {
        qDebug() << "[EditorUndoSystem::remapItemIDs] Remapping command in redo stack #" << redoCount++;
        cmd->remapItemID(oldID, newID);
    }

    qDebug() << "[EditorUndoSystem::remapItemIDs] Remapping complete";
}
