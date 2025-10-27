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
TTrigger* importTriggerFromXML(const QString& xmlSnapshot, TTrigger* pParent, Host* host, int position = -1) {
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

    // Create new trigger without parent (so it doesn't auto-add to end)
    auto pT = new TTrigger(nullptr, host);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importTriggerFromXML: Adding trigger to parent at position" << position;
        pT->setParent(pParent);
        // Use explicit enum mode for clarity
        pParent->addChild(pT, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);
        host->getTriggerUnit()->registerTrigger(pT); // This will call addTrigger() since pT has a parent

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pT) {
                qDebug() << "importTriggerFromXML: Trigger actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    } else {
        // Root level trigger - register first (adds to end of root list)
        qDebug() << "importTriggerFromXML: Adding trigger to root at position" << position;
        host->getTriggerUnit()->registerTrigger(pT);

        // Now reposition it if a specific position was requested
        auto rootListSize = host->getTriggerUnit()->getTriggerRootNodeList().size();
        qDebug() << "importTriggerFromXML: Root list size after register:" << rootListSize;

        if (position != -1 && position < rootListSize) {
            // Use reParentTrigger with explicit AtPosition mode to insert at specific position
            qDebug() << "importTriggerFromXML: Repositioning from end to position" << position;
            host->getTriggerUnit()->reParentTrigger(pT->getID(), -1, -1, TriggerInsertMode::AtPosition, position);

            // Verify final position in root list
            auto finalRootList = host->getTriggerUnit()->getTriggerRootNodeList();
            qDebug() << "importTriggerFromXML: After reposition, root list has" << finalRootList.size() << "items:";
            int pos = 0;
            for (auto* trigger : finalRootList) {
                qDebug() << "  Position" << pos << ":" << (trigger ? QString("ID=%1").arg(trigger->getID()) : "NULL");
                if (trigger && trigger->getID() == pT->getID()) {
                    qDebug() << "    ^^^ This is the trigger we just added! (ID" << pT->getID() << ")";
                }
                pos++;
            }
        } else if (position >= rootListSize) {
            qDebug() << "importTriggerFromXML: Position" << position << "is at or beyond list size" << rootListSize << "- leaving at end";
        }
    }

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

    // Recursively import child triggers
    for (auto childNode : triggerNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Trigger" || childNodeName == "TriggerGroup") {
            // Create XML snapshot for the child
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("TriggerSnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            // Recursively import the child with current trigger as parent (position -1 = append to end)
            importTriggerFromXML(childXML, pT, host, -1);
        }
    }

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

// =============================================================================
// ALIAS import/update functions
// =============================================================================

// Import a single alias from XML string
TAlias* importAliasFromXML(const QString& xmlSnapshot, TAlias* pParent, Host* host, int position = -1) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importAliasFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("AliasSnapshot");
    if (!root) {
        qWarning() << "importAliasFromXML: No AliasSnapshot root element found";
        return nullptr;
    }

    auto aliasNode = root.child("Alias");
    if (!aliasNode) {
        aliasNode = root.child("AliasGroup");
    }
    if (!aliasNode) {
        qWarning() << "importAliasFromXML: No Alias/AliasGroup element found";
        return nullptr;
    }

    // Create new alias without parent (so it doesn't auto-add to end)
    auto pA = new TAlias(nullptr, host);
    host->getAliasUnit()->registerAlias(pA);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importAliasFromXML: Adding alias to parent at position" << position;
        pA->setParent(pParent);
        pParent->addChild(pA, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pA) {
                qDebug() << "importAliasFromXML: Alias actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    }

    // Read attributes
    pA->setIsActive(QString::fromStdString(aliasNode.attribute("isActive").value()) == "yes");
    pA->setIsFolder(QString::fromStdString(aliasNode.attribute("isFolder").value()) == "yes");
    pA->setTemporary(QString::fromStdString(aliasNode.attribute("isTempAlias").value()) == "yes");

    // Read child elements
    for (auto node : aliasNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pA->setName(nodeValue);
        } else if (nodeName == "script") {
            pA->setScript(nodeValue);
        } else if (nodeName == "packageName") {
            pA->mPackageName = nodeValue;
        } else if (nodeName == "regex") {
            pA->setRegexCode(nodeValue);
        } else if (nodeName == "command") {
            pA->setCommand(nodeValue);
        }
    }

    // Compile the alias
    pA->compileAll();

    // Recursively import child aliases
    for (auto childNode : aliasNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Alias" || childNodeName == "AliasGroup") {
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("AliasSnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            importAliasFromXML(childXML, pA, host);
        }
    }

    return pA;
}

// Update an existing alias from XML string
bool updateAliasFromXML(TAlias* pA, const QString& xmlSnapshot) {
    if (!pA || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateAliasFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("AliasSnapshot");
    if (!root) {
        return false;
    }

    auto aliasNode = root.child("Alias");
    if (!aliasNode) {
        aliasNode = root.child("AliasGroup");
    }
    if (!aliasNode) {
        return false;
    }

    // Update attributes
    pA->setIsActive(QString::fromStdString(aliasNode.attribute("isActive").value()) == "yes");
    pA->setIsFolder(QString::fromStdString(aliasNode.attribute("isFolder").value()) == "yes");
    pA->setTemporary(QString::fromStdString(aliasNode.attribute("isTempAlias").value()) == "yes");

    // Update child elements
    for (auto node : aliasNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pA->setName(nodeValue);
        } else if (nodeName == "script") {
            pA->setScript(nodeValue);
        } else if (nodeName == "regex") {
            pA->setRegexCode(nodeValue);
        } else if (nodeName == "command") {
            pA->setCommand(nodeValue);
        } else if (nodeName == "packageName") {
            pA->mPackageName = nodeValue;
        }
    }

    // Compile the alias
    pA->compileAll();

    return true;
}

// =============================================================================
// TIMER import/update functions
// =============================================================================

// Import a single timer from XML string
TTimer* importTimerFromXML(const QString& xmlSnapshot, TTimer* pParent, Host* host, int position = -1) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importTimerFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("TimerSnapshot");
    if (!root) {
        qWarning() << "importTimerFromXML: No TimerSnapshot root element found";
        return nullptr;
    }

    auto timerNode = root.child("Timer");
    if (!timerNode) {
        timerNode = root.child("TimerGroup");
    }
    if (!timerNode) {
        qWarning() << "importTimerFromXML: No Timer/TimerGroup element found";
        return nullptr;
    }

    // Create new timer without parent (so it doesn't auto-add to end)
    auto pT = new TTimer(nullptr, host);
    host->getTimerUnit()->registerTimer(pT);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importTimerFromXML: Adding timer to parent at position" << position;
        pT->setParent(pParent);
        pParent->addChild(pT, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pT) {
                qDebug() << "importTimerFromXML: Timer actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    }

    // Read attributes
    pT->setShouldBeActive(QString::fromStdString(timerNode.attribute("isActive").value()) == "yes");
    pT->setIsFolder(QString::fromStdString(timerNode.attribute("isFolder").value()) == "yes");
    pT->setTemporary(QString::fromStdString(timerNode.attribute("isTempTimer").value()) == "yes");

    // Read child elements
    for (auto node : timerNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pT->setName(nodeValue);
        } else if (nodeName == "script") {
            pT->setScript(nodeValue);
        } else if (nodeName == "packageName") {
            pT->mPackageName = nodeValue;
        } else if (nodeName == "command") {
            pT->setCommand(nodeValue);
        } else if (nodeName == "time") {
            pT->setTime(QTime::fromString(nodeValue, "hh:mm:ss.zzz"));
        }
    }

    // Compile the timer
    pT->compileAll();

    // Recursively import child timers
    for (auto childNode : timerNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Timer" || childNodeName == "TimerGroup") {
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("TimerSnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            importTimerFromXML(childXML, pT, host);
        }
    }

    return pT;
}

// Update an existing timer from XML string
bool updateTimerFromXML(TTimer* pT, const QString& xmlSnapshot) {
    if (!pT || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateTimerFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("TimerSnapshot");
    if (!root) {
        return false;
    }

    auto timerNode = root.child("Timer");
    if (!timerNode) {
        timerNode = root.child("TimerGroup");
    }
    if (!timerNode) {
        return false;
    }

    // Update attributes
    pT->setShouldBeActive(QString::fromStdString(timerNode.attribute("isActive").value()) == "yes");
    pT->setIsFolder(QString::fromStdString(timerNode.attribute("isFolder").value()) == "yes");
    pT->setTemporary(QString::fromStdString(timerNode.attribute("isTempTimer").value()) == "yes");

    // Update child elements
    for (auto node : timerNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pT->setName(nodeValue);
        } else if (nodeName == "script") {
            pT->setScript(nodeValue);
        } else if (nodeName == "command") {
            pT->setCommand(nodeValue);
        } else if (nodeName == "time") {
            pT->setTime(QTime::fromString(nodeValue, "hh:mm:ss.zzz"));
        } else if (nodeName == "packageName") {
            pT->mPackageName = nodeValue;
        }
    }

    // Compile the timer
    pT->compileAll();

    return true;
}

// =============================================================================
// SCRIPT import/update functions
// =============================================================================

// Import a single script from XML string
TScript* importScriptFromXML(const QString& xmlSnapshot, TScript* pParent, Host* host, int position = -1) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importScriptFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("ScriptSnapshot");
    if (!root) {
        qWarning() << "importScriptFromXML: No ScriptSnapshot root element found";
        return nullptr;
    }

    auto scriptNode = root.child("Script");
    if (!scriptNode) {
        scriptNode = root.child("ScriptGroup");
    }
    if (!scriptNode) {
        qWarning() << "importScriptFromXML: No Script/ScriptGroup element found";
        return nullptr;
    }

    // Create new script without parent (so it doesn't auto-add to end)
    auto pS = new TScript(nullptr, host);
    host->getScriptUnit()->registerScript(pS);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importScriptFromXML: Adding script to parent at position" << position;
        pS->setParent(pParent);
        pParent->addChild(pS, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pS) {
                qDebug() << "importScriptFromXML: Script actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    }

    // Read attributes
    pS->setIsActive(QString::fromStdString(scriptNode.attribute("isActive").value()) == "yes");
    pS->setIsFolder(QString::fromStdString(scriptNode.attribute("isFolder").value()) == "yes");

    // Read child elements
    QStringList eventHandlers;
    for (auto node : scriptNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pS->setName(nodeValue);
        } else if (nodeName == "packageName") {
            pS->mPackageName = nodeValue;
        } else if (nodeName == "script") {
            pS->setScript(nodeValue);
        } else if (nodeName == "eventHandlerList") {
            for (auto eventNode : node.children("string")) {
                eventHandlers << QString::fromStdString(eventNode.child_value());
            }
        }
    }

    // Set event handlers
    if (!eventHandlers.isEmpty()) {
        pS->setEventHandlerList(eventHandlers);
    }

    // Compile the script
    pS->compileAll();

    // Recursively import child scripts
    for (auto childNode : scriptNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Script" || childNodeName == "ScriptGroup") {
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("ScriptSnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            importScriptFromXML(childXML, pS, host);
        }
    }

    return pS;
}

// Update an existing script from XML string
bool updateScriptFromXML(TScript* pS, const QString& xmlSnapshot) {
    if (!pS || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateScriptFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("ScriptSnapshot");
    if (!root) {
        return false;
    }

    auto scriptNode = root.child("Script");
    if (!scriptNode) {
        scriptNode = root.child("ScriptGroup");
    }
    if (!scriptNode) {
        return false;
    }

    // Update attributes
    pS->setIsActive(QString::fromStdString(scriptNode.attribute("isActive").value()) == "yes");
    pS->setIsFolder(QString::fromStdString(scriptNode.attribute("isFolder").value()) == "yes");

    // Update child elements
    QStringList eventHandlers;
    for (auto node : scriptNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pS->setName(nodeValue);
        } else if (nodeName == "script") {
            pS->setScript(nodeValue);
        } else if (nodeName == "packageName") {
            pS->mPackageName = nodeValue;
        } else if (nodeName == "eventHandlerList") {
            for (auto eventNode : node.children("string")) {
                eventHandlers << QString::fromStdString(eventNode.child_value());
            }
        }
    }

    // Set event handlers
    if (!eventHandlers.isEmpty()) {
        pS->setEventHandlerList(eventHandlers);
    }

    // Compile the script
    pS->compileAll();

    return true;
}

// =============================================================================
// KEY import/update functions
// =============================================================================

// Import a single key from XML string
TKey* importKeyFromXML(const QString& xmlSnapshot, TKey* pParent, Host* host, int position = -1) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importKeyFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("KeySnapshot");
    if (!root) {
        qWarning() << "importKeyFromXML: No KeySnapshot root element found";
        return nullptr;
    }

    auto keyNode = root.child("Key");
    if (!keyNode) {
        keyNode = root.child("KeyGroup");
    }
    if (!keyNode) {
        qWarning() << "importKeyFromXML: No Key/KeyGroup element found";
        return nullptr;
    }

    // Create new key without parent (so it doesn't auto-add to end)
    auto pK = new TKey(nullptr, host);
    host->getKeyUnit()->registerKey(pK);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importKeyFromXML: Adding key to parent at position" << position;
        pK->setParent(pParent);
        pParent->addChild(pK, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pK) {
                qDebug() << "importKeyFromXML: Key actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    }

    // Read attributes
    pK->setIsActive(QString::fromStdString(keyNode.attribute("isActive").value()) == "yes");
    pK->setIsFolder(QString::fromStdString(keyNode.attribute("isFolder").value()) == "yes");

    // Read child elements
    for (auto node : keyNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pK->setName(nodeValue);
        } else if (nodeName == "packageName") {
            pK->mPackageName = nodeValue;
        } else if (nodeName == "script") {
            pK->setScript(nodeValue);
        } else if (nodeName == "command") {
            pK->setCommand(nodeValue);
        } else if (nodeName == "keyCode") {
            pK->setKeyCode(nodeValue.toInt());
        } else if (nodeName == "keyModifier") {
            pK->setKeyModifiers(nodeValue.toInt());
        }
    }

    // Compile the key
    pK->compileAll();

    // Recursively import child keys
    for (auto childNode : keyNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Key" || childNodeName == "KeyGroup") {
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("KeySnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            importKeyFromXML(childXML, pK, host);
        }
    }

    return pK;
}

// Update an existing key from XML string
bool updateKeyFromXML(TKey* pK, const QString& xmlSnapshot) {
    if (!pK || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateKeyFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("KeySnapshot");
    if (!root) {
        return false;
    }

    auto keyNode = root.child("Key");
    if (!keyNode) {
        keyNode = root.child("KeyGroup");
    }
    if (!keyNode) {
        return false;
    }

    // Update attributes
    pK->setIsActive(QString::fromStdString(keyNode.attribute("isActive").value()) == "yes");
    pK->setIsFolder(QString::fromStdString(keyNode.attribute("isFolder").value()) == "yes");

    // Update child elements
    for (auto node : keyNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pK->setName(nodeValue);
        } else if (nodeName == "script") {
            pK->setScript(nodeValue);
        } else if (nodeName == "command") {
            pK->setCommand(nodeValue);
        } else if (nodeName == "keyCode") {
            pK->setKeyCode(nodeValue.toInt());
        } else if (nodeName == "keyModifier") {
            pK->setKeyModifiers(nodeValue.toInt());
        } else if (nodeName == "packageName") {
            pK->mPackageName = nodeValue;
        }
    }

    // Compile the key
    pK->compileAll();

    return true;
}

// =============================================================================
// ACTION import/update functions
// =============================================================================

// Import a single action from XML string
TAction* importActionFromXML(const QString& xmlSnapshot, TAction* pParent, Host* host, int position = -1) {
    if (xmlSnapshot.isEmpty() || !host) {
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "importActionFromXML: Failed to parse XML:" << result.description();
        return nullptr;
    }

    auto root = doc.child("ActionSnapshot");
    if (!root) {
        qWarning() << "importActionFromXML: No ActionSnapshot root element found";
        return nullptr;
    }

    auto actionNode = root.child("Action");
    if (!actionNode) {
        actionNode = root.child("ActionGroup");
    }
    if (!actionNode) {
        qWarning() << "importActionFromXML: No Action/ActionGroup element found";
        return nullptr;
    }

    // Create new action without parent (so it doesn't auto-add to end)
    auto pA = new TAction(nullptr, host);
    host->getActionUnit()->registerAction(pA);

    // Manually add to parent at the correct position
    if (pParent) {
        qDebug() << "importActionFromXML: Adding action to parent at position" << position;
        pA->Tree<TAction>::setParent(pParent);
        pParent->addChild(pA, (position >= 0) ? TreeInsertMode::AtPosition : TreeInsertMode::Append, position);

        // Verify position
        auto children = pParent->getChildrenList();
        int actualPos = 0;
        for (auto* child : *children) {
            if (child == pA) {
                qDebug() << "importActionFromXML: Action actual position is" << actualPos << "in parent with" << children->size() << "children";
                break;
            }
            actualPos++;
        }
    }

    // Read attributes
    pA->setIsActive(QString::fromStdString(actionNode.attribute("isActive").value()) == "yes");
    pA->setIsFolder(QString::fromStdString(actionNode.attribute("isFolder").value()) == "yes");
    pA->mIsPushDownButton = QString::fromStdString(actionNode.attribute("isPushButton").value()) == "yes";
    pA->mButtonFlat = QString::fromStdString(actionNode.attribute("isFlatButton").value()) == "yes";
    pA->mUseCustomLayout = QString::fromStdString(actionNode.attribute("useCustomLayout").value()) == "yes";

    // Read child elements
    for (auto node : actionNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pA->setName(nodeValue);
        } else if (nodeName == "packageName") {
            pA->mPackageName = nodeValue;
        } else if (nodeName == "script") {
            pA->setScript(nodeValue);
        } else if (nodeName == "css") {
            pA->css = nodeValue;
        } else if (nodeName == "commandButtonUp") {
            pA->setCommandButtonUp(nodeValue);
        } else if (nodeName == "commandButtonDown") {
            pA->setCommandButtonDown(nodeValue);
        } else if (nodeName == "icon") {
            pA->setIcon(nodeValue);
        } else if (nodeName == "orientation") {
            pA->mOrientation = nodeValue.toInt();
        } else if (nodeName == "location") {
            pA->mLocation = nodeValue.toInt();
        } else if (nodeName == "buttonRotation") {
            pA->mButtonRotation = nodeValue.toInt();
        } else if (nodeName == "sizeX") {
            pA->mSizeX = nodeValue.toInt();
        } else if (nodeName == "sizeY") {
            pA->mSizeY = nodeValue.toInt();
        } else if (nodeName == "mButtonColumns") {
            pA->mButtonColumns = nodeValue.toInt();
        } else if (nodeName == "buttonColor") {
            // Deprecated - skip this element
        } else if (nodeName == "posX") {
            pA->mPosX = nodeValue.toInt();
        } else if (nodeName == "posY") {
            pA->mPosY = nodeValue.toInt();
        }
    }

    // Compile the action
    pA->compileAll();

    // Recursively import child actions
    for (auto childNode : actionNode.children()) {
        QString childNodeName = QString::fromStdString(childNode.name());
        if (childNodeName == "Action" || childNodeName == "ActionGroup") {
            pugi::xml_document childDoc;
            auto childRoot = childDoc.append_child("ActionSnapshot");
            childRoot.append_copy(childNode);

            std::ostringstream oss;
            childDoc.save(oss);
            QString childXML = QString::fromStdString(oss.str());

            importActionFromXML(childXML, pA, host);
        }
    }

    return pA;
}

// Update an existing action from XML string
bool updateActionFromXML(TAction* pA, const QString& xmlSnapshot) {
    if (!pA || xmlSnapshot.isEmpty()) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlSnapshot.toStdString().c_str());
    if (!result) {
        qWarning() << "updateActionFromXML: Failed to parse XML:" << result.description();
        return false;
    }

    auto root = doc.child("ActionSnapshot");
    if (!root) {
        return false;
    }

    auto actionNode = root.child("Action");
    if (!actionNode) {
        actionNode = root.child("ActionGroup");
    }
    if (!actionNode) {
        return false;
    }

    // Update attributes
    pA->setIsActive(QString::fromStdString(actionNode.attribute("isActive").value()) == "yes");
    pA->setIsFolder(QString::fromStdString(actionNode.attribute("isFolder").value()) == "yes");
    pA->mIsPushDownButton = QString::fromStdString(actionNode.attribute("isPushButton").value()) == "yes";
    pA->mButtonFlat = QString::fromStdString(actionNode.attribute("isFlatButton").value()) == "yes";
    pA->mUseCustomLayout = QString::fromStdString(actionNode.attribute("useCustomLayout").value()) == "yes";

    // Update child elements
    for (auto node : actionNode.children()) {
        QString nodeName = QString::fromStdString(node.name());
        QString nodeValue = QString::fromStdString(node.child_value());

        if (nodeName == "name") {
            pA->setName(nodeValue);
        } else if (nodeName == "script") {
            pA->setScript(nodeValue);
        } else if (nodeName == "css") {
            pA->css = nodeValue;
        } else if (nodeName == "commandButtonUp") {
            pA->setCommandButtonUp(nodeValue);
        } else if (nodeName == "commandButtonDown") {
            pA->setCommandButtonDown(nodeValue);
        } else if (nodeName == "icon") {
            pA->setIcon(nodeValue);
        } else if (nodeName == "orientation") {
            pA->mOrientation = nodeValue.toInt();
        } else if (nodeName == "location") {
            pA->mLocation = nodeValue.toInt();
        } else if (nodeName == "buttonRotation") {
            pA->mButtonRotation = nodeValue.toInt();
        } else if (nodeName == "sizeX") {
            pA->mSizeX = nodeValue.toInt();
        } else if (nodeName == "sizeY") {
            pA->mSizeY = nodeValue.toInt();
        } else if (nodeName == "mButtonColumns") {
            pA->mButtonColumns = nodeValue.toInt();
        } else if (nodeName == "buttonColor") {
            // Deprecated - skip this element
        } else if (nodeName == "posX") {
            pA->mPosX = nodeValue.toInt();
        } else if (nodeName == "posY") {
            pA->mPosY = nodeValue.toInt();
        } else if (nodeName == "packageName") {
            pA->mPackageName = nodeValue;
        }
    }

    // Compile the action
    pA->compileAll();

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

        // Get parent trigger
        TTrigger* pParent = nullptr;
        if (mParentID != -1) {
            pParent = mpHost->getTriggerUnit()->getTrigger(mParentID);
        }

        // Recreate the trigger from XML snapshot
        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            TTrigger* pNewTrigger = importTriggerFromXML(mItemSnapshot, pParent, mpHost, -1);
            if (pNewTrigger) {
                // Store the new ID (it may be different from original)
                mItemID = pNewTrigger->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate trigger from snapshot";
            }
            break;
        }
        case EditorViewType::cmAliasView: {
            TAlias* pAliasParent = nullptr;
            if (mParentID != -1) {
                pAliasParent = mpHost->getAliasUnit()->getAlias(mParentID);
            }
            TAlias* pNewAlias = importAliasFromXML(mItemSnapshot, pAliasParent, mpHost);
            if (pNewAlias) {
                mItemID = pNewAlias->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate alias from snapshot";
            }
            break;
        }
        case EditorViewType::cmTimerView: {
            TTimer* pTimerParent = nullptr;
            if (mParentID != -1) {
                pTimerParent = mpHost->getTimerUnit()->getTimer(mParentID);
            }
            TTimer* pNewTimer = importTimerFromXML(mItemSnapshot, pTimerParent, mpHost);
            if (pNewTimer) {
                mItemID = pNewTimer->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate timer from snapshot";
            }
            break;
        }
        case EditorViewType::cmScriptView: {
            TScript* pScriptParent = nullptr;
            if (mParentID != -1) {
                pScriptParent = mpHost->getScriptUnit()->getScript(mParentID);
            }
            TScript* pNewScript = importScriptFromXML(mItemSnapshot, pScriptParent, mpHost);
            if (pNewScript) {
                mItemID = pNewScript->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate script from snapshot";
            }
            break;
        }
        case EditorViewType::cmKeysView: {
            TKey* pKeyParent = nullptr;
            if (mParentID != -1) {
                pKeyParent = mpHost->getKeyUnit()->getKey(mParentID);
            }
            TKey* pNewKey = importKeyFromXML(mItemSnapshot, pKeyParent, mpHost);
            if (pNewKey) {
                mItemID = pNewKey->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate key from snapshot";
            }
            break;
        }
        case EditorViewType::cmActionView: {
            TAction* pActionParent = nullptr;
            if (mParentID != -1) {
                pActionParent = mpHost->getActionUnit()->getAction(mParentID);
            }
            TAction* pNewAction = importActionFromXML(mItemSnapshot, pActionParent, mpHost);
            if (pNewAction) {
                mItemID = pNewAction->getID();
                if (mOldItemID != mItemID) {
                }
            } else {
                qWarning() << "AddItemCommand::redo() - Failed to recreate action from snapshot";
            }
            break;
        }
        default:
            qWarning() << "AddItemCommand::redo() - Unknown item type";
            break;
        }
    }
}

void AddItemCommand::remapItemID(int oldID, int newID) {
    if (mItemID == oldID) {
        mItemID = newID;
    }
    if (mParentID == oldID) {
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
        auto& info = mDeletedItems[i];  // Non-const reference so we can update the ID

        qDebug() << "DeleteItemCommand::undo() - Restoring item" << info.itemName
                 << "at position" << info.positionInParent
                 << "in parent" << info.parentID;

        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            // Get parent trigger
            TTrigger* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getTriggerUnit()->getTrigger(info.parentID);
            }

            // Restore the trigger from XML snapshot at its original position
            TTrigger* pRestoredTrigger = importTriggerFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredTrigger) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore trigger" << info.itemName;
            } else {
                // Update the stored ID to the new ID so redo can find it
                info.itemID = pRestoredTrigger->getID();
            }
            break;
        }
        case EditorViewType::cmAliasView: {
            TAlias* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getAliasUnit()->getAlias(info.parentID);
            }

            TAlias* pRestoredAlias = importAliasFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredAlias) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore alias" << info.itemName;
            } else {
                info.itemID = pRestoredAlias->getID();
            }
            break;
        }
        case EditorViewType::cmTimerView: {
            TTimer* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getTimerUnit()->getTimer(info.parentID);
            }

            TTimer* pRestoredTimer = importTimerFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredTimer) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore timer" << info.itemName;
            } else {
                info.itemID = pRestoredTimer->getID();
            }
            break;
        }
        case EditorViewType::cmScriptView: {
            TScript* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getScriptUnit()->getScript(info.parentID);
            }

            TScript* pRestoredScript = importScriptFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredScript) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore script" << info.itemName;
            } else {
                info.itemID = pRestoredScript->getID();
            }
            break;
        }
        case EditorViewType::cmKeysView: {
            TKey* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getKeyUnit()->getKey(info.parentID);
            }

            TKey* pRestoredKey = importKeyFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredKey) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore key" << info.itemName;
            } else {
                info.itemID = pRestoredKey->getID();
            }
            break;
        }
        case EditorViewType::cmActionView: {
            TAction* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getActionUnit()->getAction(info.parentID);
            }

            TAction* pRestoredAction = importActionFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredAction) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore action" << info.itemName;
            } else {
                info.itemID = pRestoredAction->getID();
            }
            break;
        }
        default:
            qWarning() << "DeleteItemCommand::undo() - Unknown item type";
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
    for (auto& info : mDeletedItems) {
        if (info.itemID == oldID) {
            info.itemID = newID;
        }
        if (info.parentID == oldID) {
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
    case EditorViewType::cmAliasView: {
        TAlias* pAlias = mpHost->getAliasUnit()->getAlias(mItemID);
        if (pAlias) {
            if (!updateAliasFromXML(pAlias, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore alias" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Alias" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmTimerView: {
        TTimer* pTimer = mpHost->getTimerUnit()->getTimer(mItemID);
        if (pTimer) {
            if (!updateTimerFromXML(pTimer, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore timer" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Timer" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmScriptView: {
        TScript* pScript = mpHost->getScriptUnit()->getScript(mItemID);
        if (pScript) {
            if (!updateScriptFromXML(pScript, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore script" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Script" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmKeysView: {
        TKey* pKey = mpHost->getKeyUnit()->getKey(mItemID);
        if (pKey) {
            if (!updateKeyFromXML(pKey, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore key" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Key" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmActionView: {
        TAction* pAction = mpHost->getActionUnit()->getAction(mItemID);
        if (pAction) {
            if (!updateActionFromXML(pAction, mOldStateXML)) {
                qWarning() << "ModifyPropertyCommand::undo() - Failed to restore action" << mItemName << "to old state";
            }
        } else {
            qWarning() << "ModifyPropertyCommand::undo() - Action" << mItemName << "not found";
        }
        break;
    }
    default:
        qWarning() << "ModifyPropertyCommand::undo() - Unknown item type";
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
    case EditorViewType::cmAliasView: {
        TAlias* pAlias = mpHost->getAliasUnit()->getAlias(mItemID);
        if (pAlias) {
            if (!updateAliasFromXML(pAlias, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to alias" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Alias" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmTimerView: {
        TTimer* pTimer = mpHost->getTimerUnit()->getTimer(mItemID);
        if (pTimer) {
            if (!updateTimerFromXML(pTimer, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to timer" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Timer" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmScriptView: {
        TScript* pScript = mpHost->getScriptUnit()->getScript(mItemID);
        if (pScript) {
            if (!updateScriptFromXML(pScript, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to script" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Script" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmKeysView: {
        TKey* pKey = mpHost->getKeyUnit()->getKey(mItemID);
        if (pKey) {
            if (!updateKeyFromXML(pKey, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to key" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Key" << mItemName << "not found";
        }
        break;
    }
    case EditorViewType::cmActionView: {
        TAction* pAction = mpHost->getActionUnit()->getAction(mItemID);
        if (pAction) {
            if (!updateActionFromXML(pAction, mNewStateXML)) {
                qWarning() << "ModifyPropertyCommand::redo() - Failed to apply new state to action" << mItemName;
            }
        } else {
            qWarning() << "ModifyPropertyCommand::redo() - Action" << mItemName << "not found";
        }
        break;
    }
    default:
        qWarning() << "ModifyPropertyCommand::redo() - Unknown item type";
        break;
    }
}

QString ModifyPropertyCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    return QObject::tr("Modify %1 \"%2\"").arg(typeName, mItemName);
}

void ModifyPropertyCommand::remapItemID(int oldID, int newID) {
    if (mItemID == oldID) {
        mItemID = newID;
    }
}

// =============================================================================
// MoveItemCommand implementation
// =============================================================================

MoveItemCommand::MoveItemCommand(EditorViewType viewType, int itemID,
                                 int oldParentID, int newParentID,
                                 const QString& itemName,
                                 Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mOldParentID(oldParentID)
, mNewParentID(newParentID)
, mItemName(itemName)
{
}

void MoveItemCommand::undo() {

    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        mpHost->getTriggerUnit()->reParentTrigger(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    case EditorViewType::cmAliasView: {
        mpHost->getAliasUnit()->reParentAlias(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    case EditorViewType::cmTimerView: {
        mpHost->getTimerUnit()->reParentTimer(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    case EditorViewType::cmScriptView: {
        mpHost->getScriptUnit()->reParentScript(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    case EditorViewType::cmKeysView: {
        mpHost->getKeyUnit()->reParentKey(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    case EditorViewType::cmActionView: {
        mpHost->getActionUnit()->reParentAction(mItemID, mNewParentID, mOldParentID, -1, -1);
        break;
    }
    default:
        qWarning() << "MoveItemCommand::undo() - Unknown view type";
        break;
    }
}

void MoveItemCommand::redo() {

    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        mpHost->getTriggerUnit()->reParentTrigger(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    case EditorViewType::cmAliasView: {
        mpHost->getAliasUnit()->reParentAlias(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    case EditorViewType::cmTimerView: {
        mpHost->getTimerUnit()->reParentTimer(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    case EditorViewType::cmScriptView: {
        mpHost->getScriptUnit()->reParentScript(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    case EditorViewType::cmKeysView: {
        mpHost->getKeyUnit()->reParentKey(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    case EditorViewType::cmActionView: {
        mpHost->getActionUnit()->reParentAction(mItemID, mOldParentID, mNewParentID, -1, -1);
        break;
    }
    default:
        qWarning() << "MoveItemCommand::redo() - Unknown view type";
        break;
    }
}

QString MoveItemCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    return QObject::tr("Move %1 \"%2\"").arg(typeName, mItemName);
}

void MoveItemCommand::remapItemID(int oldID, int newID) {
    if (mItemID == oldID) {
        mItemID = newID;
    }
    if (mOldParentID == oldID) {
        mOldParentID = newID;
    }
    if (mNewParentID == oldID) {
        mNewParentID = newID;
    }
}

// =============================================================================
// ToggleActiveCommand implementation
// =============================================================================

ToggleActiveCommand::ToggleActiveCommand(EditorViewType viewType, int itemID,
                                         bool oldState, bool newState,
                                         const QString& itemName,
                                         Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mOldActiveState(oldState)
, mNewActiveState(newState)
, mItemName(itemName)
{
}

void ToggleActiveCommand::undo() {

    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* pT = mpHost->getTriggerUnit()->getTrigger(mItemID);
        if (pT) {
            pT->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Trigger" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmAliasView: {
        TAlias* pA = mpHost->getAliasUnit()->getAlias(mItemID);
        if (pA) {
            pA->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Alias" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmTimerView: {
        TTimer* pT = mpHost->getTimerUnit()->getTimer(mItemID);
        if (pT) {
            pT->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Timer" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmScriptView: {
        TScript* pS = mpHost->getScriptUnit()->getScript(mItemID);
        if (pS) {
            pS->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Script" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmKeysView: {
        TKey* pK = mpHost->getKeyUnit()->getKey(mItemID);
        if (pK) {
            pK->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Key" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmActionView: {
        TAction* pA = mpHost->getActionUnit()->getAction(mItemID);
        if (pA) {
            pA->setIsActive(mOldActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::undo() - Action" << mItemID << "not found";
        }
        break;
    }
    default:
        qWarning() << "ToggleActiveCommand::undo() - Unknown view type";
        break;
    }
}

void ToggleActiveCommand::redo() {

    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        TTrigger* pT = mpHost->getTriggerUnit()->getTrigger(mItemID);
        if (pT) {
            pT->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Trigger" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmAliasView: {
        TAlias* pA = mpHost->getAliasUnit()->getAlias(mItemID);
        if (pA) {
            pA->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Alias" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmTimerView: {
        TTimer* pT = mpHost->getTimerUnit()->getTimer(mItemID);
        if (pT) {
            pT->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Timer" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmScriptView: {
        TScript* pS = mpHost->getScriptUnit()->getScript(mItemID);
        if (pS) {
            pS->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Script" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmKeysView: {
        TKey* pK = mpHost->getKeyUnit()->getKey(mItemID);
        if (pK) {
            pK->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Key" << mItemID << "not found";
        }
        break;
    }
    case EditorViewType::cmActionView: {
        TAction* pA = mpHost->getActionUnit()->getAction(mItemID);
        if (pA) {
            pA->setIsActive(mNewActiveState);
        } else {
            qWarning() << "ToggleActiveCommand::redo() - Action" << mItemID << "not found";
        }
        break;
    }
    default:
        qWarning() << "ToggleActiveCommand::redo() - Unknown view type";
        break;
    }
}

QString ToggleActiveCommand::text() const {
    QString typeName = getViewTypeName(mViewType);
    if (mNewActiveState) {
        return QObject::tr("Activate %1 \"%2\"").arg(typeName, mItemName);
    } else {
        return QObject::tr("Deactivate %1 \"%2\"").arg(typeName, mItemName);
    }
}

void ToggleActiveCommand::remapItemID(int oldID, int newID) {
    if (mItemID == oldID) {
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
    cmd->undo();
    // Get affected IDs after undo() completes, in case IDs changed (e.g., restored items may get new IDs)
    QList<int> affectedIDs = cmd->affectedItemIDs();

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
    cmd->redo();

    // Check if this was an AddItemCommand that changed the item ID
    // This happens when an item is deleted then recreated - the ID may change
    if (auto* addCmd = dynamic_cast<AddItemCommand*>(cmd.get())) {
        if (addCmd->didItemIDChange()) {
            int oldID = addCmd->getOldItemID();
            int newID = addCmd->getNewItemID();
            remapItemIDs(oldID, newID);
        }
    }

    // Get affected IDs after redo() completes, in case IDs changed
    QList<int> affectedIDs = cmd->affectedItemIDs();

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

    // Update all commands in undo stack
    int undoCount = 0;
    for (auto& cmd : mUndoStack) {
        cmd->remapItemID(oldID, newID);
    }

    // Update all commands in redo stack
    int redoCount = 0;
    for (auto& cmd : mRedoStack) {
        cmd->remapItemID(oldID, newID);
    }

}
