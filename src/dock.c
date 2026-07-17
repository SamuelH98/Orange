#include "orange/dock.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/glass.h"
#include "orange/shell.h"
#include "orange/util.h"
#define ORANGE_GNOME_SETTINGS_DESKTOP "GNOME:Unity:ubuntu"
#define ORANGE_GNOME_APP_ENV "env -u GTK_THEME -u GTK_ICON_THEME NO_AT_BRIDGE=1 GSK_RENDERER=cairo XDG_CURRENT_DESKTOP=" ORANGE_GNOME_SETTINGS_DESKTOP " XDG_SESSION_DESKTOP=gnome DESKTOP_SESSION=gnome GNOME_DESKTOP_SESSION_ID=this-is-deprecated "
#define ORANGE_GNOME_SETTINGS_ENV ORANGE_GNOME_APP_ENV
#define ORANGE_GNOME_SETTINGS_FAST_ENV ORANGE_GNOME_SETTINGS_ENV
#define ORANGE_SETTINGS_COMMAND "if command -v gnome-control-center >/dev/null 2>&1; then " ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center || " ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center applications || " ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center multitasking || " ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center display || " ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center system; elif command -v systemsettings >/dev/null 2>&1; then systemsettings; elif command -v xfce4-settings-manager >/dev/null 2>&1; then xfce4-settings-manager; fi; true"
#define ORANGE_TRASH_COMMAND "if command -v nautilus >/dev/null 2>&1; then " ORANGE_GNOME_APP_ENV "nautilus trash:///; elif command -v gio >/dev/null 2>&1; then gio open trash:///; else xdg-open trash:///; fi; true"

/* Built-in dock entries (not from .desktop files) */
static const char *builtin_label(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "Launcher";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return "Trash";
	}
	return NULL;
}

static const char *builtin_icon(const char *app_id, bool trash_full) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "view-app-grid";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return trash_full ? "user-trash-full" : "user-trash";
	}
	return NULL;
}

const char *orange_dock_builtin_command(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "if [ -n \"$ORANGE_APP_PICKER\" ]; then $ORANGE_APP_PICKER; else wofi --show drun || rofi -show drun || true; fi";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return ORANGE_TRASH_COMMAND;
	}
	if (strcmp(app_id, "orange-settings") == 0 ||
			strcmp(app_id, "dev.orange.Settings") == 0 ||
			strcmp(app_id, "settings") == 0 ||
			strcmp(app_id, "org.gnome.Settings.desktop") == 0) {
		return ORANGE_SETTINGS_COMMAND;
	}
	if (orange_desktop_entry_id_matches(app_id, "firefox.desktop") ||
			orange_desktop_entry_id_matches(app_id,
				"org.mozilla.firefox.desktop")) {
		return "/usr/bin/firefox || firefox || /usr/bin/firefox-esr || firefox-esr";
	}
	return NULL;
}

static bool app_id_matches(const char *app_id, const char *needle) {
	return app_id != NULL && needle != NULL &&
		(orange_desktop_entry_id_matches(needle, app_id) ||
		 orange_desktop_entry_id_matches(app_id, needle) ||
		 strstr(app_id, needle) != NULL);
}

static const char *dock_fallback_label(const char *app_id) {
	if (app_id_matches(app_id, "org.gnome.Nautilus") ||
			app_id_matches(app_id, "nautilus")) {
		return "Files";
	}
	if (app_id_matches(app_id, "firefox") ||
			app_id_matches(app_id, "browser")) {
		return "Browser";
	}
	if (app_id_matches(app_id, "org.gnome.Calculator") ||
			app_id_matches(app_id, "calculator")) {
		return "Calculator";
	}
	if (app_id_matches(app_id, "org.gnome.TextEditor") ||
			app_id_matches(app_id, "gedit")) {
		return "Notes";
	}
	if (app_id_matches(app_id, "org.gnome.Settings") ||
			app_id_matches(app_id, "control-center")) {
		return "Settings";
	}
	if (app_id_matches(app_id, "org.gnome.Software") ||
			app_id_matches(app_id, "discover")) {
		return "Software";
	}
	if (app_id_matches(app_id, "org.gnome.Terminal") ||
			app_id_matches(app_id, "terminal") ||
			app_id_matches(app_id, "ghostty")) {
		return "Terminal";
	}
	if (app_id_matches(app_id, "org.gnome.Weather") ||
			app_id_matches(app_id, "weather")) {
		return "Weather";
	}
	if (app_id_matches(app_id, "org.gnome.Calendar") ||
			app_id_matches(app_id, "calendar")) {
		return "Calendar";
	}
	if (app_id_matches(app_id, "org.gnome.Maps") ||
			app_id_matches(app_id, "maps")) {
		return "Maps";
	}
	if (app_id_matches(app_id, "org.gnome.Loupe") ||
			app_id_matches(app_id, "loupe") ||
			app_id_matches(app_id, "ImageViewer")) {
		return "Image Viewer";
	}
	if (app_id_matches(app_id, "org.gnome.Contacts") ||
			app_id_matches(app_id, "contacts")) {
		return "Contacts";
	}
	if (app_id_matches(app_id, "org.gnome.Showtime") ||
			app_id_matches(app_id, "showtime")) {
		return "Video Player";
	}
	if (app_id_matches(app_id, "org.gnome.Decibels") ||
			app_id_matches(app_id, "decibels")) {
		return "Audio Player";
	}
	return NULL;
}

static const char *dock_fallback_icon(const char *app_id) {
	if (app_id_matches(app_id, "org.gnome.Nautilus") ||
			app_id_matches(app_id, "nautilus")) {
		return "system-file-manager";
	}
	if (app_id_matches(app_id, "firefox") ||
			app_id_matches(app_id, "browser")) {
		return "web-browser";
	}
	if (app_id_matches(app_id, "org.gnome.Calculator") ||
			app_id_matches(app_id, "calculator")) {
		return "accessories-calculator";
	}
	if (app_id_matches(app_id, "org.gnome.Settings") ||
			app_id_matches(app_id, "control-center")) {
		return "preferences-system";
	}
	if (app_id_matches(app_id, "org.gnome.Software") ||
			app_id_matches(app_id, "discover")) {
		return "system-software-install";
	}
	if (app_id_matches(app_id, "org.gnome.Terminal") ||
			app_id_matches(app_id, "terminal") ||
			app_id_matches(app_id, "ghostty")) {
		return "utilities-terminal";
	}
	if (app_id_matches(app_id, "org.gnome.Weather") ||
			app_id_matches(app_id, "weather")) {
		return "weather-clear";
	}
	if (app_id_matches(app_id, "org.gnome.Calendar") ||
			app_id_matches(app_id, "calendar")) {
		return "x-office-calendar";
	}
	if (app_id_matches(app_id, "org.gnome.Maps") ||
			app_id_matches(app_id, "maps")) {
		return "maps-app";
	}
	if (app_id_matches(app_id, "org.gnome.Notes") ||
			app_id_matches(app_id, "notes")) {
		return "org.gnome.Notes";
	}

	if (app_id_matches(app_id, "org.gnome.Loupe") ||
			app_id_matches(app_id, "loupe") ||
			app_id_matches(app_id, "ImageViewer")) {
		return "image-viewer";
	}
	if (app_id_matches(app_id, "org.gnome.Contacts") ||
			app_id_matches(app_id, "contacts")) {
		return "x-office-address-book";
	}
	if (app_id_matches(app_id, "org.gnome.Showtime") ||
			app_id_matches(app_id, "showtime")) {
		return "org.gnome.Showtime";
	}
	if (app_id_matches(app_id, "org.gnome.Decibels") ||
			app_id_matches(app_id, "decibels")) {
		return "org.gnome.Decibels";
	}
	return NULL;
}


static const char *dock_fallback_command(const char *app_id) {
	if (app_id_matches(app_id, "org.gnome.Nautilus") ||
			app_id_matches(app_id, "nautilus")) {
		return ORANGE_GNOME_APP_ENV "nautilus || nemo || thunar || dolphin || xdg-open \"$HOME\"";
	}
	if (app_id_matches(app_id, "firefox") ||
			app_id_matches(app_id, "browser")) {
		return "firefox || firefox-esr || brave-browser --enable-features=UseOzonePlatform --ozone-platform=wayland || chromium --enable-features=UseOzonePlatform --ozone-platform=wayland || xdg-open \"https://duckduckgo.com\"";
	}
	if (app_id_matches(app_id, "org.gnome.Calculator") ||
			app_id_matches(app_id, "calculator")) {
		return "gnome-calculator || kcalc || galculator || xcalc || true";
	}
	if (app_id_matches(app_id, "org.gnome.TextEditor") ||
			app_id_matches(app_id, "gedit")) {
		return "gnome-text-editor || gedit || mousepad || kate || ${ORANGE_TERMINAL:-ghostty} -e nano";
	}
	if (app_id_matches(app_id, "org.gnome.Settings") ||
			app_id_matches(app_id, "control-center")) {
		return ORANGE_SETTINGS_COMMAND;
	}
	if (app_id_matches(app_id, "org.gnome.Software") ||
			app_id_matches(app_id, "discover")) {
		return "gnome-software || plasma-discover || true";
	}
	if (app_id_matches(app_id, "org.gnome.Terminal") ||
			app_id_matches(app_id, "terminal") ||
			app_id_matches(app_id, "ghostty")) {
		return "ghostty || foot || alacritty || kitty || gnome-terminal || konsole || weston-terminal || xterm";
	}
	if (app_id_matches(app_id, "org.gnome.Weather") ||
			app_id_matches(app_id, "weather")) {
		return "gnome-weather || xdg-open \"https://weather.com\" || true";
	}
	if (app_id_matches(app_id, "org.gnome.Calendar") ||
			app_id_matches(app_id, "calendar")) {
		return "gnome-calendar || xdg-open \"https://calendar.google.com\" || true";
	}
	if (app_id_matches(app_id, "org.gnome.Maps") ||
			app_id_matches(app_id, "maps")) {
		return "gnome-maps || xdg-open \"https://openstreetmap.org\" || true";
	}
	if (app_id_matches(app_id, "org.gnome.Loupe") ||
			app_id_matches(app_id, "loupe") ||
			app_id_matches(app_id, "ImageViewer")) {
		return "loupe || eog || gimp || xdg-open \"$HOME/Pictures\" || true";
	}
	if (app_id_matches(app_id, "org.gnome.Contacts") ||
			app_id_matches(app_id, "contacts")) {
		return "gnome-contacts || evolution || true";
	}
	if (app_id_matches(app_id, "org.gnome.Showtime") ||
			app_id_matches(app_id, "showtime")) {
		return "showtime || totem || vlc || xdg-open \"$HOME/Videos\" || true";
	}
	if (app_id_matches(app_id, "org.gnome.Decibels") ||
			app_id_matches(app_id, "decibels")) {
		return "decibels || totem || vlc || xdg-open \"$HOME/Music\" || true";
	}
	return NULL;
}

static const struct orange_desktop_entry *lookup_entry(
		const char *app_id,
		const struct orange_desktop_entry *entries,
		int entry_count) {
	if (entries == NULL || app_id == NULL) {
		return NULL;
	}
	const struct orange_desktop_entry *best = NULL;
	int best_score = 0;
	for (int i = 0; i < entry_count; i++) {
		if (!orange_desktop_entry_is_available(&entries[i])) {
			continue;
		}
		int score = orange_desktop_entry_match_score(&entries[i], app_id);
		if (score <= 0 && app_id_matches(entries[i].id, app_id)) {
			score = 10;
		}
		if (score > best_score) {
			best = &entries[i];
			best_score = score;
		}
	}
	return best;
}

static double layout_scale(const struct orange_shell_layout *layout) {
	return clamp(ui_scale_for_size(layout->width, layout->height),
		ORANGE_MIN_UI_SCALE, ORANGE_MAX_UI_SCALE);
}

static struct orange_rect clamp_rect_to_size(
		struct orange_rect rect,
		int width,
		int height) {
	int x1 = rect.x;
	int y1 = rect.y;
	int x2 = rect.x + rect.width;
	int y2 = rect.y + rect.height;
	if (x1 < 0) {
		x1 = 0;
	}
	if (y1 < 0) {
		y1 = 0;
	}
	if (x2 > width) {
		x2 = width;
	}
	if (y2 > height) {
		y2 = height;
	}
	if (x2 <= x1 || y2 <= y1) {
		return (struct orange_rect){0, 0, 0, 0};
	}
	return (struct orange_rect){x1, y1, x2 - x1, y2 - y1};
}

struct orange_rect orange_dock_visual_dirty_bounds(
		const struct orange_shell_layout *layout,
		int width,
		int height) {
	if (layout == NULL || layout->dock.width <= 0 ||
			layout->dock.height <= 0 || width <= 0 || height <= 0) {
		return (struct orange_rect){0, 0, 0, 0};
	}
	int item_size = layout->dock.height;
	if (layout->dock_item_count > 0) {
		item_size = layout->dock_items[0].width > layout->dock_items[0].height ?
			layout->dock_items[0].width : layout->dock_items[0].height;
	}
	int pad = item_size * 2;
	if (pad < 96) {
		pad = 96;
	}
	if (pad > 360) {
		pad = 360;
	}
	struct orange_rect bounds = {
		0,
		layout->dock.y - pad,
		width,
		layout->dock.height + pad * 2,
	};
	bool vertical =
		layout->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		layout->dock_position == ORANGE_DOCK_POSITION_RIGHT;
	if (vertical) {
		int min_y = layout->menu_bar.y + layout->menu_bar.height;
		int bottom = bounds.y + bounds.height;
		bounds.y = layout->dock.y - pad;
		if (bounds.y < min_y) {
			bounds.y = min_y;
			bounds.height = bottom - bounds.y;
		}
	}
	return clamp_rect_to_size(bounds, width, height);
}

static void rounded_rect(cairo_t *cr, double x, double y,
		double width, double height, double radius) {
	double r = clamp(radius, 0.0, fmin(width, height) / 2.0);
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + width - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_arc(cr, x + width - r, y + height - r, r, 0.0, M_PI / 2.0);
	cairo_arc(cr, x + r, y + height - r, r, M_PI / 2.0, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static void draw_image_cover(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmax((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	rounded_rect(cr, r.x, r.y, r.width, r.height, r.width * 0.22);
	cairo_clip(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
	cairo_restore(cr);
}

static void draw_image_tinted(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, int red, int green, int blue, double alpha) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmax((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	rounded_rect(cr, r.x, r.y, r.width, r.height, r.width * 0.22);
	cairo_clip(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_rgba(cr, red / 255.0, green / 255.0, blue / 255.0, alpha);
	cairo_mask_surface(cr, surface, 0, 0);
	cairo_restore(cr);
}

__attribute__((unused)) static struct orange_rect centered_rect_in(
		struct orange_rect bounds,
		int width,
		int height) {
	return (struct orange_rect){
		bounds.x + (bounds.width - width) / 2,
		bounds.y + (bounds.height - height) / 2,
		width,
		height,
	};
}

static bool dock_launcher_is_trash(
		const struct orange_config *config,
		int launcher_idx) {
	return config != NULL &&
		launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
		strcmp(config->dock_apps[launcher_idx], "__trash__") == 0;
}

static bool dock_launcher_is_grid(
		const struct orange_config *config,
		int launcher_idx) {
	return config != NULL &&
		launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
		strcmp(config->dock_apps[launcher_idx], "__launcher__") == 0 &&
		strcmp(config->icon_theme, "Adwaita") == 0;
}

static int normalize_dock_launchers(
		const struct orange_config *config,
		int launchers[ORANGE_DOCK_MAX]) {
	if (config == NULL) {
		return 0;
	}

	bool used[ORANGE_DOCK_MAX] = {0};
	int visible = 0;
	int dock_count = orange_dock_count(config);
	for (int i = 0; i < dock_count && visible < ORANGE_DOCK_MAX; i++) {
		int idx = config->dock_order[i];
		if (idx < 0 || idx >= ORANGE_DOCK_MAX ||
				config->dock_apps[idx][0] == '\0' || used[idx]) {
			continue;
		}
		launchers[visible++] = idx;
		used[idx] = true;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX && visible < dock_count; i++) {
		if (config->dock_apps[i][0] != '\0' && !used[i]) {
			launchers[visible++] = i;
			used[i] = true;
		}
	}
	return visible;
}

static bool dock_ids_match(const char *a, const char *b) {
	if (a == NULL || b == NULL || a[0] == '\0' || b[0] == '\0') {
		return false;
	}
	if (a[0] == '_' || b[0] == '_') {
		return strcmp(a, b) == 0;
	}
	return strcmp(a, b) == 0 ||
		orange_desktop_entry_id_matches(a, b) ||
		orange_desktop_entry_id_matches(b, a);
}

bool orange_dock_app_is_permanent(const char *app_id) {
	return app_id != NULL &&
		(strcmp(app_id, "__launcher__") == 0 ||
		 strcmp(app_id, "__trash__") == 0);
}

bool orange_dock_config_contains_app(
		const struct orange_config *config,
		const char *app_id) {
	return orange_dock_config_find_app(config, app_id) >= 0;
}

int orange_dock_config_find_app(
		const struct orange_config *config,
		const char *app_id) {
	if (config == NULL || app_id == NULL || app_id[0] == '\0') {
		return -1;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (dock_ids_match(config->dock_apps[i], app_id)) {
			return i;
		}
	}
	return -1;
}

bool orange_dock_temporary_contains(
		const char app_ids[ORANGE_DOCK_MAX][128],
		int app_count,
		const char *app_id) {
	if (app_ids == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	if (app_count < 0) {
		app_count = 0;
	}
	if (app_count > ORANGE_DOCK_MAX) {
		app_count = ORANGE_DOCK_MAX;
	}
	for (int i = 0; i < app_count; i++) {
		if (dock_ids_match(app_ids[i], app_id)) {
			return true;
		}
	}
	return false;
}

bool orange_dock_temporary_add(
		char app_ids[ORANGE_DOCK_MAX][128],
		int *app_count,
		const char *app_id,
		const struct orange_config *config) {
	if (app_ids == NULL || app_count == NULL ||
			app_id == NULL || app_id[0] == '\0' ||
			orange_dock_app_is_permanent(app_id) ||
			orange_dock_config_contains_app(config, app_id)) {
		return false;
	}
	int count = *app_count;
	if (count < 0) {
		count = 0;
	}
	if (count > ORANGE_DOCK_MAX) {
		count = ORANGE_DOCK_MAX;
	}
	if (orange_dock_temporary_contains(
			(const char (*)[128])app_ids, count, app_id) ||
			count >= ORANGE_DOCK_MAX) {
		*app_count = count;
		return false;
	}
	snprintf(app_ids[count], sizeof(app_ids[count]), "%s", app_id);
	*app_count = count + 1;
	return true;
}

bool orange_dock_temporary_remove(
		char app_ids[ORANGE_DOCK_MAX][128],
		int *app_count,
		const char *app_id) {
	if (app_ids == NULL || app_count == NULL ||
			app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	int count = *app_count;
	if (count < 0) {
		count = 0;
	}
	if (count > ORANGE_DOCK_MAX) {
		count = ORANGE_DOCK_MAX;
	}
	bool removed = false;
	int out = 0;
	for (int i = 0; i < count; i++) {
		if (dock_ids_match(app_ids[i], app_id)) {
			removed = true;
			continue;
		}
		if (out != i) {
			memmove(app_ids[out], app_ids[i], sizeof(app_ids[out]));
		}
		out++;
	}
	for (int i = out; i < ORANGE_DOCK_MAX; i++) {
		app_ids[i][0] = '\0';
	}
	*app_count = out;
	return removed;
}

int orange_dock_temporary_prune_config(
		char app_ids[ORANGE_DOCK_MAX][128],
		int app_count,
		const struct orange_config *config) {
	if (app_ids == NULL) {
		return 0;
	}
	if (app_count < 0) {
		app_count = 0;
	}
	if (app_count > ORANGE_DOCK_MAX) {
		app_count = ORANGE_DOCK_MAX;
	}
	int out = 0;
	for (int i = 0; i < app_count; i++) {
		const char *app_id = app_ids[i];
		if (app_id[0] == '\0' ||
				orange_dock_app_is_permanent(app_id) ||
				orange_dock_config_contains_app(config, app_id)) {
			continue;
		}
		bool duplicate = false;
		for (int j = 0; j < out; j++) {
			if (dock_ids_match(app_ids[j], app_id)) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			continue;
		}
		if (out != i) {
			memmove(app_ids[out], app_ids[i], sizeof(app_ids[out]));
		}
		out++;
	}
	for (int i = out; i < ORANGE_DOCK_MAX; i++) {
		app_ids[i][0] = '\0';
	}
	return out;
}

static int dock_visible_ids(
		const struct orange_config *config,
		char ids[ORANGE_DOCK_MAX][128]) {
	int launchers[ORANGE_DOCK_MAX] = {0};
	int count = normalize_dock_launchers(config, launchers);
	for (int i = 0; i < count; i++) {
		snprintf(ids[i], 128, "%s", config->dock_apps[launchers[i]]);
	}
	return count;
}

static int dock_visible_trash_index(char ids[ORANGE_DOCK_MAX][128], int count) {
	for (int i = 0; i < count; i++) {
		if (strcmp(ids[i], "__trash__") == 0) {
			return i;
		}
	}
	return -1;
}

static void dock_config_rebuild(
		struct orange_config *config,
		char ids[ORANGE_DOCK_MAX][128],
		int count) {
	if (count < 0) {
		count = 0;
	}
	if (count > ORANGE_DOCK_MAX) {
		count = ORANGE_DOCK_MAX;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (i < count) {
			snprintf(config->dock_apps[i], 128, "%s", ids[i]);
		} else {
			config->dock_apps[i][0] = '\0';
		}
		config->dock_order[i] = i;
	}
}

bool orange_dock_config_remove_visible(
		struct orange_config *config,
		int visible_index) {
	if (config == NULL) {
		return false;
	}
	char ids[ORANGE_DOCK_MAX][128] = {{0}};
	int count = dock_visible_ids(config, ids);
	if (visible_index < 0 || visible_index >= count ||
			orange_dock_app_is_permanent(ids[visible_index])) {
		return false;
	}
	for (int i = visible_index; i + 1 < count; i++) {
		memmove(ids[i], ids[i + 1], sizeof(ids[i]));
	}
	count--;
	dock_config_rebuild(config, ids, count);
	return true;
}

bool orange_dock_config_reorder_visible(
		struct orange_config *config,
		int from_visible_index,
		int to_visible_index) {
	if (config == NULL) {
		return false;
	}
	char ids[ORANGE_DOCK_MAX][128] = {{0}};
	char remaining[ORANGE_DOCK_MAX][128] = {{0}};
	char rebuilt[ORANGE_DOCK_MAX][128] = {{0}};
	int count = dock_visible_ids(config, ids);
	if (from_visible_index < 0 || from_visible_index >= count ||
			orange_dock_app_is_permanent(ids[from_visible_index])) {
		return false;
	}

	char dragged[128];
	snprintf(dragged, sizeof(dragged), "%s", ids[from_visible_index]);
	int remaining_count = 0;
	for (int i = 0; i < count; i++) {
		if (i == from_visible_index) {
			continue;
		}
		snprintf(remaining[remaining_count], sizeof(remaining[remaining_count]),
			"%s", ids[i]);
		remaining_count++;
	}

	if (to_visible_index < 0) {
		to_visible_index = 0;
	}
	if (to_visible_index > remaining_count) {
		to_visible_index = remaining_count;
	}
	int trash = dock_visible_trash_index(remaining, remaining_count);
	if (trash >= 0 && to_visible_index > trash) {
		to_visible_index = trash;
	}

	int out = 0;
	for (int i = 0; i <= remaining_count && out < ORANGE_DOCK_MAX; i++) {
		if (i == to_visible_index) {
			snprintf(rebuilt[out], sizeof(rebuilt[out]), "%s", dragged);
			out++;
		}
		if (i < remaining_count && out < ORANGE_DOCK_MAX) {
			snprintf(rebuilt[out], sizeof(rebuilt[out]), "%s", remaining[i]);
			out++;
		}
	}

	bool changed = false;
	for (int i = 0; i < count; i++) {
		if (strcmp(ids[i], rebuilt[i]) != 0) {
			changed = true;
			break;
		}
	}
	if (!changed) {
		return false;
	}
	dock_config_rebuild(config, rebuilt, count);
	return true;
}

bool orange_dock_config_insert_app(
		struct orange_config *config,
		const char *app_id,
		int visible_index) {
	if (config == NULL || app_id == NULL || app_id[0] == '\0' ||
			orange_dock_config_contains_app(config, app_id)) {
		return false;
	}
	char ids[ORANGE_DOCK_MAX][128] = {{0}};
	char rebuilt[ORANGE_DOCK_MAX][128] = {{0}};
	int count = dock_visible_ids(config, ids);
	if (count >= ORANGE_DOCK_MAX) {
		return false;
	}
	if (visible_index < 0) {
		visible_index = count;
	}
	if (visible_index > count) {
		visible_index = count;
	}
	int trash = dock_visible_trash_index(ids, count);
	if (trash >= 0 && visible_index > trash) {
		visible_index = trash;
	}

	int out = 0;
	for (int i = 0; i <= count && out < ORANGE_DOCK_MAX; i++) {
		if (i == visible_index) {
			snprintf(rebuilt[out], sizeof(rebuilt[out]), "%s", app_id);
			out++;
		}
		if (i < count && out < ORANGE_DOCK_MAX) {
			snprintf(rebuilt[out], sizeof(rebuilt[out]), "%s", ids[i]);
			out++;
		}
	}
	dock_config_rebuild(config, rebuilt, count + 1);
	return true;
}

static const char *dock_icon_name(int launcher_idx,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	const char *app_id = config != NULL && launcher_idx >= 0 &&
		launcher_idx < ORANGE_DOCK_MAX ? config->dock_apps[launcher_idx] : NULL;
	if (app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	const char *bi = builtin_icon(app_id, state != NULL && state->trash_full);
	if (bi != NULL) {
		return bi;
	}
	const struct orange_desktop_entry *entries =
		state != NULL ? state->desktop_entries : NULL;
	int entry_count = state != NULL ? state->desktop_entry_count : 0;
	const struct orange_desktop_entry *entry =
		lookup_entry(app_id, entries, entry_count);
	if (entry != NULL && entry->icon[0] != '\0') {
		return entry->icon;
	}
	const char *fallback = dock_fallback_icon(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return "application-x-executable";
}

static bool dock_layout_is_vertical(
		const struct orange_shell_layout *layout) {
	return layout != NULL &&
		(layout->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		 layout->dock_position == ORANGE_DOCK_POSITION_RIGHT);
}

static double dock_magnification_for_item(
		const struct orange_rect *item,
		double pointer_axis,
		bool vertical,
		double max_scale) {
	double center = vertical ?
		item->y + item->height / 2.0 :
		item->x + item->width / 2.0;
	double distance = fabs(pointer_axis - center);
	double base = vertical ? item->height : item->width;
	double radius = fmax(1.0, base * 2.35);
	if (distance >= radius) {
		return 1.0;
	}

	double t = distance / radius;
	double weight = 0.5 + cos(t * M_PI) * 0.5;
	return 1.0 + (max_scale - 1.0) * weight;
}

static bool dock_magnification_active(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (layout->dock_item_count <= 0 || state == NULL || config == NULL ||
			!config->dock_magnification ||
			config->dock_magnification_scale <= 1.0 ||
			state->dock_drag_index >= 0 ||
			state->dock_drag_insert_before >= 0) {
		return false;
	}
	return state->hot_dock_index >= 0 &&
		state->hot_dock_index < layout->dock_item_count;
}

static int dock_visual_step(const struct orange_shell_layout *layout) {
	if (layout == NULL || layout->dock_item_count <= 0) {
		return 0;
	}
	if (layout->dock_item_count > 1) {
		int step = dock_layout_is_vertical(layout) ?
			layout->dock_items[1].y - layout->dock_items[0].y :
			layout->dock_items[1].x - layout->dock_items[0].x;
		if (step > 0) {
			return step;
		}
	}
	return layout->dock_items[0].width + layout->dock_items[0].width / 5;
}

static struct orange_rect dock_rect_for_slot(
		const struct orange_shell_layout *layout,
		int slot) {
	if (layout == NULL || layout->dock_item_count <= 0) {
		return (struct orange_rect){0, 0, 0, 0};
	}
	if (slot < 0) {
		slot = 0;
	}
	if (slot < layout->dock_item_count) {
		return layout->dock_items[slot];
	}
	struct orange_rect last = layout->dock_items[layout->dock_item_count - 1];
	if (dock_layout_is_vertical(layout)) {
		last.y += dock_visual_step(layout) *
			(slot - layout->dock_item_count + 1);
	} else {
		last.x += dock_visual_step(layout) *
			(slot - layout->dock_item_count + 1);
	}
	return last;
}

static void apply_dock_drag_spacing(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		struct orange_dock_visual_item visual[ORANGE_DOCK_MAX]) {
	if (layout == NULL || state == NULL || layout->dock_item_count <= 0) {
		return;
	}
	int insert = state->dock_drag_insert_before;
	int dragged = state->dock_drag_index;
	if (insert < 0 && !(state->dock_drag_remove && dragged >= 0)) {
		return;
	}

	int count = layout->dock_item_count;
	bool vertical = dock_layout_is_vertical(layout);
	if (dragged < 0) {
		int step = dock_visual_step(layout);
		if (step <= 0) {
			return;
		}
		int half_step = step / 2;
		for (int i = 0; i < count; i++) {
			if (vertical) {
				visual[i].rect.y += i >= insert ?
					half_step : -half_step;
			} else {
				visual[i].rect.x += i >= insert ?
					half_step : -half_step;
			}
		}
		return;
	}

	if (dragged >= count) {
		return;
	}
	if (state->dock_drag_remove) {
		for (int i = dragged + 1; i < count; i++) {
			visual[i].rect = dock_rect_for_slot(layout, i - 1);
		}
		return;
	}
	if (insert < 0) {
		return;
	}
	if (insert > dragged) {
		for (int i = dragged + 1; i <= insert && i < count; i++) {
			visual[i].rect = dock_rect_for_slot(layout, i - 1);
		}
	} else if (insert < dragged) {
		for (int i = insert; i < dragged && i < count; i++) {
			visual[i].rect = dock_rect_for_slot(layout, i + 1);
		}
	}
}

static void clamp_vertical_dock_visual_items(
		const struct orange_shell_layout *layout,
		struct orange_dock_visual_item visual[ORANGE_DOCK_MAX]) {
	if (!dock_layout_is_vertical(layout) || layout->dock_item_count <= 0) {
		return;
	}

	double s = layout_scale(layout);
	int safe_top = layout->menu_bar.y + layout->menu_bar.height +
		scaled_i(4, s);
	int safe_bottom = layout->height - scaled_i(6, s);
	if (safe_bottom <= safe_top) {
		return;
	}

	int top = visual[0].rect.y;
	int bottom = visual[0].rect.y + visual[0].rect.height;
	for (int i = 1; i < layout->dock_item_count; i++) {
		if (visual[i].rect.y < top) {
			top = visual[i].rect.y;
		}
		int item_bottom = visual[i].rect.y + visual[i].rect.height;
		if (item_bottom > bottom) {
			bottom = item_bottom;
		}
	}

	int dy = 0;
	if (top < safe_top) {
		dy = safe_top - top;
	}
	if (bottom + dy > safe_bottom) {
		int candidate = safe_bottom - bottom;
		if (top + candidate >= safe_top) {
			dy = candidate;
		}
	}
	if (dy == 0) {
		return;
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		visual[i].rect.y += dy;
	}
}

void orange_dock_compute_visual_items(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_dock_visual_item visual[ORANGE_DOCK_MAX]) {
	if (visual == NULL) {
		return;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		visual[i] = (struct orange_dock_visual_item){
			.rect = {0, 0, 0, 0},
			.scale = 1.0,
		};
	}
	if (layout == NULL || layout->dock_item_count <= 0) {
		return;
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		visual[i] = (struct orange_dock_visual_item){
			.rect = layout->dock_items[i],
			.scale = 1.0,
		};
	}

	apply_dock_drag_spacing(layout, state, visual);

	if (!dock_magnification_active(layout, state, config)) {
		return;
	}

	int hot = state->hot_dock_index;
	bool vertical = dock_layout_is_vertical(layout);
	double pointer_axis = vertical ?
		(state->dock_pointer_y > 0 ?
			state->dock_pointer_y :
			layout->dock_items[hot].y +
				layout->dock_items[hot].height / 2.0) :
		(state->dock_pointer_x > 0 ?
			state->dock_pointer_x :
			layout->dock_items[hot].x +
				layout->dock_items[hot].width / 2.0);
	double max_scale = clamp(config->dock_magnification_scale, 1.0, 2.20);
	for (int i = 0; i < layout->dock_item_count; i++) {
		struct orange_rect base = layout->dock_items[i];
		double item_scale = dock_magnification_for_item(
			&base, pointer_axis, vertical, max_scale);
		int width = (int)lrint(base.width * item_scale);
		int height = (int)lrint(base.height * item_scale);
		if (vertical) {
			int x = layout->dock_position == ORANGE_DOCK_POSITION_RIGHT ?
				base.x + base.width - width : base.x;
			int y = (int)lrint(base.y + base.height / 2.0 -
				height / 2.0);
			visual[i].rect = (struct orange_rect){x, y, width, height};
		} else {
			int x = (int)lrint(base.x + base.width / 2.0 -
				width / 2.0);
			visual[i].rect = (struct orange_rect){
				x,
				base.y + base.height - height,
				width,
				height,
			};
		}
		visual[i].scale = item_scale;
	}
	clamp_vertical_dock_visual_items(layout, visual);
}

const char *orange_dock_visible_label(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		int visible_index) {
	if (layout == NULL || state == NULL || visible_index < 0 ||
			visible_index >= layout->dock_item_count ||
			visible_index >= ORANGE_DOCK_MAX) {
		return NULL;
	}
	if (layout->dock_minimized[visible_index]) {
		int minimized_index = layout->dock_minimized_indices[visible_index];
		if (minimized_index >= 0 && minimized_index < ORANGE_DOCK_MAX &&
				state->dock_minimized_titles[minimized_index][0] != '\0') {
			return state->dock_minimized_titles[minimized_index];
		}
		return "Minimized Window";
	}

	int launcher_idx = orange_dock_launcher_index(layout, visible_index);
	if (launcher_idx >= 0) {
		return orange_dock_label(launcher_idx,
			state->desktop_entries, state->desktop_entry_count, config);
	}
	if (layout->dock_temporary[visible_index]) {
		const char *app_id = layout->dock_temporary_app_ids[visible_index];
		const struct orange_desktop_entry *entry =
			lookup_entry(app_id, state->desktop_entries,
				state->desktop_entry_count);
		if (entry != NULL && entry->name[0] != '\0') {
			return entry->name;
		}
		const char *label = dock_fallback_label(app_id);
		return label != NULL ? label : app_id;
	}
	return NULL;
}

struct orange_rect orange_dock_minimized_thumbnail_rect(
		struct orange_rect item) {
	int pad_x = item.width / 18;
	int pad_y = item.height / 18;
	if (pad_x < 2) {
		pad_x = 2;
	}
	if (pad_y < 2) {
		pad_y = 2;
	}
	if (pad_x * 2 >= item.width) {
		pad_x = item.width > 2 ? item.width / 3 : 0;
	}
	if (pad_y * 2 >= item.height) {
		pad_y = item.height > 2 ? item.height / 3 : 0;
	}
	return (struct orange_rect){
		item.x + pad_x,
		item.y + pad_y,
		item.width - pad_x * 2,
		item.height - pad_y * 2,
	};
}

static size_t previous_utf8_boundary(const char *text, size_t len) {
	if (text == NULL || len == 0) {
		return 0;
	}
	len--;
	while (len > 0 && (((unsigned char)text[len] & 0xc0) == 0x80)) {
		len--;
	}
	return len;
}

static void dock_label_ellipsize_to_width(cairo_t *cr,
		const char *text,
		double max_width,
		char *out,
		size_t out_sz) {
	if (out == NULL || out_sz == 0) {
		return;
	}
	snprintf(out, out_sz, "%s", text != NULL ? text : "");
	cairo_text_extents_t extents;
	cairo_text_extents(cr, out, &extents);
	if (extents.x_advance <= max_width) {
		return;
	}

	const char *suffix = "...";
	size_t suffix_len = strlen(suffix);
	size_t len = strlen(out);
	while (len > 0) {
		len = previous_utf8_boundary(out, len);
		out[len] = '\0';
		if (len + suffix_len + 1 <= out_sz) {
			memcpy(out + len, suffix, suffix_len + 1);
		}
		cairo_text_extents(cr, out, &extents);
		if (extents.x_advance <= max_width) {
			return;
		}
	}
	snprintf(out, out_sz, "%s", suffix);
}

static void draw_dock_hover_label(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		const struct orange_dock_visual_item visual[ORANGE_DOCK_MAX]) {
	int hot = state->hot_dock_index;
	if (state->dock_drag_insert_before >= 0 ||
			hot < 0 || hot >= layout->dock_item_count ||
			hot == state->dock_drag_index) {
		return;
	}

	const char *label = orange_dock_visible_label(layout, state, config, hot);
	if (label == NULL) {
		return;
	}

	double s = layout_scale(layout);
	struct orange_rect item = visual[hot].rect;
	double font_size = fmax(11.0, 19.0 * s);
	double pad_x = fmax(8.0, 12.0 * s);
	double bubble_h = fmax(24.0, 34.0 * s);
	double gap = fmax(7.0, 10.0 * s);

	cairo_select_font_face(cr, orange_font_family,
		CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, label, &extents);
	double bubble_w = extents.x_advance + pad_x * 2.0;
	double max_bubble_w = layout->width - 12.0 * s;
	if (max_bubble_w < 80.0 * s) {
		max_bubble_w = 80.0 * s;
	}
	if (bubble_w > max_bubble_w) {
		double min_font_size = fmax(10.0, 14.0 * s);
		while (bubble_w > max_bubble_w &&
				font_size - 0.5 >= min_font_size) {
			font_size -= 0.5;
			cairo_set_font_size(cr, font_size);
			cairo_text_extents(cr, label, &extents);
			bubble_w = extents.x_advance + pad_x * 2.0;
		}
		if (bubble_w > max_bubble_w) {
			bubble_w = max_bubble_w;
		}
	}
	char fitted_label[ORANGE_DESKTOP_ENTRY_MAX_TEXT + 4];
	const char *draw_label = label;
	double text_w = bubble_w - pad_x * 2.0;
	if (text_w < 1.0) {
		text_w = 1.0;
	}
	if (extents.x_advance > text_w) {
		dock_label_ellipsize_to_width(cr, label, text_w,
			fitted_label, sizeof(fitted_label));
		draw_label = fitted_label;
		cairo_text_extents(cr, draw_label, &extents);
		bubble_w = extents.x_advance + pad_x * 2.0;
	}
	double bubble_x = item.x + item.width / 2.0 - bubble_w / 2.0;
	double bubble_y = item.y - gap - bubble_h;
	if (dock_layout_is_vertical(layout)) {
		bubble_y = item.y + item.height / 2.0 - bubble_h / 2.0;
		if (layout->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
			bubble_x = item.x - gap - bubble_w;
		} else {
			bubble_x = item.x + item.width + gap;
		}
	}
	if (bubble_x < 6.0 * s) {
		bubble_x = 6.0 * s;
	}
	if (bubble_x + bubble_w > layout->width - 6.0 * s) {
		bubble_x = layout->width - 6.0 * s - bubble_w;
	}
	if (bubble_y < layout->menu_bar.height + 4.0 * s) {
		bubble_y = item.y + item.height + gap;
	}
	if (bubble_y + bubble_h > layout->height - 6.0 * s) {
		bubble_y = layout->height - 6.0 * s - bubble_h;
	}

	rounded_rect(cr, bubble_x, bubble_y, bubble_w, bubble_h, bubble_h * 0.34);
	set_source_rgba255(cr, 28, 29, 34, 0.78);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.16);
	cairo_set_line_width(cr, fmax(1.0, s));
	cairo_stroke(cr);

	cairo_save(cr);
	cairo_rectangle(cr, bubble_x + pad_x - 1.0, bubble_y,
		fmax(1.0, bubble_w - pad_x * 2.0 + 2.0), bubble_h);
	cairo_clip(cr);
	draw_text(cr, draw_label,
		bubble_x + pad_x - extents.x_bearing,
		bubble_y + (bubble_h + extents.height) / 2.0 - extents.y_bearing * 0.25,
		font_size, 255, 255, 255, 0.96, true);
	cairo_restore(cr);
}

static void draw_dock_remove_bubble(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		struct orange_rect ghost_item) {
	if (state == NULL || !state->dock_drag_remove) {
		return;
	}
	double s = layout_scale(layout);
	const char *label = "Remove";
	double font_size = fmax(11.0, 18.0 * s);
	double pad_x = fmax(8.0, 12.0 * s);
	double bubble_h = fmax(24.0, 32.0 * s);
	double gap = fmax(8.0, 12.0 * s);
	cairo_select_font_face(cr, orange_font_family,
		CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, label, &extents);
	double bubble_w = extents.x_advance + pad_x * 2.0;
	double bubble_x = ghost_item.x + ghost_item.width / 2.0 - bubble_w / 2.0;
	double bubble_y = ghost_item.y - gap - bubble_h;
	if (bubble_x < 6.0 * s) {
		bubble_x = 6.0 * s;
	}
	if (bubble_x + bubble_w > layout->width - 6.0 * s) {
		bubble_x = layout->width - 6.0 * s - bubble_w;
	}
	if (bubble_y < layout->menu_bar.height + 4.0 * s) {
		bubble_y = ghost_item.y + ghost_item.height + gap;
	}

	rounded_rect(cr, bubble_x, bubble_y, bubble_w, bubble_h, bubble_h * 0.34);
	set_source_rgba255(cr, 42, 44, 52, 0.82);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.18);
	cairo_set_line_width(cr, fmax(1.0, s));
	cairo_stroke(cr);
	draw_text(cr, label,
		bubble_x + pad_x - extents.x_bearing,
		bubble_y + (bubble_h - extents.height) / 2.0 - extents.y_bearing,
		font_size, 255, 255, 255, 0.96, true);
}

static void draw_dock_drag_ghost(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (state == NULL || state->dock_drag_index < 0 ||
			state->dock_drag_index >= layout->dock_item_count) {
		return;
	}
	bool dark = is_dark_config(config);
	int launcher_idx = orange_dock_launcher_index(
		layout, state->dock_drag_index);
	struct orange_rect drag_source = layout->dock_items[state->dock_drag_index];
	struct orange_rect ghost_item = {
		state->dock_drag_x - drag_source.width / 2,
		state->dock_drag_y - drag_source.height / 2,
		drag_source.width,
		drag_source.height,
	};
	cairo_push_group(cr);
	int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	bool grid_icon = dock_launcher_is_grid(config, launcher_idx);
	if (state->assets != NULL && launcher_idx >= 0) {
		const char *icon_name = dock_icon_name(launcher_idx, state, config);
		cairo_surface_t *icon_surface = icon_name != NULL ?
			orange_assets_icon(state->assets, variant, icon_name) : NULL;
		if (icon_surface != NULL) {
			if (grid_icon) {
				draw_image_tinted(cr, icon_surface, ghost_item,
					255, 255, 255, 1.0);
			} else {
				draw_image_cover(cr, icon_surface, ghost_item);
			}
		}
	}
	cairo_pop_group_to_source(cr);
	cairo_paint_with_alpha(cr, state->dock_drag_remove ? 0.55 : 0.78);
	draw_dock_remove_bubble(cr, layout, state, ghost_item);
}

double orange_dock_bounce_offset(const struct orange_dock_bounce *bounce,
		uint32_t now_ms, double scale) {
	if (bounce == NULL || !bounce->active) {
		return 0.0;
	}
	uint32_t elapsed = now_ms - bounce->start_time;
	if (elapsed >= ORANGE_DOCK_BOUNCE_DURATION_MS) {
		return 0.0;
	}
	double t = (double)elapsed / (double)ORANGE_DOCK_BOUNCE_DURATION_MS;
	double bt = t * 3.0;
	int bounce_idx = (int)bt;
	double bt_f = bt - (double)bounce_idx;
	if (bounce_idx > 2) {
		return 0.0;
	}
	static const double amplitudes[] = {1.0, 0.50, 0.18};
	double amp = amplitudes[bounce_idx];
	double result = amp * sin(bt_f * M_PI);
	return -result * 22.0 * scale;
}

double orange_dock_auto_hide_progress(
		double start_progress,
		bool revealed,
		uint32_t elapsed_ms) {
	start_progress = clamp(start_progress, 0.0, 1.0);
	double target = revealed ? 1.0 : 0.0;
	if (elapsed_ms >= ORANGE_DOCK_AUTO_HIDE_DURATION_MS) {
		return target;
	}
	double t = (double)elapsed_ms /
		(double)ORANGE_DOCK_AUTO_HIDE_DURATION_MS;
	double eased = t < 0.5 ?
		4.0 * t * t * t :
		1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
	return start_progress + (target - start_progress) * eased;
}

struct orange_dock_bounce_displacement orange_dock_bounce_displacement(
		const struct orange_dock_bounce *bounce,
		uint32_t now_ms,
		enum orange_dock_position position,
		double scale) {
	double offset = orange_dock_bounce_offset(bounce, now_ms, scale);
	switch (position) {
	case ORANGE_DOCK_POSITION_LEFT:
		return (struct orange_dock_bounce_displacement){-offset, 0.0};
	case ORANGE_DOCK_POSITION_RIGHT:
		return (struct orange_dock_bounce_displacement){offset, 0.0};
	case ORANGE_DOCK_POSITION_BOTTOM:
	default:
		return (struct orange_dock_bounce_displacement){0.0, offset};
	}
}

static bool dock_temporary_visible_item_is_open(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		int visible_index) {
	if (layout == NULL || state == NULL || visible_index < 0 ||
			visible_index >= ORANGE_DOCK_MAX ||
			!layout->dock_temporary[visible_index]) {
		return false;
	}
	const char *app_id = layout->dock_temporary_app_ids[visible_index];
	if (app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	int count = state->dock_temporary_count;
	if (count < 0) {
		count = 0;
	}
	if (count > ORANGE_DOCK_MAX) {
		count = ORANGE_DOCK_MAX;
	}
	for (int i = 0; i < count; i++) {
		if (state->dock_temporary_open[i] &&
				dock_ids_match(state->dock_temporary_app_ids[i], app_id)) {
			return true;
		}
	}
	return false;
}

static void draw_dock(cairo_t *cr, const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct orange_rect r = layout->dock;
	if (state != NULL && state->dock_drag_index < 0 &&
			state->dock_drag_insert_before >= 0 &&
			layout->dock_item_count > 0) {
		int step = dock_visual_step(layout);
		if (dock_layout_is_vertical(layout)) {
			r.y -= step / 2;
			r.height += step;
		} else {
			r.x -= step / 2;
			r.width += step;
		}
	}
	orange_glass_draw(cr, &r, 44 * s, dark);

	rounded_rect(cr, r.x + 0.5, r.y + 0.5,
		r.width - 1.0, r.height - 1.0, 44 * s);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
		dark ? 0.15 : 0.18);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	struct orange_dock_visual_item visual[ORANGE_DOCK_MAX];
	orange_dock_compute_visual_items(layout, state, config, visual);

	for (int i = 0; i < layout->dock_item_count; i++) {
		int launcher_idx = orange_dock_launcher_index(layout, i);

		if (i == state->dock_drag_index) {
			continue;
		}

		struct orange_rect item = visual[i].rect;
		if (state != NULL && state->dock_bounce_active &&
				state->dock_bounce_launcher_idx == launcher_idx &&
				config != NULL && config->animate_opening_applications) {
			struct orange_dock_bounce tmp = {
				.active = true,
				.launcher_idx = state->dock_bounce_launcher_idx,
				.start_time = state->dock_bounce_start_time,
			};
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			uint32_t now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
			struct orange_dock_bounce_displacement bounce =
				orange_dock_bounce_displacement(
					&tmp, now_ms, layout->dock_position, s);
			item.x += (int)bounce.x;
			item.y += (int)bounce.y;
		}
		int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
		if (state->assets != NULL && launcher_idx >= 0) {
			bool grid_icon = dock_launcher_is_grid(config, launcher_idx);
			const char *icon_name = dock_icon_name(launcher_idx, state, config);
			cairo_surface_t *icon_surface = icon_name != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon_surface != NULL) {
				if (grid_icon) {
					draw_image_tinted(cr, icon_surface, item, 255, 255, 255, 0.92);
				} else {
					draw_image_cover(cr, icon_surface, item);
				}
			}
		} else if (state->assets != NULL && layout->dock_temporary[i]) {
			const char *app_id = layout->dock_temporary_app_ids[i];
			const struct orange_desktop_entry *entry =
				lookup_entry(app_id, state->desktop_entries,
					state->desktop_entry_count);
			const char *icon_name = entry != NULL &&
				entry->icon[0] != '\0' ? entry->icon : NULL;
			if (icon_name == NULL) {
				icon_name = dock_fallback_icon(app_id);
			}
			if (icon_name == NULL) {
				icon_name = "application-x-executable";
			}
			cairo_surface_t *icon_surface =
				orange_assets_icon(state->assets, variant, icon_name);
			if (icon_surface != NULL) {
				draw_image_cover(cr, icon_surface, item);
			}
		}
		bool show_dot = false;
		if (launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				state->dock_open[launcher_idx]) {
			show_dot = true;
		} else if (dock_temporary_visible_item_is_open(layout, state, i)) {
			show_dot = true;
		}
		if ((config == NULL || config->dock_show_indicators) && show_dot) {
			double item_s = (double)item.width / 106.0;
			set_source_rgba255(cr, dark ? 255 : 34, dark ? 255 : 37, dark ? 255 : 42, 0.70);
			if (dock_layout_is_vertical(layout)) {
				double indicator_x =
					layout->dock_position == ORANGE_DOCK_POSITION_RIGHT ?
					r.x + r.width - scaled_i(10, s) :
					r.x + scaled_i(10, s);
				cairo_arc(cr, indicator_x,
					item.y + item.height / 2.0,
					4.5 * item_s, 0, 2.0 * M_PI);
			} else {
				cairo_arc(cr, item.x + item.width / 2.0,
					r.y + r.height - scaled_i(10, s),
					4.5 * item_s, 0, 2.0 * M_PI);
			}
			cairo_fill(cr);
		}
		if (layout->dock_temporary_separator.width > 0 &&
				layout->dock_temporary_separator.height > 0 && i > 0 &&
				layout->dock_temporary[i] && !layout->dock_temporary[i - 1]) {
			struct orange_rect sep = layout->dock_temporary_separator;
			set_source_rgba255(cr, dark ? 255 : 30, dark ? 255 : 34, dark ? 255 : 38, dark ? 0.45 : 0.32);
			if (dock_layout_is_vertical(layout)) {
				double sep_y = sep.y + sep.height / 2.0;
				cairo_rectangle(cr,
					r.x + scaled_i(12, s),
					sep_y,
					r.width - scaled_i(24, s),
					fmax(1.0, 2.0 * s));
			} else {
				double sep_x = sep.x + sep.width / 2.0;
				cairo_rectangle(cr, sep_x,
					r.y + scaled_i(12, s), fmax(1.0, 2.0 * s),
					r.height - scaled_i(24, s));
			}
			cairo_fill(cr);
		}
		if (launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				dock_launcher_is_trash(config, launcher_idx) && i > 0) {
			struct orange_rect separator = layout->dock_separator;
			set_source_rgba255(cr, dark ? 255 : 30, dark ? 255 : 34, dark ? 255 : 38, dark ? 0.45 : 0.32);
			if (dock_layout_is_vertical(layout)) {
				double separator_y = separator.y + separator.height / 2.0;
				cairo_rectangle(cr,
					r.x + scaled_i(12, s),
					separator_y,
					r.width - scaled_i(24, s),
					fmax(1.0, 2.0 * s));
			} else {
				double separator_x = separator.x + separator.width / 2.0;
				cairo_rectangle(cr, separator_x,
					r.y + scaled_i(12, s), fmax(1.0, 2.0 * s),
					r.height - scaled_i(24, s));
			}
			cairo_fill(cr);
		}
	}

	draw_dock_hover_label(cr, layout, state, config, visual);
}

int orange_dock_count(const struct orange_config *config) {
	if (config == NULL) {
		return 0;
	}
	int count = 0;
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (config->dock_apps[i][0] != '\0') {
			count++;
		}
	}
	return count;
}

int orange_dock_launcher_index(
		const struct orange_shell_layout *layout,
		int visible_index) {
	if (layout == NULL || visible_index < 0 ||
			visible_index >= layout->dock_item_count ||
			visible_index >= ORANGE_DOCK_MAX) {
		return -1;
	}
	return layout->dock_launcher_indices[visible_index];
}

const char *orange_dock_label(int index,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	if (config == NULL || index < 0 || index >= orange_dock_count(config)) {
		return NULL;
	}
	const char *app_id = config->dock_apps[index];
	const char *bl = builtin_label(app_id);
	if (bl != NULL) {
		return bl;
	}
	const struct orange_desktop_entry *entry = lookup_entry(app_id, entries, entry_count);
	if (entry != NULL) {
		return entry->name;
	}
	const char *fallback = dock_fallback_label(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return app_id;
}

const char *orange_dock_command(int index,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	if (config == NULL || index < 0 || index >= orange_dock_count(config)) {
		return NULL;
	}
	const char *app_id = config->dock_apps[index];
	const char *bc = orange_dock_builtin_command(app_id);
	if (bc != NULL) {
		return bc;
	}
	const struct orange_desktop_entry *entry = lookup_entry(app_id, entries, entry_count);
	if (entry != NULL) {
		return entry->exec;
	}
	const char *fallback = dock_fallback_command(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return NULL;
}

void orange_dock_draw(
		cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	draw_dock(cr, layout, state, config);
}

void orange_dock_draw_drag_overlay(
		cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	draw_dock_drag_ghost(cr, layout, state, config);
}
