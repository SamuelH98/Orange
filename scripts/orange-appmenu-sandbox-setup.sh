#!/bin/sh
set -eu

arch_triplet=${ORANGE_APPMENU_TRIPLET:-x86_64-linux-gnu}
data_home=${XDG_DATA_HOME:-"$HOME/.local/share"}
bundle=${ORANGE_APPMENU_BUNDLE:-"$data_home/orange/appmenu-gtk3"}

module_paths="
/usr/lib/$arch_triplet/gtk-3.0/modules/libappmenu-gtk-module.so
/usr/lib64/gtk-3.0/modules/libappmenu-gtk-module.so
/usr/lib/gtk-3.0/modules/libappmenu-gtk-module.so
"

find_first_existing() {
	for path in "$@"; do
		if [ -e "$path" ]; then
			printf '%s\n' "$path"
			return 0
		fi
	done
	return 1
}

copy_glob() {
	pattern=$1
	dest=$2
	found=false
	for path in $pattern; do
		if [ -e "$path" ]; then
			cp -a "$path" "$dest/"
			found=true
		fi
	done
	if [ "$found" = false ]; then
		printf 'orange-appmenu: missing required file pattern: %s\n' "$pattern" >&2
		return 1
	fi
}

install_bundle() {
	module_path=$(find_first_existing $module_paths) || {
		printf '%s\n' \
			"orange-appmenu: appmenu GTK3 module is not installed." \
			"Install appmenu-gtk3-module or the distro equivalent first." >&2
		return 1
	}

	lib_dir="$bundle/lib/$arch_triplet"
	gtk_dir="$lib_dir/gtk-3.0/modules"
	schema_dir="$bundle/share/glib-2.0/schemas"
	mkdir -p "$gtk_dir" "$lib_dir" "$schema_dir"
	cp -a "$module_path" "$gtk_dir/"
	copy_glob "/usr/lib/$arch_triplet/libappmenu-gtk3-parser.so.0*" "$lib_dir"
	copy_glob "/usr/lib/$arch_triplet/libdbusmenu-glib.so.4*" "$lib_dir"
	copy_glob "/usr/lib/$arch_triplet/libdbusmenu-gtk3.so.4*" "$lib_dir"
	cp -a /usr/share/glib-2.0/schemas/org.appmenu.gtk-module.gschema.xml \
		"$schema_dir/"
	if command -v glib-compile-schemas >/dev/null 2>&1; then
		glib-compile-schemas "$schema_dir"
	else
		printf '%s\n' \
			"orange-appmenu: glib-compile-schemas not found; schema is copied but not compiled." >&2
	fi
}

configure_flatpak() {
	if ! command -v flatpak >/dev/null 2>&1; then
		return 0
	fi
	flatpak override --user \
		--filesystem="$bundle:ro" \
		--talk-name=com.canonical.AppMenu.Registrar \
		--env=GTK_MODULES=appmenu-gtk-module \
		--env=GTK_PATH="$bundle/lib/$arch_triplet/gtk-3.0" \
		--env=LD_LIBRARY_PATH="$bundle/lib/$arch_triplet" \
		--env=GSETTINGS_SCHEMA_DIR="$bundle/share/glib-2.0/schemas"
}

install_firefox_snap_wrapper() {
	if ! command -v snap >/dev/null 2>&1 ||
			! snap list firefox >/dev/null 2>&1; then
		return 0
	fi

	snap_bundle="$HOME/snap/firefox/common/orange-appmenu-gtk3"
	mkdir -p "$(dirname "$snap_bundle")"
	rm -rf "$snap_bundle"
	cp -a "$bundle" "$snap_bundle"

	mkdir -p "$HOME/.local/bin" "$data_home/applications"
	wrapper="$HOME/.local/bin/firefox-appmenu"
	cat > "$wrapper" <<'EOF'
#!/bin/sh
set -eu

append_colon_var() {
	name=$1
	value=$2
	eval "current=\${$name-}"
	case ":$current:" in
		*:"$value":*) ;;
		*)
			if [ -n "$current" ]; then
				export "$name=$current:$value"
			else
				export "$name=$value"
			fi
			;;
	esac
}

bundle="$HOME/snap/firefox/common/orange-appmenu-gtk3"
arch_triplet=${ORANGE_APPMENU_TRIPLET:-x86_64-linux-gnu}
append_colon_var GTK_MODULES appmenu-gtk-module
if [ -d "$bundle/lib/$arch_triplet/gtk-3.0" ]; then
	export GTK_PATH="$bundle/lib/$arch_triplet/gtk-3.0${GTK_PATH:+:$GTK_PATH}"
fi
if [ -d "$bundle/share/glib-2.0/schemas" ]; then
	export GSETTINGS_SCHEMA_DIR="$bundle/share/glib-2.0/schemas"
fi

exec snap run --shell firefox -c '
set -eu
bundle="$SNAP_USER_COMMON/orange-appmenu-gtk3"
arch_triplet=${ORANGE_APPMENU_TRIPLET:-x86_64-linux-gnu}
case ":${GTK_MODULES-}:" in
	*:appmenu-gtk-module:*) ;;
	*) export GTK_MODULES="${GTK_MODULES:+$GTK_MODULES:}appmenu-gtk-module" ;;
esac
if [ -d "$bundle/share/glib-2.0/schemas" ]; then
	export GSETTINGS_SCHEMA_DIR="$bundle/share/glib-2.0/schemas"
fi
if [ -d "$bundle/lib/$arch_triplet/gtk-3.0" ]; then
	export GTK_PATH="$bundle/lib/$arch_triplet/gtk-3.0${GTK_PATH:+:$GTK_PATH}"
fi
if [ -d "$bundle/lib/$arch_triplet" ]; then
	export LD_LIBRARY_PATH="$bundle/lib/$arch_triplet${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$SNAP/firefox.launcher" "$@"
' firefox "$@"
EOF
	chmod 0755 "$wrapper"

	desktop=/var/lib/snapd/desktop/applications/firefox_firefox.desktop
	if [ -f "$desktop" ]; then
		sed "s#Exec=/snap/bin/firefox#Exec=$wrapper#g" "$desktop" \
			> "$data_home/applications/firefox_firefox.desktop"
	fi
}

install_bundle
configure_flatpak
install_firefox_snap_wrapper
printf 'orange-appmenu: installed sandbox bridge at %s\n' "$bundle"
