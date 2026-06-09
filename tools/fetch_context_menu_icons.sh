#!/bin/sh
set -eu

# Fetches macOS-style SF Symbol PNGs for all context menu items.
# Source: https://github.com/andrewtavis/sf-symbols-online

BASE_URL="https://raw.githubusercontent.com/andrewtavis/sf-symbols-online/master"
OUT_DIR="${1:-assets/context-menu}"
OUT_DIR_WHITE="${2:-assets/context-menu-white}"
OUT_DIR_APPLE="${3:-assets/apple/context-menu}"
mkdir -p "$OUT_DIR" "$OUT_DIR_WHITE" "$OUT_DIR_APPLE"

ALL_SYMBOLS=""

# Apple Menu
ALL_SYMBOLS="$ALL_SYMBOLS info.circle"           # About Tahoe wlroots
ALL_SYMBOLS="$ALL_SYMBOLS gear"                   # System Settings...
ALL_SYMBOLS="$ALL_SYMBOLS bag"                    # App Store...
ALL_SYMBOLS="$ALL_SYMBOLS clock"                  # Recent Items
ALL_SYMBOLS="$ALL_SYMBOLS xmark.octagon"          # Force Quit...
ALL_SYMBOLS="$ALL_SYMBOLS moon.zzz"              # Sleep
ALL_SYMBOLS="$ALL_SYMBOLS arrow.counterclockwise" # Restart...
ALL_SYMBOLS="$ALL_SYMBOLS power"                  # Shut Down...
ALL_SYMBOLS="$ALL_SYMBOLS lock"                   # Lock Screen
ALL_SYMBOLS="$ALL_SYMBOLS arrow.right.to.line.alt" # Log Out

# Dock Context Menu
ALL_SYMBOLS="$ALL_SYMBOLS arrow.up.doc"                # Open
ALL_SYMBOLS="$ALL_SYMBOLS magnifyingglass"             # Show in Finder
ALL_SYMBOLS="$ALL_SYMBOLS minus.circle"                # Remove from Dock
ALL_SYMBOLS="$ALL_SYMBOLS person.crop.circle.badge.plus" # Open at Login

# Widget Context Menu
ALL_SYMBOLS="$ALL_SYMBOLS square.and.pencil" # Edit Widget
ALL_SYMBOLS="$ALL_SYMBOLS rectangle.3.offgrid" # Small
ALL_SYMBOLS="$ALL_SYMBOLS rectangle.grid.3x2" # Large

# Desktop Context Menu
ALL_SYMBOLS="$ALL_SYMBOLS folder.badge.plus" # New Folder
ALL_SYMBOLS="$ALL_SYMBOLS rectangle.stack"   # Use Stacks
ALL_SYMBOLS="$ALL_SYMBOLS arrow.up.arrow.down" # Sort By
ALL_SYMBOLS="$ALL_SYMBOLS wand.and.stars"    # Clean Up By
ALL_SYMBOLS="$ALL_SYMBOLS sidebar.right"     # Show View Options
ALL_SYMBOLS="$ALL_SYMBOLS photo"             # Change Desktop Background...

# Desktop Icon Context Menu
ALL_SYMBOLS="$ALL_SYMBOLS doc.on.doc"        # Copy
ALL_SYMBOLS="$ALL_SYMBOLS plus.square.on.square" # Duplicate
ALL_SYMBOLS="$ALL_SYMBOLS eye"               # Quick Look
ALL_SYMBOLS="$ALL_SYMBOLS square.and.arrow.up" # Share
ALL_SYMBOLS="$ALL_SYMBOLS trash"             # Move to Trash

# Deduplicate
SYMBOLS=$(echo "$ALL_SYMBOLS" | tr ' ' '\n' | sort -u | tr '\n' ' ')

echo "Fetching context menu SF Symbols..."
COUNT=0
FAIL=0
for sym in $SYMBOLS; do
	out="$OUT_DIR/$sym.png"
	out_white="$OUT_DIR_WHITE/$sym.png"
	code=$(curl -s -o "$out" -w "%{http_code}" "$BASE_URL/glyphs/$sym.png")
	if [ "$code" = "200" ]; then
		COUNT=$((COUNT + 1))
	else
		rm -f "$out"
		FAIL=$((FAIL + 1))
		echo "  MISSING: $sym"
	fi
	code_w=$(curl -s -o "$out_white" -w "%{http_code}" "$BASE_URL/glyphs_white/$sym.png")
	if [ "$code_w" != "200" ]; then
		rm -f "$out_white"
	fi
done

# Copy Apple menu icons to assets/apple/context-menu/
for sym in info.circle gear bag clock xmark.octagon moon.zzz arrow.counterclockwise power lock arrow.right.to.line.alt; do
	cp "$OUT_DIR/$sym.png" "$OUT_DIR_APPLE/$sym.png" 2>/dev/null || true
done

echo "Fetched $COUNT symbols ($FAIL missing)"
echo "  Light:  $OUT_DIR/"
echo "  White:  $OUT_DIR_WHITE/"
echo "  Apple:  $OUT_DIR_APPLE/"
