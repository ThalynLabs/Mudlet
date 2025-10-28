# Migration Plan: EditorUndoSystem → Qt's QUndoStack Framework

## Overview
Migrate Mudlet's custom undo system (3,120 lines) to Qt's built-in QUndoStack framework, reducing code by ~70% while gaining professional features.

## Benefits
- **2,200+ fewer lines** of custom code to maintain
- **Automatic action integration** via `createUndoAction()`/`createRedoAction()`
- **Future features**: QUndoView (visual history), QUndoGroup (multi-profile), command merging
- **Battle-tested** Qt framework instead of custom implementation

## Phase 1: Create Base Infrastructure (2-3 days)

### 1.1 Create MudletCommand Base Class
- File: `src/MudletCommand.h` (~100 lines)
- Extends `QUndoCommand` with Mudlet-specific features:
  - `virtual EditorViewType viewType() const = 0`
  - `virtual QList<int> affectedItemIDs() const = 0`
  - `virtual void remapItemID(int oldID, int newID) = 0`
- Maintains `Host* mpHost` member

### 1.2 Create MudletUndoStack Wrapper
- File: `src/MudletUndoStack.h` + `.cpp` (~200 lines)
- Extends `QUndoStack` to add:
  - `remapItemIDs()` method for ID tracking
  - `itemsChanged` signal for UI updates
  - Override `undo()`/`redo()` to emit custom signals

### 1.3 Add to dlgTriggerEditor
- Add `MudletUndoStack* mpQtUndoStack` alongside existing `mpUndoSystem`
- Connect signals to existing slots
- Both systems will run in parallel during migration

## Phase 2: Migrate Commands (1-2 weeks)

Convert commands one at a time, in order of complexity:

### 2.1 ToggleActiveCommand (Day 1-2)
- Simplest command (18 usages)
- Good proof-of-concept
- Dual-push to both systems for validation

### 2.2 MoveItemCommand (Day 3)
- Simple logic, tests batch operations
- Only 1 direct usage but tested via drag-and-drop

### 2.3 AddItemCommand (Day 4-5)
- 6 usages, has ID change tracking
- Test ID remapping thoroughly

### 2.4 ModifyPropertyCommand (Day 6-7)
- 6 usages, uses XML snapshots
- Verify compression still works

### 2.5 DeleteItemCommand (Day 8-10)
- Most complex (6 usages)
- Parent-child ID remapping during undo
- Extensive testing needed

### 2.6 BatchCommand → Macro (Day 11)
- Convert `beginBatch()`/`endBatch()` to `beginMacro()`/`endMacro()`
- 2 call sites in TTreeWidget batch move code

## Phase 3: Testing & Validation (1 week)

### 3.1 Verification Testing
- Compare outputs from both systems
- Add logging to verify identical behavior
- Test all 6 item types: triggers, aliases, timers, scripts, keys, actions

### 3.2 Complex Scenarios
- Multi-item drag-and-drop with undo/redo
- Delete item → undo → redo with ID changes
- Parent-child relationship preservation
- Batch operations (macros)
- Switching between editor views
- Undo limit enforcement

### 3.3 Edge Cases
- Undo/redo after profile switch
- XML compression/decompression
- Item ID remapping chains

## Phase 4: Cutover (1 day)

### 4.1 Remove Old System
- Delete `EditorUndoSystem.h` and `.cpp` (3,120 lines)
- Remove dual-push code (37 sites)
- Rename `mpQtUndoStack` → `mpUndoStack`

### 4.2 Update Signal Connections
- 5 connection sites already compatible
- Update naming: `beginBatch` → `beginMacro`, `endBatch` → `endMacro`
- Replace `clear()` with new stack creation

### 4.3 Code Cleanup
- Remove EditorCommand base class
- Update includes and forward declarations
- Remove deprecated comments

## Changes Required

### Files to Create
- `src/MudletCommand.h` (~100 lines)
- `src/MudletUndoStack.h` (~50 lines)
- `src/MudletUndoStack.cpp` (~150 lines)
- `src/commands/MudletToggleActiveCommand.h` + `.cpp`
- `src/commands/MudletMoveItemCommand.h` + `.cpp`
- `src/commands/MudletAddItemCommand.h` + `.cpp`
- `src/commands/MudletModifyPropertyCommand.h` + `.cpp`
- `src/commands/MudletDeleteItemCommand.h` + `.cpp`

### Files to Modify
- `src/dlgTriggerEditor.h` (add mpQtUndoStack, update includes)
- `src/dlgTriggerEditor.cpp` (37 command creation sites, 5 signal connections)
- `src/TTreeWidget.cpp` (2 batch operation sites)
- `src/CMakeLists.txt` (add new source files)

### Files to Delete (after cutover)
- `src/EditorUndoSystem.h` (301 lines)
- `src/EditorUndoSystem.cpp` (2,820 lines)

## Risk Mitigation

### Low Risk Strategy
- Parallel system approach allows safe rollback
- Incremental command-by-command migration
- Extensive testing before cutover
- Keep old system until validation complete

### Validation Checks
- Assert both systems produce identical results
- Log all operations for comparison
- Automated tests for each command type
- Manual testing of complex scenarios

## Timeline

**Total Estimate: 3-4 weeks**
- Phase 1 (Infrastructure): 2-3 days
- Phase 2 (Commands): 1-2 weeks
- Phase 3 (Testing): 1 week
- Phase 4 (Cutover): 1 day

## Success Criteria

✅ All 6 command types migrated and tested
✅ ID remapping works correctly
✅ Batch operations (macros) work as before
✅ All signals emit correctly
✅ Undo/redo behavior identical to old system
✅ No memory leaks or crashes
✅ UI updates correctly after undo/redo
✅ 2,200+ lines of code removed

## Technical Details

### MudletCommand Base Class Design

```cpp
// File: src/MudletCommand.h
class MudletCommand : public QUndoCommand {
public:
    explicit MudletCommand(Host* host, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), mpHost(host) {}

    explicit MudletCommand(const QString& text, Host* host, QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent), mpHost(host) {}

    virtual ~MudletCommand() = default;

    // Mudlet-specific extensions
    virtual EditorViewType viewType() const = 0;
    virtual QList<int> affectedItemIDs() const = 0;
    virtual void remapItemID(int oldID, int newID) = 0;

    // QUndoCommand overrides (still pure virtual)
    void undo() override = 0;
    void redo() override = 0;

protected:
    Host* mpHost;
};
```

### MudletUndoStack Wrapper Design

```cpp
// File: src/MudletUndoStack.h
class MudletUndoStack : public QUndoStack {
    Q_OBJECT

public:
    explicit MudletUndoStack(QObject* parent = nullptr);

    void remapItemIDs(int oldID, int newID);

    void undo() override;
    void redo() override;

signals:
    void itemsChanged(EditorViewType viewType, QList<int> affectedItemIDs);

protected:
    using QUndoStack::command;  // Access protected method
};
```

### Example Command Migration

```cpp
// Before: ToggleActiveCommand in EditorUndoSystem
class ToggleActiveCommand : public EditorCommand {
    // ... implementation
};

// After: MudletToggleActiveCommand with QUndoCommand
class MudletToggleActiveCommand : public MudletCommand {
public:
    MudletToggleActiveCommand(EditorViewType viewType, int itemID,
                              bool oldState, bool newState,
                              const QString& itemName, Host* host)
        : MudletCommand(generateText(viewType, itemName, newState), host)
        , mViewType(viewType)
        , mItemID(itemID)
        , mOldActiveState(oldState)
        , mNewActiveState(newState)
        , mItemName(itemName)
    {}

    void undo() override {
        setItemActiveState(mItemID, mOldActiveState);
    }

    void redo() override {
        setItemActiveState(mItemID, mNewActiveState);
    }

    EditorViewType viewType() const override { return mViewType; }
    QList<int> affectedItemIDs() const override { return {mItemID}; }

    void remapItemID(int oldID, int newID) override {
        if (mItemID == oldID) {
            mItemID = newID;
        }
    }

private:
    EditorViewType mViewType;
    int mItemID;
    bool mOldActiveState;
    bool mNewActiveState;
    QString mItemName;

    void setItemActiveState(int itemID, bool active);
    static QString generateText(EditorViewType viewType,
                               const QString& itemName,
                               bool newState);
};
```

### Dual-Push Pattern During Migration

```cpp
// In dlgTriggerEditor.cpp during Phase 2
void dlgTriggerEditor::someFunction() {
    // ... create command parameters ...

    // Push to old system (for comparison)
    auto oldCmd = std::make_unique<ToggleActiveCommand>(
        viewType, itemID, oldState, newState, itemName, mpHost
    );
    mpUndoSystem->pushCommand(std::move(oldCmd));

    // Push to new Qt system (for validation)
    auto* newCmd = new MudletToggleActiveCommand(
        viewType, itemID, oldState, newState, itemName, mpHost
    );
    mpQtUndoStack->push(newCmd);  // Qt takes ownership
}
```

## API Mapping

| Current EditorUndoSystem | Qt QUndoStack | Notes |
|--------------------------|---------------|-------|
| `pushCommand(unique_ptr)` | `push(QUndoCommand*)` | Change from unique_ptr to raw pointer |
| `undo()` | `undo()` | ✅ Same |
| `redo()` | `redo()` | ✅ Same |
| `canUndo()` | `canUndo()` | ✅ Same |
| `canRedo()` | `canRedo()` | ✅ Same |
| `undoText()` | `undoText()` | ✅ Same |
| `redoText()` | `redoText()` | ✅ Same |
| `setUndoLimit()` | `setUndoLimit()` | ✅ Same |
| `beginBatch()` | `beginMacro()` | ⚠️ Rename |
| `endBatch()` | `endMacro()` | ⚠️ Rename |
| `clear()` | `clear()` or create new stack | ⚠️ Different approach |
| `remapItemIDs()` | Custom in MudletUndoStack | ❌ Must implement |
| Signal: `itemsChanged` | Custom in MudletUndoStack | ❌ Must implement |

## Questions Before Starting

1. **Approve phased approach?** Or prefer direct replacement?
2. **Timeline acceptable?** 3-4 weeks of development time?
3. **Future features desired?** QUndoView visual history, QUndoGroup multi-profile support?
4. **Testing scope?** Manual only or add automated tests?

## References

- Qt QUndoStack Documentation: https://doc.qt.io/qt-6/qundostack.html
- Qt QUndoCommand Documentation: https://doc.qt.io/qt-6/qundocommand.html
- Qt Undo Framework Overview: https://doc.qt.io/qt-6/qundo.html
- Current Implementation: `src/EditorUndoSystem.h` and `.cpp`
- Usage Analysis: See `UNDO_FRAMEWORK_EVALUATION.md` for detailed comparison
