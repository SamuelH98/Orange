#include "orange/shell.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/glass.h"
#include "orange/util.h"
#define ORANGE_SETTINGS_COMMAND "if [ -x build/orange-settings ]; then GSK_RENDERER=cairo build/orange-settings orange.conf; elif command -v systemsettings >/dev/null 2>&1; then systemsettings; elif command -v xfce4-settings-manager >/dev/null 2>&1; then xfce4-settings-manager; fi; true"

static bool app_id_matches(const char *app_id, const char *needle) {
	return app_id != NULL && needle != NULL &&
		(orange_desktop_entry_id_matches(needle, app_id) ||
		 orange_desktop_entry_id_matches(app_id, needle) ||
		 strstr(app_id, needle) != NULL);
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
		return "video-player";
	}
	if (app_id_matches(app_id, "org.gnome.Decibels") ||
			app_id_matches(app_id, "decibels")) {
		return "audio-x-generic";
	}
	return NULL;
}

/* Data tables for system menu and context menus.
 * Also used by layout_set_context_menu (via sizeof). */

static const char *menu_labels[] = {
	"About Orange",
	"System Settings",
	"App Store",
	"Recent Items",
	"Force Quit",
	"Sleep",
	"Restart",
	"Shut Down",
	"Lock Screen",
	"Log Out",
};

static const int menu_separator_before[] = {3, 4, 5, 8, -1};

static const char *app_context_labels[] = {
	"About App",
	"Settings...",
	"Services",
	"Hide App",
	"Hide Others",
	"Show All",
	"Quit App",
};

static const int app_context_separator_before[] = {1, 2, 3, 6, -1};

static const char *file_context_labels[] = {
	"New",
	"Open...",
	"Open Recent",
	"Close Window",
	"Save",
	"Save As...",
	"Print...",
};

static const int file_context_separator_before[] = {2, 3, 4, 6, -1};

static const char *edit_context_labels[] = {
	"Undo",
	"Redo",
	"Cut",
	"Copy",
	"Paste",
	"Select All",
	"Find...",
};

static const int edit_context_separator_before[] = {2, 5, 6, -1};

static const char *view_context_labels[] = {
	"Zoom In",
	"Zoom Out",
	"Actual Size",
	"Fit to Window",
	"Enter Full Screen",
};

static const int view_context_separator_before[] = {4, -1};

static const char *go_context_labels[] = {
	"Home",
	"Desktop",
	"Documents",
	"Downloads",
	"Applications",
};

static const int go_context_separator_before[] = {-1};

static const char *window_context_labels[] = {
	"Minimize",
	"Zoom",
	"Cycle Through Windows",
	"Bring All to Front",
};

static const int window_context_separator_before[] = {2, -1};

static const char *history_context_labels[] = {
	"Back",
	"Forward",
	"Home",
	"Show All History",
	"Clear Recent History...",
};

static const char *bookmarks_context_labels[] = {
	"Bookmark Current Tab",
	"Search Bookmarks",
	"Bookmarks Sidebar",
	"Manage Bookmarks",
};

static const char *tools_context_labels[] = {
	"Downloads",
	"Add-ons and Themes",
	"Browser Tools",
	"Settings",
};

static const char *help_context_labels[] = {
	"Search",
	"App Help",
};

static const int help_context_separator_before[] = {1, -1};

static const char *dock_context_labels[] = {
	"Open",
	"Show Recents",
	"Show in Files",
	"Open at Login",
	"Remove from Dock",
};

static const int dock_context_separator_before[] = {1, 2, -1};

static const char *dock_running_context_labels[] = {
	"Show All Windows",
	"Hide",
	"Keep in Dock",
	"Show in Files",
	"Open at Login",
	"Quit",
};

static const int dock_running_context_separator_before[] = {2, 5, -1};

static const char *dock_separator_context_labels[] = {
	"Turn Magnification On/Off",
	"Adjust Dock Size...",
	"Position on Screen",
	"Minimize Using",
	"Dock Settings...",
};

static const int dock_separator_context_separator_before[] = {1, 4, -1};

static const char *widget_context_labels[] = {
	"Edit Widget",
	"Small",
	"Medium",
	"Large",
	"Remove Widget",
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

static const int desktop_context_separator_before[] = {1, 2, 6, 7, -1};

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

static const int status_context_separator_before[] = {4, 7, 9, -1};

static const char *status_wifi_context_labels[] = {
	"Wi-Fi",
	"Other Networks...",
	"Network Settings...",
};

static const int status_wifi_context_separator_before[] = {2, -1};

static const char *status_sound_context_labels[] = {
	"Sound",
	"Output Settings...",
	"Sound Settings...",
};

static const int status_sound_context_separator_before[] = {2, -1};

static const char *status_battery_context_labels[] = {
	"Battery",
	"Power Settings...",
	"Battery Health...",
};

static const int status_battery_context_separator_before[] = {1, -1};

static bool rect_contains(const struct orange_rect *rect, int x, int y) {
	return x >= rect->x && y >= rect->y &&
		x < rect->x + rect->width && y < rect->y + rect->height;
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
	return clamp(ui_scale_for_size(layout->width, layout->height),
		ORANGE_MIN_UI_SCALE, ORANGE_MAX_UI_SCALE);
}

static int app_menu_tab_for_context_kind(enum orange_context_menu_kind kind) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_APP:
		return ORANGE_APP_MENU_TAB_APP;
	case ORANGE_CONTEXT_MENU_APP_FILE:
		return ORANGE_APP_MENU_TAB_FILE;
	case ORANGE_CONTEXT_MENU_APP_EDIT:
		return ORANGE_APP_MENU_TAB_EDIT;
	case ORANGE_CONTEXT_MENU_APP_VIEW:
		return ORANGE_APP_MENU_TAB_VIEW;
	case ORANGE_CONTEXT_MENU_APP_GO:
		return ORANGE_APP_MENU_TAB_GO;
	case ORANGE_CONTEXT_MENU_APP_WINDOW:
		return ORANGE_APP_MENU_TAB_WINDOW;
	case ORANGE_CONTEXT_MENU_APP_HISTORY:
		return ORANGE_APP_MENU_TAB_GO;
	case ORANGE_CONTEXT_MENU_APP_BOOKMARKS:
		return ORANGE_APP_MENU_TAB_WINDOW;
	case ORANGE_CONTEXT_MENU_APP_TOOLS:
		return ORANGE_APP_MENU_TAB_TOOLS;
	case ORANGE_CONTEXT_MENU_APP_HELP:
		return ORANGE_APP_MENU_TAB_HELP;
	default:
		return -1;
	}
}

static int app_menu_tab_text_width(
		const char *label,
		bool bold,
		double s) {
	if (label == NULL || label[0] == '\0') {
		return 0;
	}
	cairo_surface_t *tmp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(tmp);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 28.0 * s);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, label, &extents);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	int width = (int)ceil(extents.x_advance) +
		scaled_i(32, s);
	int min_w = scaled_i(bold ? 60 : 42, s);
	if (width < min_w) {
		width = min_w;
	}
	int max_w = scaled_i(bold ? 280 : 174, s);
	if (width > max_w) {
		width = max_w;
	}
	return width;
}

static bool label_matches_token(const char *label, const char *token) {
	return label != NULL && token != NULL && strcasestr(label, token) != NULL;
}

static bool app_menu_should_use_firefox_profile(const char *active_app_label) {
	return label_matches_token(active_app_label, "firefox") ||
		label_matches_token(active_app_label, "browser");
}

static const char *fallback_app_menu_tab_label(
		int tab,
		const char *active_app_label) {
	if (tab == ORANGE_APP_MENU_TAB_APP) {
		return active_app_label != NULL && active_app_label[0] != '\0' ?
			active_app_label : "Files";
	}
	if (app_menu_should_use_firefox_profile(active_app_label)) {
		static const char *firefox_labels[ORANGE_APP_MENU_TAB_COUNT] = {
			"", "File", "Edit", "View", "History", "Bookmarks", "Tools", "Help",
		};
		return tab >= 0 && tab < ORANGE_APP_MENU_TAB_COUNT ?
			firefox_labels[tab] : "";
	}
	static const char *generic_labels[ORANGE_APP_MENU_TAB_COUNT] = {
		"", "File", "Edit", "View", "Go", "Window", "", "Help",
	};
	return tab >= 0 && tab < ORANGE_APP_MENU_TAB_COUNT ?
		generic_labels[tab] : "";
}

static const char *app_menu_tab_label(
		const struct orange_app_menu_model *app_menu,
		int tab,
		const char *active_app_label) {
	if (app_menu != NULL && app_menu->available) {
		if (tab >= 0 && tab < app_menu->tab_count &&
				app_menu->tab_labels[tab][0] != '\0') {
			return app_menu->tab_labels[tab];
		}
		return "";
	}
	return fallback_app_menu_tab_label(tab, active_app_label);
}

static int app_menu_item_count_for_kind(
		enum orange_context_menu_kind kind,
		const struct orange_app_menu_model *app_menu) {
	int tab = app_menu_tab_for_context_kind(kind);
	if (app_menu != NULL && app_menu->available) {
		if (tab >= 0 && tab < app_menu->tab_count) {
			return app_menu->item_counts[tab];
		}
		return 0;
	}
	return -1;
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

static void draw_liquid_panel(cairo_t *cr, const struct orange_rect *rect,
		double radius, double alpha, bool dark) {
	orange_glass_draw(cr, rect, radius, dark);

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
	static const int month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	int days_in_month = month_days[local.tm_mon];
	if (days_in_month == 28) {
		int year = local.tm_year + 1900;
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
			days_in_month = 29;
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

static cairo_surface_t *desktop_icon_surface_for_entry(
		struct orange_assets *assets,
		int variant,
		const struct orange_desktop_entry *entry) {
	if (assets == NULL || entry == NULL) {
		return NULL;
	}
	const char *icon = entry->icon;
	if (icon == NULL || icon[0] == '\0') {
		icon = fallback_icon(entry->id);
	}
	if (icon != NULL && icon[0] != '\0') {
		cairo_surface_t *surface = orange_assets_icon(assets, variant, icon);
		if (surface != NULL) {
			return surface;
		}
	}
	cairo_surface_t *surface = orange_assets_icon(assets, variant,
		"application-x-executable");
	if (surface != NULL) {
		return surface;
	}
	return orange_assets_icon(assets, variant, "folder");
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

static const char *desktop_item_label(
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		int i) {
	if (i < 0 || i >= layout->desktop_item_count) {
		return NULL;
	}
	enum orange_desktop_item_kind kind = layout->desktop_item_info[i].kind;
	int idx = layout->desktop_item_info[i].index;
	if (kind == ORANGE_DESKTOP_ITEM_ENTRY &&
			state->desktop_entries != NULL &&
			idx < state->desktop_entry_count) {
		return state->desktop_entries[idx].name;
	}
	if (kind == ORANGE_DESKTOP_ITEM_VOLUME &&
			state->volumes != NULL &&
			idx < state->volume_count) {
		return state->volumes[idx].label;
	}
	if (kind == ORANGE_DESKTOP_ITEM_FILE &&
			state->desktop_files != NULL &&
			idx < state->desktop_file_count) {
		return state->desktop_files[idx].name;
	}
	return NULL;
}

static void draw_desktop_icon_for_item(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		int i, bool dark, int variant) {
	struct orange_rect r = layout->desktop_items[i];
	enum orange_desktop_item_kind kind = layout->desktop_item_info[i].kind;
	int idx = layout->desktop_item_info[i].index;

	int icon_size = (int)lrint(r.width * 0.82);
	if (icon_size > r.height - 20) {
		icon_size = r.height - 20;
	}
	struct orange_rect icon_rect = {
		r.x + (r.width - icon_size) / 2,
		r.y,
		icon_size,
		icon_size,
	};

	if (kind == ORANGE_DESKTOP_ITEM_ENTRY) {
		const struct orange_desktop_entry *entry =
			state->desktop_entries != NULL && idx < state->desktop_entry_count ?
			&state->desktop_entries[idx] : NULL;
		if (entry != NULL) {
			cairo_surface_t *icon = desktop_icon_surface_for_entry(
				state->assets, variant, entry);
			if (icon != NULL) {
				draw_image_fit(cr, icon, icon_rect, 1.0);
			} else {
				draw_desktop_placeholder_icon(cr, entry, icon_rect, dark);
			}
		}
	} else if (kind == ORANGE_DESKTOP_ITEM_VOLUME) {
		const struct orange_volume_info *vol = state->volumes;
		if (vol != NULL && idx < state->volume_count) {
			const char *icon_name = vol[idx].icon_name[0] != '\0' ?
				vol[idx].icon_name : "drive-harddisk";
			cairo_surface_t *icon = state->assets != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon != NULL) {
				draw_image_fit(cr, icon, icon_rect, 1.0);
			} else {
				cairo_surface_t *fallback = orange_assets_icon(
					state->assets, variant, "drive-harddisk");
				if (fallback != NULL) {
					draw_image_fit(cr, fallback, icon_rect, 1.0);
				}
			}
		}
	} else if (kind == ORANGE_DESKTOP_ITEM_FILE) {
		const struct orange_file_info *files = state->desktop_files;
		if (files != NULL && idx < state->desktop_file_count) {
			const char *icon_name = files[idx].is_directory ?
				"folder" : files[idx].icon_name;
			cairo_surface_t *icon = state->assets != NULL ?
				orange_assets_icon(state->assets, variant, icon_name) : NULL;
			if (icon != NULL) {
				draw_image_fit(cr, icon, icon_rect, 1.0);
			} else {
				cairo_surface_t *fallback = orange_assets_icon(
					state->assets, variant, "text-x-generic");
				if (fallback != NULL) {
					draw_image_fit(cr, fallback, icon_rect, 1.0);
				}
			}
		}
	}
}

static void draw_desktop_label_for_item(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		int i, const struct orange_config *config) {
	struct orange_rect r = layout->desktop_items[i];
	const char *label = desktop_item_label(layout, state, i);
	if (label == NULL) {
		return;
	}

	double s = layout_scale(layout);
	bool label_right = config != NULL &&
		config->desktop_label_position == ORANGE_DESKTOP_LABEL_RIGHT;

	if (label_right) {
		int icon_size = (int)lrint(r.width * 0.82);
		if (icon_size > r.height - 20) {
			icon_size = r.height - 20;
		}
		struct orange_rect label_rect = {
			r.x + icon_size + scaled_i(8, s),
			r.y,
			r.width - icon_size - scaled_i(8, s),
			r.height,
		};
		draw_centered_label(cr, label, label_rect,
			(config != NULL ? config->desktop_label_size : 13) * 1.65 * s,
			255, 255, 255);
	} else {
		int icon_size = (int)lrint(r.width * 0.82);
		if (icon_size > r.height - 20) {
			icon_size = r.height - 20;
		}
		struct orange_rect label_rect = {
			r.x - scaled_i(6, s),
			r.y + icon_size + scaled_i(4, s),
			r.width + scaled_i(12, s),
			r.height - icon_size - scaled_i(4, s),
		};
		if (label_rect.height > 0) {
			draw_centered_label(cr, label, label_rect,
				(config != NULL ? config->desktop_label_size : 13) * 1.65 * s,
				255, 255, 255);
		}
	}
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
	for (int i = 0; i < layout->desktop_item_count; i++) {
		draw_desktop_icon_for_item(cr, layout, state, i, dark, variant);
		draw_desktop_label_for_item(cr, layout, state, i, config);
	}
}

static void draw_notification_card_background(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		bool dark) {
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.22 : 0.12);
	rounded_rect(cr, rect->x + 1.5, rect->y + 3.0,
		rect->width, rect->height, radius);
	cairo_fill(cr);
	draw_liquid_panel(cr, rect, radius, dark ? 0.62 : 0.84, dark);
}

static void draw_notification_app_icon(cairo_t *cr,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		const char *icon_name,
		const char *fallback_letter,
		struct orange_rect rect,
		int red,
		int green,
		int blue) {
	double radius = rect.width * 0.24;
	rounded_rect(cr, rect.x, rect.y, rect.width, rect.height, radius);
	set_source_rgba255(cr, red, green, blue, 0.95);
	cairo_fill(cr);

	bool dark = is_dark_config(config);
	cairo_surface_t *icon = state != NULL && state->assets != NULL ?
		orange_assets_icon(state->assets,
			dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
			icon_name) : NULL;
	if (icon != NULL) {
		draw_image_fit(cr, icon,
			(struct orange_rect){
				rect.x + rect.width / 5,
				rect.y + rect.height / 5,
				rect.width * 3 / 5,
				rect.height * 3 / 5,
			},
			0.92);
		return;
	}

	cairo_text_extents_t extents;
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, rect.height * 0.48);
	cairo_text_extents(cr, fallback_letter, &extents);
	draw_text(cr, fallback_letter,
		rect.x + (rect.width - extents.width) / 2.0 - extents.x_bearing,
		rect.y + (rect.height - extents.height) / 2.0 - extents.y_bearing,
		rect.height * 0.48, 255, 255, 255, 0.96, true);
}

static void draw_notification_message_card(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 190 : 88;

	draw_notification_card_background(cr, &r, 23 * s, dark);
	draw_notification_app_icon(cr, state, config, "mail-message-new", "M",
		(struct orange_rect){
			r.x + scaled_i(18, s), r.y + scaled_i(18, s),
			scaled_i(44, s), scaled_i(44, s),
		},
		57, 198, 94);
	draw_text(cr, "Messages", r.x + scaled_i(76, s), r.y + scaled_i(33, s),
		17 * s, secondary, secondary, secondary, 0.86, true);
	draw_text(cr, "now", r.x + r.width - scaled_i(54, s),
		r.y + scaled_i(33, s), 15 * s, secondary, secondary, secondary, 0.78, false);
	draw_text(cr, "Foodie Friends", r.x + scaled_i(76, s),
		r.y + scaled_i(57, s), 23 * s, primary, primary, primary, 0.96, true);
	draw_text(cr, "Brunch after soccer Saturday;",
		r.x + scaled_i(76, s), r.y + scaled_i(84, s),
		18 * s, primary, primary, primary, 0.88, false);
	draw_text(cr, "host or restaurant suggested.",
		r.x + scaled_i(76, s), r.y + scaled_i(107, s),
		18 * s, primary, primary, primary, 0.88, false);
}

static void draw_notification_calendar_card(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 190 : 88;

	draw_notification_card_background(cr, &r, 23 * s, dark);
	draw_notification_app_icon(cr, state, config, "x-office-calendar", "C",
		(struct orange_rect){
			r.x + scaled_i(18, s), r.y + scaled_i(18, s),
			scaled_i(44, s), scaled_i(44, s),
		},
		242, 78, 79);
	draw_text(cr, "Calendar", r.x + scaled_i(76, s), r.y + scaled_i(33, s),
		17 * s, secondary, secondary, secondary, 0.86, true);
	draw_text(cr, "4m ago", r.x + r.width - scaled_i(72, s),
		r.y + scaled_i(33, s), 15 * s, secondary, secondary, secondary, 0.78, false);
	draw_text(cr, "Bicycle tune-up", r.x + scaled_i(76, s),
		r.y + scaled_i(58, s), 23 * s, primary, primary, primary, 0.96, true);
	draw_text(cr, "Today at 1:30 PM",
		r.x + scaled_i(76, s), r.y + scaled_i(88, s),
		18 * s, primary, primary, primary, 0.82, false);
}

static void draw_notification_empty_card(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 188 : 88;

	draw_notification_card_background(cr, &r, 23 * s, dark);
	draw_text(cr, "No Notifications",
		r.x + scaled_i(20, s), r.y + scaled_i(48, s),
		23 * s, primary, primary, primary, 0.94, true);
	draw_text(cr, "You're all caught up.",
		r.x + scaled_i(20, s), r.y + scaled_i(80, s),
		18 * s, secondary, secondary, secondary, 0.78, false);
}

static void draw_notification_calendar_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 188 : 88;
	time_t now = state != NULL && state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);

	draw_notification_card_background(cr, &r, 24 * s, dark);
	draw_text(cr, "Calendar", r.x + scaled_i(20, s), r.y + scaled_i(36, s),
		22 * s, primary, primary, primary, 0.96, true);
	draw_text(cr, "Today", r.x + scaled_i(20, s), r.y + scaled_i(63, s),
		17 * s, 238, 70, 76, 0.95, true);
	draw_text(cr, "1:30  Bicycle tune-up", r.x + scaled_i(20, s),
		r.y + scaled_i(96, s), 18 * s, primary, primary, primary, 0.90, false);
	draw_text(cr, "3:30  Girls coding club", r.x + scaled_i(20, s),
		r.y + scaled_i(124, s), 18 * s, primary, primary, primary, 0.86, false);

	int mini_w = scaled_i(156, s);
	int mini_x = r.x + r.width - mini_w - scaled_i(18, s);
	int mini_y = r.y + scaled_i(42, s);
	char month[24];
	strftime(month, sizeof(month), "%b", &local);
	for (char *p = month; *p != '\0'; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p = (char)(*p - 'a' + 'A');
		}
	}
	draw_text(cr, month, mini_x, r.y + scaled_i(35, s),
		14 * s, 238, 70, 76, 0.95, true);
	int cell = scaled_i(20, s);
	for (int day = 1; day <= 30; day++) {
		int col = (day - 1) % 7;
		int row = (day - 1) / 7;
		int cx = mini_x + col * cell + cell / 2;
		int cy = mini_y + row * cell;
		if (day == local.tm_mday) {
			set_source_rgba255(cr, 242, 78, 79, 1.0);
			cairo_arc(cr, cx, cy, scaled_i(8, s), 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		char text[8];
		snprintf(text, sizeof(text), "%d", day);
		draw_text(cr, text, cx - scaled_i(day < 10 ? 3 : 7, s),
			cy + scaled_i(5, s), 10 * s,
			day == local.tm_mday ? 255 : secondary,
			day == local.tm_mday ? 255 : secondary,
			day == local.tm_mday ? 255 : secondary,
			0.90, true);
	}
}

static void draw_notification_screen_time_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 188 : 88;

	draw_notification_card_background(cr, &r, 24 * s, dark);
	draw_text(cr, "Screen Time", r.x + scaled_i(20, s), r.y + scaled_i(34, s),
		19 * s, secondary, secondary, secondary, 0.84, true);
	draw_text(cr, "1h 20m", r.x + scaled_i(20, s), r.y + scaled_i(78, s),
		40 * s, primary, primary, primary, 0.96, false);
	int graph_x = r.x + scaled_i(174, s);
	int graph_y = r.y + scaled_i(34, s);
	int graph_w = r.width - scaled_i(194, s);
	int graph_h = scaled_i(82, s);
	set_source_rgba255(cr, dark ? 255 : 30, dark ? 255 : 34,
		dark ? 255 : 38, dark ? 0.10 : 0.08);
	for (int i = 0; i < 4; i++) {
		cairo_rectangle(cr, graph_x, graph_y + i * graph_h / 3,
			graph_w, 1);
		cairo_fill(cr);
	}
	const double bars[] = {0.22, 0.36, 0.30, 0.62, 0.88, 0.56};
	int bar_w = graph_w / 10;
	for (int i = 0; i < 6; i++) {
		int bh = (int)lrint(graph_h * bars[i]);
		int bx = graph_x + scaled_i(14, s) + i * graph_w / 7;
		set_source_rgba255(cr, i >= 3 ? 31 : 80,
			i >= 3 ? 138 : 172,
			i >= 3 ? 255 : 242, 0.92);
		rounded_rect(cr, bx, graph_y + graph_h - bh, bar_w, bh, 2 * s);
		cairo_fill(cr);
	}
}

static void draw_notification_weather_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int primary = dark ? 255 : 255;

	draw_notification_card_background(cr, &r, 24 * s, true);
	cairo_pattern_t *shade = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.16, 0.45, 0.83, 0.86);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.05, 0.11, 0.32, 0.82);
	rounded_rect(cr, r.x, r.y, r.width, r.height, 24 * s);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	draw_text(cr, "Weather", r.x + scaled_i(20, s), r.y + scaled_i(35, s),
		18 * s, primary, primary, primary, 0.78, true);
	draw_text(cr, "Memphis", r.x + scaled_i(20, s), r.y + scaled_i(66, s),
		25 * s, primary, primary, primary, 0.96, true);
	draw_text(cr, "79°", r.x + scaled_i(20, s), r.y + scaled_i(125, s),
		56 * s, primary, primary, primary, 0.97, false);
	cairo_surface_t *weather_icon = state != NULL && state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT, "weather-clear") : NULL;
	if (weather_icon != NULL) {
		draw_image_fit(cr, weather_icon,
			(struct orange_rect){
				r.x + r.width - scaled_i(82, s),
				r.y + scaled_i(42, s),
				scaled_i(52, s),
				scaled_i(52, s),
			},
			0.95);
	} else {
		draw_weather_crescent(cr,
			r.x + r.width - scaled_i(55, s),
			r.y + scaled_i(70, s),
			scaled_i(19, s));
	}
	draw_text(cr, "Air quality alert", r.x + r.width - scaled_i(156, s),
		r.y + r.height - scaled_i(26, s), 17 * s,
		primary, primary, primary, 0.84, true);
	(void)dark;
}

static void draw_notification_center(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (layout->notification_center_panel.width <= 0) {
		return;
	}

	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct orange_rect panel = layout->notification_center_panel;
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.22 : 0.13);
	rounded_rect(cr, panel.x + scaled_i(4, s), panel.y + scaled_i(7, s),
		panel.width, panel.height, 31 * s);
	cairo_fill(cr);

	int primary = dark ? 246 : 26;

	int notification_index = 0;
	for (int i = 0; i < layout->notification_center_card_count; i++) {
		struct orange_rect card = layout->notification_center_cards[i];
		switch (layout->notification_center_card_kinds[i]) {
		case ORANGE_NOTIFICATION_CENTER_CARD_EMPTY:
			draw_notification_empty_card(cr, layout, config, card);
			break;
		case ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION:
			if (notification_index == 1) {
				draw_notification_calendar_card(cr, layout, state, config, card);
			} else {
				draw_notification_message_card(cr, layout, state, config, card);
			}
			notification_index++;
			break;
		case ORANGE_NOTIFICATION_CENTER_CARD_CALENDAR_WIDGET:
			draw_notification_calendar_widget(cr, layout, state, config, card);
			break;
		case ORANGE_NOTIFICATION_CENTER_CARD_SCREEN_TIME_WIDGET:
			draw_notification_screen_time_widget(cr, layout, config, card);
			break;
		case ORANGE_NOTIFICATION_CENTER_CARD_WEATHER_WIDGET:
		default:
			draw_notification_weather_widget(cr, layout, state, config, card);
			break;
		}
	}

	struct orange_rect edit = layout->notification_center_edit_button;
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.25 : 0.10);
	rounded_rect(cr, edit.x, edit.y + scaled_i(2, s),
		edit.width, edit.height, edit.height / 2.0);
	cairo_fill(cr);
	draw_liquid_panel(cr, &edit, edit.height / 2.0, dark ? 0.54 : 0.78, dark);
	draw_text(cr, "Edit Widgets",
		edit.x + scaled_i(29, s),
		edit.y + scaled_i(27, s),
		17 * s,
		primary, primary, primary, 0.92, true);
}

static void desktop_grid_position(
		int index,
		int cols,
		int rows_per_col,
		int cell_w,
		int cell_h,
		int grid_left,
		int grid_top,
		int *out_x,
		int *out_y) {
	int col = index / rows_per_col;
	int row = index % rows_per_col;
	if (col >= cols) {
		col = cols - 1;
		row = rows_per_col - 1;
	}
	*out_x = grid_left + col * cell_w;
	*out_y = grid_top + row * cell_h;
}

static int desktop_grid_col_from_x(int x, int grid_left, int cell_w) {
	int col = (x - grid_left + cell_w / 2) / cell_w;
	return col < 0 ? 0 : col;
}

static int desktop_grid_row_from_y(int y, int grid_top, int cell_h) {
	int row = (y - grid_top + cell_h / 2) / cell_h;
	return row < 0 ? 0 : row;
}

static struct orange_rect status_item_before(
		int *right,
		int width,
		int height,
		int gap) {
	*right -= width;
	struct orange_rect rect = {
		*right,
		0,
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
	if (layout->dock.width > 0 &&
			layout->dock_position == ORANGE_DOCK_POSITION_LEFT) {
		int dock_right = layout->dock.x + layout->dock.width + margin;
		if (dock_right > min_x) {
			min_x = dock_right;
		}
	}
	if (layout->dock.width > 0 &&
			layout->dock_position == ORANGE_DOCK_POSITION_BOTTOM &&
			layout->dock.y > min_y) {
		max_y_bottom = layout->dock.y - margin;
	}

	for (int i = 0; i < layout->desktop_item_count; i++) {
		struct orange_rect *item = &layout->desktop_items[i];
		int max_x = layout->width - margin - item->width;
		int max_y = max_y_bottom - item->height;
		if (layout->dock.width > 0 &&
				layout->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
			int dock_left = layout->dock.x - margin - item->width;
			if (dock_left < max_x) {
				max_x = dock_left;
			}
		}
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

void orange_shell_layout_compute(
		int width,
		int height,
		bool system_menu_open,
		const struct orange_config *config,
		int desktop_entry_count,
		int desktop_volume_count,
		const struct orange_file_info *desktop_files,
		int desktop_file_count,
		struct orange_shell_layout *layout) {
	(void)desktop_entry_count;
	(void)desktop_files;
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
	int clock_w = scaled_i(320, s);
	int status_right = width - scaled_i(30, s);
	layout->status_items[ORANGE_STATUS_ITEM_CLOCK] = (struct orange_rect){
		status_right - clock_w,
		0,
		clock_w,
		layout->menu_bar.height,
	};
	status_right = layout->status_items[ORANGE_STATUS_ITEM_CLOCK].x -
		scaled_i(10, s);
	int status_gap = scaled_i(20, s);
	layout->status_items[ORANGE_STATUS_ITEM_CONTROL_CENTER] =
		status_item_before(&status_right, scaled_i(40, s),
			layout->menu_bar.height, status_gap);
	layout->status_items[ORANGE_STATUS_ITEM_SOUND] =
		status_item_before(&status_right, scaled_i(40, s),
			layout->menu_bar.height, status_gap);
	layout->status_items[ORANGE_STATUS_ITEM_SEARCH] =
		status_item_before(&status_right, scaled_i(40, s),
			layout->menu_bar.height, status_gap);
	layout->status_items[ORANGE_STATUS_ITEM_BATTERY] =
		status_item_before(&status_right, scaled_i(50, s),
			layout->menu_bar.height, status_gap);
	layout->status_items[ORANGE_STATUS_ITEM_WIFI] =
		status_item_before(&status_right, scaled_i(44, s),
			layout->menu_bar.height, status_gap);
	layout->status_area = (struct orange_rect){
		layout->status_items[ORANGE_STATUS_ITEM_WIFI].x,
		0,
		width - scaled_i(30, s) -
			layout->status_items[ORANGE_STATUS_ITEM_WIFI].x,
		layout->menu_bar.height,
	};
	orange_shell_layout_set_app_menu_tabs(layout, "Files", NULL);

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

	int total_items = desktop_file_count + desktop_volume_count;
	layout->desktop_item_count = config->desktop_icons_visible ? total_items : 0;
	if (layout->desktop_item_count > ORANGE_DESKTOP_MAX) {
		layout->desktop_item_count = ORANGE_DESKTOP_MAX;
	}

	int icon_w = scaled_i(100 * config->desktop_icon_scale, s);
	int icon_h = scaled_i(100 * config->desktop_icon_scale, s);
	int label_h = scaled_i(24 * config->desktop_icon_scale, s);
	if (config->desktop_label_position == ORANGE_DESKTOP_LABEL_BOTTOM) {
		label_h = scaled_i(40 * config->desktop_icon_scale, s);
	}
	int cell_w = icon_w + scaled_i(config->desktop_grid_spacing, s);
	int cell_h = icon_h + label_h + scaled_i(config->desktop_grid_spacing, s);
	int cell_w_total = cell_w;
	int cell_h_total = cell_h;

	int grid_top = layout->menu_bar.height + scaled_i(32, s);
	int grid_bottom = height - scaled_i(120, s);
	int grid_left = width - scaled_i(180, s) -
		ORANGE_DESKTOP_GRID_COLS * cell_w_total;
	if (grid_left < scaled_i(40, s)) {
		grid_left = scaled_i(40, s);
	}
	int grid_avail_h = grid_bottom - grid_top;
	int rows_per_col = grid_avail_h / cell_h_total;
	if (rows_per_col < 1) {
		rows_per_col = 1;
	}
	int cols = (layout->desktop_item_count + rows_per_col - 1) / rows_per_col;
	if (cols > ORANGE_DESKTOP_GRID_COLS) {
		cols = ORANGE_DESKTOP_GRID_COLS;
		int max_items = cols * rows_per_col;
		if (layout->desktop_item_count > max_items) {
			layout->desktop_item_count = max_items;
		}
	}
	int grid_width = cols * cell_w_total;
	int grid_x = width - grid_width - scaled_i(48, s);

	for (int i = 0; i < layout->desktop_item_count; i++) {
		if (i < desktop_file_count) {
			layout->desktop_item_info[i].kind = ORANGE_DESKTOP_ITEM_FILE;
			layout->desktop_item_info[i].index = i;
		} else {
			layout->desktop_item_info[i].kind = ORANGE_DESKTOP_ITEM_VOLUME;
			layout->desktop_item_info[i].index = i - desktop_file_count;
		}
	}
	for (int i = 0; i < layout->desktop_item_count; i++) {
		int px, py;
		desktop_grid_position(i, cols, rows_per_col,
			cell_w_total, cell_h_total,
			grid_x, grid_top, &px, &py);
		if ((config->desktop_sort_by == ORANGE_DESKTOP_SORT_NONE ||
				config->desktop_sort_by == ORANGE_DESKTOP_SORT_SNAP_TO_GRID) &&
				i < ORANGE_DESKTOP_POSITION_MAX &&
				config->desktop_positions[i].valid) {
			int col = desktop_grid_col_from_x(
				config->desktop_positions[i].x, grid_x, cell_w_total);
			int row = desktop_grid_row_from_y(
				config->desktop_positions[i].y, grid_top, cell_h_total);
			if (col >= cols) {
				col = cols - 1;
			}
			if (row >= rows_per_col) {
				row = rows_per_col - 1;
			}
			px = grid_x + col * cell_w_total;
			py = grid_top + row * cell_h_total;
		}
		layout->desktop_items[i] = (struct orange_rect){
			px + (cell_w_total - icon_w) / 2,
			py,
			icon_w,
			icon_h + label_h,
		};
	}

	double dock_s = s * config->dock_scale;
	int dock_icon = scaled_i(106, s * config->dock_icon_scale * config->dock_scale);
	int dock_gap = scaled_i(18, dock_s);
	int dock_sep_extra = scaled_i(50, dock_s);
	int dock_left_pad = scaled_i(22, dock_s);
	int dock_right_pad = scaled_i(22, dock_s);
	int dock_top_pad = scaled_i(16, dock_s);
	int dock_bottom_pad = scaled_i(10, dock_s);
	int dock_bottom_margin = scaled_i(8, dock_s);
	int visible_launchers[ORANGE_DOCK_MAX] = {0};
	int dock_count = normalize_dock_launchers(config, visible_launchers);
	int dock_sep_count = 0;
	for (int i = 1; i < dock_count; i++) {
		if (dock_launcher_is_trash(config, visible_launchers[i])) {
			dock_sep_count++;
		}
	}
	bool dock_vertical =
		config->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		config->dock_position == ORANGE_DOCK_POSITION_RIGHT;
	layout->dock_position = config->dock_position;
	int dock_length = dock_count * dock_icon +
		(dock_count > 1 ? (dock_count - 1) * dock_gap : 0) +
		dock_sep_count * dock_sep_extra;
	int dock_width = dock_vertical ?
		dock_icon + dock_left_pad + dock_right_pad :
		dock_length + dock_left_pad + dock_right_pad;
	int dock_height = dock_vertical ?
		dock_length + dock_top_pad + dock_bottom_pad :
		dock_icon + dock_top_pad + dock_bottom_pad;
	int max_dock_length = dock_vertical ?
		height - layout->menu_bar.height - scaled_i(48, s) :
		width - scaled_i(80, s);
	if (max_dock_length < scaled_i(160, s)) {
		max_dock_length = scaled_i(160, s);
	}
	while ((dock_vertical ? dock_height : dock_width) > max_dock_length &&
			dock_icon > scaled_i(40, s)) {
		dock_icon -= 2;
		dock_gap = dock_gap > 6 ? dock_gap - 1 : dock_gap;
		dock_sep_extra = dock_sep_extra > scaled_i(18, s) ?
			dock_sep_extra - 2 : dock_sep_extra;
		dock_left_pad = dock_left_pad > scaled_i(8, s) ?
			dock_left_pad - 1 : dock_left_pad;
		dock_right_pad = dock_right_pad > scaled_i(4, s) ?
			dock_right_pad - 1 : dock_right_pad;
		dock_length = dock_count * dock_icon +
			(dock_count > 1 ? (dock_count - 1) * dock_gap : 0) +
			dock_sep_count * dock_sep_extra;
		dock_width = dock_vertical ?
			dock_icon + dock_left_pad + dock_right_pad :
			dock_length + dock_left_pad + dock_right_pad;
		dock_height = dock_vertical ?
			dock_length + dock_top_pad + dock_bottom_pad :
			dock_icon + dock_top_pad + dock_bottom_pad;
	}
	layout->dock_item_count = dock_count;
	if (config->dock_position == ORANGE_DOCK_POSITION_LEFT) {
		int dock_y_min = layout->menu_bar.height + scaled_i(12, s);
		int dock_y_space = height - dock_y_min - scaled_i(12, s);
		int dock_y = dock_y_min + (dock_y_space - dock_height) / 2;
		if (dock_y < dock_y_min) {
			dock_y = dock_y_min;
		}
		layout->dock = (struct orange_rect){
			dock_bottom_margin,
			dock_y,
			dock_width,
			dock_height,
		};
	} else if (config->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
		int dock_y_min = layout->menu_bar.height + scaled_i(12, s);
		int dock_y_space = height - dock_y_min - scaled_i(12, s);
		int dock_y = dock_y_min + (dock_y_space - dock_height) / 2;
		if (dock_y < dock_y_min) {
			dock_y = dock_y_min;
		}
		layout->dock = (struct orange_rect){
			width - dock_width - dock_bottom_margin,
			dock_y,
			dock_width,
			dock_height,
		};
	} else {
		layout->dock = (struct orange_rect){
			(width - dock_width) / 2,
			height - dock_height - dock_bottom_margin,
			dock_width,
			dock_height,
		};
	}
	layout->dock_separator = (struct orange_rect){0};
	int dx = dock_vertical ?
		layout->dock.x + (layout->dock.width - dock_icon) / 2 :
		layout->dock.x + dock_left_pad;
	int dy = dock_vertical ?
		layout->dock.y + dock_top_pad :
		layout->dock.y + dock_top_pad - scaled_i(2, dock_s);
	for (int i = 0; i < dock_count; i++) {
		int launcher_idx = visible_launchers[i];
		layout->dock_launcher_indices[i] = launcher_idx;
		if (i > 0 && dock_launcher_is_trash(config, launcher_idx)) {
			if (dock_vertical) {
				int sep_w = layout->dock.width - scaled_i(16, dock_s);
				int sep_h = scaled_i(16, dock_s);
				if (sep_w < scaled_i(28, dock_s)) {
					sep_w = scaled_i(28, dock_s);
				}
				layout->dock_separator = (struct orange_rect){
					layout->dock.x + (layout->dock.width - sep_w) / 2,
					dy + dock_sep_extra / 2 - sep_h / 2,
					sep_w,
					sep_h,
				};
				dy += dock_sep_extra;
			} else {
				int sep_w = scaled_i(16, dock_s);
				int sep_h = layout->dock.height - scaled_i(16, dock_s);
				if (sep_h < scaled_i(28, dock_s)) {
					sep_h = scaled_i(28, dock_s);
				}
				layout->dock_separator = (struct orange_rect){
					dx + dock_sep_extra / 2 - sep_w / 2,
					layout->dock.y + (layout->dock.height - sep_h) / 2,
					sep_w,
					sep_h,
				};
				dx += dock_sep_extra;
			}
		}
		layout->dock_items[i] = (struct orange_rect){dx, dy, dock_icon, dock_icon};
		if (dock_vertical) {
			dy += dock_icon + dock_gap;
		} else {
			dx += dock_icon + dock_gap;
		}
	}
	clamp_desktop_items_to_visible_area(layout, s);

	if (system_menu_open) {
		layout->system_menu_item_count =
			(int)(sizeof(menu_labels) / sizeof(menu_labels[0]));
		layout->system_menu_panel = (struct orange_rect){
			scaled_i(18, s), layout->menu_bar.height + scaled_i(4, s),
			scaled_i(310, s),
			scaled_i(22, s) + layout->system_menu_item_count * scaled_i(54, s)};
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
				layout->system_menu_panel.y + scaled_i(14, s) + i * scaled_i(54, s),
				layout->system_menu_panel.width - scaled_i(16, s),
				scaled_i(48, s),
			};
		}
	}
}

void orange_shell_layout_clean_up(
		struct orange_shell_layout *layout,
		const struct orange_config *config) {
	(void)layout;
	(void)config;
	for (int i = 0; i < ORANGE_DESKTOP_POSITION_MAX; i++) {
	}
}

void orange_shell_layout_sort_by(
		struct orange_shell_layout *layout,
		enum orange_desktop_sort_by sort_by,
		const struct orange_config *config,
		const struct orange_desktop_entry *entries,
		int entry_count,
		const struct orange_volume_info *volumes,
		int volume_count) {
	(void)layout;
	(void)sort_by;
	(void)config;
	(void)entries;
	(void)entry_count;
	(void)volumes;
	(void)volume_count;
	if (sort_by == ORANGE_DESKTOP_SORT_NAME && entries != NULL) {
	}
}

void orange_shell_layout_snap_to_grid(
		struct orange_shell_layout *layout,
		int index,
		int *x,
		int *y,
		const struct orange_config *config) {
	(void)layout;
	(void)index;
	double s = ui_scale_for_size(layout->width, layout->height);
	int icon_w = scaled_i(100 * config->desktop_icon_scale, s);
	int icon_h = scaled_i(100 * config->desktop_icon_scale, s);
	int label_h = scaled_i(40 * config->desktop_icon_scale, s);
	int cell_w = icon_w + scaled_i(config->desktop_grid_spacing, s);
	int cell_h = icon_h + label_h + scaled_i(config->desktop_grid_spacing, s);
	int cell_w_total = cell_w;
	int cell_h_total = cell_h;

	int grid_top = layout->menu_bar.height + scaled_i(32, s);
	int grid_left = layout->width - scaled_i(180, s) -
		ORANGE_DESKTOP_GRID_COLS * cell_w_total;
	if (grid_left < scaled_i(40, s)) {
		grid_left = scaled_i(40, s);
	}

	int col = desktop_grid_col_from_x(*x, grid_left, cell_w_total);
	int row = desktop_grid_row_from_y(*y, grid_top, cell_h_total);

	int cols = ORANGE_DESKTOP_GRID_COLS;
	int grid_avail_h = layout->height - scaled_i(120, s) - grid_top;
	int rows_per_col = grid_avail_h / cell_h_total;
	if (rows_per_col < 1) {
		rows_per_col = 1;
	}
	if (row >= rows_per_col) {
		row = rows_per_col - 1;
	}
	if (col >= cols) {
		col = cols - 1;
	}

	int grid_width = cols * cell_w_total;
	int grid_x = layout->width - grid_width - scaled_i(48, s);

	*x = grid_x + col * cell_w_total + (cell_w_total - icon_w) / 2;
	*y = grid_top + row * cell_h_total;
}

void orange_shell_layout_set_app_menu_tabs(
		struct orange_shell_layout *layout,
		const char *active_app_label,
		const struct orange_app_menu_model *app_menu) {
	if (layout == NULL) {
		return;
	}
	double s = layout_scale(layout);
	int x = scaled_i(95, s);
	int max_right = layout->status_area.x - scaled_i(18, s);
	layout->app_menu_item_count = ORANGE_APP_MENU_TAB_COUNT;
	for (int i = 0; i < ORANGE_APP_MENU_TAB_COUNT; i++) {
		const char *label = app_menu_tab_label(app_menu, i, active_app_label);
		if (label == NULL || label[0] == '\0') {
			layout->app_menu_items[i] = (struct orange_rect){x, 0, 0, 0};
			continue;
		}
		if (x >= max_right) {
			layout->app_menu_items[i] = (struct orange_rect){x, 0, 0, 0};
			continue;
		}
		bool bold = i == ORANGE_APP_MENU_TAB_APP;
		int width = app_menu_tab_text_width(label, bold, s);
		if (x + width > max_right) {
			width = max_right - x;
		}
		if (width < scaled_i(42, s)) {
			layout->app_menu_items[i] = (struct orange_rect){x, 0, 0, 0};
			continue;
		}
		layout->app_menu_items[i] = (struct orange_rect){
			x,
			0,
			width,
			layout->menu_bar.height,
		};
		x += width;
	}
	for (int i = ORANGE_APP_MENU_TAB_COUNT - 1; i >= 0; i--) {
		if (layout->app_menu_items[i].width > 0) {
			int right = layout->app_menu_items[i].x +
				layout->app_menu_items[i].width;
			if (right < max_right) {
				layout->app_menu_items[i].width = max_right -
					layout->app_menu_items[i].x;
			}
			break;
		}
	}
	layout->app_menu_button = layout->app_menu_items[ORANGE_APP_MENU_TAB_APP];
}

void orange_shell_layout_set_context_menu(
		struct orange_shell_layout *layout,
		enum orange_context_menu_kind kind,
		int index,
		int cursor_x,
		int cursor_y,
		const struct orange_app_menu_model *app_menu) {
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
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING &&
			index >= 0 && index < layout->dock_item_count) {
		anchor = layout->dock_items[index];
		item_count = (int)(sizeof(dock_running_context_labels) /
			sizeof(dock_running_context_labels[0]));
		separator_before = dock_running_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR &&
			layout->dock_separator.width > 0) {
		anchor = layout->dock_separator;
		item_count = (int)(sizeof(dock_separator_context_labels) /
			sizeof(dock_separator_context_labels[0]));
		separator_before = dock_separator_context_separator_before;
	} else if (app_menu_tab_for_context_kind(kind) >= 0) {
		int tab = app_menu_tab_for_context_kind(kind);
		if (tab >= layout->app_menu_item_count) {
			return;
		}
		anchor = layout->app_menu_items[tab];
		item_count = app_menu_item_count_for_kind(kind, app_menu);
		if (item_count < 0) {
			switch (kind) {
			case ORANGE_CONTEXT_MENU_APP:
				item_count = (int)(sizeof(app_context_labels) /
					sizeof(app_context_labels[0]));
				separator_before = app_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_FILE:
				item_count = (int)(sizeof(file_context_labels) /
					sizeof(file_context_labels[0]));
				separator_before = file_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_EDIT:
				item_count = (int)(sizeof(edit_context_labels) /
					sizeof(edit_context_labels[0]));
				separator_before = edit_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_VIEW:
				item_count = (int)(sizeof(view_context_labels) /
					sizeof(view_context_labels[0]));
				separator_before = view_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_GO:
				item_count = (int)(sizeof(go_context_labels) /
					sizeof(go_context_labels[0]));
				separator_before = go_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_WINDOW:
				item_count = (int)(sizeof(window_context_labels) /
					sizeof(window_context_labels[0]));
				separator_before = window_context_separator_before;
				break;
			case ORANGE_CONTEXT_MENU_APP_HISTORY:
				item_count = (int)(sizeof(history_context_labels) /
					sizeof(history_context_labels[0]));
				break;
			case ORANGE_CONTEXT_MENU_APP_BOOKMARKS:
				item_count = (int)(sizeof(bookmarks_context_labels) /
					sizeof(bookmarks_context_labels[0]));
				break;
			case ORANGE_CONTEXT_MENU_APP_TOOLS:
				item_count = (int)(sizeof(tools_context_labels) /
					sizeof(tools_context_labels[0]));
				break;
			case ORANGE_CONTEXT_MENU_APP_HELP:
				item_count = (int)(sizeof(help_context_labels) /
					sizeof(help_context_labels[0]));
				separator_before = help_context_separator_before;
				break;
			default:
				return;
			}
		}
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
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_VOLUME &&
			index >= 0 && index < layout->desktop_item_count) {
		anchor = layout->desktop_items[index];
		item_count = 3;
		separator_before = NULL;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE &&
			index >= 0 && index < layout->desktop_item_count) {
		anchor = layout->desktop_items[index];
		item_count = 3;
		separator_before = NULL;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		item_count = (int)(sizeof(desktop_context_labels) /
			sizeof(desktop_context_labels[0]));
		separator_before = desktop_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		anchor = layout->status_area;
		item_count = (int)(sizeof(status_context_labels) /
			sizeof(status_context_labels[0]));
		separator_before = status_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_WIFI) {
		anchor = layout->status_items[ORANGE_STATUS_ITEM_WIFI];
		item_count = (int)(sizeof(status_wifi_context_labels) /
			sizeof(status_wifi_context_labels[0]));
		separator_before = status_wifi_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_SOUND) {
		anchor = layout->status_items[ORANGE_STATUS_ITEM_SOUND];
		item_count = (int)(sizeof(status_sound_context_labels) /
			sizeof(status_sound_context_labels[0]));
		separator_before = status_sound_context_separator_before;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY) {
		anchor = layout->status_items[ORANGE_STATUS_ITEM_BATTERY];
		item_count = (int)(sizeof(status_battery_context_labels) /
			sizeof(status_battery_context_labels[0]));
		separator_before = status_battery_context_separator_before;
	} else {
		return;
	}
	if (item_count > ORANGE_CONTEXT_MENU_ITEM_MAX) {
		item_count = ORANGE_CONTEXT_MENU_ITEM_MAX;
	}
	if (item_count <= 0) {
		return;
	}

	int item_h = scaled_i(48, s);
	int panel_width_base = 270;
	if (kind == ORANGE_CONTEXT_MENU_APP) {
		panel_width_base = 340;
	} else if (kind == ORANGE_CONTEXT_MENU_APP_FILE ||
			kind == ORANGE_CONTEXT_MENU_APP_EDIT ||
			kind == ORANGE_CONTEXT_MENU_APP_VIEW ||
			kind == ORANGE_CONTEXT_MENU_APP_WINDOW ||
			kind == ORANGE_CONTEXT_MENU_APP_HELP) {
		panel_width_base = 320;
	} else if (kind == ORANGE_CONTEXT_MENU_APP_GO) {
		panel_width_base = 280;
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK) {
		panel_width_base = 234;
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING) {
		panel_width_base = 282;
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR) {
		panel_width_base = 340;
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		panel_width_base = 256;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		panel_width_base = 390;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		panel_width_base = 400;
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_WIFI ||
			kind == ORANGE_CONTEXT_MENU_STATUS_SOUND ||
			kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY) {
		panel_width_base = 306;
	}
	int panel_w = scaled_i(panel_width_base, s);
	int panel_h = scaled_i(14, s) + item_count * item_h;
	int x, y;
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP ||
			kind == ORANGE_CONTEXT_MENU_DESKTOP_ICON) {
		x = cursor_x;
		y = cursor_y;
	} else if (app_menu_tab_for_context_kind(kind) >= 0) {
		x = anchor.x;
		y = layout->menu_bar.height + scaled_i(6, s);
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK ||
			kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING ||
			kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR) {
		if (layout->dock_position == ORANGE_DOCK_POSITION_LEFT) {
			x = layout->dock.x + layout->dock.width + scaled_i(10, s);
			y = anchor.y + anchor.height / 2 - panel_h / 2;
		} else if (layout->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
			x = layout->dock.x - panel_w - scaled_i(10, s);
			y = anchor.y + anchor.height / 2 - panel_h / 2;
		} else {
			x = anchor.x + anchor.width / 2 - panel_w / 2;
			y = layout->dock.y - panel_h - scaled_i(10, s);
		}
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		x = cursor_x - panel_w / 2;
		y = cursor_y - scaled_i(8, s);
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		x = anchor.x + anchor.width - panel_w;
		y = layout->menu_bar.height + scaled_i(6, s);
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_WIFI ||
			kind == ORANGE_CONTEXT_MENU_STATUS_SOUND ||
			kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY) {
		x = anchor.x + anchor.width / 2 - panel_w / 2;
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
			x + scaled_i(8, s),
			y + scaled_i(8, s) + i * item_h,
			panel_w - scaled_i(16, s),
			item_h,
		};
	}
}

void orange_shell_layout_set_notification_center(
		struct orange_shell_layout *layout,
		int notification_count) {
	if (layout == NULL) {
		return;
	}

	double s = layout_scale(layout);
	int margin = scaled_i(24, s);
	int panel_w = scaled_i(510, s);
	int max_w = layout->width - margin * 2;
	if (panel_w > max_w) {
		panel_w = max_w;
	}
	if (panel_w <= scaled_i(180, s)) {
		return;
	}

	int panel_x = layout->width - panel_w - margin;
	int panel_y = layout->menu_bar.height + scaled_i(18, s);
	int panel_bottom = layout->height - scaled_i(24, s);
	if (panel_bottom - panel_y <= scaled_i(160, s)) {
		return;
	}

	int pad = scaled_i(18, s);
	int gap = scaled_i(14, s);
	int edit_h = scaled_i(42, s);
	int edit_w = scaled_i(168, s);
	if (edit_w > panel_w - pad * 2) {
		edit_w = panel_w - pad * 2;
	}

	int card_x = panel_x + pad;
	int card_w = panel_w - pad * 2;
	int y = panel_y + pad;
	int limit = panel_bottom - edit_h - gap - pad;
	layout->notification_center_card_count = 0;

	enum orange_notification_center_card_kind kinds[ORANGE_NOTIFICATION_CENTER_CARD_MAX];
	int card_heights[ORANGE_NOTIFICATION_CENTER_CARD_MAX];
	int requested = 0;
	if (notification_count <= 0) {
		kinds[requested] = ORANGE_NOTIFICATION_CENTER_CARD_EMPTY;
		card_heights[requested++] = 126;
	} else {
		int visible_notifications = notification_count;
		if (visible_notifications > ORANGE_NOTIFICATION_CENTER_CARD_MAX - 3) {
			visible_notifications = ORANGE_NOTIFICATION_CENTER_CARD_MAX - 3;
		}
		for (int i = 0; i < visible_notifications; i++) {
			kinds[requested] = ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION;
			card_heights[requested++] = i == 0 ? 126 : 116;
		}
	}
	if (requested < ORANGE_NOTIFICATION_CENTER_CARD_MAX) {
		kinds[requested] = ORANGE_NOTIFICATION_CENTER_CARD_CALENDAR_WIDGET;
		card_heights[requested++] = 196;
	}
	if (requested < ORANGE_NOTIFICATION_CENTER_CARD_MAX) {
		kinds[requested] = ORANGE_NOTIFICATION_CENTER_CARD_SCREEN_TIME_WIDGET;
		card_heights[requested++] = 142;
	}
	if (requested < ORANGE_NOTIFICATION_CENTER_CARD_MAX) {
		kinds[requested] = ORANGE_NOTIFICATION_CENTER_CARD_WEATHER_WIDGET;
		card_heights[requested++] = 154;
	}

	for (int i = 0; i < requested &&
			layout->notification_center_card_count <
				ORANGE_NOTIFICATION_CENTER_CARD_MAX; i++) {
		int card_h = scaled_i(card_heights[i], s);
		if (y + card_h > limit) {
			break;
		}
		int index = layout->notification_center_card_count++;
		layout->notification_center_cards[index] =
			(struct orange_rect){card_x, y, card_w, card_h};
		layout->notification_center_card_kinds[index] = kinds[i];
		y += card_h + gap;
	}
	if (layout->notification_center_card_count > 0) {
		y -= gap;
	}
	int edit_y = y + gap;
	if (edit_y + edit_h > panel_bottom - pad) {
		edit_y = panel_bottom - pad - edit_h;
	}
	layout->notification_center_edit_button = (struct orange_rect){
		panel_x + (panel_w - edit_w) / 2,
		edit_y,
		edit_w,
		edit_h,
	};
	layout->notification_center_panel = (struct orange_rect){
		panel_x,
		panel_y,
		panel_w,
		layout->notification_center_edit_button.y + edit_h + pad - panel_y,
	};
}

struct orange_rect orange_shell_layout_work_area(
		const struct orange_shell_layout *layout) {
	if (layout == NULL || layout->width <= 0 || layout->height <= 0) {
		return (struct orange_rect){0, 0, 0, 0};
	}

	int x = 0;
	int y = layout->menu_bar.y + layout->menu_bar.height;
	int right = layout->width;
	int bottom = layout->height;

	if (layout->dock.width > 0 && layout->dock.height > 0) {
		switch (layout->dock_position) {
		case ORANGE_DOCK_POSITION_LEFT:
			if (layout->dock.x + layout->dock.width > x) {
				x = layout->dock.x + layout->dock.width;
			}
			break;
		case ORANGE_DOCK_POSITION_RIGHT:
			if (layout->dock.x < right) {
				right = layout->dock.x;
			}
			break;
		case ORANGE_DOCK_POSITION_BOTTOM:
		default:
			if (layout->dock.y > y && layout->dock.y < bottom) {
				bottom = layout->dock.y;
			}
			break;
		}
	}

	if (right < x) {
		right = x;
	}
	if (bottom < y) {
		bottom = y;
	}
	return (struct orange_rect){x, y, right - x, bottom - y};
}

static int launcher_filter_tab_width(int index, double scale) {
	static const int widths[] = {42, 74, 104, 64, 60, 50};
	int count = (int)(sizeof(widths) / sizeof(widths[0]));
	int base = index >= 0 && index < count ? widths[index] : 72;
	return (int)fmax((double)base, (double)base * 2.0 * scale);
}

void orange_shell_layout_set_launcher(
		struct orange_shell_layout *layout,
		bool searching,
		enum orange_launcher_mode display_mode,
		int app_count) {
	if (layout == NULL) {
		return;
	}
	layout->launcher_visible = true;
	layout->launcher_searching = searching;
	layout->launcher_display_mode = display_mode;
	int requested_category_count = layout->launcher_category_count;
	int requested_category_active = layout->launcher_category_active;
	layout->launcher_panel = (struct orange_rect){0, 0, 0, 0};
	layout->launcher_viewport = (struct orange_rect){0, 0, 0, 0};
	layout->launcher_scroll_track = (struct orange_rect){0, 0, 0, 0};
	layout->launcher_scroll_thumb = (struct orange_rect){0, 0, 0, 0};
	layout->launcher_grid_cell_count = 0;
	for (int i = 0; i < ORANGE_LAUNCHER_CELL_MAX; i++) {
		layout->launcher_grid_indices[i] = -1;
	}
	layout->launcher_grid_cols = 0;
	layout->launcher_grid_cell_w = 0;
	layout->launcher_grid_cell_h = 0;
	layout->launcher_grid_icon_size = 0;
	layout->launcher_scroll = 0;
	layout->launcher_max_scroll = 0;
	layout->launcher_section_count = 0;
	for (int i = 0; i < ORANGE_LAUNCHER_SECTION_MAX; i++) {
		layout->launcher_section_headers[i] = (struct orange_rect){0, 0, 0, 0};
		layout->launcher_section_starts[i] = 0;
		layout->launcher_section_lengths[i] = 0;
	}

	double s = layout_scale(layout);
	int w = layout->width;
	int h = layout->height;
	for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
		layout->launcher_mode_buttons[i] = (struct orange_rect){0, 0, 0, 0};
	}
	for (int i = 0; i < ORANGE_LAUNCHER_CATEGORY_MAX; i++) {
		layout->launcher_category_tabs[i] = (struct orange_rect){0, 0, 0, 0};
	}

	int search_h = (int)clamp((double)h * 0.048, 38.0, 48.0);
	int option_size = search_h;
	int option_gap = scaled_i(8, s);
	int pill_w = (int)clamp((double)w * 0.30, 320.0, 460.0);
	if (pill_w > w - scaled_i(80, s)) {
		pill_w = w - scaled_i(80, s);
	}
	int compact_button_count = ORANGE_LAUNCHER_MODE_COUNT;
	int compact_row_w = pill_w + option_gap +
		compact_button_count * option_size +
		(compact_button_count - 1) * option_gap;
	int field_x = (w - compact_row_w) / 2;
	int field_y = (int)((double)h * 0.38) - search_h / 2;
	if (layout->launcher_position_set) {
		field_x = layout->launcher_x;
		field_y = layout->launcher_y;
	}
	int margin = scaled_i(24, s);
	int field_max_x = display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY ?
		w - margin - compact_row_w : w - margin - pill_w;
	if (field_max_x < margin) {
		field_max_x = margin;
	}
	if (field_x < margin) {
		field_x = margin;
	}
	if (field_x > field_max_x) {
		field_x = field_max_x;
	}
	if (field_y < layout->menu_bar.height + margin) {
		field_y = layout->menu_bar.height + margin;
	}
	if (field_y + search_h > h - margin) {
		field_y = h - margin - search_h;
	}

	if (display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
		layout->launcher_panel = (struct orange_rect){
			field_x, field_y, pill_w, search_h};
		layout->launcher_search_field = layout->launcher_panel;
		int button_x = field_x + pill_w + option_gap;
		for (int i = 0; i < compact_button_count; i++) {
			layout->launcher_mode_buttons[i] = (struct orange_rect){
				button_x + i * (option_size + option_gap),
				field_y,
				option_size,
				option_size,
			};
		}
		return;
	}

	if (requested_category_count < 0) {
		requested_category_count = 0;
	}
	if (requested_category_count > ORANGE_LAUNCHER_CATEGORY_MAX) {
		requested_category_count = ORANGE_LAUNCHER_CATEGORY_MAX;
	}
	bool query_results = searching;
	bool show_filters =
		!query_results &&
		layout->launcher_current_mode == ORANGE_LAUNCHER_MODE_APPS &&
		requested_category_count > 0;
	if (!show_filters) {
		requested_category_count = 0;
		requested_category_active = 0;
	}
	if (requested_category_active < 0 ||
			requested_category_active >= requested_category_count) {
		requested_category_active = 0;
	}
	layout->launcher_category_count = requested_category_count;
	layout->launcher_category_active = requested_category_active;

	int panel_w = query_results ?
		(int)clamp((double)w * 0.44, 500.0, 680.0) :
		(int)clamp((double)w * 0.48, 560.0, 760.0);
	if (panel_w > w - margin * 2) {
		panel_w = w - margin * 2;
	}
	int pad_x = (int)clamp((double)panel_w * 0.026, 12.0, 20.0);
	int header_h = (int)clamp((double)h * 0.082, 62.0, 82.0);
	int panel_x = (w - panel_w) / 2;
	int panel_h = query_results ?
		(int)clamp((double)h * 0.42, 300.0, 390.0) :
		(int)clamp((double)h * 0.50, 360.0, 480.0);
	int panel_y;
	if (query_results) {
		panel_y = field_y - (header_h - search_h) / 2;
	} else {
		panel_y = (h - panel_h) / 2 - scaled_i(32, s);
	}
	if (layout->launcher_position_set) {
		panel_x = field_x - pad_x;
		panel_y = field_y - (header_h - search_h) / 2;
	}
	if (panel_x < margin) {
		panel_x = margin;
	}
	if (panel_x + panel_w > w - margin) {
		panel_x = w - margin - panel_w;
	}
	if (panel_y < layout->menu_bar.height + margin) {
		panel_y = layout->menu_bar.height + margin;
	}
	if (panel_y + panel_h > h - margin) {
		panel_y = h - margin - panel_h;
	}
	int search_y = panel_y + (header_h - search_h) / 2;
	int option_x = panel_x + panel_w - pad_x - option_size;
	layout->launcher_search_field = (struct orange_rect){
		panel_x + pad_x,
		search_y,
		option_x - (panel_x + pad_x) - option_gap,
		search_h};
	layout->launcher_mode_buttons[0] = (struct orange_rect){
		option_x,
		search_y + (search_h - option_size) / 2,
		option_size,
		option_size};

	int content_x = panel_x + pad_x;
	int content_w = panel_w - pad_x * 2;
	int section_top = panel_y + header_h;
	int filter_h = (int)clamp((double)h * 0.033, 26.0, 34.0);
	int filter_gap = (int)fmax(5.0, 8.0 * s);
	int filters_visible = 0;
	int filter_y = section_top;
	if (show_filters) {
		int tab_x = content_x;
		int max_x = content_x + content_w;
		for (int i = 0; i < requested_category_count; i++) {
			int tab_w = launcher_filter_tab_width(i, s);
			if (i > 0 && tab_x + tab_w > max_x) {
				break;
			}
			layout->launcher_category_tabs[i] = (struct orange_rect){
				tab_x, filter_y, tab_w, filter_h};
			tab_x += tab_w + filter_gap;
			filters_visible++;
		}
		layout->launcher_category_count = filters_visible;
		if (layout->launcher_category_active >= filters_visible) {
			layout->launcher_category_active = 0;
		}
	}
	int content_y = filters_visible > 0 ?
		filter_y + filter_h + (int)fmax(10.0, 18.0 * s) :
		section_top + (int)fmax(8.0, 14.0 * s);
	int content_bottom = panel_y + panel_h - (int)fmax(2.0, 6.0 * s);
	if (content_bottom < content_y) {
		content_bottom = content_y;
	}

	int cols = ORANGE_LAUNCHER_COLS;
	int cell_w = content_w / cols;
	if (cell_w < 1) {
		cell_w = 1;
	}
	int icon_size = query_results ?
		(int)clamp(content_w * 0.105,
			fmax(42.0, 80.0 * s),
			fmax(56.0, 108.0 * s)) :
		(int)clamp(content_w * 0.120,
			fmax(50.0, 92.0 * s),
			fmax(70.0, 118.0 * s));
	if (icon_size > cell_w - scaled_i(14, s)) {
		icon_size = cell_w - scaled_i(14, s);
	}
	if (icon_size < 42) {
		icon_size = 42;
	}
	int icon_top_pad = (int)fmax(4.0, 8.0 * s);
	int icon_label_gap = (int)fmax(8.0, 14.0 * s);
	int label_h = (int)fmax(24.0, 32.0 * s);
	int cell_h = icon_top_pad + icon_size + icon_label_gap + label_h;
	int viewport_h = content_bottom - content_y;
	int visible_rows = viewport_h / cell_h;
	if (visible_rows < 1) {
		visible_rows = 1;
	}
	int max_visible_rows = query_results ? 3 : 4;
	if (visible_rows > max_visible_rows) {
		visible_rows = max_visible_rows;
	}
	int total_rows = app_count > 0 ? (app_count + cols - 1) / cols : 0;
	if (query_results) {
		int rows_needed = total_rows > 0 ? total_rows : 1;
		if (visible_rows > rows_needed) {
			visible_rows = rows_needed;
		}
	}
	int used_viewport_h = visible_rows * cell_h;
	int bottom_pad = (int)fmax(2.0, 6.0 * s);
	panel_h = content_y - panel_y + used_viewport_h + bottom_pad;
	layout->launcher_panel = (struct orange_rect){
		panel_x, panel_y, panel_w, panel_h};
	content_bottom = panel_y + panel_h - bottom_pad;
	layout->launcher_viewport = (struct orange_rect){
		content_x, content_y, content_w, content_bottom - content_y};
	int max_scroll_row = total_rows - visible_rows;
	if (max_scroll_row < 0) {
		max_scroll_row = 0;
	}
	int scroll_row = layout->launcher_scroll_row;
	if (scroll_row < 0) {
		scroll_row = 0;
	}
	if (scroll_row > max_scroll_row) {
		scroll_row = max_scroll_row;
	}
	layout->launcher_scroll_row = scroll_row;
	if (max_scroll_row > 0 && cell_h > 0) {
		int track_w = (int)fmax(10.0, 16.0 * s);
		int track_x = panel_x + panel_w - track_w - (int)fmax(4.0, 8.0 * s);
		int track_y = content_y + scaled_i(2, s);
		int track_h = content_bottom - content_y - scaled_i(4, s);
		if (track_h > track_w * 2) {
			int thumb_h = (int)((double)track_h * (double)visible_rows /
				(double)(visible_rows + max_scroll_row));
			int min_thumb_h = (int)fmax(28.0, 44.0 * s);
			if (thumb_h < min_thumb_h) {
				thumb_h = min_thumb_h;
			}
			if (thumb_h > track_h) {
				thumb_h = track_h;
			}
			int thumb_y = track_y;
			if (track_h > thumb_h) {
				thumb_y += (int)lrint((double)(track_h - thumb_h) *
					(double)scroll_row / (double)max_scroll_row);
			}
			layout->launcher_scroll_track =
				(struct orange_rect){track_x, track_y, track_w, track_h};
			layout->launcher_scroll_thumb =
				(struct orange_rect){track_x, thumb_y, track_w, thumb_h};
		}
	}
	layout->launcher_section_count = 1;
	layout->launcher_section_headers[0] = (struct orange_rect){
		content_x,
		section_top,
		content_w,
		0};
	layout->launcher_section_starts[0] = scroll_row * cols;
	layout->launcher_section_lengths[0] = app_count;

	int start = scroll_row * cols;
	int max_visible = visible_rows * cols;
	int visible = app_count - start;
	if (visible < 0) {
		visible = 0;
	}
	if (visible > max_visible) {
		visible = max_visible;
	}
	if (visible > ORANGE_LAUNCHER_CELL_MAX) {
		visible = ORANGE_LAUNCHER_CELL_MAX;
	}
	int grid_x = content_x;
	for (int i = 0; i < visible; i++) {
		int row = i / cols;
		int col = i % cols;
		layout->launcher_grid_cells[i] = (struct orange_rect){
			grid_x + col * cell_w,
			content_y + row * cell_h,
			cell_w,
			cell_h,
		};
		layout->launcher_grid_indices[i] = start + i;
	}
	layout->launcher_grid_cell_count = visible;
	layout->launcher_grid_cols = cols;
	layout->launcher_grid_cell_w = cell_w;
	layout->launcher_grid_cell_h = cell_h;
	layout->launcher_grid_icon_size = icon_size;
	layout->launcher_scroll = scroll_row * cell_h;
	layout->launcher_max_scroll = max_scroll_row;
}

struct orange_shell_hit orange_shell_hit_test(
		const struct orange_shell_layout *layout,
		int x,
		int y) {
	if (layout->launcher_visible) {
		if (rect_contains(&layout->launcher_search_field, x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_LAUNCHER_SEARCH, -1};
		}
		for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
			if (layout->launcher_mode_buttons[i].width > 0 &&
					rect_contains(&layout->launcher_mode_buttons[i], x, y)) {
				return (struct orange_shell_hit){ORANGE_HIT_LAUNCHER_MODE, i};
			}
		}
		for (int i = 0; i < layout->launcher_category_count; i++) {
			if (rect_contains(&layout->launcher_category_tabs[i], x, y)) {
				return (struct orange_shell_hit){ORANGE_HIT_LAUNCHER_CATEGORY, i};
			}
		}
		if (layout->launcher_scroll_track.width > 0 &&
				rect_contains(&layout->launcher_scroll_track, x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_LAUNCHER_SCROLLBAR, -1};
		}
		for (int i = 0; i < layout->launcher_grid_cell_count; i++) {
			if (rect_contains(&layout->launcher_grid_cells[i], x, y)) {
				return (struct orange_shell_hit){
					ORANGE_HIT_LAUNCHER_APP,
					layout->launcher_grid_indices[i]};
			}
		}
		return (struct orange_shell_hit){ORANGE_HIT_LAUNCHER_BACKGROUND, -1};
	}
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
	if (layout->notification_center_panel.width > 0 &&
			rect_contains(&layout->notification_center_panel, x, y)) {
		if (rect_contains(&layout->notification_center_edit_button, x, y)) {
			return (struct orange_shell_hit){
				ORANGE_HIT_NOTIFICATION_CENTER_EDIT, -1};
		}
		return (struct orange_shell_hit){ORANGE_HIT_NOTIFICATION_CENTER, -1};
	}
	if (rect_contains(&layout->system_menu_button, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_SYSTEM_MENU, -1};
	}
	for (int i = 0; i < layout->app_menu_item_count; i++) {
		if (rect_contains(&layout->app_menu_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_APP_MENU, i};
		}
	}
	for (int i = 0; i < ORANGE_STATUS_ITEM_COUNT; i++) {
		if (rect_contains(&layout->status_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_STATUS_ITEM, i};
		}
	}
	if (rect_contains(&layout->status_area, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_STATUS_AREA, -1};
	}
	if (rect_contains(&layout->menu_bar, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_MENU_BAR, -1};
	}
	if (layout->dock_separator.width > 0 &&
			rect_contains(&layout->dock_separator, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_DOCK_SEPARATOR, -1};
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		if (rect_contains(&layout->dock_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_DOCK_ITEM, i};
		}
	}
	if (rect_contains(&layout->dock, x, y)) {
		return (struct orange_shell_hit){ORANGE_HIT_DOCK, -1};
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
		.skip_transient_overlays = false,
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
		state->desktop_entry_count, state->desktop_volume_count,
		state->desktop_files, state->desktop_file_count, &layout);
	orange_shell_layout_set_app_menu_tabs(&layout,
		orange_menubar_active_app_label(state), &state->app_menu);
	orange_shell_layout_set_context_menu(&layout,
		state->context_menu_kind,
		state->context_menu_index,
		state->context_menu_cursor_x,
		state->context_menu_cursor_y,
		&state->app_menu);
	if (state->notification_center_open) {
		orange_shell_layout_set_notification_center(&layout,
			state->notification_count);
	}
	if (state->launcher_open) {
		bool searching = state->launcher_query[0] != '\0';
		layout.launcher_position_set = state->launcher_position_set;
		layout.launcher_x = state->launcher_x;
		layout.launcher_y = state->launcher_y;
		layout.launcher_scroll_row = state->launcher_scroll_row;
		layout.launcher_current_mode = state->launcher_current_mode;
		layout.launcher_category_count = state->launcher_category_count;
		layout.launcher_category_active = state->launcher_category_active;
		orange_shell_layout_set_launcher(&layout, searching,
			state->launcher_display_mode, state->launcher_app_count);
	}

	if (options == NULL || options->draw_wallpaper) {
		draw_wallpaper(cr, width, height, state->assets, config);
	}
	orange_menubar_draw(cr, &layout, state, config);
	draw_widget_layer(cr, &layout, state, config);
	draw_desktop_items(cr, &layout, state, config);
	orange_dock_draw(cr, &layout, state, config);
	bool draw_transient_overlays =
		options == NULL || !options->skip_transient_overlays;
	if (draw_transient_overlays) {
		if (state->notification_center_open) {
			draw_notification_center(cr, &layout, state, config);
		}
		if (state->system_menu_open) {
			orange_menubar_draw_system_menu(cr, &layout, state, config);
		}
		orange_menubar_draw_context_menu(cr, &layout, state, config);
		if (state->launcher_open) {
			orange_launcher_draw(cr, &layout, state, config);
		}
		if (state->launcher_app_drag_active) {
			orange_launcher_draw_app_drag_overlay(cr, &layout, state, config);
		}
	}

	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

static void draw_overlay_contents(cairo_t *cr,
	int width,
	int height,
	const struct orange_shell_state *state);
static const struct orange_config *compute_overlay_layout(
	struct orange_shell_layout *layout,
	int width,
	int height,
	const struct orange_shell_state *state,
	struct orange_config *fallback_config);

void orange_shell_draw_overlay(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct orange_shell_state *state) {
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	draw_overlay_contents(cr, width, height, state);

	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

static bool rect_has_area(const struct orange_rect *rect) {
	return rect != NULL && rect->width > 0 && rect->height > 0;
}

static void include_overlay_rect(struct orange_rect *bounds,
		bool *has_bounds,
		const struct orange_rect *rect) {
	if (!rect_has_area(rect)) {
		return;
	}
	if (!*has_bounds) {
		*bounds = *rect;
		*has_bounds = true;
		return;
	}
	int x1 = bounds->x < rect->x ? bounds->x : rect->x;
	int y1 = bounds->y < rect->y ? bounds->y : rect->y;
	int x2 = bounds->x + bounds->width;
	int y2 = bounds->y + bounds->height;
	int rx2 = rect->x + rect->width;
	int ry2 = rect->y + rect->height;
	if (rx2 > x2) {
		x2 = rx2;
	}
	if (ry2 > y2) {
		y2 = ry2;
	}
	*bounds = (struct orange_rect){x1, y1, x2 - x1, y2 - y1};
}

static struct orange_rect pad_overlay_bounds(struct orange_rect rect,
		int width,
		int height) {
	const int pad = 64;
	int x1 = rect.x - pad;
	int y1 = rect.y - pad;
	int x2 = rect.x + rect.width + pad;
	int y2 = rect.y + rect.height + pad;
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
	return (struct orange_rect){x1, y1, x2 - x1, y2 - y1};
}

static const struct orange_config *compute_overlay_layout(
		struct orange_shell_layout *layout,
		int width,
		int height,
		const struct orange_shell_state *state,
		struct orange_config *fallback_config) {
	const struct orange_config *config = state_config(state, fallback_config);
	orange_shell_layout_compute(width, height, state->system_menu_open, config,
		state->desktop_entry_count, state->desktop_volume_count,
		NULL, 0, layout);
	orange_shell_layout_set_app_menu_tabs(layout,
		orange_menubar_active_app_label(state), &state->app_menu);
	orange_shell_layout_set_context_menu(layout,
		state->context_menu_kind,
		state->context_menu_index,
		state->context_menu_cursor_x,
		state->context_menu_cursor_y,
		&state->app_menu);
	if (state->notification_center_open) {
		orange_shell_layout_set_notification_center(layout,
			state->notification_count);
	}
	if (state->launcher_open) {
		bool searching = state->launcher_query[0] != '\0';
		layout->launcher_position_set = state->launcher_position_set;
		layout->launcher_x = state->launcher_x;
		layout->launcher_y = state->launcher_y;
		layout->launcher_scroll_row = state->launcher_scroll_row;
		layout->launcher_current_mode = state->launcher_current_mode;
		layout->launcher_category_count = state->launcher_category_count;
		layout->launcher_category_active = state->launcher_category_active;
		orange_shell_layout_set_launcher(layout, searching,
			state->launcher_display_mode, state->launcher_app_count);
	}
	return config;
}

bool orange_shell_overlay_bounds(
		int width,
		int height,
		const struct orange_shell_state *state,
		struct orange_rect *out_bounds) {
	if (out_bounds == NULL) {
		return false;
	}
	*out_bounds = (struct orange_rect){0, 0, 0, 0};
	if (state == NULL || width <= 0 || height <= 0) {
		return false;
	}

	struct orange_config fallback_config;
	struct orange_shell_layout layout;
	compute_overlay_layout(&layout, width, height, state, &fallback_config);

	struct orange_rect bounds = {0, 0, 0, 0};
	bool has_bounds = false;
	if (state->notification_center_open) {
		include_overlay_rect(&bounds, &has_bounds,
			&layout.notification_center_panel);
	}
	if (state->system_menu_open) {
		include_overlay_rect(&bounds, &has_bounds,
			&layout.system_menu_panel);
	}
	if (layout.context_menu_kind != ORANGE_CONTEXT_MENU_NONE) {
		include_overlay_rect(&bounds, &has_bounds,
			&layout.context_menu_panel);
	}
	if (state->launcher_open) {
		if (layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_FULL) {
			include_overlay_rect(&bounds, &has_bounds,
				&layout.launcher_panel);
		} else {
			include_overlay_rect(&bounds, &has_bounds,
				&layout.launcher_search_field);
			for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
				include_overlay_rect(&bounds, &has_bounds,
					&layout.launcher_mode_buttons[i]);
			}
		}
	}
	if (state->dock_drag_insert_before >= 0) {
		include_overlay_rect(&bounds, &has_bounds, &layout.dock);
	}
	if (state->dock_drag_index >= 0 &&
			state->dock_drag_index < layout.dock_item_count) {
		struct orange_rect item = layout.dock_items[state->dock_drag_index];
		struct orange_rect ghost = {
			state->dock_drag_x - item.width / 2,
			state->dock_drag_y - item.height / 2 - item.height / 2,
			item.width,
			item.height * 2,
		};
		include_overlay_rect(&bounds, &has_bounds, &ghost);
	}
	if (state->launcher_app_drag_active) {
		double s = layout_scale(&layout);
		int icon_size = layout.launcher_grid_icon_size > 0 ?
			layout.launcher_grid_icon_size : scaled_i(92, s);
		if (icon_size < 42) {
			icon_size = 42;
		}
		struct orange_rect ghost = {
			state->launcher_app_drag_x - icon_size / 2,
			state->launcher_app_drag_y - icon_size / 2,
			icon_size,
			icon_size,
		};
		include_overlay_rect(&bounds, &has_bounds, &ghost);
	}
	if (!has_bounds) {
		return false;
	}
	*out_bounds = pad_overlay_bounds(bounds, width, height);
	return rect_has_area(out_bounds);
}

static void draw_overlay_contents(cairo_t *cr,
		int width,
		int height,
		const struct orange_shell_state *state) {
	struct orange_config fallback_config;
	struct orange_shell_layout layout;
	const struct orange_config *config = compute_overlay_layout(&layout,
		width, height, state, &fallback_config);

	if (state->notification_center_open) {
		draw_notification_center(cr, &layout, state, config);
	}
	if (state->system_menu_open) {
		orange_menubar_draw_system_menu(cr, &layout, state, config);
	}
	orange_menubar_draw_context_menu(cr, &layout, state, config);
	if (state->launcher_open) {
		orange_launcher_draw(cr, &layout, state, config);
	}
	if (state->dock_drag_index >= 0 ||
			state->dock_drag_insert_before >= 0) {
		orange_dock_draw_drag_overlay(cr, &layout, state, config);
	}
	if (state->launcher_app_drag_active) {
		orange_launcher_draw_app_drag_overlay(cr, &layout, state, config);
	}
}

static int div_ceil_int(int numerator, int denominator) {
	if (numerator <= 0 || denominator <= 0) {
		return 0;
	}
	return (numerator + denominator - 1) / denominator;
}

static int channel_unblend_alpha(int composed, int backdrop) {
	if (composed < backdrop) {
		return backdrop > 0 ?
			div_ceil_int(255 * (backdrop - composed), backdrop) : 0;
	}
	if (composed > backdrop) {
		return backdrop < 255 ?
			div_ceil_int(255 * (composed - backdrop), 255 - backdrop) : 255;
	}
	return 0;
}

static int unblend_channel(int composed, int backdrop, int alpha) {
	int retained = (backdrop * (255 - alpha) + 127) / 255;
	int channel = composed - retained;
	if (channel < 0) {
		return 0;
	}
	if (channel > alpha) {
		return alpha;
	}
	return channel;
}

static uint32_t overlay_source_from_composed(uint32_t composed,
		uint32_t backdrop) {
	if (composed == backdrop) {
		return 0;
	}

	int composed_a = (int)((composed >> 24) & 0xff);
	int backdrop_a = (int)((backdrop >> 24) & 0xff);
	if (composed_a != 255 || backdrop_a != 255) {
		return composed;
	}

	int cr = (int)((composed >> 16) & 0xff);
	int cg = (int)((composed >> 8) & 0xff);
	int cb = (int)(composed & 0xff);
	int br = (int)((backdrop >> 16) & 0xff);
	int bg = (int)((backdrop >> 8) & 0xff);
	int bb = (int)(backdrop & 0xff);

	int alpha = channel_unblend_alpha(cr, br);
	int ga = channel_unblend_alpha(cg, bg);
	int ba = channel_unblend_alpha(cb, bb);
	if (ga > alpha) {
		alpha = ga;
	}
	if (ba > alpha) {
		alpha = ba;
	}
	if (alpha <= 0) {
		return 0;
	}
	if (alpha > 255) {
		alpha = 255;
	}

	int sr = unblend_channel(cr, br, alpha);
	int sg = unblend_channel(cg, bg, alpha);
	int sb = unblend_channel(cb, bb, alpha);
	return ((uint32_t)alpha << 24) |
		((uint32_t)sr << 16) |
		((uint32_t)sg << 8) |
		(uint32_t)sb;
}

static void unblend_overlay_from_backdrop(uint32_t *pixels,
		int stride,
		const uint32_t *backdrop_pixels,
		int backdrop_stride,
		const struct orange_rect *bounds) {
	if (!rect_has_area(bounds)) {
		return;
	}
	int x_start = bounds->x;
	int x_end = bounds->x + bounds->width;
	int y_end = bounds->y + bounds->height;
	for (int y = bounds->y; y < y_end; y++) {
		uint32_t *row = (uint32_t *)((unsigned char *)pixels +
			(size_t)y * (size_t)stride);
		const uint32_t *backdrop = (const uint32_t *)((const unsigned char *)backdrop_pixels +
			(size_t)y * (size_t)backdrop_stride);
		for (int x = x_start; x < x_end; x++) {
			row[x] = overlay_source_from_composed(row[x], backdrop[x]);
		}
	}
}

void orange_shell_draw_overlay_with_backdrop(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const uint32_t *backdrop_pixels,
		int backdrop_stride,
		const struct orange_shell_state *state) {
	if (pixels == NULL || backdrop_pixels == NULL || width <= 0 ||
			height <= 0 || stride < width * 4 ||
			backdrop_stride < width * 4) {
		orange_shell_draw_overlay(pixels, width, height, stride, state);
		return;
	}

	struct orange_rect bounds;
	if (!orange_shell_overlay_bounds(width, height, state, &bounds)) {
		orange_shell_draw_overlay(pixels, width, height, stride, state);
		return;
	}

	size_t row_bytes = (size_t)bounds.width * sizeof(uint32_t);
	for (int y = bounds.y; y < bounds.y + bounds.height; y++) {
		memcpy((unsigned char *)pixels + (size_t)y * (size_t)stride +
				(size_t)bounds.x * sizeof(uint32_t),
			(const unsigned char *)backdrop_pixels +
				(size_t)y * (size_t)backdrop_stride +
				(size_t)bounds.x * sizeof(uint32_t),
			row_bytes);
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_t *cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
	cairo_clip(cr);
	draw_overlay_contents(cr, width, height, state);
	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);

	unblend_overlay_from_backdrop(pixels, stride,
		backdrop_pixels, backdrop_stride, &bounds);
}

const char *orange_shell_volume_label(const struct orange_volume_info *volumes,
		int volume_count, int index) {
	if (volumes == NULL || index < 0 || index >= volume_count) {
		return NULL;
	}
	return volumes[index].label;
}

const char *orange_shell_volume_icon_name(const struct orange_volume_info *volumes,
		int volume_count, int index) {
	if (volumes == NULL || index < 0 || index >= volume_count) {
		return NULL;
	}
	return volumes[index].icon_name;
}
