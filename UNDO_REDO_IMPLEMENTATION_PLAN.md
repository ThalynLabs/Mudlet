# Mudlet Editor Undo/Redo Implementation Plan

**Issue**: [#707 - Undo/redo support in editor](https://github.com/Mudlet/Mudlet/issues/707)
**Date**: 2025-10-25
**Status**: Planning → Implementation

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Two-Phase Implementation Strategy](#two-phase-implementation-strategy)
4. [Phase 1: Text Editor Undo/Redo](#phase-1-text-editor-undoredo)
5. [Phase 2: Item-Level Undo/Redo](#phase-2-item-level-undoredo)
6. [XML-Based Approach Analysis](#xml-based-approach-analysis)
7. [Architecture Details](#architecture-details)
8. [Implementation Checklist](#implementation-checklist)
9. [Testing Strategy](#testing-strategy)

---

## Executive Summary

### Problem Statement

Users cannot undo accidental operations in Mudlet's trigger editor:
- Deleting triggers/aliases/scripts by mistake
- Moving items to wrong groups
- Losing work when switching between items
- No Ctrl+Z for text editing beyond current item

### Solution Overview

**Two-Phase Approach**:

| Phase | Scope | Complexity | Time | Value |
|-------|-------|------------|------|-------|
| Phase 1 | Text editor undo/redo with UI controls | Low | 2-3 days | High (quick win) |
| Phase 2 | Item-level undo/redo (add/delete/modify/move) | High | 8-10 weeks | Very High |

### Key Decisions

- **Use edbee's built-in undo/redo** for Phase 1 (already exists!)
- **Use XML serialization** for Phase 2 (leverage existing code)
- **Hybrid approach** with metadata for better UX
- **50-level undo stack** (configurable)
- **Focus-based priority** (text editor vs item operations)

---

## Current State Analysis

### Editor Architecture

```
dlgTriggerEditor (Main Editor Dialog)
├── Tree Widgets (QTreeWidget)
│   ├── treeWidget_triggers
│   ├── treeWidget_aliases
│   ├── treeWidget_timers
│   ├── treeWidget_scripts
│   ├── treeWidget_keys
│   └── treeWidget_actions
│
├── Source Editor (edbee)
│   ├── mpSourceEditorEdbee (edbee::TextEditorWidget)
│   ├── mpSourceEditorEdbeeDocument (edbee::CharTextDocument)
│   └── controller (edbee::TextEditorController) ← HAS UNDO/REDO!
│
└── Property Panels (dlg*MainArea)
    ├── mpTriggersMainArea
    ├── mpAliasMainArea
    └── ... (25+ editable properties per type)
```

### Existing Undo Capabilities

**edbee text editor** (lines 11076-11082 in dlgTriggerEditor.cpp):
```cpp
controller->beginUndoGroup();
// ... modify text ...
controller->endUndoGroup(edbee::CoalesceId_None, false);
```

Methods available:
- `controller->undo()` - Undo text changes
- `controller->redo()` - Redo text changes
- `controller->createAction("undo", ...)` - Create QAction
- `controller->createAction("redo", ...)` - Create QAction

**Problem**: No UI exposed to users (no buttons, no keyboard shortcuts).

### Item Management

| Operation | File Location | Current State |
|-----------|---------------|---------------|
| Add Item | dlgTriggerEditor.cpp:4318 (`addTrigger()`) | ✅ Works, no undo |
| Delete Item | dlgTriggerEditor.cpp:3265 (`delete_trigger()`) | ✅ Works, no undo |
| Modify Item | dlgTriggerEditor.cpp:5068 (`saveTrigger()`) | ✅ Works, no undo |
| Move Item | TriggerUnit.h:65 (`reParentTrigger()`) | ✅ Works, no undo |
| Toggle Active | dlgTriggerEditor.cpp:3412 (`activeToggle_trigger()`) | ✅ Works, no undo |

### Item Data Structures

All items inherit from `Tree<T>` (src/Tree.h):

```cpp
template <class T>
class Tree {
    T* mpParent;                    // Parent in hierarchy
    std::list<T*>* mpMyChildrenList; // Children
    int mID;                        // Unique identifier
    QString mPackageName;           // Module membership
    bool mFolder;                   // Is group/folder
    bool mActive;                   // Active state
    bool mUserActiveState;          // User's desired state
    bool mTemporary;                // Temporary (not saved)
};
```

**6 Item Types**:
1. `TTrigger` (src/TTrigger.h) - 25+ properties
2. `TAlias` (src/TAlias.h) - 5 properties
3. `TTimer` (src/TTimer.h) - 6 properties
4. `TScript` (src/TScript.h) - 4 properties
5. `TKey` (src/TKey.h) - 5 properties
6. `TAction` (src/TAction.h) - 18 properties

### Serialization Infrastructure

**Already exists!** (3,527 lines of production code)

| File | Lines | Purpose |
|------|-------|---------|
| src/XMLexport.cpp | 1,353 | Serialize items to XML |
| src/XMLimport.cpp | 2,174 | Deserialize items from XML |

Functions available:
- `XMLexport::writeTrigger(TTrigger*, xml_node)` - Complete state export
- `XMLimport::readTrigger(TTrigger*)` - Complete state import
- Similar functions for all 6 item types
- **Recursive** - handles folder hierarchies automatically

---

## Two-Phase Implementation Strategy

### Why Two Phases?

1. **Phase 1 = Quick Win**: 2-3 days → immediate user value
2. **Phase 2 = Full Solution**: 8-10 weeks → complete feature
3. **Independent**: Phase 1 can ship without Phase 2
4. **Risk Management**: Test UI/UX with Phase 1 before complex Phase 2

### User Value Progression

```
Week 0 (Current):
❌ No undo at all
❌ Lost work when deleting items
❌ Can't undo text changes when switching items

Week 1 (Phase 1):
✅ Undo/redo text changes within current item
✅ Ctrl+Z / Ctrl+Y keyboard shortcuts
✅ Undo/redo buttons in toolbar
✅ Context menu with undo/redo
⚠️ Still can't undo item deletion/moves

Week 12 (Phase 2):
✅ All Phase 1 features
✅ Undo item deletion
✅ Undo item moves
✅ Undo property changes
✅ 50-level undo history
✅ Undo/redo menu with history
```

---

## Phase 1: Text Editor Undo/Redo

### Goal

Expose edbee's existing undo/redo functionality to users via:
- Toolbar buttons
- Keyboard shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z)
- Right-click context menu

### Estimated Effort

- **Time**: 2-3 days
- **Code changes**: ~100 lines
- **Files modified**: 2 (dlgTriggerEditor.h, dlgTriggerEditor.cpp)
- **Complexity**: LOW

### Implementation Details

#### 1. Add Actions to dlgTriggerEditor.h

**Location**: src/dlgTriggerEditor.h, around line 555 (after existing QAction declarations)

```cpp
class dlgTriggerEditor : public QWidget, public Ui::trigger_editor {
    // ... existing members ...

    QAction* mpAction_searchOptions = nullptr;
    QAction* mpAction_searchCaseSensitive = nullptr;
    // ... other existing actions ...

    // NEW: Undo/redo actions for text editor
    QAction* mpUndoTextAction = nullptr;
    QAction* mpRedoTextAction = nullptr;
};
```

#### 2. Create Actions in Constructor

**Location**: src/dlgTriggerEditor.cpp, around line 387 (where other source editor shortcuts are created)

```cpp
// In dlgTriggerEditor constructor, after existing source editor actions:

// Existing code:
auto openSourceFindAction = new QAction(this);
// ... other actions ...

// NEW CODE:
// Undo action
mpUndoTextAction = new QAction(this);
mpUndoTextAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
mpUndoTextAction->setShortcut(QKeySequence(QKeySequence::Undo)); // Ctrl+Z
mpSourceEditorArea->addAction(mpUndoTextAction);
connect(mpUndoTextAction, &QAction::triggered, this, [this]() {
    mpSourceEditorEdbee->controller()->undo();
});

// Redo action
mpRedoTextAction = new QAction(this);
mpRedoTextAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
mpRedoTextAction->setShortcut(QKeySequence(QKeySequence::Redo)); // Ctrl+Y or Ctrl+Shift+Z
mpSourceEditorArea->addAction(mpRedoTextAction);
connect(mpRedoTextAction, &QAction::triggered, this, [this]() {
    mpSourceEditorEdbee->controller()->redo();
});
```

#### 3. Add Toolbar Buttons

**Location**: src/dlgTriggerEditor.cpp, around line 730-756 (where toolbar is populated)

```cpp
// In dlgTriggerEditor constructor, after toolBar is created:

toolBar->setMovable(true);
toolBar->addAction(toggleActiveAction);
toolBar->addAction(mSaveItem);

// NEW CODE: Add undo/redo to toolbar
toolBar->addSeparator();

QAction* undoToolbarAction = new QAction(
    QIcon::fromTheme(qsl("edit-undo"), QIcon(qsl(":/icons/edit-undo.png"))),
    tr("Undo"), this);
undoToolbarAction->setToolTip(utils::richText(tr("Undo changes in the editor")));
undoToolbarAction->setStatusTip(tr("Undo the last change in the code editor"));
connect(undoToolbarAction, &QAction::triggered, this, [this]() {
    mpSourceEditorEdbee->controller()->undo();
});

QAction* redoToolbarAction = new QAction(
    QIcon::fromTheme(qsl("edit-redo"), QIcon(qsl(":/icons/edit-redo.png"))),
    tr("Redo"), this);
redoToolbarAction->setToolTip(utils::richText(tr("Redo changes in the editor")));
redoToolbarAction->setStatusTip(tr("Redo the last undone change in the code editor"));
connect(redoToolbarAction, &QAction::triggered, this, [this]() {
    mpSourceEditorEdbee->controller()->redo();
});

toolBar->addAction(undoToolbarAction);
toolBar->addAction(redoToolbarAction);

// END NEW CODE

toolBar->setWindowTitle(tr("Editor Toolbar - %1 - Actions").arg(hostName));
// ... rest of existing code ...
```

#### 4. Add to Context Menu

**Location**: src/dlgTriggerEditor.cpp, around line 11057-11069 (in `slot_editorContextMenu()`)

```cpp
void dlgTriggerEditor::slot_editorContextMenu(const QPoint&)
{
    QMenu* menu = new QMenu(this);
    auto controller = mpSourceEditorEdbee->controller();

    // NEW CODE: Add undo/redo at top of menu
    if (qApp->testAttribute(Qt::AA_DontShowIconsInMenus)) {
        menu->addAction(controller->createAction("undo", tr("Undo"), QIcon(), menu));
        menu->addAction(controller->createAction("redo", tr("Redo"), QIcon(), menu));
    } else {
        menu->addAction(controller->createAction("undo", tr("Undo"),
            QIcon::fromTheme(qsl("edit-undo"), QIcon(qsl(":/icons/edit-undo.png"))), menu));
        menu->addAction(controller->createAction("redo", tr("Redo"),
            QIcon::fromTheme(qsl("edit-redo"), QIcon(qsl(":/icons/edit-redo.png"))), menu));
    }
    menu->addSeparator();
    // END NEW CODE

    // Existing code:
    if (qApp->testAttribute(Qt::AA_DontShowIconsInMenus)) {
        menu->addAction(controller->createAction("cut", tr("Cut"), QIcon(), menu));
        // ... rest of existing menu items ...
    }

    // ... rest of function ...
}
```

### Testing Phase 1

**Manual Tests**:

1. ✅ **Keyboard shortcuts work**
   - Open trigger editor
   - Edit script text
   - Press Ctrl+Z → text reverts
   - Press Ctrl+Y → text restored

2. ✅ **Toolbar buttons work**
   - Edit script text
   - Click Undo button → text reverts
   - Click Redo button → text restored

3. ✅ **Context menu works**
   - Right-click in editor
   - Click "Undo" → text reverts
   - Right-click → Click "Redo" → text restored

4. ✅ **Multi-level undo**
   - Type "abc"
   - Type "def"
   - Type "ghi"
   - Undo 3 times → all text removed
   - Redo 3 times → all text restored

5. ✅ **Undo persists within item**
   - Edit trigger A
   - Switch to trigger B
   - Switch back to trigger A
   - Undo still works for trigger A's edits

6. ⚠️ **Undo DOES NOT persist across items** (expected limitation)
   - Edit trigger A
   - Switch to trigger B
   - Cannot undo trigger A's changes (need Phase 2)

### Deliverables Phase 1

- [ ] Modified dlgTriggerEditor.h
- [ ] Modified dlgTriggerEditor.cpp
- [ ] Manual test checklist completed
- [ ] Git commit with message
- [ ] Push to branch

---

## Phase 2: Item-Level Undo/Redo

### Goal

Full undo/redo system for:
- Adding items (triggers, aliases, timers, scripts, keys, actions)
- Deleting items (single or multiple selection)
- Moving items (drag-drop to different groups)
- Modifying properties (name, script, patterns, etc.)
- Toggling active/inactive state

### Estimated Effort

- **Time**: 8-10 weeks
- **Code changes**: ~1,950 lines
- **Files created**: 2 new files
- **Files modified**: 3 existing files
- **Complexity**: HIGH

### Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                  dlgTriggerEditor                       │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │           EditorUndoSystem                       │  │
│  │                                                  │  │
│  │  - Undo Stack (vector<EditorCommand>)          │  │
│  │  - Redo Stack (vector<EditorCommand>)          │  │
│  │  - Undo Limit (default: 50)                    │  │
│  │                                                  │  │
│  │  Methods:                                       │  │
│  │  + pushCommand(cmd)                            │  │
│  │  + undo()                                      │  │
│  │  + redo()                                      │  │
│  │  + canUndo() / canRedo()                       │  │
│  │                                                  │  │
│  │  Signals:                                       │  │
│  │  - canUndoChanged(bool)                        │  │
│  │  - canRedoChanged(bool)                        │  │
│  │  - undoTextChanged(QString)                    │  │
│  └──────────────────────────────────────────────────┘  │
│                           │                             │
│                           │ contains                    │
│                           ▼                             │
│  ┌──────────────────────────────────────────────────┐  │
│  │         EditorCommand (abstract base)            │  │
│  │                                                  │  │
│  │  + virtual void undo() = 0                      │  │
│  │  + virtual void redo() = 0                      │  │
│  │  + virtual QString text() const = 0             │  │
│  └──────────────────────────────────────────────────┘  │
│                           │                             │
│          ┌───────────────┼───────────────┬────────┐    │
│          ▼               ▼               ▼        ▼    │
│   ┌──────────┐   ┌──────────┐   ┌──────────┐  etc.   │
│   │   Add    │   │  Delete  │   │  Modify  │         │
│   │  Item    │   │   Item   │   │ Property │         │
│   │ Command  │   │ Command  │   │ Command  │         │
│   └──────────┘   └──────────┘   └──────────┘         │
│        │                │                │             │
│        └────────────────┴────────────────┘             │
│                         │                              │
│                         ▼                              │
│              Uses XMLexport/XMLimport                  │
│              to serialize/restore state                │
└─────────────────────────────────────────────────────────┘
```

### New Files

#### 1. src/EditorUndoSystem.h

```cpp
#ifndef MUDLET_EDITORUNDOSYSTEM_H
#define MUDLET_EDITORUNDOSYSTEM_H

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class Host;
class EditorCommand;
enum class EditorViewType;

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
    void setUndoLimit(int limit) { mUndoLimit = limit; }
    int undoLimit() const { return mUndoLimit; }

signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void undoTextChanged(const QString& text);
    void redoTextChanged(const QString& text);

private:
    void clearRedoStack();
    void enforceUndoLimit();

    std::vector<std::unique_ptr<EditorCommand>> mUndoStack;
    std::vector<std::unique_ptr<EditorCommand>> mRedoStack;
    int mUndoLimit = 50;
    Host* mpHost;
};

// Base command class
class EditorCommand {
public:
    explicit EditorCommand(Host* host) : mpHost(host) {}
    virtual ~EditorCommand() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual QString text() const = 0;

protected:
    Host* mpHost;
};

// Concrete command classes
class AddItemCommand : public EditorCommand {
public:
    AddItemCommand(EditorViewType viewType, int itemID, int parentID,
                   bool isFolder, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;

private:
    EditorViewType mViewType;
    int mItemID;
    int mParentID;
    bool mIsFolder;
    QString mItemSnapshot;
    QString mChangeDescription;
};

class DeleteItemCommand : public EditorCommand {
public:
    DeleteItemCommand(EditorViewType viewType, const QList<int>& itemIDs, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;

private:
    struct DeletedItemInfo {
        int itemID;
        int parentID;
        int positionInParent;
        QString xmlSnapshot;
    };

    EditorViewType mViewType;
    QList<DeletedItemInfo> mDeletedItems;
    QString mChangeDescription;
};

class ModifyPropertyCommand : public EditorCommand {
public:
    ModifyPropertyCommand(EditorViewType viewType, int itemID,
                          const QString& oldStateXML, const QString& newStateXML,
                          const QString& description, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;

private:
    EditorViewType mViewType;
    int mItemID;
    QString mOldStateXML;
    QString mNewStateXML;
    QString mChangeDescription;
};

class MoveItemCommand : public EditorCommand {
public:
    MoveItemCommand(EditorViewType viewType, int itemID,
                    int oldParentID, int newParentID,
                    int oldPosition, int newPosition, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;

private:
    EditorViewType mViewType;
    int mItemID;
    int mOldParentID, mNewParentID;
    int mOldPosition, mNewPosition;
    QString mChangeDescription;
};

class ToggleActiveCommand : public EditorCommand {
public:
    ToggleActiveCommand(EditorViewType viewType, int itemID, bool oldState, Host* host);

    void undo() override;
    void redo() override;
    QString text() const override;

private:
    EditorViewType mViewType;
    int mItemID;
    bool mOldActiveState;
    QString mChangeDescription;
};

#endif // MUDLET_EDITORUNDOSYSTEM_H
```

#### 2. src/EditorUndoSystem.cpp

```cpp
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

// Implementation to be completed
// Approximately 800-1000 lines
```

### Modified Files

#### 1. src/dlgTriggerEditor.h

Add member:
```cpp
class dlgTriggerEditor : public QWidget, public Ui::trigger_editor {
    // ... existing members ...

    // NEW: Undo system
    EditorUndoSystem* mpUndoSystem = nullptr;

    // NEW: Undo/redo actions
    QAction* mpUndoItemAction = nullptr;
    QAction* mpRedoItemAction = nullptr;
};
```

#### 2. src/dlgTriggerEditor.cpp

Intercept all item operations to push commands.

#### 3. src/CMakeLists.txt and src/mudlet.pro

Add new files to build system.

### XML Serialization Helpers

Need helper functions to export/import individual items:

```cpp
// In EditorUndoSystem.cpp

QString exportTriggerToXML(TTrigger* trigger) {
    pugi::xml_document doc;
    auto root = doc.append_child("TriggerSnapshot");
    XMLexport exporter;
    exporter.writeTrigger(trigger, root);

    std::ostringstream oss;
    doc.save(oss);
    return QString::fromStdString(oss.str());
}

TTrigger* importTriggerFromXML(const QString& xml, Host* host, TTrigger* parent) {
    pugi::xml_document doc;
    doc.load_string(xml.toUtf8().constData());
    auto root = doc.child("TriggerSnapshot");

    XMLimport importer(host);
    return importer.readTrigger(root.first_child(), parent);
}

// Similar functions for Alias, Timer, Script, Key, Action
```

---

## XML-Based Approach Analysis

### Pros ✅

1. **Leverages Existing Code** (3,527 lines battle-tested)
   - XMLexport.cpp (1,353 lines)
   - XMLimport.cpp (2,174 lines)
   - Already handles all 25+ trigger properties
   - Already handles all 6 item types
   - Already recursive (folder hierarchies)
   - Already tested (used for save/load/import/export)

2. **Complete State Capture**
   - Zero risk of missing properties
   - Future-proof (new properties auto-included)
   - Handles complex data (colors, patterns, scripts)

3. **Handles Hierarchies Automatically**
   - Folder with 50 children → one function call
   - Parent-child relationships preserved
   - Position in tree maintained

4. **Format Agnostic**
   - Refactor TTrigger → XML schema compatible
   - Change Tree<T> → Doesn't break undo
   - Add new item types → Just use their export

5. **Human-Readable Debugging**
   ```xml
   <Trigger isActive="yes" isFolder="no">
     <name>Health Monitor</name>
     <script>send("heal")</script>
   </Trigger>
   ```

6. **No Pointer Invalidation**
   - Stores data, not pointers
   - Safe across delete/recreate
   - Qt smart pointers not needed

7. **Memory Safety**
   - Can restore deleted items
   - No dangling pointers
   - No use-after-free bugs

8. **Copy-on-Write Efficiency**
   - QString uses implicit sharing
   - 50 similar snapshots → minimal memory
   - Only copies when modified

### Cons ❌

1. **Performance Overhead**

   Estimated times:
   ```
   Simple trigger:   1-2 ms
   Complex trigger:  5-10 ms
   Folder (50 items): 50-100 ms
   Deserialization:  2-3× slower
   ```

   **Mitigation**: Debounce auto-saves (500ms delay)

2. **Memory Usage**

   ```
   Average trigger: 1-2 KB XML
   Complex trigger: 5-10 KB
   Folder (50 items): 50-500 KB

   50-level undo stack:
   - Best case: 50 KB
   - Typical: 500 KB - 2 MB
   - Worst case: 10-25 MB
   ```

   **Mitigation**: qCompress() for 3-5× reduction

3. **Lossy for Runtime State**

   Not serialized:
   - `mNeedsToBeCompiled` flag
   - Runtime match state
   - Compiled regex objects

   Must recompile after restore.

   **Impact**: Minor - should recompile anyway

4. **ID Management Complexity**

   ```cpp
   // Original trigger ID = 42
   delete pTrigger;  // ID freed

   // New trigger gets ID = 43

   // Undo deletion - restore ID 42 or assign 43?
   // If ID changes, Lua scripts using enableTrigger(42) break!
   ```

   **Solution**: Track allocated IDs, restore originals

5. **No Granular Property Tracking**

   ```cpp
   // User changes only name (10 bytes)
   // XML stores entire state (2 KB)
   // 99% waste
   ```

   **Mitigation**: Add lightweight metadata (50 bytes)

6. **Generic Undo Text**

   ```
   Undo menu shows: "Modify Trigger"
   Better UX: "Change name to 'Health Monitor'"
   ```

   **Solution**: Hybrid approach with metadata

7. **Can't Coalesce Edits**

   ```
   User types: "H" → "e" → "l" → "l" → "o"
   Result: 5 undo commands × 2 KB = 10 KB
   Better: 1 command "Changed name to 'Hello'"
   ```

   **Mitigation**: Debounce (coalesce within 500ms)

8. **Schema Coupling**

   Changing XML format → must migrate undo system

   **Impact**: Low - XML schema is stable

9. **Expiry/Temporary State Ambiguity**

   ```cpp
   // Trigger with expiryCount = 1
   pT->execute();  // Fires, count becomes 0, auto-deletes

   // Undo deletion
   // Should expiryCount be 0 or 1?
   ```

   **Solution**: Snapshot at last stable state

### Alternative Approaches Compared

| Approach | Implementation Time | Robustness | Performance | Memory | UX |
|----------|-------------------|------------|-------------|--------|-----|
| **XML** | ⭐⭐⭐⭐⭐ (2-3 weeks) | ⭐⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ |
| Property Tracking | ⭐⭐ (8-12 weeks) | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Object Cloning | ⭐⭐⭐ (4-6 weeks) | ⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐ |
| Differential | ⭐ (12+ weeks) | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ |
| Event Sourcing | ⭐⭐ (8-10 weeks) | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

### Recommendation: Hybrid XML Approach

Combine XML robustness with metadata for UX:

```cpp
class ModifyPropertyCommand {
    QString mFullStateXML;           // Robust restore
    QString mChangeDescription;      // "Change name to 'X'"
    QSet<QString> mModifiedProperties;  // {"name", "script"}

    void undo() {
        importFromXML(mFullStateXML);  // Robust
    }

    QString text() const {
        return mChangeDescription;  // Good UX
    }
};
```

**Cost**: ~50 bytes per command
**Benefit**: Best of both worlds

---

## Architecture Details

### Command Lifecycle

```
1. User performs operation (e.g., deletes trigger)
   ↓
2. Operation handler captures state
   - Export item to XML
   - Capture metadata (IDs, positions, description)
   ↓
3. Perform the operation
   - Actually delete the trigger
   ↓
4. Create command object
   - new DeleteItemCommand(...)
   ↓
5. Push to undo system
   - mpUndoSystem->pushCommand(cmd)
   ↓
6. Clear redo stack
   - Can't redo after new operation
   ↓
7. Emit signals
   - canUndoChanged(true)
   - undoTextChanged("Delete Trigger 'X'")
   ↓
8. User presses Ctrl+Z
   ↓
9. Command::undo() is called
   - Import from XML
   - Restore tree widget
   - Update selection
   ↓
10. Move command to redo stack
    ↓
11. Emit signals
    - canRedoChanged(true)
```

### Focus-Based Priority

When user presses Ctrl+Z:

```cpp
void dlgTriggerEditor::slot_undo() {
    // Check what has focus
    QWidget* focusWidget = QApplication::focusWidget();

    if (focusWidget == mpSourceEditorEdbee ||
        mpSourceEditorEdbee->isAncestorOf(focusWidget)) {
        // Text editor has focus → undo text changes
        mpSourceEditorEdbee->controller()->undo();
    } else {
        // Tree or properties have focus → undo item operations
        if (mpUndoSystem->canUndo()) {
            mpUndoSystem->undo();
        }
    }
}
```

### ID Management Strategy

```cpp
class EditorUndoSystem {
private:
    QSet<int> mAllocatedIDs;  // Track IDs in use

    void trackID(int id) {
        mAllocatedIDs.insert(id);
    }

    void releaseID(int id) {
        mAllocatedIDs.remove(id);
    }

    bool isIDAvailable(int id) {
        return !mAllocatedIDs.contains(id);
    }
};

// In DeleteItemCommand::undo():
void DeleteItemCommand::undo() {
    for (auto& info : mDeletedItems) {
        // Check if original ID is available
        if (mpUndoSystem->isIDAvailable(info.itemID)) {
            // Restore with original ID
            TTrigger* restored = importFromXML(info.xmlSnapshot);
            restored->setID(info.itemID);
        } else {
            // ID taken - need to assign new ID
            // Log warning for user
            qWarning() << "Cannot restore original ID" << info.itemID;
            TTrigger* restored = importFromXML(info.xmlSnapshot);
            // New ID assigned automatically
        }
    }
}
```

### Memory Optimization

```cpp
QString compressXML(const QString& xml) {
    QByteArray compressed = qCompress(xml.toUtf8(), 9); // Max compression
    return QString::fromLatin1(compressed.toBase64());
}

QString decompressXML(const QString& compressed) {
    QByteArray data = QByteArray::fromBase64(compressed.toLatin1());
    QByteArray decompressed = qUncompress(data);
    return QString::fromUtf8(decompressed);
}

// Typical compression ratio: 3-5×
// 2 KB XML → 400-600 bytes compressed
```

---

## Implementation Checklist

### Phase 1: Text Editor Undo/Redo

- [ ] Add `mpUndoTextAction` and `mpRedoTextAction` to dlgTriggerEditor.h
- [ ] Create actions with keyboard shortcuts in constructor
- [ ] Add toolbar buttons
- [ ] Add context menu items
- [ ] Test keyboard shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z)
- [ ] Test toolbar buttons
- [ ] Test context menu
- [ ] Test multi-level undo/redo
- [ ] Write commit message
- [ ] Push to branch

### Phase 2A: Foundation (Weeks 1-2)

- [ ] Create src/EditorUndoSystem.h
- [ ] Create src/EditorUndoSystem.cpp
- [ ] Implement `EditorUndoSystem` class
- [ ] Implement `EditorCommand` base class
- [ ] Add to CMakeLists.txt
- [ ] Add to mudlet.pro
- [ ] Add `mpUndoSystem` member to dlgTriggerEditor
- [ ] Create undo/redo actions
- [ ] Add to toolbar
- [ ] Connect signals
- [ ] Unit tests for EditorUndoSystem

### Phase 2B: Core Commands (Weeks 3-4)

- [ ] Implement `AddItemCommand` for triggers
- [ ] Implement `DeleteItemCommand` for triggers
- [ ] Implement `ModifyPropertyCommand` for triggers
- [ ] Write XML export/import helpers
- [ ] Intercept `addTrigger()` to push commands
- [ ] Intercept `delete_trigger()` to push commands
- [ ] Intercept `saveTrigger()` to push commands
- [ ] Test add/undo/redo trigger
- [ ] Test delete/undo/redo trigger
- [ ] Test modify/undo/redo trigger
- [ ] Unit tests for commands

### Phase 2C: Advanced Commands (Weeks 5-6)

- [ ] Implement `MoveItemCommand`
- [ ] Implement `ToggleActiveCommand`
- [ ] Implement ID tracking system
- [ ] Handle multi-selection deletions
- [ ] Handle folder hierarchies
- [ ] Test move operations
- [ ] Test toggle operations
- [ ] Test group deletions

### Phase 2D: All Item Types (Weeks 7-8)

- [ ] Extend AddItemCommand to aliases
- [ ] Extend AddItemCommand to timers
- [ ] Extend AddItemCommand to scripts
- [ ] Extend AddItemCommand to keys
- [ ] Extend AddItemCommand to actions
- [ ] Same for DeleteItemCommand
- [ ] Same for ModifyPropertyCommand
- [ ] Same for MoveItemCommand
- [ ] Same for ToggleActiveCommand
- [ ] Test all item types

### Phase 2E: Polish (Weeks 9-10)

- [ ] Implement focus-based undo priority
- [ ] Add undo/redo menu with history
- [ ] Implement memory compression
- [ ] Implement debouncing for auto-saves
- [ ] Add user preferences for undo limit
- [ ] Optimize performance
- [ ] Fix UI state synchronization issues
- [ ] Comprehensive testing
- [ ] Performance benchmarks
- [ ] Memory profiling
- [ ] Documentation
- [ ] User guide updates

---

## Testing Strategy

### Unit Tests

**File**: src/test/TTestEditorUndo.cpp

```cpp
class TestEditorUndo : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Basic operations
    void testAddTriggerUndo();
    void testDeleteTriggerUndo();
    void testModifyTriggerUndo();
    void testToggleActiveUndo();

    // Multiple operations
    void testMultipleUndoRedo();
    void testUndoLimit();

    // Hierarchies
    void testGroupDeletion();
    void testFolderWithChildren();

    // Edge cases
    void testUndoRedoClear();
    void testMemoryLeaks();
    void testIDRestoration();

    // All item types
    void testAliasUndo();
    void testTimerUndo();
    void testScriptUndo();
    void testKeyUndo();
    void testActionUndo();
};
```

### Integration Tests

**Manual Test Plan**:

1. **Add/Delete Cycle**
   - Add 5 triggers
   - Delete all 5
   - Undo 5 times → all restored
   - Redo 5 times → all deleted
   - Undo 5 times → all restored again

2. **Modify Properties**
   - Create trigger
   - Modify name 10 times
   - Undo 10 times → original name
   - Redo 10 times → final name

3. **Folder Operations**
   - Create folder
   - Add 10 children
   - Delete folder
   - Undo → folder + all 10 children restored

4. **Move Operations**
   - Create 2 folders A and B
   - Create trigger in A
   - Move to B
   - Undo → trigger back in A
   - Redo → trigger in B

5. **Mixed Operations**
   - Add trigger
   - Modify it
   - Toggle active
   - Move to folder
   - Undo 4 times → trigger deleted
   - Redo 4 times → back to final state

6. **Performance**
   - Create 100 triggers
   - Delete all (should take < 1 second)
   - Undo 100 times (should take < 2 seconds)

7. **Memory**
   - Perform 1000 operations
   - Check memory usage (should be < 50 MB)
   - Clear undo history
   - Memory should be released

### Stress Tests

```cpp
void stressTestUndo() {
    // Create 1000 triggers
    for (int i = 0; i < 1000; i++) {
        addTrigger(false);
    }

    // Undo all
    for (int i = 0; i < 1000; i++) {
        mpUndoSystem->undo();
    }

    // Verify empty
    QVERIFY(triggerCount() == 0);

    // Redo all
    for (int i = 0; i < 1000; i++) {
        mpUndoSystem->redo();
    }

    // Verify restored
    QVERIFY(triggerCount() == 1000);
}
```

---

## Performance Targets

| Operation | Target Time | Max Time |
|-----------|-------------|----------|
| Add trigger + undo push | < 5 ms | < 10 ms |
| Delete trigger + undo push | < 5 ms | < 10 ms |
| Modify trigger + undo push | < 10 ms | < 20 ms |
| Undo operation | < 20 ms | < 50 ms |
| Redo operation | < 20 ms | < 50 ms |
| Delete folder (50 items) | < 100 ms | < 200 ms |

## Memory Targets

| Scenario | Target | Max |
|----------|--------|-----|
| 50 simple triggers in undo | < 500 KB | < 2 MB |
| 50 complex triggers in undo | < 2 MB | < 5 MB |
| 50 folder operations in undo | < 5 MB | < 10 MB |

---

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Performance too slow | Medium | High | Debouncing, compression, profiling |
| Memory usage too high | Low | Medium | Compression, configurable limits |
| ID conflicts after undo | Medium | High | ID tracking system |
| UI sync issues | High | Medium | Thorough testing, use existing slots |
| Pointer bugs | Low | Critical | Use XML (no pointers) |
| Complex hierarchies fail | Medium | High | Leverage existing XML recursion |
| User confusion | Medium | Low | Clear undo text, tooltips |

---

## Success Criteria

### Phase 1 Success

- [ ] Ctrl+Z undoes text changes
- [ ] Ctrl+Y redoes text changes
- [ ] Toolbar buttons work
- [ ] Context menu works
- [ ] Multi-level undo works
- [ ] Zero crashes
- [ ] User feedback positive

### Phase 2 Success

- [ ] Can undo item deletion
- [ ] Can undo item addition
- [ ] Can undo property changes
- [ ] Can undo moves
- [ ] Supports 50 undo levels
- [ ] Performance < 20ms per operation
- [ ] Memory < 5 MB for 50 levels
- [ ] Zero memory leaks
- [ ] Works for all 6 item types
- [ ] Zero crashes
- [ ] Passes all unit tests
- [ ] Passes all integration tests
- [ ] User feedback positive

---

## Timeline

```
Week 1:  Phase 1 implementation + testing
Week 2:  Phase 2A - Foundation
Week 3:  Phase 2B - Core commands (triggers)
Week 4:  Phase 2B - Testing and refinement
Week 5:  Phase 2C - Advanced commands
Week 6:  Phase 2C - Testing
Week 7:  Phase 2D - All item types
Week 8:  Phase 2D - Testing all types
Week 9:  Phase 2E - Polish and optimization
Week 10: Phase 2E - Final testing and docs
Week 11: Buffer for issues
Week 12: Release
```

---

## Open Questions

1. Should undo history persist across editor sessions?
   - **Proposal**: No - clear on editor close
   - **Rationale**: Simpler, less complexity

2. Should there be separate undo stacks per item type?
   - **Proposal**: No - single unified stack
   - **Rationale**: Matches user mental model

3. Should undo work across profiles?
   - **Proposal**: No - one stack per profile
   - **Rationale**: Profiles are independent

4. Should we show undo preview?
   - **Proposal**: Phase 3 feature
   - **Rationale**: Nice to have, not critical

5. Should compressed XML be optional?
   - **Proposal**: Always compress
   - **Rationale**: Memory savings worth CPU cost

---

## Future Enhancements (Phase 3)

- Visual undo history timeline
- Branching undo (like Git)
- Undo preview before applying
- Named checkpoints
- Export/import undo history
- Collaborative undo (multi-user)

---

## References

- [GitHub Issue #707](https://github.com/Mudlet/Mudlet/issues/707)
- [PR #8167](https://github.com/Mudlet/Mudlet/pull/8167) (Draft)
- [PR #8297](https://github.com/Mudlet/Mudlet/pull/8297) (Draft)
- [Qt Undo Framework](https://doc.qt.io/qt-6/qundo.html)
- [Command Pattern](https://refactoring.guru/design-patterns/command)
- [Memento Pattern](https://refactoring.guru/design-patterns/memento)

---

**Document Version**: 1.0
**Last Updated**: 2025-10-25
**Author**: Claude (AI Assistant)
**Status**: Ready for Implementation
