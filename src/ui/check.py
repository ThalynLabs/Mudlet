import xml.etree.ElementTree as ET

tree = ET.parse('profile_preferences.ui')
root = tree.getroot()

conflicts = 0
remaining_tooltips = 0

for widget in root.findall('.//widget'):
    has_tooltip = False
    has_accessible = False
    
    for prop in widget.findall('property'):
        prop_name = prop.get('name')
        if prop_name == 'toolTip':
            has_tooltip = True
        elif prop_name in ('accessibleName', 'accessibleDescription'):
            has_accessible = True
    
    if has_tooltip:
        remaining_tooltips += 1
        if has_accessible:
            conflicts += 1
            print(f"CONFLICT: {widget.get('name')}")

print(f"\nRemaining conflicts: {conflicts}")
print(f"Total remaining tooltips: {remaining_tooltips}")
