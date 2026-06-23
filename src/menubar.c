#include "orange/menubar.h"

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

/* Index of first separator after each group; -1 marks end.
   A separator is drawn before the item at this index. */

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

static const char *app_context_labels[] = {
	"About App",
	"Settings...",
	"Services",
	"Hide App",
	"Hide Others",
	"Show All",
	"Quit App",
};

static const char *app_context_icon_names[] = {
	"help-about",
	"preferences-system",
	"applications-system",
	"view-hidden",
	"view-hidden",
	"view-visible",
	"window-close",
};


static const char *file_context_labels[] = {
	"New",
	"Open...",
	"Open Recent",
	"Close Window",
	"Save",
	"Save As...",
	"Print...",
};

static const char *file_context_icon_names[] = {
	"document-new",
	"document-open",
	"document-open-recent",
	"window-close",
	"document-save",
	"document-save-as",
	"document-print",
};


static const char *edit_context_labels[] = {
	"Undo",
	"Redo",
	"Cut",
	"Copy",
	"Paste",
	"Select All",
	"Find...",
};

static const char *edit_context_icon_names[] = {
	"edit-undo",
	"edit-redo",
	"edit-cut",
	"edit-copy",
	"edit-paste",
	"edit-select-all",
	"edit-find",
};


static const char *view_context_labels[] = {
	"Zoom In",
	"Zoom Out",
	"Actual Size",
	"Fit to Window",
	"Enter Full Screen",
};

static const char *view_context_icon_names[] = {
	"zoom-in",
	"zoom-out",
	"zoom-original",
	"zoom-fit-best",
	"view-fullscreen",
};


static const char *go_context_labels[] = {
	"Home",
	"Desktop",
	"Documents",
	"Downloads",
	"Applications",
};

static const char *go_context_icon_names[] = {
	"user-home",
	"user-desktop",
	"folder-documents",
	"folder-download",
	"applications-system",
};


static const char *window_context_labels[] = {
	"Minimize",
	"Zoom",
	"Cycle Through Windows",
	"Bring All to Front",
};

static const char *window_context_icon_names[] = {
	"window-minimize",
	"window-maximize",
	"view-list-symbolic",
	"go-top",
};


static const char *history_context_labels[] = {
	"Back",
	"Forward",
	"Home",
	"Show All History",
	"Clear Recent History...",
};

static const char *history_context_icon_names[] = {
	"go-previous",
	"go-next",
	"go-home",
	"document-open-recent",
	"edit-clear",
};

static const char *bookmarks_context_labels[] = {
	"Bookmark Current Tab",
	"Search Bookmarks",
	"Bookmarks Sidebar",
	"Manage Bookmarks",
};

static const char *bookmarks_context_icon_names[] = {
	"user-bookmarks",
	"edit-find",
	"view-list",
	"folder-bookmarks",
};

static const char *tools_context_labels[] = {
	"Downloads",
	"Add-ons and Themes",
	"Browser Tools",
	"Settings",
};

static const char *tools_context_icon_names[] = {
	"folder-download",
	"applications-system",
	"applications-utilities",
	"preferences-system",
};

static const char *help_context_labels[] = {
	"Search",
	"App Help",
};

static const char *help_context_icon_names[] = {
	"system-search",
	"help-contents",
};


static const char *dock_context_labels[] = {
	"Open",
	"Show Recents",
	"Show in Files",
	"Open at Login",
	"Remove from Dock",
};

static const char *dock_context_icon_names[] = {
	"document-open",
	"document-open-recent",
	"system-file-manager",
	"system-run",
	"list-remove",
};

static const char *dock_running_context_labels[] = {
	"Show All Windows",
	"Hide",
	"Keep in Dock",
	"Show in Files",
	"Open at Login",
	"Quit",
};

static const char *dock_running_context_icon_names[] = {
	"view-restore",
	"view-hidden",
	"starred",
	"system-file-manager",
	"system-run",
	"application-exit",
};

static const char *dock_separator_context_labels[] = {
	"Turn Magnification On/Off",
	"Adjust Dock Size...",
	"Position on Screen",
	"Minimize Using",
	"Dock Settings...",
};

static const char *dock_separator_context_icon_names[] = {
	"zoom-in",
	"zoom-fit-best",
	"preferences-desktop-display",
	"window-minimize",
	"preferences-system",
};


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


static const char *status_wifi_context_labels[] = {
	"Wi-Fi",
	"Other Networks...",
	"Network Settings...",
};

static const char *status_wifi_context_icon_names[] = {
	"network-wireless",
	"network-workgroup",
	"preferences-system-network",
};


static const char *status_sound_context_labels[] = {
	"Sound",
	"Output Settings...",
	"Sound Settings...",
};

static const char *status_sound_context_icon_names[] = {
	"audio-volume-high",
	"audio-card",
	"preferences-desktop-sound",
};


static const char *status_battery_context_labels[] = {
	"Battery",
	"Power Settings...",
	"Battery Health...",
};

static const char *status_battery_context_icon_names[] = {
	"battery",
	"preferences-system-power",
	"battery-good",
};


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

/* Paint a surface's alpha channel as a solid color (silhouette tint).
 * Useful for forcing monochrome-style icons (e.g. the launcher grid) to a
 * fixed color regardless of the source icon's palette. */
static void draw_tinted_image_cover(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, double opacity, int red, int green, int blue) {
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
	cairo_rectangle(cr, r.x, r.y, r.width, r.height);
	cairo_clip(cr);
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

static void draw_menu_bar_text(cairo_t *cr,
		const char *text,
		struct orange_rect item,
		double s,
		int r,
		int g,
		int b,
		bool bold) {
	if (text == NULL || text[0] == '\0' || item.width <= 0) {
		return;
	}

	double font_size = 28.0 * s;
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	cairo_font_extents_t fe;
	cairo_font_extents(cr, &fe);

	int pad = scaled_i(16, s);
	double x = item.x + pad;
	double baseline = item.y +
		((double)item.height - fe.ascent - fe.descent) * 0.5 + fe.ascent;
	if (bold) {
		cairo_select_font_face(cr, "Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	}

	cairo_save(cr);
	cairo_rectangle(cr, item.x, item.y, item.width, item.height);
	cairo_clip(cr);
	draw_text(cr, text, x, baseline, font_size, r, g, b, 0.95, bold);
	cairo_restore(cr);
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
		.text_r = 255,
		.text_g = 255,
		.text_b = 255,
		.text_alpha = dark ? 0.94 : 0.96,
		.separator_r = 255,
		.separator_g = 255,
		.separator_b = 255,
		.separator_alpha = dark ? 0.14 : 0.22,
		.icon_variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
	};
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

static void draw_menu_bar(cairo_t *cr, const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_scale(layout);
	bool dark = config != NULL &&
		config->appearance == ORANGE_APPEARANCE_DARK;
	orange_glass_draw(cr, &layout->menu_bar, 0, dark);
	int text_r = 255;
	int text_g = 255;
	int text_b = 255;

	cairo_surface_t *orange_menu = state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT, "orange-menu") : NULL;
	if (orange_menu != NULL) {
		draw_tinted_image_cover(cr, orange_menu,
			(struct orange_rect){
				scaled_i(37, s),
				scaled_i(4, s),
				scaled_i(40, s),
				scaled_i(40, s),
			},
			0.98,
			255, 255, 255);
	} else {
		draw_text(cr, "O", scaled_i(48, s), scaled_i(30, s),
			28 * s, 255, 255, 255, 0.98, true);
	}

	const char *app_label = orange_menubar_active_app_label(state);
	for (int i = 0; i < layout->app_menu_item_count; i++) {
		const char *label = app_menu_tab_label(&state->app_menu, i, app_label);
		if (label == NULL || label[0] == '\0') {
			continue;
		}
		struct orange_rect item = layout->app_menu_items[i];
		draw_menu_bar_text(cr, label, item, s, text_r, text_g, text_b,
			i == ORANGE_APP_MENU_TAB_APP);
	}

	time_t now = state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	char clock_text[64];
	char day_month[24];
	char time_text[24];
	strftime(day_month, sizeof(day_month), "%a %d %b", &local);
	strftime(time_text, sizeof(time_text), "%I:%M %p", &local);
	const char *trimmed_time = time_text[0] == '0' ? time_text + 1 : time_text;
	snprintf(clock_text, sizeof(clock_text), "%s  %s",
		day_month, trimmed_time);

	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 28 * s);
	cairo_text_extents_t clock_extents;
	cairo_text_extents(cr, clock_text, &clock_extents);
	struct orange_rect clock_rect =
		layout->status_items[ORANGE_STATUS_ITEM_CLOCK];
	double clock_x = clock_rect.x + clock_rect.width -
		scaled_i(8, s) - clock_extents.x_advance;
	double clock_baseline = clock_rect.y +
		((double)clock_rect.height - clock_extents.height) * 0.5 -
		clock_extents.y_bearing;
	draw_text(cr, clock_text, clock_x, clock_baseline, 28 * s,
		text_r, text_g, text_b, 0.95, true);

	struct orange_assets *assets = state->assets;
	struct orange_rect control_rect = centered_rect_in(
		layout->status_items[ORANGE_STATUS_ITEM_CONTROL_CENTER],
		scaled_i(40, s), scaled_i(40, s));
	struct orange_rect search_rect = centered_rect_in(
		layout->status_items[ORANGE_STATUS_ITEM_SEARCH],
		scaled_i(40, s), scaled_i(40, s));
	struct orange_rect battery_rect = centered_rect_in(
		layout->status_items[ORANGE_STATUS_ITEM_BATTERY],
		scaled_i(40, s), scaled_i(40, s));
	struct orange_rect sound_rect = centered_rect_in(
		layout->status_items[ORANGE_STATUS_ITEM_SOUND],
		scaled_i(40, s), scaled_i(40, s));
	struct orange_rect wifi_rect = centered_rect_in(
		layout->status_items[ORANGE_STATUS_ITEM_WIFI],
		scaled_i(40, s), scaled_i(40, s));

	int variant = ORANGE_ASSET_ICON_LIGHT;
	const char *wifi_name = state->status.network_icon[0] != '\0' ?
		state->status.network_icon : "network-wireless";
	cairo_surface_t *wifi_icon = assets != NULL ?
		orange_assets_icon(assets, variant, wifi_name) : NULL;
	if (wifi_icon != NULL) {
		draw_tinted_image_cover(cr, wifi_icon, wifi_rect, 0.92, 255, 255, 255);
	}
	const char *sound_name = state->status.sound_icon[0] != '\0' ?
		state->status.sound_icon : "audio-volume-high";
	cairo_surface_t *sound_icon = assets != NULL ?
		orange_assets_icon(assets, variant, sound_name) : NULL;
	if (sound_icon != NULL) {
		draw_tinted_image_cover(cr, sound_icon, sound_rect, 0.88, 255, 255, 255);
	}
	const char *battery_name = state->status.battery_icon[0] != '\0' ?
		state->status.battery_icon : "battery";
	cairo_surface_t *battery_icon = assets != NULL ?
		orange_assets_icon(assets, variant, battery_name) : NULL;
	if (battery_icon != NULL) {
		draw_tinted_image_cover(cr, battery_icon, battery_rect, 0.92, 255, 255, 255);
	} else {
		draw_status_battery(cr, battery_rect);
	}
	cairo_surface_t *search_icon = assets != NULL ?
		orange_assets_icon(assets, variant, "edit-find") : NULL;
	if (search_icon != NULL) {
		draw_tinted_image_cover(cr, search_icon, search_rect, 0.88, 255, 255, 255);
	}
	cairo_surface_t *control_icon = assets != NULL ?
		orange_assets_icon(assets, variant, "control-center") : NULL;
	if (control_icon == NULL && assets != NULL) {
		control_icon = orange_assets_icon(assets, variant, "preferences-system");
	}
	if (control_icon != NULL) {
		draw_tinted_image_cover(cr, control_icon, control_rect, 0.88, 255, 255, 255);
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
	orange_glass_draw(cr, &panel, 14 * s, dark);
	int icon_size = scaled_i(22, s);
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
					item.x + scaled_i(12, s),
					item.y + (item.height - icon_size) / 2,
					icon_size,
					icon_size,
				};
				draw_tinted_image_cover(cr, icon, icon_rect, 0.85,
					palette.text_r, palette.text_g, palette.text_b);
			}
		}
		draw_text(cr, orange_menubar_menu_label(i),
			item.x + scaled_i(40, s),
			item.y + scaled_i(32, s),
			22 * s,
			palette.text_r, palette.text_g, palette.text_b,
			palette.text_alpha, i == 0);
	}
}

static const char *context_menu_label_for_draw(
		enum orange_context_menu_kind kind,
		int index,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		char *buffer,
		size_t buffer_size) {
	int tab = app_menu_tab_for_context_kind(kind);
	if (state != NULL && state->app_menu.available &&
			tab >= 0 && tab < state->app_menu.tab_count &&
			index >= 0 && index < state->app_menu.item_counts[tab] &&
			state->app_menu.items[tab][index].label[0] != '\0') {
		return state->app_menu.items[tab][index].label;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR && index == 0 &&
			config != NULL) {
		return config->dock_magnification ?
			"Turn Magnification Off" : "Turn Magnification On";
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR && index == 1 &&
			config != NULL) {
		const char *size = "Medium";
		if (config->dock_scale < 0.93) {
			size = "Small";
		} else if (config->dock_scale > 1.09) {
			size = "Large";
		}
		snprintf(buffer, buffer_size, "Dock Size: %s", size);
		return buffer;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR && index == 2 &&
			config != NULL) {
		const char *position = "Bottom";
		if (config->dock_position == ORANGE_DOCK_POSITION_LEFT) {
			position = "Left";
		} else if (config->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
			position = "Right";
		}
		snprintf(buffer, buffer_size, "Position on Screen: %s", position);
		return buffer;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR && index == 3 &&
			config != NULL) {
		snprintf(buffer, buffer_size, "Minimize Using: %s",
			config->minimize_effect == ORANGE_MINIMIZE_EFFECT_SCALE ?
				"Scale Effect" : "Genie Effect");
		return buffer;
	}
	if (kind != ORANGE_CONTEXT_MENU_APP || buffer == NULL || buffer_size == 0) {
		return orange_menubar_context_menu_label(kind, index);
	}

	const char *app_label = orange_menubar_active_app_label(state);
	switch (index) {
	case 0:
		snprintf(buffer, buffer_size, "About %s", app_label);
		return buffer;
	case 3:
		snprintf(buffer, buffer_size, "Hide %s", app_label);
		return buffer;
	case 6:
		snprintf(buffer, buffer_size, "Quit %s", app_label);
		return buffer;
	default:
		return orange_menubar_context_menu_label(kind, index);
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
	orange_glass_draw(cr, &panel, 13 * s, dark);
	if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR &&
			layout->dock_separator.width > 0) {
		double tail_w = scaled_i(22, s);
		double tail_h = scaled_i(12, s);
		double center_x = layout->dock_separator.x +
			layout->dock_separator.width / 2.0;
		double center_y = layout->dock_separator.y +
			layout->dock_separator.height / 2.0;
		if (layout->dock_position == ORANGE_DOCK_POSITION_LEFT) {
			double min_center = panel.y + scaled_i(24, s);
			double max_center = panel.y + panel.height - scaled_i(24, s);
			if (center_y < min_center) {
				center_y = min_center;
			}
			if (center_y > max_center) {
				center_y = max_center;
			}
			cairo_move_to(cr, panel.x + 1, center_y - tail_w / 2.0);
			cairo_line_to(cr, panel.x + 1, center_y + tail_w / 2.0);
			cairo_line_to(cr, panel.x - tail_h, center_y);
		} else if (layout->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
			double min_center = panel.y + scaled_i(24, s);
			double max_center = panel.y + panel.height - scaled_i(24, s);
			if (center_y < min_center) {
				center_y = min_center;
			}
			if (center_y > max_center) {
				center_y = max_center;
			}
			cairo_move_to(cr, panel.x + panel.width - 1,
				center_y - tail_w / 2.0);
			cairo_line_to(cr, panel.x + panel.width - 1,
				center_y + tail_w / 2.0);
			cairo_line_to(cr, panel.x + panel.width + tail_h, center_y);
		} else {
			double min_center = panel.x + scaled_i(24, s);
			double max_center = panel.x + panel.width - scaled_i(24, s);
			if (center_x < min_center) {
				center_x = min_center;
			}
			if (center_x > max_center) {
				center_x = max_center;
			}
			cairo_move_to(cr, center_x - tail_w / 2.0,
				panel.y + panel.height - 1);
			cairo_line_to(cr, center_x + tail_w / 2.0,
				panel.y + panel.height - 1);
			cairo_line_to(cr, center_x, panel.y + panel.height + tail_h);
		}
		cairo_close_path(cr);
		set_source_rgba255(cr,
			dark ? 42 : 246,
			dark ? 45 : 248,
			dark ? 52 : 252,
			dark ? 0.78 : 0.84);
		cairo_fill_preserve(cr);
		set_source_rgba255(cr, 255, 255, 255, dark ? 0.08 : 0.28);
		cairo_set_line_width(cr, fmax(1.0, s));
		cairo_stroke(cr);
	}
	int icon_size = scaled_i(22, s);
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
			const char *icon_name = orange_menubar_context_menu_icon_name(
				layout->context_menu_kind, i);
			if (icon_name != NULL) {
				cairo_surface_t *icon = orange_assets_icon(
					state->assets, palette.icon_variant, icon_name);
				if (icon != NULL) {
					struct orange_rect icon_rect = {
						item.x + scaled_i(12, s),
						item.y + (item.height - icon_size) / 2,
						icon_size,
						icon_size,
					};
					draw_tinted_image_cover(cr, icon, icon_rect, 0.85,
						palette.text_r, palette.text_g, palette.text_b);
				}
			}
		}
		char label_buffer[128];
		const char *label = context_menu_label_for_draw(
			layout->context_menu_kind, i, state, config,
			label_buffer, sizeof(label_buffer));
		draw_text(cr,
			label,
			item.x + scaled_i(42, s),
			item.y + scaled_i(32, s),
			22 * s,
			palette.text_r, palette.text_g, palette.text_b,
			palette.text_alpha, false);
	}
}

const char *orange_menubar_active_app_label(const struct orange_shell_state *state) {
	if (state != NULL && state->active_app_label[0] != '\0') {
		return state->active_app_label;
	}
	return "Files";
}

const char *orange_menubar_menu_label(int index) {
	if (index < 0 || index >= (int)(sizeof(menu_labels) / sizeof(menu_labels[0]))) {
		return NULL;
	}
	return menu_labels[index];
}

static bool seen_icon_name(char seen[][ORANGE_ASSET_ICON_NAME_MAX],
		int *seen_count,
		int seen_capacity,
		const char *name) {
	if (name == NULL || name[0] == '\0' || seen == NULL ||
			seen_count == NULL) {
		return true;
	}
	for (int i = 0; i < *seen_count; i++) {
		if (strcmp(seen[i], name) == 0) {
			return true;
		}
	}
	if (*seen_count < seen_capacity) {
		snprintf(seen[*seen_count], ORANGE_ASSET_ICON_NAME_MAX, "%s", name);
		(*seen_count)++;
	}
	return false;
}

static void warm_icon_surface(struct orange_assets *assets,
		const char *name,
		char seen[][ORANGE_ASSET_ICON_NAME_MAX],
		int *seen_count,
		int seen_capacity) {
	if (assets == NULL ||
			seen_icon_name(seen, seen_count, seen_capacity, name)) {
		return;
	}
	(void)orange_assets_icon(assets, ORANGE_ASSET_ICON_LIGHT, name);
	(void)orange_assets_icon(assets, ORANGE_ASSET_ICON_DARK, name);
}

static void preload_icon_path(struct orange_assets *assets,
		const char *name,
		char seen[][ORANGE_ASSET_ICON_NAME_MAX],
		int *seen_count,
		int seen_capacity) {
	if (assets == NULL ||
			seen_icon_name(seen, seen_count, seen_capacity, name)) {
		return;
	}
	orange_assets_preload_icon(assets, name);
}

static void warm_icon_array(struct orange_assets *assets,
		const char *const *names,
		int count,
		char seen[][ORANGE_ASSET_ICON_NAME_MAX],
		int *seen_count,
		int seen_capacity) {
	for (int i = 0; i < count; i++) {
		warm_icon_surface(assets, names[i], seen, seen_count,
			seen_capacity);
	}
}

static void preload_context_kind(struct orange_assets *assets,
		enum orange_context_menu_kind kind,
		char seen[][ORANGE_ASSET_ICON_NAME_MAX],
		int *seen_count,
		int seen_capacity) {
	for (int i = 0; i < ORANGE_CONTEXT_MENU_ITEM_MAX; i++) {
		preload_icon_path(assets,
			orange_menubar_context_menu_icon_name(kind, i),
			seen, seen_count, seen_capacity);
	}
}

void orange_menubar_warm_assets(struct orange_assets *assets) {
	if (assets == NULL) {
		return;
	}

	char surface_seen[96][ORANGE_ASSET_ICON_NAME_MAX];
	char preload_seen[192][ORANGE_ASSET_ICON_NAME_MAX];
	int surface_seen_count = 0;
	int preload_seen_count = 0;

	warm_icon_array(assets, menu_icon_names,
		(int)(sizeof(menu_icon_names) / sizeof(menu_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, app_context_icon_names,
		(int)(sizeof(app_context_icon_names) / sizeof(app_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, file_context_icon_names,
		(int)(sizeof(file_context_icon_names) / sizeof(file_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, edit_context_icon_names,
		(int)(sizeof(edit_context_icon_names) / sizeof(edit_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, desktop_context_icon_names,
		(int)(sizeof(desktop_context_icon_names) / sizeof(desktop_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, dock_context_icon_names,
		(int)(sizeof(dock_context_icon_names) / sizeof(dock_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, dock_running_context_icon_names,
		(int)(sizeof(dock_running_context_icon_names) /
			sizeof(dock_running_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, dock_separator_context_icon_names,
		(int)(sizeof(dock_separator_context_icon_names) /
			sizeof(dock_separator_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));

	static const enum orange_context_menu_kind preload_kinds[] = {
		ORANGE_CONTEXT_MENU_APP_VIEW,
		ORANGE_CONTEXT_MENU_APP_GO,
		ORANGE_CONTEXT_MENU_APP_WINDOW,
		ORANGE_CONTEXT_MENU_APP_HISTORY,
		ORANGE_CONTEXT_MENU_APP_BOOKMARKS,
		ORANGE_CONTEXT_MENU_APP_TOOLS,
		ORANGE_CONTEXT_MENU_APP_HELP,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR,
		ORANGE_CONTEXT_MENU_WIDGET,
		ORANGE_CONTEXT_MENU_DESKTOP_ICON,
		ORANGE_CONTEXT_MENU_DESKTOP_VOLUME,
		ORANGE_CONTEXT_MENU_STATUS,
		ORANGE_CONTEXT_MENU_STATUS_WIFI,
		ORANGE_CONTEXT_MENU_STATUS_SOUND,
		ORANGE_CONTEXT_MENU_STATUS_BATTERY,
	};
	for (int k = 0; k < (int)(sizeof(preload_kinds) /
			sizeof(preload_kinds[0])); k++) {
		preload_context_kind(assets, preload_kinds[k],
			preload_seen, &preload_seen_count,
			(int)(sizeof(preload_seen) / sizeof(preload_seen[0])));
	}

	static const char *const chrome_icons[] = {
		"orange-menu",
		"network-wireless",
		"network-offline",
		"audio-volume-high",
		"audio-volume-muted",
		"battery",
		"battery-good",
		"edit-find",
		"control-center",
		"preferences-system",
	};
	for (int i = 0; i < (int)(sizeof(chrome_icons) /
			sizeof(chrome_icons[0])); i++) {
		warm_icon_surface(assets, chrome_icons[i], surface_seen,
			&surface_seen_count,
			(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	}
}

	const char *orange_menubar_context_menu_label(
		enum orange_context_menu_kind kind,
		int index) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_APP:
		if (index >= 0 &&
				index < (int)(sizeof(app_context_labels) /
					sizeof(app_context_labels[0]))) {
			return app_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_FILE:
		if (index >= 0 &&
				index < (int)(sizeof(file_context_labels) /
					sizeof(file_context_labels[0]))) {
			return file_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_EDIT:
		if (index >= 0 &&
				index < (int)(sizeof(edit_context_labels) /
					sizeof(edit_context_labels[0]))) {
			return edit_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_VIEW:
		if (index >= 0 &&
				index < (int)(sizeof(view_context_labels) /
					sizeof(view_context_labels[0]))) {
			return view_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_GO:
		if (index >= 0 &&
				index < (int)(sizeof(go_context_labels) /
					sizeof(go_context_labels[0]))) {
			return go_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_WINDOW:
		if (index >= 0 &&
				index < (int)(sizeof(window_context_labels) /
					sizeof(window_context_labels[0]))) {
			return window_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_HISTORY:
		if (index >= 0 &&
				index < (int)(sizeof(history_context_labels) /
					sizeof(history_context_labels[0]))) {
			return history_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_BOOKMARKS:
		if (index >= 0 &&
				index < (int)(sizeof(bookmarks_context_labels) /
					sizeof(bookmarks_context_labels[0]))) {
			return bookmarks_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_TOOLS:
		if (index >= 0 &&
				index < (int)(sizeof(tools_context_labels) /
					sizeof(tools_context_labels[0]))) {
			return tools_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_HELP:
		if (index >= 0 &&
				index < (int)(sizeof(help_context_labels) /
					sizeof(help_context_labels[0]))) {
			return help_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_VOLUME: {
		(void)index;
		static const char *vol_labels[] = {
			"Open",
			"Get Info",
			"Eject",
		};
		if (index >= 0 && index < 3) {
			return vol_labels[index];
		}
		break;
	}
	case ORANGE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_labels) /
					sizeof(dock_context_labels[0]))) {
			return dock_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_RUNNING:
		if (index >= 0 &&
				index < (int)(sizeof(dock_running_context_labels) /
					sizeof(dock_running_context_labels[0]))) {
			return dock_running_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_SEPARATOR:
		if (index >= 0 &&
				index < (int)(sizeof(dock_separator_context_labels) /
					sizeof(dock_separator_context_labels[0]))) {
			return dock_separator_context_labels[index];
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
	case ORANGE_CONTEXT_MENU_STATUS_WIFI:
		if (index >= 0 &&
				index < (int)(sizeof(status_wifi_context_labels) /
					sizeof(status_wifi_context_labels[0]))) {
			return status_wifi_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS_SOUND:
		if (index >= 0 &&
				index < (int)(sizeof(status_sound_context_labels) /
					sizeof(status_sound_context_labels[0]))) {
			return status_sound_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS_BATTERY:
		if (index >= 0 &&
				index < (int)(sizeof(status_battery_context_labels) /
					sizeof(status_battery_context_labels[0]))) {
			return status_battery_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}

	const char *orange_menubar_context_menu_icon_name(
		enum orange_context_menu_kind kind,
		int index) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_APP:
		if (index >= 0 &&
				index < (int)(sizeof(app_context_icon_names) /
					sizeof(app_context_icon_names[0]))) {
			return app_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_FILE:
		if (index >= 0 &&
				index < (int)(sizeof(file_context_icon_names) /
					sizeof(file_context_icon_names[0]))) {
			return file_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_EDIT:
		if (index >= 0 &&
				index < (int)(sizeof(edit_context_icon_names) /
					sizeof(edit_context_icon_names[0]))) {
			return edit_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_VIEW:
		if (index >= 0 &&
				index < (int)(sizeof(view_context_icon_names) /
					sizeof(view_context_icon_names[0]))) {
			return view_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_GO:
		if (index >= 0 &&
				index < (int)(sizeof(go_context_icon_names) /
					sizeof(go_context_icon_names[0]))) {
			return go_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_WINDOW:
		if (index >= 0 &&
				index < (int)(sizeof(window_context_icon_names) /
					sizeof(window_context_icon_names[0]))) {
			return window_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_HISTORY:
		if (index >= 0 &&
				index < (int)(sizeof(history_context_icon_names) /
					sizeof(history_context_icon_names[0]))) {
			return history_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_BOOKMARKS:
		if (index >= 0 &&
				index < (int)(sizeof(bookmarks_context_icon_names) /
					sizeof(bookmarks_context_icon_names[0]))) {
			return bookmarks_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_TOOLS:
		if (index >= 0 &&
				index < (int)(sizeof(tools_context_icon_names) /
					sizeof(tools_context_icon_names[0]))) {
			return tools_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_APP_HELP:
		if (index >= 0 &&
				index < (int)(sizeof(help_context_icon_names) /
					sizeof(help_context_icon_names[0]))) {
			return help_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_VOLUME: {
		(void)index;
		static const char *vol_icons[] = {
			"document-open",
			"document-properties",
			"media-eject",
		};
		if (index >= 0 && index < 3) {
			return vol_icons[index];
		}
		break;
	}
	case ORANGE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_icon_names) /
					sizeof(dock_context_icon_names[0]))) {
			return dock_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_RUNNING:
		if (index >= 0 &&
				index < (int)(sizeof(dock_running_context_icon_names) /
					sizeof(dock_running_context_icon_names[0]))) {
			return dock_running_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_SEPARATOR:
		if (index >= 0 &&
				index < (int)(sizeof(dock_separator_context_icon_names) /
					sizeof(dock_separator_context_icon_names[0]))) {
			return dock_separator_context_icon_names[index];
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
	case ORANGE_CONTEXT_MENU_STATUS_WIFI:
		if (index >= 0 &&
				index < (int)(sizeof(status_wifi_context_icon_names) /
					sizeof(status_wifi_context_icon_names[0]))) {
			return status_wifi_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS_SOUND:
		if (index >= 0 &&
				index < (int)(sizeof(status_sound_context_icon_names) /
					sizeof(status_sound_context_icon_names[0]))) {
			return status_sound_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_STATUS_BATTERY:
		if (index >= 0 &&
				index < (int)(sizeof(status_battery_context_icon_names) /
					sizeof(status_battery_context_icon_names[0]))) {
			return status_battery_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}

const char *orange_menubar_app_menu_tab_label(
		const struct orange_app_menu_model *app_menu,
		int tab,
		const char *active_app_label) {
	return app_menu_tab_label(app_menu, tab, active_app_label);
}

int orange_menubar_app_menu_item_count(
		enum orange_context_menu_kind kind,
		const struct orange_app_menu_model *app_menu) {
	return app_menu_item_count_for_kind(kind, app_menu);
}

enum orange_app_menu_tab orange_menubar_tab_for_context_kind(
		enum orange_context_menu_kind kind) {
	return app_menu_tab_for_context_kind(kind);
}

void orange_menubar_draw(
		cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	draw_menu_bar(cr, layout, state, config);
}

void orange_menubar_draw_system_menu(
		cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	draw_system_menu(cr, layout, state, config);
}

void orange_menubar_draw_context_menu(
		cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	draw_context_menu(cr, layout, state, config);
}
