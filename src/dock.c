#include "orange/dock.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/shell.h"
#include "orange/util.h"
#define ORANGE_SETTINGS_COMMAND "if [ -x build/orange-settings ]; then GSK_RENDERER=cairo build/orange-settings orange.conf; elif command -v systemsettings >/dev/null 2>&1; then systemsettings; elif command -v xfce4-settings-manager >/dev/null 2>&1; then xfce4-settings-manager; fi; true"

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

const char *orange_dock_builtin_command(const char *app_id) {
	if (strcmp(app_id, "__launcher__") == 0) {
		return "if [ -n \"$ORANGE_APP_PICKER\" ]; then $ORANGE_APP_PICKER; else wofi --show drun || rofi -show drun || true; fi";
	}
	if (strcmp(app_id, "__trash__") == 0) {
		return "gio open trash:// || xdg-open trash:// || true";
	}
	if (strcmp(app_id, "orange-settings") == 0 ||
			strcmp(app_id, "dev.orange.Settings") == 0 ||
			strcmp(app_id, "settings") == 0 ||
			strcmp(app_id, "org.gnome.Settings.desktop") == 0) {
		return ORANGE_SETTINGS_COMMAND;
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
		return ORANGE_SETTINGS_COMMAND;
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
		if (app_id_matches(entries[i].id, app_id)) {
			return &entries[i];
		}
	}
	return NULL;
}

static bool rect_contains(const struct orange_rect *rect, int x, int y) {
	return x >= rect->x && y >= rect->y &&
		x < rect->x + rect->width && y < rect->y + rect->height;
}

static double layout_scale(const struct orange_shell_layout *layout) {
	return clamp(ui_scale_for_size(layout->width, layout->height),
		ORANGE_MIN_UI_SCALE, ORANGE_MAX_UI_SCALE);
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

static struct orange_rect centered_rect_in(
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

static const char *dock_icon_name(int launcher_idx,
		const struct orange_desktop_entry *entries, int entry_count,
		const struct orange_config *config) {
	const char *app_id = config != NULL && launcher_idx >= 0 &&
		launcher_idx < ORANGE_DOCK_MAX ? config->dock_apps[launcher_idx] : NULL;
	if (app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	const char *bi = builtin_icon(app_id);
	if (bi != NULL) {
		return bi;
	}
	const struct orange_desktop_entry *entry = lookup_entry(app_id, entries, entry_count);
	if (entry != NULL && entry->icon[0] != '\0') {
		return entry->icon;
	}
	const char *fallback = dock_fallback_icon(app_id);
	if (fallback != NULL) {
		return fallback;
	}
	return "application-x-executable";
}

static void draw_dock_glass(cairo_t *cr, const struct orange_rect *rect,
		double radius, bool dark) {
	cairo_surface_t *main = cairo_get_target(cr);
	cairo_surface_flush(main);

	double blur_scale = 0.25;
	int sw = (int)(rect->width * blur_scale);
	int sh = (int)(rect->height * blur_scale);
	if (sw < 1) sw = 1;
	if (sh < 1) sh = 1;

	cairo_surface_t *blurred = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, sw, sh);
	cairo_t *blur_cr = cairo_create(blurred);
	cairo_scale(blur_cr, blur_scale, blur_scale);
	cairo_set_source_surface(blur_cr, main, -rect->x, -rect->y);
	cairo_pattern_set_filter(cairo_get_source(blur_cr), CAIRO_FILTER_BILINEAR);
	cairo_paint(blur_cr);
	cairo_destroy(blur_cr);

	cairo_save(cr);
	cairo_translate(cr, rect->x, rect->y);
	cairo_scale(cr, 1.0 / blur_scale, 1.0 / blur_scale);
	cairo_set_source_surface(cr, blurred, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_surface_destroy(blurred);

	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.12, 0.14, 0.18, 0.35);
		cairo_pattern_add_color_stop_rgba(shade, 0.50, 0.10, 0.12, 0.15, 0.30);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.04, 0.05, 0.07, 0.25);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.50, 0.50, 0.50, 0.22);
		cairo_pattern_add_color_stop_rgba(shade, 0.50, 0.45, 0.45, 0.45, 0.18);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.40, 0.40, 0.40, 0.14);
	}
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);
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

	int launcher_idx = orange_dock_launcher_index(layout, hot);
	const char *label = orange_dock_label(launcher_idx,
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
	draw_dock_glass(cr, &r, 44 * s, dark);

	rounded_rect(cr, r.x + 0.5, r.y + 0.5,
		r.width - 1.0, r.height - 1.0, 44 * s);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
		dark ? 0.15 : 0.18);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	struct dock_visual_item visual[ORANGE_DOCK_MAX];
	compute_dock_visual_items(layout, state, config, visual);

	for (int i = 0; i < layout->dock_item_count; i++) {
		int launcher_idx = orange_dock_launcher_index(layout, i);

		if (i == state->dock_drag_index) {
			continue;
		}

		struct orange_rect item = visual[i].rect;
		int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
		bool grid_icon = dock_launcher_is_grid(config, launcher_idx);
		if (state->assets != NULL && launcher_idx >= 0) {
			const char *icon_name = dock_icon_name(launcher_idx,
				state->desktop_entries, state->desktop_entry_count, config);
			cairo_surface_t *icon_surface = icon_name != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon_surface != NULL) {
				if (grid_icon) {
					draw_image_tinted(cr, icon_surface, item, 255, 255, 255, 0.92);
				} else {
					draw_image_cover(cr, icon_surface, item);
				}
			}
		}
		if ((config == NULL || config->dock_show_indicators) &&
				launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				state->dock_open[launcher_idx]) {
			double item_s = (double)item.width / 106.0;
			set_source_rgba255(cr, dark ? 255 : 34, dark ? 255 : 37, dark ? 255 : 42, 0.70);
			cairo_arc(cr, item.x + item.width / 2.0,
				r.y + r.height - scaled_i(10, s),
				4.5 * item_s, 0, 2.0 * M_PI);
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
			const char *icon_name = dock_icon_name(launcher_idx,
				state->desktop_entries, state->desktop_entry_count, config);
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
		cairo_paint_with_alpha(cr, 0.70);
	}
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