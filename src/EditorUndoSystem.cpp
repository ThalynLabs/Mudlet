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
#include "utils.h"

#include <QDebug>
#include <sstream>

// =============================================================================
// Helper functions for XML serialization and compression
// =============================================================================

namespace {

// Typical compression ratio: 3-5× (2 KB → 400-600 bytes)
QString compressXML(const QString& xml) {
    if (xml.isEmpty()) {
        return QString();
    }

    QByteArray compressed = qCompress(xml.toUtf8(), 9);
    return QString::fromLatin1(compressed.toBase64());
}

QString decompressXML(const QString& data) {
    if (data.isEmpty()) {
        return QString();
    }

    // Backward compatibility: check if data is already raw XML
    if (data.startsWith('<')) {
        return data;
    }

    // Decompress Base64-encoded compressed data
    QByteArray compressed = QByteArray::fromBase64(data.toLatin1());
    QByteArray decompressed = qUncompress(compressed);
    if (decompressed.isEmpty()) {
        qWarning() << "EditorUndoSystem: Failed to decompress XML data";
        return QString();
    }

    return QString::fromUtf8(decompressed);
}

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
    return compressXML(QString::fromStdString(oss.str()));
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
    return compressXML(QString::fromStdString(oss.str()));
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
    return compressXML(QString::fromStdString(oss.str()));
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
    return compressXML(QString::fromStdString(oss.str()));
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
    return compressXML(QString::fromStdString(oss.str()));
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
    return compressXML(QString::fromStdString(oss.str()));
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importTriggerFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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
        pT->setParent(pParent);
        // Use explicit enum mode for clarity
        pParent->addChild(pT, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getTriggerUnit()->registerTrigger(pT); // This will call addTrigger() since pT has a parent
    } else {
        // Root level trigger - register first (adds to end of root list)
        host->getTriggerUnit()->registerTrigger(pT);

        // Now reposition it if a specific position was requested
        auto rootListSize = host->getTriggerUnit()->getTriggerRootNodeList().size();

        if (position != -1 && position < rootListSize) {
            // Use reParentTrigger with explicit AtPosition mode to insert at specific position
            host->getTriggerUnit()->reParentTrigger(pT->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importAliasFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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

    // Manually add to parent at the correct position
    if (pParent) {
        pA->setParent(pParent);
        pParent->addChild(pA, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getAliasUnit()->registerAlias(pA); // This will call addAlias() since pA has a parent
    } else {
        // Root level alias - register first (adds to end of root list)
        host->getAliasUnit()->registerAlias(pA);

        // Now reposition it if a specific position was requested
        if (position != -1) {
            host->getAliasUnit()->reParentAlias(pA->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importTimerFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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

    // Manually add to parent at the correct position
    if (pParent) {
        pT->setParent(pParent);
        pParent->addChild(pT, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getTimerUnit()->registerTimer(pT); // This will call addTimer() since pT has a parent
    } else {
        // Root level timer - register first (adds to end of root list)
        host->getTimerUnit()->registerTimer(pT);

        // Reposition it if a specific position was requested
        if (position != -1) {
            host->getTimerUnit()->reParentTimer(pT->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importScriptFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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

    // Manually add to parent at the correct position
    if (pParent) {
        pS->setParent(pParent);
        pParent->addChild(pS, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getScriptUnit()->registerScript(pS); // This will call addScript() since pS has a parent
    } else {
        // Root level script - register first (adds to end of root list)
        host->getScriptUnit()->registerScript(pS);

        // Reposition it if a specific position was requested
        if (position != -1) {
            host->getScriptUnit()->reParentScript(pS->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importKeyFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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

    // Manually add to parent at the correct position
    if (pParent) {
        pK->setParent(pParent);
        pParent->addChild(pK, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getKeyUnit()->registerKey(pK); // This will call addKey() since pK has a parent
    } else {
        // Root level key - register first (adds to end of root list)
        host->getKeyUnit()->registerKey(pK);

        // Reposition it if a specific position was requested
        if (position != -1) {
            host->getKeyUnit()->reParentKey(pK->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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

    // Decompress XML
    QString xml = decompressXML(xmlSnapshot);
    if (xml.isEmpty()) {
        qWarning() << "importActionFromXML: Failed to decompress XML";
        return nullptr;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.toStdString().c_str());
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

    // Manually add to parent at the correct position
    if (pParent) {
        pA->Tree<TAction>::setParent(pParent);
        pParent->addChild(pA, (position >= 0) ? TreeItemInsertMode::AtPosition : TreeItemInsertMode::Append, position);
        host->getActionUnit()->registerAction(pA); // This will call addAction() since pA has a parent
    } else {
        // Root level action - register first (adds to end of root list)
        host->getActionUnit()->registerAction(pA);

        // Reposition it if a specific position was requested
        if (position != -1) {
            host->getActionUnit()->reParentAction(pA->getID(), -1, -1, TreeItemInsertMode::AtPosition, position);
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
                               int positionInParent, bool isFolder, const QString& itemName, Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mParentID(parentID)
, mPositionInParent(positionInParent)
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
            TTrigger* pNewTrigger = importTriggerFromXML(mItemSnapshot, pParent, mpHost, mPositionInParent);
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
            TAlias* pNewAlias = importAliasFromXML(mItemSnapshot, pAliasParent, mpHost, mPositionInParent);
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
            TTimer* pNewTimer = importTimerFromXML(mItemSnapshot, pTimerParent, mpHost, mPositionInParent);
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
            TScript* pNewScript = importScriptFromXML(mItemSnapshot, pScriptParent, mpHost, mPositionInParent);
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
            TKey* pNewKey = importKeyFromXML(mItemSnapshot, pKeyParent, mpHost, mPositionInParent);
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
            TAction* pNewAction = importActionFromXML(mItemSnapshot, pActionParent, mpHost, mPositionInParent);
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
    // Sort items to restore in correct order:
    // 1. Parents before children (items whose parent is not in deleted list come first)
    // 2. Siblings in position order (lower position first)
    QList<DeletedItemInfo> sortedItems = mDeletedItems;
    std::sort(sortedItems.begin(), sortedItems.end(), [&sortedItems](const DeletedItemInfo& a, const DeletedItemInfo& b) {
        // Check if parent is in the deleted list
        bool aParentDeleted = std::any_of(sortedItems.begin(), sortedItems.end(),
                                          [&a](const DeletedItemInfo& item) { return item.itemID == a.parentID; });
        bool bParentDeleted = std::any_of(sortedItems.begin(), sortedItems.end(),
                                          [&b](const DeletedItemInfo& item) { return item.itemID == b.parentID; });

        // Items whose parent is not deleted come first
        if (aParentDeleted != bParentDeleted) {
            return !aParentDeleted; // a comes first if its parent is not deleted
        }

        // Within same parent, sort by position (ascending)
        if (a.parentID == b.parentID) {
            return a.positionInParent < b.positionInParent;
        }

        // Different parents, maintain original order
        return false;
    });

    for (int i = 0; i < sortedItems.size(); ++i) {
        auto& info = sortedItems[i];

        // Skip items whose parent was also deleted - they'll be restored from parent's XML
        bool skipRestore = false;
        if (info.parentID != -1) {
            bool parentWasDeleted = std::any_of(mDeletedItems.begin(), mDeletedItems.end(),
                                                [&info](const DeletedItemInfo& item) {
                                                    return item.itemID == info.parentID;
                                                });
            if (parentWasDeleted) {
                skipRestore = true;
            }
        }

        // Find the corresponding item in mDeletedItems to update the ID
        auto it = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                               [&info](const DeletedItemInfo& item) {
                                   return item.itemName == info.itemName && item.parentID == info.parentID;
                               });
        if (it == mDeletedItems.end()) {
            qWarning() << "DeleteItemCommand::undo() - Could not find item in original list:" << info.itemName;
            continue;
        }
        auto& originalInfo = *it;

        if (skipRestore) {
            // Item was restored from parent's XML, but we still need to find its new ID for redo
            // This will be done after parent is restored
            continue;
        }

        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            // Get parent trigger
            TTrigger* pParent = nullptr;
            if (info.parentID != -1) {
                pParent = mpHost->getTriggerUnit()->getTrigger(info.parentID);
                if (!pParent) {
                    qWarning() << "DeleteItemCommand::undo() - Parent trigger not found for" << info.itemName
                               << "parentID=" << info.parentID;
                }
            }

            // Restore the trigger from XML snapshot at its original position
            TTrigger* pRestoredTrigger = importTriggerFromXML(info.xmlSnapshot, pParent, mpHost, info.positionInParent);
            if (!pRestoredTrigger) {
                qWarning() << "DeleteItemCommand::undo() - Failed to restore trigger" << info.itemName;
            } else {
                int newID = pRestoredTrigger->getID();
                int oldID = info.itemID;

                // Update the stored ID in original list so redo can find it
                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored trigger's children and update their IDs in mDeletedItems
                // (children were restored from XML, not individually)
                std::function<void(TTrigger*, int)> updateChildIDs = [&](TTrigger* pT, int parentID) {
                    if (!pT || !pT->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pT->mpMyChildrenList) {
                        // Find this child in mDeletedItems by name and parent ID
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                // Recursively update grandchildren
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredTrigger, oldID);
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
                int newID = pRestoredAlias->getID();
                int oldID = info.itemID;

                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored alias's children and update their IDs in mDeletedItems
                std::function<void(TAlias*, int)> updateChildIDs = [&](TAlias* pA, int parentID) {
                    if (!pA || !pA->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pA->mpMyChildrenList) {
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredAlias, oldID);
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
                int newID = pRestoredTimer->getID();
                int oldID = info.itemID;

                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored timer's children and update their IDs in mDeletedItems
                std::function<void(TTimer*, int)> updateChildIDs = [&](TTimer* pT, int parentID) {
                    if (!pT || !pT->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pT->mpMyChildrenList) {
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredTimer, oldID);
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
                int newID = pRestoredScript->getID();
                int oldID = info.itemID;

                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored script's children and update their IDs in mDeletedItems
                std::function<void(TScript*, int)> updateChildIDs = [&](TScript* pS, int parentID) {
                    if (!pS || !pS->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pS->mpMyChildrenList) {
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredScript, oldID);
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
                int newID = pRestoredKey->getID();
                int oldID = info.itemID;

                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored key's children and update their IDs in mDeletedItems
                std::function<void(TKey*, int)> updateChildIDs = [&](TKey* pK, int parentID) {
                    if (!pK || !pK->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pK->mpMyChildrenList) {
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredKey, oldID);
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
                int newID = pRestoredAction->getID();
                int oldID = info.itemID;

                originalInfo.itemID = newID;

                // If ID changed, update all remaining items that reference this as their parent
                if (newID != oldID) {
                    for (int j = i + 1; j < sortedItems.size(); ++j) {
                        if (sortedItems[j].parentID == oldID) {
                            sortedItems[j].parentID = newID;

                            // Also update in mDeletedItems so we can find it later
                            auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                        [&sortedItems, j, oldID](const DeletedItemInfo& item) {
                                                            return item.itemName == sortedItems[j].itemName && item.parentID == oldID;
                                                        });
                            if (childIt != mDeletedItems.end()) {
                                childIt->parentID = newID;
                            }
                        }
                    }
                }

                // Walk the restored action's children and update their IDs in mDeletedItems
                std::function<void(TAction*, int)> updateChildIDs = [&](TAction* pA, int parentID) {
                    if (!pA || !pA->mpMyChildrenList) {
                        return;
                    }
                    for (auto* pChild : *pA->mpMyChildrenList) {
                        auto childIt = std::find_if(mDeletedItems.begin(), mDeletedItems.end(),
                                                     [pChild, parentID](const DeletedItemInfo& item) {
                                                         return item.itemName == pChild->getName() && item.parentID == parentID;
                                                     });
                        if (childIt != mDeletedItems.end()) {
                            int childOldID = childIt->itemID;
                            int childNewID = pChild->getID();
                            if (childOldID != childNewID) {
                                childIt->itemID = childNewID;
                                updateChildIDs(pChild, childNewID);
                            }
                        }
                    }
                };
                updateChildIDs(pRestoredAction, oldID);
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

    // Important: Set mpHost to null before deleting to prevent items from trying to
    // unregister themselves during destruction (which could cause iterator invalidation
    // or access to partially-destroyed parent items)
    for (const auto& info : mDeletedItems) {
        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            TTrigger* trigger = mpHost->getTriggerUnit()->getTrigger(info.itemID);
            if (trigger) {
                // Nullify mpHost on trigger and all children recursively
                trigger->mpHost = nullptr;
                std::function<void(TTrigger*)> nullifyChildren = [&nullifyChildren](TTrigger* t) {
                    for (auto child : *t->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(trigger);
            }
            break;
        }
        case EditorViewType::cmAliasView: {
            TAlias* alias = mpHost->getAliasUnit()->getAlias(info.itemID);
            if (alias) {
                // Nullify mpHost on alias and all children recursively
                alias->mpHost = nullptr;
                std::function<void(TAlias*)> nullifyChildren = [&nullifyChildren](TAlias* a) {
                    for (auto child : *a->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(alias);
            }
            break;
        }
        case EditorViewType::cmTimerView: {
            TTimer* timer = mpHost->getTimerUnit()->getTimer(info.itemID);
            if (timer) {
                // Nullify mpHost on timer and all children recursively
                timer->mpHost = nullptr;
                std::function<void(TTimer*)> nullifyChildren = [&nullifyChildren](TTimer* t) {
                    for (auto child : *t->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(timer);
            }
            break;
        }
        case EditorViewType::cmScriptView: {
            TScript* script = mpHost->getScriptUnit()->getScript(info.itemID);
            if (script) {
                // Nullify mpHost on script and all children recursively
                script->mpHost = nullptr;
                std::function<void(TScript*)> nullifyChildren = [&nullifyChildren](TScript* s) {
                    for (auto child : *s->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(script);
            }
            break;
        }
        case EditorViewType::cmKeysView: {
            TKey* key = mpHost->getKeyUnit()->getKey(info.itemID);
            if (key) {
                // Nullify mpHost on key and all children recursively
                key->mpHost = nullptr;
                std::function<void(TKey*)> nullifyChildren = [&nullifyChildren](TKey* k) {
                    for (auto child : *k->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(key);
            }
            break;
        }
        case EditorViewType::cmActionView: {
            TAction* action = mpHost->getActionUnit()->getAction(info.itemID);
            if (action) {
                // Nullify mpHost on action and all children recursively
                action->mpHost = nullptr;
                std::function<void(TAction*)> nullifyChildren = [&nullifyChildren](TAction* a) {
                    for (auto child : *a->mpMyChildrenList) {
                        child->mpHost = nullptr;
                        nullifyChildren(child);
                    }
                };
                nullifyChildren(action);
            }
            break;
        }
        default:
            break;
        }
    }

    // Now manually unregister and delete all items
    // Since mpHost is null, destructors won't try to unregister, so we must do it manually
    for (const auto& info : mDeletedItems) {
        switch (mViewType) {
        case EditorViewType::cmTriggerView: {
            TTrigger* trigger = mpHost->getTriggerUnit()->getTrigger(info.itemID);
            if (trigger) {
                mpHost->getTriggerUnit()->unregisterTrigger(trigger);
                delete trigger;
            }
            break;
        }
        case EditorViewType::cmAliasView: {
            TAlias* alias = mpHost->getAliasUnit()->getAlias(info.itemID);
            if (alias) {
                mpHost->getAliasUnit()->unregisterAlias(alias);
                delete alias;
            }
            break;
        }
        case EditorViewType::cmTimerView: {
            TTimer* timer = mpHost->getTimerUnit()->getTimer(info.itemID);
            if (timer) {
                mpHost->getTimerUnit()->unregisterTimer(timer);
                delete timer;
            }
            break;
        }
        case EditorViewType::cmScriptView: {
            TScript* script = mpHost->getScriptUnit()->getScript(info.itemID);
            if (script) {
                mpHost->getScriptUnit()->unregisterScript(script);
                delete script;
            }
            break;
        }
        case EditorViewType::cmKeysView: {
            TKey* key = mpHost->getKeyUnit()->getKey(info.itemID);
            if (key) {
                mpHost->getKeyUnit()->unregisterKey(key);
                delete key;
            }
            break;
        }
        case EditorViewType::cmActionView: {
            TAction* action = mpHost->getActionUnit()->getAction(info.itemID);
            if (action) {
                mpHost->getActionUnit()->unregisterAction(action);
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
                                 int oldPosition, int newPosition,
                                 const QString& itemName,
                                 Host* host)
: EditorCommand(host)
, mViewType(viewType)
, mItemID(itemID)
, mOldParentID(oldParentID)
, mNewParentID(newParentID)
, mOldPosition(oldPosition)
, mNewPosition(newPosition)
, mItemName(itemName)
{
}

void MoveItemCommand::undo() {
    // Move the item back to its original parent at its original position
    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        mpHost->getTriggerUnit()->reParentTrigger(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    case EditorViewType::cmAliasView: {
        mpHost->getAliasUnit()->reParentAlias(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    case EditorViewType::cmTimerView: {
        mpHost->getTimerUnit()->reParentTimer(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    case EditorViewType::cmScriptView: {
        mpHost->getScriptUnit()->reParentScript(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    case EditorViewType::cmKeysView: {
        mpHost->getKeyUnit()->reParentKey(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    case EditorViewType::cmActionView: {
        mpHost->getActionUnit()->reParentAction(mItemID, mNewParentID, mOldParentID, TreeItemInsertMode::AtPosition, mOldPosition);
        break;
    }
    default:
        qWarning() << "MoveItemCommand::undo() - Unknown view type";
        break;
    }
}

void MoveItemCommand::redo() {
    // Move the item to the new parent at the new position
    switch (mViewType) {
    case EditorViewType::cmTriggerView: {
        mpHost->getTriggerUnit()->reParentTrigger(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
        break;
    }
    case EditorViewType::cmAliasView: {
        mpHost->getAliasUnit()->reParentAlias(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
        break;
    }
    case EditorViewType::cmTimerView: {
        mpHost->getTimerUnit()->reParentTimer(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
        break;
    }
    case EditorViewType::cmScriptView: {
        mpHost->getScriptUnit()->reParentScript(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
        break;
    }
    case EditorViewType::cmKeysView: {
        mpHost->getKeyUnit()->reParentKey(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
        break;
    }
    case EditorViewType::cmActionView: {
        mpHost->getActionUnit()->reParentAction(mItemID, mOldParentID, mNewParentID, TreeItemInsertMode::AtPosition, mNewPosition);
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

    // If in batch mode, collect commands instead of pushing directly
    if (mInBatch) {
        mBatchCommands.push_back(std::move(cmd));
        return;
    }

    mUndoStack.push_back(std::move(cmd));
    clearRedoStack();
    enforceUndoLimit();
    emitSignals();
}

void EditorUndoSystem::beginBatch(const QString& description)
{
    if (mInBatch) {
        qWarning() << "EditorUndoSystem::beginBatch() called while already in batch mode";
        return;
    }

    mInBatch = true;
    mBatchCommands.clear();
    mBatchDescription = description.isEmpty() ? tr("Move items") : description;
}

void EditorUndoSystem::endBatch()
{
    if (!mInBatch) {
        qWarning() << "EditorUndoSystem::endBatch() called while not in batch mode";
        return;
    }

    mInBatch = false;

    // If no commands were collected, just return
    if (mBatchCommands.empty()) {
        return;
    }

    // If only one command, push it directly without wrapping
    if (mBatchCommands.size() == 1) {
        auto cmd = std::move(mBatchCommands[0]);
        mBatchCommands.clear();
        mUndoStack.push_back(std::move(cmd));
        clearRedoStack();
        enforceUndoLimit();
        emitSignals();
        return;
    }

    // Create a batch command containing all collected commands
    auto batchCmd = std::make_unique<BatchCommand>(mBatchDescription, mpHost);
    for (auto& cmd : mBatchCommands) {
        batchCmd->addCommand(std::move(cmd));
    }
    mBatchCommands.clear();

    mUndoStack.push_back(std::move(batchCmd));
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
