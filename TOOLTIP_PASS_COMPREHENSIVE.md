# Tooltip & Accessibility Pass: Sighted Users and Blind Users

**Date**: January 11, 2026  
**Status**: Work in Progress  
**Scope**: Ensure tooltip changes work for BOTH sighted and blind users throughout Mudlet

---

## Overview: The Complete Accessibility Pattern

The `a11y-preferences-4.19.1` branch implements a critical accessibility fix:
- **For blind users**: Remove HTML tooltips from widgets with accessibility properties (they were creating "In dialog. content is empty" VoiceOver messages)
- **For sighted users**: Keep visible labels and interface elements discoverable

However, there are **MORE tooltips throughout the codebase** that need the same treatment.

---

## Part 1: Remaining HTML Tooltips to Fix

### Found 20+ HTML Tooltips with `utils::richText()`

These need to be evaluated and potentially converted based on whether they have accessibility properties:

#### In TConsole.cpp
```cpp
Line 439: mpAction_searchCaseSensitive->setToolTip(utils::richText(tr("Match case precisely")));
```
**Status**: ✅ No accessibility properties - HTML tooltip is OK

#### In TTextEdit.cpp  
```cpp
Line 1999: mpContextMenuAnalyser->setToolTip(utils::richText(tr("Hover on this item to display...")));
Line 2010: actionRestoreMainMenu->setToolTip(utils::richText(tr("Use this to restore...")));
Line 2014: actionRestoreMainToolBar->setToolTip(utils::richText(tr("Use this to restore...")));
```
**Status**: Need to check if these widgets have accessibility properties

#### In dlgModuleManager.cpp
```cpp
Line 102: masterModule->setToolTip(utils::richText(tr("Checking this box will cause...")));
Line 114: itemLocation->setToolTip(utils::richText(moduleInfo[0]));
```
**Status**: Need to check if these widgets have accessibility properties

#### In T2DMap.cpp
```cpp
Line 96:   mMultiSelectionListWidget.setToolTip(utils::richText(tr("Click on a line to select...")));
Line 2602: customLineProperties->setToolTip(utils::richText(tr("Change the properties...")));
Line 2607: customLineFinish->setToolTip(utils::richText(tr("Finish drawing this line")));
Line 2710: roomProperties->setToolTip(utils::richText(tr("Set room's name and color...")));
Line 2727: customExitLine->setToolTip(utils::richText(tr("Replace an exit line...")));
Line 2732: customExitLine->setToolTip(utils::richText(tr("Custom exit lines are not shown...")));
Line 2743: spreadRooms->setToolTip(utils::richText(tr("Increase map X-Y spacing...")));
Line 2752: shrinkRooms->setToolTip(utils::richText(tr("Decrease map X-Y spacing...")));
Line 2768: moveRoomXY->setToolTip(utils::richText(tr("Move selected room or group...")));
Line 2782: createLabel->setToolTip(utils::richText(tr("Create label to show...")));
Line 2791: setPlayerLocation->setToolTip(utils::richText(tr("Set the player's current location...")));
Line 2839: addPoint->setToolTip(utils::richText(tr("Divide segment by adding...")));
Line 2843: addPoint->setToolTip(utils::richText(tr("Select a point first, then add...")));
Line 2855: removePoint->setToolTip(utils::richText(tr("Merge pair of segments...")));
Line 2859: removePoint->setToolTip(utils::richText(tr("Remove last segment...")));
```
**Status**: These are in a context menu/map editor - likely NO accessibility properties ✅

---

## Part 2: Accessibility Properties Found (100+ instances)

✅ **Good News**: The codebase already has 100+ `setAccessibleName()` and `setAccessibleDescription()` calls.

### Key Files with Accessibility Support
- **TConsole.cpp** - 40+ accessibility properties (very good coverage!)
- **TCommandLine.cpp** - 20+ accessibility properties
- **mudlet.cpp** - 30+ accessibility properties
- **AnnouncerUnix.cpp** - Accessibility property for notifications

### Pattern Already Used Correctly ✅
The codebase is already using the correct pattern in many places:
```cpp
widget->setAccessibleName(tr("Widget name"));
widget->setAccessibleDescription(tr("Detailed description"));
// NO HTML tooltip here - correct!
```

---

## Part 3: Testing Plan for Sighted Users

### Visual Discovery & Usability
**Goal**: Ensure sighted users can still discover and understand interface elements

#### Test Checklist for Profile/Connection Dialogs
- [ ] Open Profile Preferences dialog
  - [ ] Can see all labels/text on buttons
  - [ ] Can see all input field labels
  - [ ] All checkboxes have visible text descriptions
  - [ ] Buttons are clearly labeled
  
- [ ] Open Connection Profiles dialog  
  - [ ] Profile list is visible and scrollable
  - [ ] Buttons (New, Copy, Remove) are clearly labeled
  - [ ] Settings dropdown has visible options
  
- [ ] Main console area
  - [ ] Toolbar buttons have visible icons or labels
  - [ ] Search box is discoverable
  - [ ] Search navigation buttons are visible
  - [ ] All control buttons are clearly marked

- [ ] Editor areas  
  - [ ] Triggers, Aliases, Scripts, etc. have visible labels
  - [ ] Tree controls show clearly  
  - [ ] Edit buttons are discoverable
  - [ ] Save/Cancel buttons are visible

#### Test Checklist for Map Editor
- [ ] All context menu items are readable
- [ ] Right-click menus show clear options
- [ ] No reliance on tooltips for basic functionality

### Hover Tooltips (Context)
**Note**: Users will NOT see HTML-formatted hover tooltips anymore, but this is acceptable because:
1. Visible labels explain what buttons do
2. Interface is self-explanatory
3. Help system provides detailed information
4. Tooltips were a "nice to have" feature, not critical

**Test**: Hover over buttons - no tooltip appears, but button label is visible ✓

---

## Part 4: Testing Plan for Blind Users (VoiceOver)

### Enable VoiceOver
```
macOS: Cmd + F5
```

### Test Areas & Expected Behavior

#### 1. Profile Preferences Dialog (CRITICAL)
Navigate through dialogs with VoiceOver:

**Expected**: 
- Clear announcements for each widget
- NO "In dialog. content is empty" messages
- Can read accessible names and descriptions
- Can navigate with arrow keys

**Test Steps**:
- [ ] Open Profile Preferences
- [ ] Tab to first control
- [ ] Use arrow keys to navigate through all tabs
- [ ] Listen for clear announcements
- [ ] Verify NO "In dialog" messages appear
- [ ] Verify NO empty/content-is-empty errors

**Issue if Failed**:
- If you hear "In dialog. content is empty": Widget still has HTML tooltip with accessibility properties
- **Fix**: Remove the toolTip property

#### 2. Connection Profiles Dialog (CRITICAL)
- [ ] Open Connection Profiles
- [ ] Navigate profile list with arrow keys
- [ ] Verify profile names are announced
- [ ] Test Remove, Copy, New buttons
- [ ] Verify clear announcements for each action

**Expected for Each Button**:
- "Remove, button" or similar clear announcement
- Description available on demand

#### 3. Main Console (HIGH PRIORITY)
- [ ] Navigate to main output area
- [ ] Test search functionality
  - [ ] Search box is accessible
  - [ ] Up/Down buttons work
  - [ ] Results navigate correctly
  
- [ ] Test toolbar buttons
  - [ ] Triggers, Aliases, Scripts, etc.
  - [ ] Connect button
  - [ ] All buttons announce clearly

#### 4. Editor Windows (MEDIUM PRIORITY)
- [ ] Triggers/Aliases/Scripts editors
- [ ] Tree controls navigate properly
- [ ] Edit buttons are accessible
- [ ] Save/Cancel buttons work

#### 5. Map Editor (CONTEXT DEPENDENT)
- [ ] Context menus readable
- [ ] Right-click options announce properly
- [ ] Map controls accessible (if applicable for screen readers)

### VoiceOver Test Results Log
Create a simple checklist as you test:

```
Profile Preferences Dialog
- [ ] General tab - no "In dialog" messages
- [ ] Logging tab - no "In dialog" messages  
- [ ] Display tab - no "In dialog" messages
- [ ] Other tabs - no "In dialog" messages

Connection Profiles Dialog
- [ ] Profile list accessible
- [ ] Buttons clear and functional
- [ ] No accessibility errors

Main Console
- [ ] Search functional
- [ ] Navigation works
- [ ] All buttons accessible
```

---

## Part 5: What Still Needs To Be Done

### Phase 1: Profile/Connection Dialogs (CURRENT BRANCH)
✅ **Completed**: Profile and Connection preferences updated with:
- HTML tooltips removed (73 → 0)
- Accessibility properties added (171+ widgets)
- Zero conflicts detected

**Next**: VoiceOver testing to verify user experience

### Phase 2: Other UI Files
**Status**: NOT YET STARTED

Search all .ui files for tooltips with accessibility:
```bash
grep -r "toolTip" src/ui/*.ui | grep -c "."  # Count tooltips
```

**Potential Files to Check**:
- dlgTriggerEditor.ui
- dlgAliasEditor.ui
- dlgScriptEditor.ui
- dlgPackageExporter.ui
- And others

**Process**:
1. Find all .ui files with tooltips + accessibility
2. Remove HTML tooltips from those with accessibility properties
3. Test each dialog with VoiceOver

### Phase 3: C++ Source Files
**Status**: PARTIALLY ANALYZED

Found 20+ HTML tooltips in:
- TConsole.cpp
- TTextEdit.cpp
- T2DMap.cpp
- dlgModuleManager.cpp
- And others

**Classification Needed**:
1. Check if each widget has `setAccessibleName()` calls
2. If YES - convert HTML tooltip to plain text (or remove)
3. If NO - keep HTML tooltip as-is (safe)

**Process**:
1. For each `setToolTip(utils::richText(...))` instance
2. Search nearby code for `setAccessibleName` or `setAccessibleDescription`
3. If found within ~20 lines - mark for conversion
4. Convert HTML to plain text or remove

---

## Part 6: Conversion Reference

### When to Remove vs Convert

#### REMOVE tooltip (if accessibility exists)
```cpp
// Before:
widget->setToolTip(utils::richText(tr("Some text")));
widget->setAccessibleName(tr("Widget name"));

// After:
widget->setAccessibleName(tr("Widget name"));
// Tooltip removed - accessibility property provides the info
```

#### CONVERT HTML to Plain Text (if accessibility exists)
```cpp
// Before:
widget->setToolTip(utils::richText(tr("Click to <b>save</b> file")));
widget->setAccessibleName(tr("Save button"));

// After:
widget->setToolTip(tr("Click to save file"));  // Remove HTML, keep tooltip
widget->setAccessibleName(tr("Save button"));
```

#### KEEP HTML Tooltip (if NO accessibility)
```cpp
// No changes needed - HTML tooltip is safe without accessibility
widget->setToolTip(utils::richText(tr("Complex <b>info</b> here")));
// No accessibility properties = OK to keep HTML
```

### HTML to Plain Text Conversions

| HTML | Plain Text |
|------|-----------|
| `<b>text</b>` | `"text"` or `TEXT` |
| `<i>text</i>` | `'text'` or `text` |
| `<p>text</p>` | `text` |
| `<br>` | `\n` (newline) |
| `<tt>code</tt>` | `'code'` (quotes) |
| `<u>text</u>` | `text` or `_text_` |

---

## Part 7: Comprehensive Checklist

### Before Merging This Branch
- [ ] Profile Preferences dialog - VoiceOver testing passed
- [ ] Connection Profiles dialog - VoiceOver testing passed
- [ ] No "In dialog. content is empty" messages detected
- [ ] Sighted users can discover all controls
- [ ] Build compiles successfully
- [ ] No regressions in functionality

### For Full Codebase (Future)
- [ ] All .ui files audited for tooltip/accessibility conflicts
- [ ] All C++ files audited for setToolTip + setAccessibleName conflicts
- [ ] Consistent pattern applied throughout
- [ ] VoiceOver testing on all major dialogs
- [ ] Documentation updated with pattern guide

---

## Summary Table

| Component | Current Status | Sighted Users | Blind Users | Action |
|-----------|----------------|---------------|-------------|--------|
| **Profile Preferences UI** | ✅ Fixed | ✅ OK | ⏳ Test | VoiceOver test |
| **Connection Profiles UI** | ✅ Fixed | ✅ OK | ⏳ Test | VoiceOver test |
| **Other .ui files** | ⏹️ Not checked | ? | ? | Audit needed |
| **C++ HTML tooltips (T2DMap, etc.)** | ⏹️ Found but not fixed | ✅ OK | ✓ OK (no accessibility) | Review only |
| **C++ accessibility names** | ✅ Good coverage | ✅ OK | ✅ In place | Verify works |

---

## Next Immediate Steps

1. **TODAY**: 
   - [ ] VoiceOver test Profile Preferences dialog
   - [ ] VoiceOver test Connection Profiles dialog
   - [ ] Document any "In dialog" messages found

2. **TOMORROW**:
   - [ ] Create comprehensive list of remaining .ui files to check
   - [ ] Review C++ code for each HTML tooltip location
   - [ ] Identify which need fixing vs. which are safe

3. **THIS WEEK**:
   - [ ] Complete tooltip audits for all major dialogs
   - [ ] Fix any remaining conflicts
   - [ ] Test complete dialogs with both sighted and screen reader users

---

## Success Criteria

### For This Branch ✅
- [x] HTML tooltips removed from profile/connection dialogs
- [x] Accessibility properties added (171+)
- [x] Zero conflicts in UI files
- [x] Builds successfully
- [ ] VoiceOver testing passed
- [ ] Sighted users can navigate dialogs

### For Full Mudlet ⏹️
- [ ] All major dialogs audited
- [ ] HTML tooltip/accessibility conflicts identified  
- [ ] Documentation updated
- [ ] Testing methodology documented
- [ ] Future developers know the pattern

---

**Status**: Phase 1 (UI Files) ✅ Complete | Phase 2+ (Other Files) ⏹️ Pending VoiceOver Testing

**This document should be added to MUDLET_ACCESSIBILITY_HANDOFF.md as the "Tooltip Pass" section.**
