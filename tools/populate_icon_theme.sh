#!/bin/sh
set -eu

src="${1:-assets}"
theme="${2:-themes/TahoeIcons}"

mkdir -p "$theme/apps/256x256" "$theme/status/64x64" "$theme/places/256x256"

icon_src="$src/icons"
status_src="$src/status"
if [ ! -d "$icon_src" ] && [ -d "$src/apple/icons" ]; then
	icon_src="$src/apple/icons"
fi
if [ ! -d "$status_src" ] && [ -d "$src/apple/status" ]; then
	status_src="$src/apple/status"
fi

if [ -d "$icon_src" ]; then
	for file in "$icon_src"/*.png; do
		[ -e "$file" ] || continue
		name="$(basename "$file")"
		cp "$file" "$theme/apps/256x256/$name"
		cp "$file" "$theme/places/256x256/$name"
	done
fi

if [ -d "$status_src" ]; then
	for file in "$status_src"/*.png; do
		[ -e "$file" ] || continue
		cp "$file" "$theme/status/64x64/$(basename "$file")"
	done
fi

printf 'Populated %s from %s\n' "$theme" "$src"
