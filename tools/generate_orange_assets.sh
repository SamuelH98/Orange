#!/bin/sh
set -eu

out="${1:-assets}"
icons="$out/icons"
status="$out/status"
mkdir -p "$icons" "$status"

make_wallpaper() {
	name="$1"
	top="$2"
	bottom="$3"
	mark="$4"
	stroke="$5"
	shadow="$6"
	convert -size 5120x3200 gradient:"$top"-"$bottom" \
		-alpha set \
		-gravity center -font DejaVu-Sans-Bold \
		-fill "$shadow" -stroke none -pointsize 1320 \
		-annotate +30+30 O \
		-fill "$mark" -stroke "$stroke" -strokewidth 7 -pointsize 1320 \
		-annotate +0+0 O \
		"$out/$name"
}

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

make_status_battery() {
	convert -size 96x54 xc:none \
		-fill none -stroke white -strokewidth 6 \
		-draw "roundrectangle 8,11 78,43 8,8" \
		-fill white -stroke none \
		-draw "roundrectangle 82,21 90,33 2,2" \
		-draw "roundrectangle 18,19 68,35 4,4" \
		"PNG32:$status/battery.100.png"
}

make_status_wifi() {
	convert -size 96x72 xc:none \
		-fill none -stroke white -strokewidth 7 \
		-draw "arc 9,2 87,80 214,326" \
		-draw "arc 27,19 69,72 214,326" \
		-draw "arc 39,34 57,64 214,326" \
		-fill white -stroke none \
		-draw "circle 48,62 48,67" \
		"PNG32:$status/wifi.png"
}

make_status_search() {
	convert -size 96x96 xc:none \
		-fill none -stroke white -strokewidth 8 \
		-draw "circle 42,42 42,21" \
		-draw "line 58,58 75,75" \
		"PNG32:$status/magnifyingglass.png"
}

make_status_control_center() {
	convert -size 96x96 xc:none \
		-fill none -stroke white -strokewidth 7 \
		-draw "roundrectangle 18,25 78,42 8,8" \
		-draw "roundrectangle 18,56 78,73 8,8" \
		-fill white -stroke none \
		-draw "circle 39,33 39,39" \
		-draw "circle 61,65 61,71" \
		"PNG32:$status/control-center.png"
}

make_status_weather() {
	convert -size 96x96 xc:none \
		-fill white -stroke none \
		-draw "circle 61,33 61,16" \
		-fill none -stroke white -strokewidth 6 \
		-draw "line 61,7 61,16" \
		-draw "line 61,50 61,59" \
		-draw "line 34,33 43,33" \
		-draw "line 79,33 88,33" \
		-draw "line 42,14 48,20" \
		-draw "line 74,46 80,52" \
		-draw "line 42,52 48,46" \
		-draw "line 74,20 80,14" \
		-fill white -stroke none \
		-draw "circle 39,61 39,42" \
		-draw "circle 57,61 57,46" \
		-draw "rectangle 25,58 72,75" \
		"PNG32:$status/weather.png"
}

make_icon orange-menu O "#f7fbff" "#d7e8ff"
convert -size 96x96 xc:none \
	-fill white -gravity center -font DejaVu-Sans-Bold -pointsize 70 \
	-annotate +0+7 O \
	"$out/orange-menu.png"
make_wallpaper wallpaper.png "#ffd36d" "#f56a00" \
	"rgba(255,248,232,0.68)" "rgba(120,45,0,0.20)" "rgba(104,38,0,0.16)"
make_wallpaper wallpaper-dark.png "#2b0d03" "#a93400" \
	"rgba(255,131,28,0.54)" "rgba(255,221,174,0.16)" "rgba(0,0,0,0.32)"
make_icon files O "#1aa7ff" "#87ddff"
make_icon launcher O "#fbfdff" "#d9e1e8"
make_icon browser O "#2e9eff" "#eef8ff"
make_icon messages O "#36d45d" "#b9ffca"
make_icon mail O "#2578d8" "#8fd1ff"
make_icon maps O "#55d979" "#3f8cff"
make_icon photos O "#ff4b6b" "#ffd45f"
make_icon video O "#28d85c" "#a6ffbf"
make_icon phone O "#25c65c" "#8fffaa"
make_icon calendar O "#ffffff" "#e8ebef"
make_icon contacts O "#c8c1a5" "#f6f1dc"
make_icon reminders O "#ffffff" "#e8eef8"
make_icon notes O "#fff37a" "#fffbe1"
make_icon tv O "#26262a" "#0c0d11"
make_icon music O "#ff2d55" "#ff879d"
make_icon rocket O "#ff5a3d" "#ffd1c6"
make_icon software O "#1976ff" "#73c7ff"
make_icon calculator O "#32343a" "#d6d8dd"
make_icon settings O "#8f969e" "#d8dbe0"
make_icon desktop-preview O "#6ba7e8" "#d8ecff"
make_icon trash O "#dfe5ea" "#aeb7c1"

for file in "$icons"/*.png; do
	base="$(basename "$file" .png)"
	case "$base" in
		*-dark) continue ;;
	esac
	convert "$file" -modulate 78,90,100 "$icons/$base-dark.png"
done

make_status_battery
make_status_wifi
make_status_search
make_status_control_center
make_status_weather

printf 'Generated Orange placeholder assets in %s\n' "$out"
