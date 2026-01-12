#!/usr/bin/env python3
"""
Fix tooltip/accessibility conflicts in Qt UI files for VoiceOver compatibility.

When a widget has both:
- HTML tooltip property
- accessibility properties (accessibleName, accessibleDescription)

VoiceOver on macOS interprets the tooltip as a separate dialog and announces
"In dialog. content is empty" which is confusing for screen reader users.

This script removes the tooltip from such widgets, since the accessibility
properties provide the necessary information to screen readers.
"""

import xml.etree.ElementTree as ET
import sys
from pathlib import Path


def fix_ui_file(filepath):
    """
    Remove tooltips from widgets that have accessibility properties.
    
    Returns: (modified_count, total_conflicts_found)
    """
    print(f"\nProcessing: {filepath}")
    
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
    except Exception as e:
        print(f"  ERROR: Failed to parse XML: {e}")
        return 0, 0
    
    modified = 0
    found = 0
    
    # Find all widgets (no namespace in these UI files)
    for widget in root.findall('.//widget'):
        widget_name = widget.get('name', 'unknown')
        
        # Check for tooltip
        tooltip_elem = widget.find("property[@name='toolTip']")
        if tooltip_elem is None:
            continue
        
        # Check for accessibility properties
        has_accessible_name = widget.find("property[@name='accessibleName']") is not None
        has_accessible_desc = widget.find("property[@name='accessibleDescription']") is not None
        
        if not (has_accessible_name or has_accessible_desc):
            continue
        
        # Found a conflict
        found += 1
        
        # Get tooltip content for reference
        tooltip_text_elem = tooltip_elem.find('string')
        tooltip_text = tooltip_text_elem.text if tooltip_text_elem is not None else "[empty]"
        
        # Remove the tooltip
        widget.remove(tooltip_elem)
        modified += 1
        
        print(f"  ✓ Removed tooltip from: {widget_name}")
        if tooltip_text and len(tooltip_text) > 60:
            print(f"    (was: {tooltip_text[:60]}...)")
        elif tooltip_text:
            print(f"    (was: {tooltip_text})")
    
    # Write back if modified
    if modified > 0:
        try:
            tree.write(filepath, encoding='UTF-8', xml_declaration=True)
            print(f"  ✅ Saved: {modified} tooltips removed")
        except Exception as e:
            print(f"  ERROR: Failed to write file: {e}")
            return 0, found
    
    return modified, found


def main():
    """Main entry point."""
    workspace = Path("/Users/threadweaver/GitHub/Mudlet")
    ui_dir = workspace / "src" / "ui"
    
    # Files to check (priority based on docs)
    target_files = [
        ui_dir / "profile_preferences.ui",
        ui_dir / "connection_profiles.ui",
    ]
    
    total_modified = 0
    total_conflicts = 0
    
    print("=" * 70)
    print("Mudlet UI Tooltip/Accessibility Conflict Fixer")
    print("=" * 70)
    
    for filepath in target_files:
        if filepath.exists():
            modified, found = fix_ui_file(filepath)
            total_modified += modified
            total_conflicts += found
        else:
            print(f"\nWARNING: File not found: {filepath}")
    
    print("\n" + "=" * 70)
    print(f"SUMMARY:")
    print(f"  Total conflicts found: {total_conflicts}")
    print(f"  Total tooltips removed: {total_modified}")
    print("=" * 70)
    
    if total_modified > 0:
        print("\n✅ Tooltip conflicts fixed!")
        print("\nNext steps:")
        print("  1. Build Mudlet: cd /Users/threadweaver/GitHub/Mudlet/build && make -j$(sysctl -n hw.ncpu)")
        print("  2. Test with VoiceOver (Cmd+F5)")
        print("  3. Verify no 'In dialog. content is empty' messages appear")
    
    return 0 if total_modified == total_conflicts else 1


if __name__ == '__main__':
    sys.exit(main())
