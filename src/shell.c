#include "orange/shell.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REFERENCE_WIDTH 2880.0
#define REFERENCE_HEIGHT 1800.0
#define MIN_UI_SCALE 0.50
#define MAX_UI_SCALE 1.60

/* Built-in dock entries (not from .desktop files) */
static const char *builtin_label(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return "Trash";
	}
	return NULL;
}

static const char *builtin_icon(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "view-app-grid";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return "user-trash";
	}
	return NULL;
}

static const char *builtin_command(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "if [ -n \"$ORANGE_APP_PICKER\" ]; then $ORANGE_APP_PICKER; else wofi --show drun || rofi -show drun || true; fi";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return "gio open trash:// || xdg-open trash:// || true";
	}
	return NULL;
}

static bool app_id_matches(const char *app_id, const char *needle) {
	return app_id != NULL && needle != NULL &&
		(orange_desktop_entry_id_matches(needle, app_id) ||
		 orange_desktop_entry_id_matches(app_id, needle) ||
		 strstr(app_id, needle) != NULL);
}

static const char *fallback_label(const char *app_id) {
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
			app_id_matches(app_id, "terminal")) {
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

static const char *fallback_icon(const char *app_id) {
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
	if (app_id_matches(app_id, "org.gnome.TextEditor") ||
			app_id_matches(app_id, "gedit")) {
		return "accessories-text-editor";
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
			app_id_matches(app_id, "terminal")) {
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
	if (app_id_matches(app_id, "org.gnome.Loupe") ||
			app_id_matches(app_id, "loupe") ||
			app_id_matches(app_id, "ImageViewer")) {
		return "image-x-generic";
	}
	if (app_id_matches(app_id, "org.gnome.Contacts") ||
			app_id_matches(app_id, "contacts")) {
		return "x-office-address-book";
	}
	if (app_id_matches(app_id, "org.gnome.Showtime") ||
			app_id_matches(app_id, "showtime")) {
		return "video-display";
	}
	if (app_id_matches(app_id, "org.gnome.Decibels") ||
			app_id_matches(app_id, "decibels")) {
		return "audio-x-generic";
	}
	return NULL;
}

static const char *fallback_command(const char *app_id) {
	if (app_id_matches(app_id, "org.gnome.Nautilus") ||
			app_id_matches(app_id, "nautilus")) {
		return "nautilus || nemo || thunar || dolphin || xdg-open \"$HOME\"";
	}
	if (app_id_matches(app_id, "firefox") ||
			app_id_matches(app_id, "browser")) {
		return "firefox || firefox-esr || brave-browser || chromium || xdg-open \"https://duckduckgo.com\"";
	}
	if (app_id_matches(app_id, "org.gnome.Calculator") ||
			app_id_matches(app_id, "calculator")) {
		return "gnome-calculator || kcalc || galculator || xcalc || true";
	}
	if (app_id_matches(app_id, "org.gnome.TextEditor") ||
			app_id_matches(app_id, "gedit")) {
		return "gnome-text-editor || gedit || mousepad || kate || ${ORANGE_TERMINAL:-xterm} -e nano";
	}
	if (app_id_matches(app_id, "org.gnome.Settings") ||
			app_id_matches(app_id, "control-center")) {
		return "GSK_RENDERER=cairo build/orange-settings orange.conf || gnome-control-center || systemsettings || true";
	}
	if (app_id_matches(app_id, "org.gnome.Software") ||
			app_id_matches(app_id, "discover")) {
		return "gnome-software || plasma-discover || true";
	}
	if (app_id_matches(app_id, "org.gnome.Terminal") ||
			app_id_matches(app_id, "terminal")) {
		return "foot || alacritty || kitty || gnome-terminal || konsole || weston-terminal || xterm";
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
	for (int i = 0; i < entry_count; i++) {
		if (orange_desktop_entry_id_matches(entries[i].id, app_id)) {
			return &entries[i];
		}
	}
	return NULL;
}

static const char *menu_labels[] = {
	"About Orange",
	"System Settings...",
	"App Store...",
	"Recent Items",
	"Force Quit...",
	"Sleep",
	"Restart...",
	"Shut Down...",
	"Lock Screen",
	"Log Out...",
};

/* Index of first separator after each group; -1 marks end.
   A separator is drawn before the item at this index. */
static const int menu_separator_before[] = {3, 4, 5, 8, -1};

static const char *menu_icon_names[] = {
	"help-about",
	"preferences-system",
	"system-software-install",
	"document-open-recent",
	"process-stop",
	"weather-clear-night",
	"system-reboot",
	"system-shutdown",
	"system-lock-screen",
	"system-log-out",
};

static const char *dock_context_labels[] = {
	"Open",
	"Show in Files",
	"Remove from Dock",
	"Open at Login",
	"Dock Settings",
};

static const char *dock_context_icon_names[] = {
	"document-open",
	"system-file-manager",
	"list-remove",
	"system-run",
	"preferences-system",
};

static const int dock_context_separator_before[] = {2, 4, -1};

static const char *widget_context_labels[] = {
	"Edit Widget",
	"Small",
	"Medium",
	"Large",
	"Remove Widget",
};

static const char *widget_context_icon_names[] = {
	"document-properties",
	"view-restore",
	"view-fullscreen",
	"zoom-fit-best",
	"list-remove",
};

static const int widget_context_separator_before[] = {1, 4, -1};

static const char *desktop_context_labels[] = {
	"New Folder",
	"Get Info",
	"Use Stacks",
	"Sort By",
	"Clean Up By",
	"Show View Options",
	"Change Desktop Background...",
	"Edit Widgets",
};

static const char *desktop_context_icon_names[] = {
	"folder-new",
	"document-properties",
	"view-sort-ascending",
	"view-sort-descending",
	"view-refresh",
	"preferences-desktop",
	"preferences-desktop-wallpaper",
	"preferences-desktop",
};

static const int desktop_context_separator_before[] = {2, 5, -1};

static const char *desktop_icon_context_labels[] = {
	"Open",
	"Show in Files",
	"Copy",
	"Get Info",
	"Rename",
	"Duplicate",
	"Quick Look",
	"Share",
	"Move to Trash",
};

static const char *desktop_icon_context_icon_names[] = {
	"document-open",
	"system-file-manager",
	"edit-copy",
	"document-properties",
	"accessories-text-editor",
	"edit-copy",
	"zoom-fit-best",
	"document-send",
	"user-trash",
};

static const int desktop_icon_context_separator_before[] = {2, 6, 8, -1};

static const char *status_context_labels[] = {
	"Wi-Fi",
	"Bluetooth",
	"AirDrop",
	"Focus",
	"Sound",
	"Screen Mirroring",
	"Display",
	"Battery",
	"Keyboard Brightness",
	"Control Center Settings...",
};

static const char *status_context_icon_names[] = {
	"network-wireless",
	"network-bluetooth",
	"folder-publicshare",
	"preferences-system-notifications",
	"audio-volume-high",
	"video-display",
	"preferences-desktop-display",
	"battery",
	"input-keyboard",
	"preferences-system",
};

static const int status_context_separator_before[] = {4, 7, 9, -1};

static bool rect_contains(const struct orange_rect *rect, int x, int y) {
	return x >= rect->x && y >= rect->y &&
		x < rect->x + rect->width && y < rect->y + rect->height;
}

static double clamp(double value, double min_value, double max_value) {
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

static double ui_scale_for_size(int width, int height) {
	double scale = fmin((double)width / REFERENCE_WIDTH,
		(double)height / REFERENCE_HEIGHT);
	return clamp(scale, MIN_UI_SCALE, MAX_UI_SCALE);
}

static int scaled_i(double value, double scale) {
	return (int)lrint(value * scale);
}

static struct orange_rect widget_rect_for_size(
		enum orange_widget_type type,
		enum orange_widget_size size,
		int x,
		int y,
		double s) {
	int small_w = type == ORANGE_WIDGET_CALENDAR ? 328 : 326;
	int small_h = type == ORANGE_WIDGET_CALENDAR ? 329 : 326;
	int width = small_w;
	int height = small_h;
	if (size == ORANGE_WIDGET_SIZE_MEDIUM) {
		width = 674;
		height = small_h;
	} else if (size == ORANGE_WIDGET_SIZE_LARGE) {
		width = 674;
		height = 674;
	}
	return (struct orange_rect){
		x,
		y,
		scaled_i(width, s),
		scaled_i(height, s),
	};
}

static double layout_scale(const struct orange_shell_layout *layout) {
	return clamp((double)layout->menu_bar.height / 48.0,
		MIN_UI_SCALE, MAX_UI_SCALE);
}

static const struct orange_config *state_config(
		const struct orange_shell_state *state,
		struct orange_config *fallback) {
	if (state != NULL && state->config != NULL) {
		return state->config;
	}
	orange_config_set_defaults(fallback);
	return fallback;
}

static bool dock_launcher_is_trash(
		const struct orange_config *config,
		int launcher_idx) {
	return config != NULL &&
		launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
		strcmp(config->dock_apps[launcher_idx], "__trash__") == 0;
}

static int normalize_dock_launchers(
		const struct orange_config *config,
		int launchers[ORANGE_DOCK_MAX]) {
	if (config == NULL) {
		return 0;
	}

	bool used[ORANGE_DOCK_MAX] = {0};
	int visible = 0;
	int dock_count = orange_shell_dock_count(config);
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

static bool is_dark_config(const struct orange_config *config) {
	return config != NULL && config->appearance == ORANGE_APPEARANCE_DARK;
}

struct menu_palette {
	int text_r;
	int text_g;
	int text_b;
	double text_alpha;
	int separator_r;
	int separator_g;
	int separator_b;
	double separator_alpha;
	int icon_variant;
};

static struct menu_palette menu_palette_for_config(
		const struct orange_config *config) {
	bool dark = is_dark_config(config);
	return (struct menu_palette){
		.text_r = dark ? 244 : 23,
		.text_g = dark ? 245 : 26,
		.text_b = dark ? 247 : 30,
		.text_alpha = dark ? 0.94 : 0.92,
		.separator_r = dark ? 255 : 32,
		.separator_g = dark ? 255 : 36,
		.separator_b = dark ? 255 : 42,
		.separator_alpha = dark ? 0.14 : 0.13,
		.icon_variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
	};
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

static void draw_image_fit(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, double opacity) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmin((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint_with_alpha(cr, opacity);
	cairo_restore(cr);
}

static void draw_tinted_image_fit(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, double opacity, int red, int green, int blue) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmin((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_rgba(cr,
		red / 255.0,
		green / 255.0,
		blue / 255.0,
		opacity);
	cairo_mask_surface(cr, surface, 0, 0);
	cairo_restore(cr);
}

static void set_source_rgba255(cairo_t *cr, int r, int g, int b, double a) {
	cairo_set_source_rgba(cr, r / 255.0, g / 255.0, b / 255.0, a);
}

static void draw_status_battery(cairo_t *cr, struct orange_rect r) {
	double u = fmin(r.width / 48.0, r.height / 28.0);
	rounded_rect(cr, r.x, r.y + 3 * u, r.width - 5 * u, r.height - 6 * u, 4 * u);
	set_source_rgba255(cr, 255, 255, 255, 0.92);
	cairo_set_line_width(cr, 2.2 * u);
	cairo_stroke(cr);
	rounded_rect(cr, r.x + r.width - 4 * u, r.y + r.height * 0.36,
		4 * u, r.height * 0.28, 1.5 * u);
	cairo_fill(cr);
	cairo_rectangle(cr, r.x + 7 * u, r.y + 8 * u,
		r.width - 20 * u, r.height - 16 * u);
	cairo_fill(cr);
}

static struct orange_rect status_rect_before(
		int *right,
		int width,
		int height,
		int center_y,
		int gap) {
	*right -= width;
	struct orange_rect rect = {
		*right,
		center_y - height / 2,
		width,
		height,
	};
	*right -= gap;
	return rect;
}

static void clamp_desktop_items_to_visible_area(
		struct orange_shell_layout *layout,
		double scale) {
	int margin = scaled_i(8, scale);
	int min_x = margin;
	int min_y = layout->menu_bar.height + margin;
	int max_y_bottom = layout->height - margin;
	if (layout->dock.width > 0 && layout->dock.y > min_y) {
		max_y_bottom = layout->dock.y - margin;
	}

	for (int i = 0; i < layout->desktop_item_count; i++) {
		struct orange_rect *item = &layout->desktop_items[i];
		int max_x = layout->width - margin - item->width;
		int max_y = max_y_bottom - item->height;
		if (max_x < min_x) {
			max_x = min_x;
		}
		if (max_y < min_y) {
			max_y = min_y;
		}
		if (item->x < min_x) {
			item->x = min_x;
		}
		if (item->x > max_x) {
			item->x = max_x;
		}
		if (item->y < min_y) {
			item->y = min_y;
		}
		if (item->y > max_y) {
			item->y = max_y;
		}
	}
}

static void draw_text(cairo_t *cr, const char *text, double x, double y,
		double size, int r, int g, int b, double alpha, bool bold) {
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, size);
	set_source_rgba255(cr, r, g, b, alpha);
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, text);
}

static const char *dock_icon_name(int launcher_idx,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	const char *app_id = config != NULL && launcher_idx >= 0 &&
		launcher_idx < ORANGE_DOCK_MAX ? config->dock_apps[launcher_idx] : NULL;
	if (app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	/* Check built-in entries first */
	const char *bi = builtin_icon(app_id);
	if (bi != NULL) {
		return bi;
	}
	/* Look up in scanned .desktop entries */
	const struct orange_desktop_entry *entry = lookup_entry(app_id, entries, entry_count);
	if (entry != NULL && entry->icon[0] != '\0') {
		return entry->icon;
	}
	const char *fallback = fallback_icon(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return "application-x-executable";
}

static void draw_wallpaper(cairo_t *cr, int width, int height,
		struct orange_assets *assets,
		const struct orange_config *config) {
	cairo_surface_t *wallpaper = NULL;
	if (assets != NULL) {
		wallpaper = orange_assets_wallpaper(assets,
			is_dark_config(config), width, height);
	}
	if (wallpaper != NULL) {
		cairo_set_source_surface(cr, wallpaper, 0, 0);
		cairo_paint(cr);
		return;
	}

	cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 0, height * 0.62);
	cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.23, 0.50, 0.76);
	cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.54, 0.73, 0.85);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_set_source(cr, sky);
	cairo_fill(cr);
	cairo_pattern_destroy(sky);

	double horizon = height * 0.53;
	set_source_rgba255(cr, 57, 90, 126, 1.0);
	cairo_move_to(cr, 0, horizon);
	for (int i = 0; i <= 16; i++) {
		double x = width * (double)i / 16.0;
		double y = horizon - 45.0 - 32.0 * sin(i * 1.7);
		cairo_line_to(cr, x, y);
	}
	cairo_line_to(cr, width, horizon + 40);
	cairo_line_to(cr, 0, horizon + 40);
	cairo_close_path(cr);
	cairo_fill(cr);

	set_source_rgba255(cr, 232, 241, 246, 0.94);
	for (int i = 0; i < 11; i++) {
		double cx = width * (0.05 + i * 0.095);
		double peak = horizon - 72.0 - 25.0 * sin(i * 1.3);
		cairo_move_to(cr, cx - 50, horizon - 30);
		cairo_line_to(cr, cx, peak);
		cairo_line_to(cr, cx + 58, horizon - 30);
		cairo_close_path(cr);
		cairo_fill(cr);
	}

	cairo_pattern_t *lake = cairo_pattern_create_linear(0, horizon, 0, height);
	cairo_pattern_add_color_stop_rgb(lake, 0.0, 0.02, 0.28, 0.45);
	cairo_pattern_add_color_stop_rgb(lake, 0.45, 0.08, 0.58, 0.65);
	cairo_pattern_add_color_stop_rgb(lake, 1.0, 0.56, 0.78, 0.70);
	cairo_rectangle(cr, 0, horizon, width, height - horizon);
	cairo_set_source(cr, lake);
	cairo_fill(cr);
	cairo_pattern_destroy(lake);

	set_source_rgba255(cr, 255, 255, 255, 0.17);
	for (int i = 0; i < 18; i++) {
		double y = horizon + 30 + i * 24;
		cairo_rectangle(cr, 0, y, width, 1.0);
		cairo_fill(cr);
	}

	for (int i = 0; i < 18; i++) {
		double x = width * fmod(0.13 + i * 0.173, 1.0);
		double y = horizon + 45 + (i % 7) * 43;
		double rw = 70 + (i % 5) * 28;
		double rh = 24 + (i % 3) * 11;
		set_source_rgba255(cr, 160, 170, 171, 0.85);
		cairo_save(cr);
		cairo_translate(cr, x, y);
		cairo_scale(cr, 1.0, 0.58);
		cairo_arc(cr, 0, 0, rw * 0.5, 0, 2.0 * M_PI);
		cairo_fill(cr);
		cairo_restore(cr);
		set_source_rgba255(cr, 245, 249, 249, 0.35);
		cairo_save(cr);
		cairo_translate(cr, x - rw * 0.13, y - rh * 0.16);
		cairo_scale(cr, 1.0, 0.45);
		cairo_arc(cr, 0, 0, rw * 0.24, 0, 2.0 * M_PI);
		cairo_fill(cr);
		cairo_restore(cr);
	}

	double tree_x = width * 0.91;
	set_source_rgba255(cr, 42, 58, 50, 0.8);
	cairo_rectangle(cr, tree_x, horizon - 15, 10, 210);
	cairo_fill(cr);
	for (int i = 0; i < 7; i++) {
		double cy = horizon - 60 + i * 22;
		double rad = 48 - i * 3;
		set_source_rgba255(cr, 37, 99, 74, 0.88);
		cairo_save(cr);
		cairo_translate(cr, tree_x + 5, cy);
		cairo_scale(cr, 0.58, 1.0);
		cairo_arc(cr, 0, 0, rad, 0, 2.0 * M_PI);
		cairo_fill(cr);
		cairo_restore(cr);
	}
}

static void draw_liquid_panel(cairo_t *cr, const struct orange_rect *rect,
		double radius, double alpha, bool dark) {
	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	if (dark) {
		set_source_rgba255(cr, 11, 13, 18, alpha);
	} else {
		set_source_rgba255(cr, 255, 255, 255, alpha);
	}
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.18 : 0.38);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.08 : 0.13);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_dock_glass(cairo_t *cr, const struct orange_rect *rect,
		double radius, bool dark) {
	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.95, 0.97, 1.0, 0.18);
		cairo_pattern_add_color_stop_rgba(shade, 0.45, 0.10, 0.13, 0.17, 0.40);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.02, 0.03, 0.04, 0.48);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 1.0, 1.0, 1.0, 0.44);
		cairo_pattern_add_color_stop_rgba(shade, 0.48, 0.86, 0.91, 0.87, 0.30);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.38, 0.45, 0.43, 0.20);
	}
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);

	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.24 : 0.42);
	cairo_set_line_width(cr, 1.1);
	cairo_stroke(cr);
	rounded_rect(cr, rect->x + 2.0, rect->y + 2.0,
		rect->width - 4.0, rect->height - 4.0, radius - 2.0);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.07 : 0.16);
	cairo_stroke(cr);
}

static void draw_menu_glass(cairo_t *cr, const struct orange_rect *rect,
		double radius, bool dark) {
	rounded_rect(cr, rect->x, rect->y + 3.0, rect->width, rect->height, radius);
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.30 : 0.18);
	cairo_fill(cr);

	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.17, 0.18, 0.20, 0.88);
		cairo_pattern_add_color_stop_rgba(shade, 0.55, 0.12, 0.13, 0.15, 0.84);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.07, 0.08, 0.10, 0.82);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 1.0, 1.0, 1.0, 0.92);
		cairo_pattern_add_color_stop_rgba(shade, 0.55, 0.96, 0.97, 0.98, 0.88);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.80, 0.84, 0.88, 0.83);
	}
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);

	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.20 : 0.48);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.07 : 0.18);
	cairo_stroke(cr);
}

static void draw_menu_bar(cairo_t *cr, const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	(void)config;
	int text_r = 255;
	int text_g = 255;
	int text_b = 255;
	int status_r = 252;
	int status_g = 252;
	int status_b = 252;

	cairo_surface_t *orange_menu = state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT, "orange-menu") : NULL;
	if (orange_menu != NULL) {
		draw_tinted_image_fit(cr, orange_menu,
			(struct orange_rect){
				scaled_i(39, s),
				scaled_i(13, s),
				scaled_i(31, s),
				scaled_i(31, s),
			},
			0.98,
			255, 255, 255);
	} else {
		draw_text(cr, "O", scaled_i(44, s), scaled_i(38, s),
			26 * s, 255, 255, 255, 0.98, true);
	}

	const char *menus[] = {
		"Files", "File", "Edit", "View", "Go", "Window", "Help",
	};
	double x = scaled_i(109, s);
	for (size_t i = 0; i < sizeof(menus) / sizeof(menus[0]); i++) {
		draw_text(cr, menus[i], x, scaled_i(40, s), 28 * s,
			text_r, text_g, text_b, 0.95, i == 0);
		cairo_text_extents_t extents;
		cairo_text_extents(cr, menus[i], &extents);
		x += extents.x_advance + scaled_i(36, s);
	}

	time_t now = state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	char clock_text[64];
	char day_month[24];
	char time_text[24];
	strftime(day_month, sizeof(day_month), "%a %b", &local);
	strftime(time_text, sizeof(time_text), "%I:%M %p", &local);
	const char *trimmed_time = time_text[0] == '0' ? time_text + 1 : time_text;
	snprintf(clock_text, sizeof(clock_text), "%s %d  %s",
		day_month, local.tm_mday, trimmed_time);

	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 28 * s);
	cairo_text_extents_t clock_extents;
	cairo_text_extents(cr, clock_text, &clock_extents);
	double clock_x = layout->width - scaled_i(34, s) - clock_extents.x_advance;
	draw_text(cr, clock_text, clock_x, scaled_i(40, s), 28 * s,
		text_r, text_g, text_b, 0.95, true);

	struct orange_assets *assets = state->assets;
	int center_y = scaled_i(29, s);
	int gap = scaled_i(16, s);
	int status_right = (int)clock_x - scaled_i(28, s);
	struct orange_rect control_rect = status_rect_before(&status_right,
		scaled_i(29, s), scaled_i(27, s), center_y, gap);
	struct orange_rect search_rect = status_rect_before(&status_right,
		scaled_i(27, s), scaled_i(27, s), center_y, gap);
	struct orange_rect battery_rect = status_rect_before(&status_right,
		scaled_i(34, s), scaled_i(24, s), center_y, gap);
	struct orange_rect sound_rect = status_rect_before(&status_right,
		scaled_i(27, s), scaled_i(27, s), center_y, gap);
	struct orange_rect wifi_rect = status_rect_before(&status_right,
		scaled_i(31, s), scaled_i(27, s), center_y, gap);

	int variant = ORANGE_ASSET_ICON_LIGHT;
	const char *wifi_name = state->status.network_icon[0] != '\0' ?
		state->status.network_icon : "network-wireless";
	cairo_surface_t *wifi_icon = assets != NULL ?
		orange_assets_icon(assets, variant, wifi_name) : NULL;
	if (wifi_icon != NULL) {
		draw_tinted_image_fit(cr, wifi_icon, wifi_rect, 0.92,
			status_r, status_g, status_b);
	}
	const char *sound_name = state->status.sound_icon[0] != '\0' ?
		state->status.sound_icon : "audio-volume-high";
	cairo_surface_t *sound_icon = assets != NULL ?
		orange_assets_icon(assets, variant, sound_name) : NULL;
	if (sound_icon != NULL) {
		draw_tinted_image_fit(cr, sound_icon, sound_rect, 0.88,
			status_r, status_g, status_b);
	}
	const char *battery_name = state->status.battery_icon[0] != '\0' ?
		state->status.battery_icon : "battery";
	cairo_surface_t *battery_icon = assets != NULL ?
		orange_assets_icon(assets, variant, battery_name) : NULL;
	if (battery_icon != NULL) {
		draw_tinted_image_fit(cr, battery_icon, battery_rect, 0.92,
			status_r, status_g, status_b);
	} else {
		draw_status_battery(cr, battery_rect);
	}
	cairo_surface_t *search_icon = assets != NULL ?
		orange_assets_icon(assets, variant, "edit-find") : NULL;
	if (search_icon != NULL) {
		draw_tinted_image_fit(cr, search_icon, search_rect, 0.88,
			status_r, status_g, status_b);
	}
	cairo_surface_t *control_icon = assets != NULL ?
		orange_assets_icon(assets, variant, "control-center") : NULL;
	if (control_icon == NULL && assets != NULL) {
		control_icon = orange_assets_icon(assets, variant, "preferences-system");
	}
	if (control_icon != NULL) {
		draw_tinted_image_fit(cr, control_icon, control_rect, 0.88,
			status_r, status_g, status_b);
	}
}

static void draw_calendar_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct orange_rect r = layout->calendar_widget;
	draw_liquid_panel(cr, &r, 30 * s, dark ? 0.58 : 0.86, dark);
	int primary = dark ? 235 : 36;

	time_t now = state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);

	char month[32];
	strftime(month, sizeof(month), "%B", &local);
	for (char *p = month; *p != '\0'; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p = (char)(*p - 'a' + 'A');
		}
	}
	draw_text(cr, month, r.x + scaled_i(36, s), r.y + scaled_i(59, s),
		22 * s, 238, 70, 76, 1.0, true);

	const char *days = "S   M   T   W   T   F   S";
	draw_text(cr, days, r.x + scaled_i(46, s), r.y + scaled_i(101, s),
		22 * s, primary, primary, primary + (dark ? 5 : 12), 0.78, true);

	struct tm first = local;
	first.tm_mday = 1;
	mktime(&first);
	int first_wday = first.tm_wday;
	int days_in_month = 31;
	for (int d = 28; d <= 31; d++) {
		struct tm probe = first;
		probe.tm_mday = d + 1;
		mktime(&probe);
		if (probe.tm_mday == 1) {
			days_in_month = d;
			break;
		}
	}

	int cell = scaled_i(40, s);
	int start_x = r.x + scaled_i(24, s);
	int start_y = r.y + scaled_i(126, s);
	for (int d = 1; d <= days_in_month; d++) {
		int n = first_wday + d - 1;
		int col = n % 7;
		int row = n / 7;
		int cx = start_x + col * cell + cell / 2;
		int cy = start_y + row * cell;
		char day_text[16];
		snprintf(day_text, sizeof(day_text), "%d", d);
		cairo_text_extents_t extents;
		cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 22 * s);
		cairo_text_extents(cr, day_text, &extents);
		double text_x = cx - extents.width / 2.0 - extents.x_bearing;
		double text_y = cy + extents.height / 2.0;
		if (d == local.tm_mday) {
			set_source_rgba255(cr, 242, 78, 79, 1.0);
			cairo_arc(cr, cx, cy,
				scaled_i(18, s), 0, 2.0 * M_PI);
			cairo_fill(cr);
			draw_text(cr, day_text, text_x, text_y,
				22 * layout_scale(layout), 255, 255, 255, 1.0, true);
		} else {
			draw_text(cr, day_text, text_x, text_y,
				22 * layout_scale(layout), primary, primary, primary + (dark ? 5 : 12),
				0.86, true);
		}
	}
}

static void draw_weather_crescent(cairo_t *cr, double cx, double cy, double radius) {
	set_source_rgba255(cr, 255, 255, 255, 0.96);
	cairo_arc(cr, cx, cy, radius, 0, 2.0 * M_PI);
	cairo_fill(cr);
	set_source_rgba255(cr, 42, 55, 74, 1.0);
	cairo_arc(cr, cx + radius * 0.42, cy - radius * 0.18,
		radius * 0.92, 0, 2.0 * M_PI);
	cairo_fill(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.96);
	cairo_arc(cr, cx + radius * 0.94, cy - radius * 0.72,
		radius * 0.22, 0, 2.0 * M_PI);
	cairo_fill(cr);
	cairo_arc(cr, cx + radius * 1.28, cy - radius * 0.25,
		radius * 0.15, 0, 2.0 * M_PI);
	cairo_fill(cr);
}

static void draw_weather_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct orange_rect r = layout->weather_widget;
	draw_liquid_panel(cr, &r, 32 * s, 0.24, true);

	cairo_pattern_t *shade = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.06, 0.06, 0.15, 0.86);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.10, 0.14, 0.27, 0.74);
	rounded_rect(cr, r.x, r.y, r.width, r.height, 32 * s);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	draw_text(cr, "Memphis", r.x + scaled_i(47, s), r.y + scaled_i(61, s),
		32 * s, 255, 255, 255, 0.96, true);
	draw_text(cr, "79°", r.x + scaled_i(47, s), r.y + scaled_i(151, s),
		90 * s, 255, 255, 255, 0.97, false);
	cairo_surface_t *weather_icon = state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT, "weather-clear") : NULL;
	if (weather_icon != NULL) {
		draw_image_fit(cr, weather_icon,
			(struct orange_rect){
				r.x + scaled_i(50, s),
				r.y + r.height - scaled_i(86, s),
				scaled_i(38, s),
				scaled_i(38, s),
			},
			0.95);
	} else {
		draw_weather_crescent(cr,
			r.x + scaled_i(69, s),
			r.y + r.height - scaled_i(67, s),
			scaled_i(16, s));
	}
	draw_text(cr, "Air quality alert", r.x + scaled_i(47, s),
		r.y + r.height - scaled_i(34, s), 26 * s,
		255, 255, 255, 0.88, true);
	(void)dark;
}

struct dock_visual_item {
	struct orange_rect rect;
	double scale;
};

static double dock_magnification_for_item(
		const struct orange_rect *item,
		double pointer_x,
		double max_scale) {
	double center_x = item->x + item->width / 2.0;
	double distance = fabs(pointer_x - center_x);
	double radius = fmax(1.0, item->width * 2.35);
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
			state->dock_drag_index >= 0) {
		return false;
	}
	return state->hot_dock_index >= 0 &&
		state->hot_dock_index < layout->dock_item_count;
}

static void compute_dock_visual_items(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct dock_visual_item visual[ORANGE_DOCK_MAX]) {
	for (int i = 0; i < layout->dock_item_count; i++) {
		visual[i] = (struct dock_visual_item){
			.rect = layout->dock_items[i],
			.scale = 1.0,
		};
	}

	if (!dock_magnification_active(layout, state, config)) {
		return;
	}

	int hot = state->hot_dock_index;
	double pointer_x = state->dock_pointer_x > 0 ?
		state->dock_pointer_x :
		layout->dock_items[hot].x + layout->dock_items[hot].width / 2.0;
	double max_scale = clamp(config->dock_magnification_scale, 1.0, 2.20);
	for (int i = 0; i < layout->dock_item_count; i++) {
		struct orange_rect base = layout->dock_items[i];
		double item_scale = dock_magnification_for_item(
			&base, pointer_x, max_scale);
		int width = (int)lrint(base.width * item_scale);
		int height = (int)lrint(base.height * item_scale);
		int x = (int)lrint(base.x + base.width / 2.0 - width / 2.0);
		visual[i].rect = (struct orange_rect){
			x,
			base.y + base.height - height,
			width,
			height,
		};
		visual[i].scale = item_scale;
	}
}

static void draw_dock_hover_label(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		const struct dock_visual_item visual[ORANGE_DOCK_MAX]) {
	int hot = state->hot_dock_index;
	if (hot < 0 || hot >= layout->dock_item_count ||
			hot == state->dock_drag_index) {
		return;
	}

	int launcher_idx = orange_shell_dock_launcher_index(layout, hot);
	const char *label = orange_shell_dock_label(launcher_idx,
		state->desktop_entries, state->desktop_entry_count, config);
	if (label == NULL) {
		return;
	}

	double s = layout_scale(layout);
	struct orange_rect item = visual[hot].rect;
	double font_size = fmax(11.0, 19.0 * s);
	double pad_x = fmax(8.0, 12.0 * s);
	double bubble_h = fmax(24.0, 34.0 * s);
	double gap = fmax(7.0, 10.0 * s);

	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, label, &extents);
	double bubble_w = extents.x_advance + pad_x * 2.0;
	double bubble_x = item.x + item.width / 2.0 - bubble_w / 2.0;
	double bubble_y = item.y - gap - bubble_h;
	if (bubble_x < 6.0 * s) {
		bubble_x = 6.0 * s;
	}
	if (bubble_x + bubble_w > layout->width - 6.0 * s) {
		bubble_x = layout->width - 6.0 * s - bubble_w;
	}
	if (bubble_y < layout->menu_bar.height + 4.0 * s) {
		bubble_y = item.y + item.height + gap;
	}

	rounded_rect(cr, bubble_x, bubble_y, bubble_w, bubble_h, bubble_h * 0.34);
	set_source_rgba255(cr, 28, 29, 34, 0.78);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.16);
	cairo_set_line_width(cr, fmax(1.0, s));
	cairo_stroke(cr);

	draw_text(cr, label,
		bubble_x + pad_x - extents.x_bearing,
		bubble_y + (bubble_h + extents.height) / 2.0 - extents.y_bearing * 0.25,
		font_size, 255, 255, 255, 0.96, true);
}

static void draw_dock(cairo_t *cr, const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct orange_rect r = layout->dock;
	set_source_rgba255(cr, 0, 0, 0, 0.18);
	rounded_rect(cr, r.x + scaled_i(2, s), r.y + scaled_i(7, s),
		r.width, r.height, 34 * s);
	cairo_fill(cr);
	draw_dock_glass(cr, &r, 34 * s, dark);

	struct dock_visual_item visual[ORANGE_DOCK_MAX];
	compute_dock_visual_items(layout, state, config, visual);

	for (int i = 0; i < layout->dock_item_count; i++) {
		int launcher_idx = orange_shell_dock_launcher_index(layout, i);

		if (i == state->dock_drag_index) {
			continue;
		}

		struct orange_rect item = visual[i].rect;
		int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
		if (state->assets != NULL && launcher_idx >= 0) {
			const char *icon_name = dock_icon_name(launcher_idx,
				state->desktop_entries, state->desktop_entry_count, config);
			cairo_surface_t *icon_surface = icon_name != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon_surface != NULL) {
				draw_image_cover(cr, icon_surface, item);
			}
		}
		if ((config == NULL || config->dock_show_indicators) &&
				launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				state->dock_open[launcher_idx]) {
			double item_s = (double)item.width / 106.0;
			set_source_rgba255(cr, 34, 37, 42, 0.70);
			cairo_arc(cr, item.x + item.width / 2.0,
				r.y + r.height - scaled_i(9, s),
				2.1 * item_s, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		if (launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				dock_launcher_is_trash(config, launcher_idx) && i > 0) {
			struct orange_rect prev_base = layout->dock_items[i - 1];
			double separator_x = prev_base.x + prev_base.width +
				(item.x - prev_base.x - prev_base.width) / 2.0;
			set_source_rgba255(cr, 30, 34, 38, 0.32);
			cairo_rectangle(cr, separator_x,
				r.y + scaled_i(17, s), fmax(1.0, 1.5 * s),
				r.height - scaled_i(34, s));
			cairo_fill(cr);
		}
	}

	draw_dock_hover_label(cr, layout, state, config, visual);

	if (state->dock_drag_index >= 0 && state->dock_drag_index < layout->dock_item_count) {
		int launcher_idx = orange_shell_dock_launcher_index(
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
		if (state->assets != NULL && launcher_idx >= 0) {
			const char *icon_name = dock_icon_name(launcher_idx,
				state->desktop_entries, state->desktop_entry_count, config);
			cairo_surface_t *icon_surface = icon_name != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon_surface != NULL) {
				draw_image_cover(cr, icon_surface, ghost_item);
			}
		}
		cairo_pop_group_to_source(cr);
		cairo_paint_with_alpha(cr, 0.70);
	}
}

static void draw_centered_label(cairo_t *cr,
		const char *label,
		struct orange_rect bounds,
		double size,
		int r,
		int g,
		int b) {
	char first[128] = {0};
	char second[128] = {0};
	const char *space = strchr(label, ' ');
	if (space != NULL && strlen(label) > 11) {
		size_t first_len = (size_t)(space - label);
		snprintf(first, sizeof(first), "%.*s", (int)first_len, label);
		snprintf(second, sizeof(second), "%s", space + 1);
	} else {
		snprintf(first, sizeof(first), "%s", label);
	}

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, size);
	const char *lines[] = {first, second};
	int line_count = second[0] != '\0' ? 2 : 1;
	double y = bounds.y + size;
	for (int i = 0; i < line_count; i++) {
		cairo_text_extents_t extents;
		cairo_text_extents(cr, lines[i], &extents);
		double x = bounds.x + (bounds.width - extents.width) / 2.0 -
			extents.x_bearing;
		draw_text(cr, lines[i], x, y, size, r, g, b, 0.96, true);
		y += size + fmax(2.0, size * 0.15);
	}
}

static bool surface_has_visible_pixels(cairo_surface_t *surface) {
	if (surface == NULL) {
		return false;
	}
	if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) {
		return true;
	}
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	if (width <= 0 || height <= 0 || stride <= 0) {
		return false;
	}
	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	if (data == NULL) {
		return false;
	}
	int step_x = width / 8;
	int step_y = height / 8;
	if (step_x < 1) {
		step_x = 1;
	}
	if (step_y < 1) {
		step_y = 1;
	}
	for (int y = 0; y < height; y += step_y) {
		for (int x = 0; x < width; x += step_x) {
			unsigned char *pixel = data + y * stride + x * 4;
			if (pixel[3] > 12) {
				return true;
			}
		}
	}
	return false;
}

static cairo_surface_t *desktop_icon_surface_for_entry(
		struct orange_assets *assets,
		int variant,
		const struct orange_desktop_entry *entry) {
	if (assets == NULL || entry == NULL) {
		return NULL;
	}
	const char *names[] = {
		entry->icon,
		fallback_icon(entry->id),
		"application-x-executable",
		"application-default-icon",
		"folder",
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (names[i] == NULL || names[i][0] == '\0') {
			continue;
		}
		bool duplicate = false;
		for (size_t j = 0; j < i; j++) {
			if (names[j] != NULL && strcmp(names[i], names[j]) == 0) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			continue;
		}
		cairo_surface_t *surface = orange_assets_icon(assets, variant, names[i]);
		if (surface_has_visible_pixels(surface)) {
			return surface;
		}
	}
	return NULL;
}

static void draw_desktop_placeholder_icon(cairo_t *cr,
		const struct orange_desktop_entry *entry,
		struct orange_rect r,
		bool dark) {
	double radius = r.width * 0.22;
	rounded_rect(cr, r.x + 2.0, r.y + 3.0, r.width, r.height, radius);
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.26 : 0.18);
	cairo_fill(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		r.x, r.y, r.x, r.y + r.height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 85 / 255.0, 91 / 255.0,
			104 / 255.0, 0.98);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 42 / 255.0, 48 / 255.0,
			60 / 255.0, 0.98);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 248 / 255.0, 251 / 255.0,
			255 / 255.0, 0.98);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 193 / 255.0, 204 / 255.0,
			219 / 255.0, 0.98);
	}
	rounded_rect(cr, r.x, r.y, r.width, r.height, radius);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	rounded_rect(cr, r.x + 1.0, r.y + 1.0, r.width - 2.0, r.height - 2.0,
		radius - 1.0);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.18 : 0.46);
	cairo_set_line_width(cr, 1.2);
	cairo_stroke(cr);

	const char *label = entry != NULL && entry->name[0] != '\0' ?
		entry->name : (entry != NULL ? entry->id : "O");
	char initial[2] = {'O', '\0'};
	for (const char *p = label; p != NULL && *p != '\0'; p++) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9')) {
			initial[0] = *p >= 'a' && *p <= 'z' ? (char)(*p - 32) : *p;
			break;
		}
	}

	double font_size = r.height * 0.46;
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, initial, &extents);
	double x = r.x + (r.width - extents.width) / 2.0 - extents.x_bearing;
	double y = r.y + (r.height - extents.height) / 2.0 - extents.y_bearing;
	draw_text(cr, initial, x, y, font_size,
		dark ? 244 : 57, dark ? 246 : 64, dark ? 250 : 74,
		0.92, true);
}

static void draw_desktop_items(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (config != NULL && !config->desktop_icons_visible) {
		return;
	}
	bool dark = is_dark_config(config);
	int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	double s = layout_scale(layout);
	for (int i = 0; i < layout->desktop_item_count; i++) {
		const struct orange_desktop_entry *entry = NULL;
		if (state->desktop_entries != NULL && i < state->desktop_entry_count) {
			entry = &state->desktop_entries[i];
		}
		if (entry == NULL) {
			continue;
		}
		struct orange_rect r = layout->desktop_items[i];
		int icon_size = (int)lrint(r.width * (116.0 / 194.0));
		struct orange_rect icon_rect = {
			r.x + (r.width - icon_size) / 2,
			r.y,
			icon_size,
			icon_size,
		};
		cairo_surface_t *icon = desktop_icon_surface_for_entry(
			state->assets, variant, entry);
		if (icon != NULL) {
			draw_image_fit(cr, icon, icon_rect, 1.0);
		} else {
			draw_desktop_placeholder_icon(cr, entry, icon_rect, dark);
		}

		struct orange_rect label_rect = {
			r.x - (int)lrint(r.width * (8.0 / 194.0)),
			r.y + (int)lrint(r.height * (128.0 / 212.0)),
			r.width + (int)lrint(r.width * (16.0 / 194.0)),
			(int)lrint(r.height * (76.0 / 212.0)),
		};
		draw_centered_label(cr, entry->name, label_rect,
			(config != NULL ? config->desktop_label_size : 20) * 1.65 * s,
			255, 255, 255);
	}
}

static void draw_system_menu(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	if (layout->system_menu_item_count == 0) {
		return;
	}

	struct orange_rect panel = layout->system_menu_panel;
	bool dark = is_dark_config(config);
	struct menu_palette palette = menu_palette_for_config(config);
	draw_menu_glass(cr, &panel, 14 * s, dark);
	int icon_size = scaled_i(18, s);
	for (int i = 0; i < layout->system_menu_item_count; i++) {
		struct orange_rect item = layout->system_menu_items[i];
		if (i > 0 && layout->system_menu_separator[i]) {
			set_source_rgba255(cr, palette.separator_r,
				palette.separator_g, palette.separator_b,
				palette.separator_alpha);
			cairo_rectangle(cr, panel.x + scaled_i(12, s),
				item.y - 1,
				panel.width - scaled_i(24, s), 1);
			cairo_fill(cr);
		}
		if (state != NULL && state->assets != NULL) {
			cairo_surface_t *icon = orange_assets_icon(
				state->assets, palette.icon_variant, menu_icon_names[i]);
			if (icon != NULL) {
				struct orange_rect icon_rect = {
					item.x + scaled_i(10, s),
					item.y + (item.height - icon_size) / 2,
					icon_size,
					icon_size,
				};
				draw_image_fit(cr, icon, icon_rect, 0.85);
			}
		}
		draw_text(cr, orange_shell_menu_label(i),
			item.x + scaled_i(36, s),
			item.y + scaled_i(28, s),
			20 * s,
			palette.text_r, palette.text_g, palette.text_b,
			palette.text_alpha, i == 0);
	}
}

static void draw_context_menu(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_NONE ||
			layout->context_menu_item_count == 0) {
		return;
	}

	struct orange_rect panel = layout->context_menu_panel;
	bool dark = is_dark_config(config);
	struct menu_palette palette = menu_palette_for_config(config);
	draw_menu_glass(cr, &panel, 13 * s, dark);
	int icon_size = scaled_i(18, s);
	for (int i = 0; i < layout->context_menu_item_count; i++) {
		struct orange_rect item = layout->context_menu_items[i];
		if (i > 0 && layout->context_menu_separator[i]) {
			set_source_rgba255(cr, palette.separator_r,
				palette.separator_g, palette.separator_b,
				palette.separator_alpha);
			cairo_rectangle(cr, panel.x + scaled_i(11, s),
				item.y - 1,
				panel.width - scaled_i(22, s), 1);
			cairo_fill(cr);
		}
		if (state != NULL && state->assets != NULL) {
			const char *icon_name = orange_shell_context_menu_icon_name(
				layout->context_menu_kind, i);
			if (icon_name != NULL) {
				cairo_surface_t *icon = orange_assets_icon(
					state->assets, palette.icon_variant, icon_name);
				if (icon != NULL) {
					struct orange_rect icon_rect = {
						item.x + scaled_i(10, s),
						item.y + (item.height - icon_size) / 2,
						icon_size,
						icon_size,
					};
					draw_image_fit(cr, icon, icon_rect, 0.85);
				}
			}
		}
		draw_text(cr,
			orange_shell_context_menu_label(layout->context_menu_kind, i),
			item.x + scaled_i(36, s),
			item.y + scaled_i(27, s),
			19 * s,
			palette.text_r, palette.text_g, palette.text_b,
			palette.text_alpha, false);
	}
}

void orange_shell_layout_compute(
		int width,
		int height,
		bool system_menu_open,
		const struct orange_config *config,
		int desktop_entry_count,
		struct orange_shell_layout *layout) {
	memset(layout, 0, sizeof(*layout));
	struct orange_config defaults;
	if (config == NULL) {
		orange_config_set_defaults(&defaults);
		config = &defaults;
	}
	double s = ui_scale_for_size(width, height);
	layout->width = width;
	layout->height = height;
	layout->menu_bar = (struct orange_rect){0, 0, width, scaled_i(48, s)};
	layout->system_menu_button = (struct orange_rect){
		scaled_i(31, s), 0, scaled_i(52, s), layout->menu_bar.height};
	layout->app_menu_button = (struct orange_rect){
		scaled_i(95, s), 0, scaled_i(116, s), layout->menu_bar.height};
	layout->status_area = (struct orange_rect){
		width - scaled_i(558, s),
		0,
		scaled_i(524, s),
		layout->menu_bar.height,
	};

	layout->calendar_widget = widget_rect_for_size(ORANGE_WIDGET_CALENDAR,
		config->calendar_widget_size, scaled_i(32, s), scaled_i(92, s), s);
	layout->weather_widget = widget_rect_for_size(ORANGE_WIDGET_WEATHER,
		config->weather_widget_size, scaled_i(383, s), scaled_i(92, s), s);
	layout->widget_count = 2;
	layout->widgets[0] = (struct orange_widget){
		.id = 1,
		.type = ORANGE_WIDGET_CALENDAR,
		.visible = config->calendar_widget_visible,
		.rect = layout->calendar_widget,
	};
	layout->widgets[1] = (struct orange_widget){
		.id = 2,
		.type = ORANGE_WIDGET_WEATHER,
		.visible = config->weather_widget_visible,
		.rect = layout->weather_widget,
	};

	layout->desktop_item_count = config->desktop_icons_visible ?
		desktop_entry_count : 0;
	if (layout->desktop_item_count > ORANGE_DESKTOP_MAX) {
		layout->desktop_item_count = ORANGE_DESKTOP_MAX;
	}
	int item_x = width - scaled_i(231, s);
	int item_step = scaled_i(200 + config->desktop_grid_spacing, s);
	int item_w = scaled_i(194 * config->desktop_icon_scale, s);
	int item_h = scaled_i(212 * config->desktop_icon_scale, s);
	int start_y = scaled_i(96, s);
	int avail = height - start_y - scaled_i(80, s);
	int max_fit = avail / item_step + 1;
	if (layout->desktop_item_count > max_fit) {
		layout->desktop_item_count = max_fit;
	}
	layout->desktop_items[0] = (struct orange_rect){
		item_x, start_y, item_w, item_h};
	for (int i = 1; i < layout->desktop_item_count; i++) {
		layout->desktop_items[i] = (struct orange_rect){
			item_x - scaled_i(7, s),
			start_y + i * item_step,
			item_w,
			item_h,
		};
	}
	for (int i = 0; i < layout->desktop_item_count &&
			i < ORANGE_DESKTOP_POSITION_MAX; i++) {
		if (config->desktop_positions[i].valid) {
			layout->desktop_items[i].x = config->desktop_positions[i].x;
			layout->desktop_items[i].y = config->desktop_positions[i].y;
		}
	}

	double dock_s = s * config->dock_scale;
	int icon = scaled_i(106, s * config->dock_icon_scale * config->dock_scale);
	int gap = scaled_i(18, dock_s);
	int separator_extra = scaled_i(66, dock_s);
	int left_pad = scaled_i(28, dock_s);
	int right_pad = scaled_i(28, dock_s);
	int top_pad = scaled_i(27, dock_s);
	int bottom_pad = scaled_i(16, dock_s);
	int bottom_margin = scaled_i(12, dock_s);
	int visible_launchers[ORANGE_DOCK_MAX] = {0};
	int dock_count = normalize_dock_launchers(config, visible_launchers);
	int separator_count = 0;
	for (int i = 1; i < dock_count; i++) {
		if (dock_launcher_is_trash(config, visible_launchers[i])) {
			separator_count++;
		}
	}
	int dock_width = dock_count * icon +
		(dock_count > 1 ? (dock_count - 1) * gap : 0) +
		separator_count * separator_extra + left_pad + right_pad;
	while (dock_width > width - scaled_i(80, s) && icon > scaled_i(40, s)) {
		icon -= 2;
		gap = gap > 6 ? gap - 1 : gap;
		separator_extra = separator_extra > scaled_i(18, s) ?
			separator_extra - 2 : separator_extra;
		left_pad = left_pad > scaled_i(8, s) ? left_pad - 1 : left_pad;
		right_pad = right_pad > scaled_i(4, s) ? right_pad - 1 : right_pad;
		dock_width = dock_count * icon +
			(dock_count > 1 ? (dock_count - 1) * gap : 0) +
			separator_count * separator_extra + left_pad + right_pad;
	}
	int dock_height = icon + top_pad + bottom_pad;
	layout->dock_item_count = dock_count;
	layout->dock = (struct orange_rect){
		(width - dock_width) / 2,
		height - dock_height - bottom_margin,
		dock_width,
		dock_height,
	};
	int x = layout->dock.x + left_pad;
	int y = layout->dock.y + top_pad;
	for (int i = 0; i < dock_count; i++) {
		int launcher_idx = visible_launchers[i];
		layout->dock_launcher_indices[i] = launcher_idx;
		if (i > 0 && dock_launcher_is_trash(config, launcher_idx)) {
			x += separator_extra;
		}
		layout->dock_items[i] = (struct orange_rect){x, y, icon, icon};
		x += icon + gap;
	}
	clamp_desktop_items_to_visible_area(layout, s);

	if (system_menu_open) {
		layout->system_menu_item_count =
			(int)(sizeof(menu_labels) / sizeof(menu_labels[0]));
		layout->system_menu_panel = (struct orange_rect){
			scaled_i(18, s), layout->menu_bar.height + scaled_i(4, s),
			scaled_i(290, s),
			scaled_i(18, s) + layout->system_menu_item_count * scaled_i(42, s)};
		memset(layout->system_menu_separator, 0,
			sizeof(layout->system_menu_separator));
		for (int si = 0; menu_separator_before[si] >= 0; si++) {
			int idx = menu_separator_before[si];
			if (idx < layout->system_menu_item_count) {
				layout->system_menu_separator[idx] = true;
			}
		}
		for (int i = 0; i < layout->system_menu_item_count; i++) {
			layout->system_menu_items[i] = (struct orange_rect){
				layout->system_menu_panel.x + scaled_i(8, s),
				layout->system_menu_panel.y + scaled_i(9, s) + i * scaled_i(42, s),
				layout->system_menu_panel.width - scaled_i(16, s),
				scaled_i(40, s),
			};
		}
	}
}

void orange_shell_layout_set_context_menu(
		struct orange_shell_layout *layout,
		enum orange_context_menu_kind kind,
		int index,
		int cursor_x,
		int cursor_y) {
	layout->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	layout->context_menu_index = -1;
	layout->context_menu_item_count = 0;
	if (kind == ORANGE_CONTEXT_MENU_NONE) {
		return;
	}

	double s = layout_scale(layout);
	int item_count = 0;
	struct orange_rect anchor = {0};
	const int *separator_before = NULL;

	if (kind == ORANGE_CONTEXT_MENU_DOCK &&
			index >= 0 && index < layout->dock_item_count) {
		anchor = layout->dock_items[index];
		item_count = (int)(sizeof(dock_context_labels) /
			sizeof(dock_context_labels[0]));
		separator_before = dock_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET &&
			index >= 0 && index < layout->widget_count &&
			layout->widgets[index].visible) {
		anchor = layout->widgets[index].rect;
		item_count = (int)(sizeof(widget_context_labels) /
			sizeof(widget_context_labels[0]));
		separator_before = widget_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_ICON &&
			index >= 0 && index < layout->desktop_item_count) {
		anchor = layout->desktop_items[index];
		item_count = (int)(sizeof(desktop_icon_context_labels) /
			sizeof(desktop_icon_context_labels[0]));
		separator_before = desktop_icon_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		item_count = (int)(sizeof(desktop_context_labels) /
			sizeof(desktop_context_labels[0]));
		separator_before = desktop_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		anchor = layout->status_area;
		item_count = (int)(sizeof(status_context_labels) /
			sizeof(status_context_labels[0]));
		separator_before = status_context_separator_before;
	} else {
		return;
	}
	if (item_count > ORANGE_CONTEXT_MENU_ITEM_MAX) {
		item_count = ORANGE_CONTEXT_MENU_ITEM_MAX;
	}

	int item_h = scaled_i(40, s);
	int panel_width_base = 250;
	if (kind == ORANGE_CONTEXT_MENU_DOCK) {
		panel_width_base = 214;
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		panel_width_base = 232;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		panel_width_base = 366;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		panel_width_base = 370;
	}
	int panel_w = scaled_i(panel_width_base, s);
	int panel_h = scaled_i(12, s) + item_count * item_h;
	int x, y;
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP ||
			kind == ORANGE_CONTEXT_MENU_DESKTOP_ICON) {
		x = cursor_x;
		y = cursor_y;
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK) {
		x = anchor.x + anchor.width / 2 - panel_w / 2;
		y = layout->dock.y - panel_h - scaled_i(10, s);
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		x = cursor_x - panel_w / 2;
		y = cursor_y - scaled_i(8, s);
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		x = anchor.x + anchor.width - panel_w;
		y = layout->menu_bar.height + scaled_i(6, s);
	} else {
		x = anchor.x + anchor.width / 2 - panel_w / 2;
		y = anchor.y + scaled_i(20, s);
	}

	int margin = scaled_i(8, s);
	if (x < margin) {
		x = margin;
	}
	if (x + panel_w > layout->width - margin) {
		x = layout->width - margin - panel_w;
	}
	if (x < margin) {
		x = margin;
	}
	int min_y = layout->menu_bar.height + margin;
	if (y < min_y) {
		y = min_y;
	}
	if (y + panel_h > layout->height - margin) {
		y = layout->height - margin - panel_h;
	}
	if (y < min_y) {
		y = min_y;
	}

	layout->context_menu_kind = kind;
	layout->context_menu_index = index;
	layout->context_menu_panel = (struct orange_rect){x, y, panel_w, panel_h};
	layout->context_menu_item_count = item_count;
	memset(layout->context_menu_separator, 0,
		sizeof(layout->context_menu_separator));
	if (separator_before != NULL) {
		for (int si = 0; separator_before[si] >= 0; si++) {
			int idx = separator_before[si];
			if (idx < item_count) {
				layout->context_menu_separator[idx] = true;
			}
		}
	}
	for (int i = 0; i < item_count; i++) {
		layout->context_menu_items[i] = (struct orange_rect){
			x + scaled_i(7, s),
			y + scaled_i(6, s) + i * item_h,
			panel_w - scaled_i(14, s),
			item_h,
		};
	}
}

struct orange_shell_hit orange_shell_hit_test(
		const struct orange_shell_layout *layout,
		int x,
		int y) {
	if (layout->context_menu_item_count > 0 &&
			rect_contains(&layout->context_menu_panel, x, y)) {
		for (int i = 0; i < layout->context_menu_item_count; i++) {
			if (rect_contains(&layout->context_menu_items[i], x, y)) {
				return (struct orange_shell_hit){ORANGE_HIT_CONTEXT_MENU_ITEM, i};
			}
		}
		return (struct orange_shell_hit){ORANGE_HIT_NONE, -1};
	}
	if (layout->system_menu_item_count > 0 &&
			rect_contains(&layout->system_menu_panel, x, y)) {
		for (int i = 0; i < layout->system_menu_item_count; i++) {
			if (rect_contains(&layout->system_menu_items[i], x, y)) {
				return (struct orange_shell_hit){ORANGE_HIT_SYSTEM_MENU_ITEM, i};
			}
		}
		return (struct orange_shell_hit){ORANGE_HIT_SYSTEM_MENU, -1};
	}
	if (rect_contains(&layout->system_menu_button, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_SYSTEM_MENU, -1};
	}
	if (rect_contains(&layout->app_menu_button, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_APP_MENU, -1};
	}
	if (rect_contains(&layout->status_area, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_STATUS_AREA, -1};
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		if (rect_contains(&layout->dock_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_DOCK_ITEM, i};
		}
	}
	for (int i = 0; i < layout->widget_count; i++) {
		if (layout->widgets[i].visible &&
				rect_contains(&layout->widgets[i].rect, x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_WIDGET, i};
		}
	}
	for (int i = 0; i < layout->desktop_item_count; i++) {
		if (rect_contains(&layout->desktop_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_DESKTOP_ITEM, i};
		}
	}
	if (y > layout->menu_bar.height) {
		return (struct orange_shell_hit){ORANGE_HIT_DESKTOP, -1};
	}
	return (struct orange_shell_hit){ORANGE_HIT_NONE, -1};
}

static void draw_widget_layer(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	for (int i = 0; i < layout->widget_count; i++) {
		if (!layout->widgets[i].visible) {
			continue;
		}
		switch (layout->widgets[i].type) {
		case ORANGE_WIDGET_CALENDAR:
			draw_calendar_widget(cr, layout, state, config);
			break;
		case ORANGE_WIDGET_WEATHER:
			draw_weather_widget(cr, layout, state, config);
			break;
		}
	}
}

void orange_shell_draw(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct orange_shell_state *state) {
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = true,
	};
	orange_shell_draw_with_options(pixels, width, height, stride, state, &options);
}

void orange_shell_draw_with_options(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct orange_shell_state *state,
		const struct orange_shell_draw_options *options) {
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_t *cr = cairo_create(surface);

	struct orange_config fallback_config;
	const struct orange_config *config = state_config(state, &fallback_config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, state->system_menu_open, config,
		state->desktop_entry_count, &layout);
	orange_shell_layout_set_context_menu(&layout,
		state->context_menu_kind,
		state->context_menu_index,
		state->context_menu_cursor_x,
		state->context_menu_cursor_y);

	if (options == NULL || options->draw_wallpaper) {
		draw_wallpaper(cr, width, height, state->assets, config);
	}
	draw_menu_bar(cr, &layout, state, config);
	draw_widget_layer(cr, &layout, state, config);
	draw_desktop_items(cr, &layout, state, config);
	draw_dock(cr, &layout, state, config);
	if (state->system_menu_open) {
		draw_system_menu(cr, &layout, state, config);
	}
	draw_context_menu(cr, &layout, state, config);

	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

int orange_shell_dock_count(const struct orange_config *config) {
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

int orange_shell_dock_launcher_index(
		const struct orange_shell_layout *layout,
		int visible_index) {
	if (layout == NULL || visible_index < 0 ||
			visible_index >= layout->dock_item_count ||
			visible_index >= ORANGE_DOCK_MAX) {
		return -1;
	}
	return layout->dock_launcher_indices[visible_index];
}

const char *orange_shell_dock_label(int index,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	if (config == NULL || index < 0 || index >= orange_shell_dock_count(config)) {
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
	const char *fallback = fallback_label(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return app_id;
}

const char *orange_shell_dock_command(int index,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	if (config == NULL || index < 0 || index >= orange_shell_dock_count(config)) {
		return NULL;
	}
	const char *app_id = config->dock_apps[index];
	const char *bc = builtin_command(app_id);
	if (bc != NULL) {
		return bc;
	}
	const struct orange_desktop_entry *entry = lookup_entry(app_id, entries, entry_count);
	if (entry != NULL) {
		return entry->exec;
	}
	const char *fallback = fallback_command(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return NULL;
}

const char *orange_shell_menu_label(int index) {
	if (index < 0 || index >= (int)(sizeof(menu_labels) / sizeof(menu_labels[0]))) {
		return NULL;
	}
	return menu_labels[index];
}

const char *orange_shell_context_menu_label(
		enum orange_context_menu_kind kind,
		int index) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_labels) /
					sizeof(dock_context_labels[0]))) {
			return dock_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_WIDGET:
		if (index >= 0 &&
				index < (int)(sizeof(widget_context_labels) /
					sizeof(widget_context_labels[0]))) {
			return widget_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_context_labels) /
					sizeof(desktop_context_labels[0]))) {
			return desktop_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_ICON:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_icon_context_labels) /
					sizeof(desktop_icon_context_labels[0]))) {
			return desktop_icon_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS:
		if (index >= 0 &&
				index < (int)(sizeof(status_context_labels) /
					sizeof(status_context_labels[0]))) {
			return status_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}

const char *orange_shell_context_menu_icon_name(
		enum orange_context_menu_kind kind,
		int index) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_icon_names) /
					sizeof(dock_context_icon_names[0]))) {
			return dock_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_WIDGET:
		if (index >= 0 &&
				index < (int)(sizeof(widget_context_icon_names) /
					sizeof(widget_context_icon_names[0]))) {
			return widget_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_context_icon_names) /
					sizeof(desktop_context_icon_names[0]))) {
			return desktop_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_ICON:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_icon_context_icon_names) /
					sizeof(desktop_icon_context_icon_names[0]))) {
			return desktop_icon_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS:
		if (index >= 0 &&
				index < (int)(sizeof(status_context_icon_names) /
					sizeof(status_context_icon_names[0]))) {
			return status_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}
