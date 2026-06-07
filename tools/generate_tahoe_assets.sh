#!/bin/sh
set -eu

out="${1:-assets}"
icons="$out/icons"
status="$out/status"
mkdir -p "$icons" "$status"

make_icon() {
	name="$1"
	label="$2"
	top="$3"
	bottom="$4"
	convert -size 256x256 gradient:"$top"-"$bottom" \
		-alpha set \
		-fill none -stroke "rgba(255,255,255,0.60)" -strokewidth 4 \
		-draw "roundrectangle 4,4 252,252 54,54" \
		-fill "rgba(255,255,255,0.92)" -stroke none \
		-gravity center -font DejaVu-Sans-Bold -pointsize 112 \
		-annotate +0+8 "$label" \
		"$icons/$name.png"
}

make_status() {
	name="$1"
	label="$2"
	convert -size 96x96 xc:none \
		-fill white -gravity center -font DejaVu-Sans-Bold -pointsize 54 \
		-annotate +0+5 "$label" \
		"$status/$name.png"
}

make_icon tahoe-menu T "#f7fbff" "#d7e8ff"
convert -size 96x96 xc:none \
	-fill white -gravity center -font DejaVu-Sans-Bold -pointsize 70 \
	-annotate +0+7 T \
	"$out/tahoe-menu.png"
make_icon finder T "#1aa7ff" "#87ddff"
make_icon launchpad T "#fbfdff" "#d9e1e8"
make_icon safari T "#2e9eff" "#eef8ff"
make_icon messages T "#36d45d" "#b9ffca"
make_icon mail T "#2578d8" "#8fd1ff"
make_icon maps T "#55d979" "#3f8cff"
make_icon photos T "#ff4b6b" "#ffd45f"
make_icon facetime T "#28d85c" "#a6ffbf"
make_icon phone T "#25c65c" "#8fffaa"
make_icon calendar T "#ffffff" "#e8ebef"
make_icon contacts T "#c8c1a5" "#f6f1dc"
make_icon reminders T "#ffffff" "#e8eef8"
make_icon notes T "#fff37a" "#fffbe1"
make_icon tv T "#26262a" "#0c0d11"
make_icon music T "#ff2d55" "#ff879d"
make_icon rocket T "#ff5a3d" "#ffd1c6"
make_icon app-store T "#1976ff" "#73c7ff"
make_icon calculator T "#32343a" "#d6d8dd"
make_icon settings T "#8f969e" "#d8dbe0"
make_icon desktop-preview T "#6ba7e8" "#d8ecff"
make_icon trash T "#dfe5ea" "#aeb7c1"

for file in "$icons"/*.png; do
	base="$(basename "$file" .png)"
	case "$base" in
		*-dark) continue ;;
	esac
	convert "$file" -modulate 78,90,100 "$icons/$base-dark.png"
done

make_status battery.100 B
make_status wifi W
make_status magnifyingglass S
make_status control-center C
make_status weather S

printf 'Generated Tahoe placeholder assets in %s\n' "$out"
