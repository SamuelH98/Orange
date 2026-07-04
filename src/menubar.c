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
	"Options",
	"Show Recents",
	"Remove from Dock",
};

static const char *dock_context_icon_names[] = {
	"document-open",
	"preferences-system",
	"document-open-recent",
	"list-remove",
};

static const char *dock_launcher_context_labels[] = {
	"Open Launchpad",
	"Dock Settings...",
};

static const char *dock_launcher_context_icon_names[] = {
	"view-app-grid",
	"preferences-system",
};

static const char *dock_trash_context_labels[] = {
	"Open Trash",
	"Empty Trash...",
	"Dock Settings...",
};

static const char *dock_trash_context_icon_names[] = {
	"user-trash",
	"user-trash-full",
	"preferences-system",
};

static const char *dock_running_context_labels[] = {
	"Show All Windows",
	"Hide Others",
	"Options",
	"Force Quit",
};

static const char *dock_running_context_icon_names[] = {
	"view-restore",
	"view-hidden",
	"preferences-system",
	"process-stop",
};

static const char *dock_options_context_labels[] = {
	"Keep in Dock",
	"Show in Files",
	"Open at Login",
};

static const char *dock_options_context_icon_names[] = {
	"starred",
	"system-file-manager",
	"system-run",
};

static const char *dock_minimize_using_context_labels[] = {
	"Genie Effect",
	"Scale Effect",
};

static const char *dock_position_context_labels[] = {
	"Left",
	"Bottom",
	"Right",
};

static const char *dock_separator_context_labels[] = {
	"Turn Hiding On/Off",
	"Turn Magnification On/Off",
	"Minimize Using",
	"Position on Screen",
	"Dock Settings...",
};

static const char *dock_separator_context_icon_names[] = {
	"view-hidden",
	"zoom-in",
	"window-minimize",
	"preferences-desktop-display",
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
	"Paste Item",
	"Get Info",
	"Change Wallpaper...",
	"Edit Widgets...",
	"Use Stacks",
	"Sort By",
	"Clean Up By",
	"Show View Options",
};

static const char *desktop_context_icon_names[] = {
	"folder-new",
	"edit-paste",
	"document-properties",
	"preferences-desktop-wallpaper",
	"preferences-desktop",
	"view-sort-ascending",
	"view-sort-descending",
	"view-refresh",
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

static const char *desktop_file_context_labels[] = {
	"Open",
	"Open With...",
	"Show in Files",
	"Copy",
	"Get Info",
	"Rename",
	"Duplicate",
	"Quick Look",
	"Share",
	"Move to Trash",
};

static const char *desktop_file_context_icon_names[] = {
	"document-open",
	"application-x-executable",
	"system-file-manager",
	"edit-copy",
	"document-properties",
	"accessories-text-editor",
	"edit-copy",
	"zoom-fit-best",
	"document-send",
	"user-trash",
};

static const char *desktop_selection_context_labels[] = {
	"Open",
	"Copy",
	"Get Info",
	"Share",
};

static const char *desktop_selection_context_icon_names[] = {
	"document-open",
	"edit-copy",
	"document-properties",
	"document-send",
};

static const char *desktop_file_selection_context_labels[] = {
	"Open",
	"Show in Files",
	"Copy",
	"Get Info",
	"Quick Look",
	"Share",
	"Move to Trash",
};

static const char *desktop_file_selection_context_icon_names[] = {
	"document-open",
	"system-file-manager",
	"edit-copy",
	"document-properties",
	"zoom-fit-best",
	"document-send",
	"user-trash",
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

static double layout_menu_scale(const struct orange_shell_layout *layout) {
	if (layout != NULL && layout->menu_scale > 0.0) {
		return layout->menu_scale;
	}
	return layout_scale(layout);
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
	cairo_rectangle(cr, r.x, r.y, r.width, r.height);
	cairo_clip(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
	cairo_restore(cr);
}

static bool is_square_icon_name(const char *name) {
	static const char *square_icons[] = {
		"preferences-system",
		"battery",
	};
	for (size_t i = 0; i < sizeof(square_icons) / sizeof(square_icons[0]); i++) {
		if (strcmp(name, square_icons[i]) == 0) {
			return true;
		}
	}
	return false;
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

static const char *status_notifier_icon_name(
		const struct orange_status_notifier_item *item) {
	if (item == NULL) {
		return "application-x-executable";
	}
	if (strcmp(item->status, "NeedsAttention") == 0 &&
			item->attention_icon_name[0] != '\0') {
		return item->attention_icon_name;
	}
	if (item->icon_name[0] != '\0') {
		return item->icon_name;
	}
	return "application-x-executable";
}

static void draw_status_notifier_fallback(cairo_t *cr,
		struct orange_rect icon_rect,
		double s) {
	cairo_save(cr);
	rounded_rect(cr, icon_rect.x, icon_rect.y,
		icon_rect.width, icon_rect.height, scaled_i(5, s));
	set_source_rgba255(cr, 255, 255, 255, 0.12);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.72);
	cairo_set_line_width(cr, fmax(1.0, 1.5 * s));
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_status_notifier_items(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		double s) {
	if (layout == NULL || state == NULL) {
		return;
	}
	int count = layout->status_notifier_item_count;
	if (count > state->status_notifier_item_count) {
		count = state->status_notifier_item_count;
	}
	if (count > ORANGE_STATUS_NOTIFIER_ITEM_MAX) {
		count = ORANGE_STATUS_NOTIFIER_ITEM_MAX;
	}
	for (int i = 0; i < count; i++) {
		struct orange_rect bounds = layout->status_notifier_items[i];
		struct orange_rect icon_rect = centered_rect_in(bounds,
			scaled_i(32, s), scaled_i(32, s));
		const char *icon_name =
			status_notifier_icon_name(&state->status_notifier_items[i]);
		cairo_surface_t *icon = state->assets != NULL ?
			orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT,
				icon_name) : NULL;
		if (icon != NULL) {
			draw_tinted_image_cover(cr, icon, icon_rect,
				0.92, 255, 255, 255);
		} else {
			draw_status_notifier_fallback(cr, icon_rect, s);
		}
	}
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

static const double MENU_GLASS_ALPHA = 0.94;
static const double MENU_GLASS_RADIUS = 10.0;

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

static double menu_glass_radius(const struct orange_shell_layout *layout) {
	return MENU_GLASS_RADIUS * layout_menu_scale(layout);
}

static void append_menu_glass_shape(cairo_t *cr,
		const struct orange_rect *rect,
		const cairo_path_t *path,
		double radius) {
	if (path != NULL && path->status == CAIRO_STATUS_SUCCESS) {
		cairo_append_path(cr, path);
	} else {
		rounded_rect(cr, rect->x, rect->y, rect->width,
			rect->height, radius);
	}
}

static void clip_menu_glass_shape(cairo_t *cr,
		const struct orange_rect *rect,
		const cairo_path_t *path,
		double radius) {
	cairo_new_path(cr);
	append_menu_glass_shape(cr, rect, path, radius);
	cairo_clip(cr);
}

static void paint_menu_glass_finish(cairo_t *cr,
		const struct orange_rect *rect,
		const cairo_path_t *path,
		double radius,
		bool dark) {
	cairo_save(cr);
	clip_menu_glass_shape(cr, rect, path, radius);
	if (dark) {
		set_source_rgba255(cr, 20, 23, 28, 0.12);
	} else {
		set_source_rgba255(cr, 42, 44, 52, 0.14);
	}
	cairo_paint(cr);
	cairo_restore(cr);
}

static void draw_menu_glass(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		bool dark) {
	cairo_push_group(cr);
	orange_glass_draw(cr, rect, radius, dark);
	paint_menu_glass_finish(cr, rect, NULL, radius, dark);
	cairo_pop_group_to_source(cr);
	cairo_paint_with_alpha(cr, MENU_GLASS_ALPHA);
}

static void draw_menu_glass_path(cairo_t *cr,
		const struct orange_rect *bounds,
		const cairo_path_t *path,
		double radius,
		bool dark) {
	cairo_push_group(cr);
	orange_glass_draw_path(cr, bounds, path, radius, dark);
	paint_menu_glass_finish(cr, bounds, path, radius, dark);
	cairo_pop_group_to_source(cr);
	cairo_paint_with_alpha(cr, MENU_GLASS_ALPHA);
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

static bool app_menu_should_use_files_profile(const char *active_app_label) {
	return active_app_label == NULL || active_app_label[0] == '\0' ||
		strcasecmp(active_app_label, "Files") == 0 ||
		strcasecmp(active_app_label, "Nautilus") == 0;
}

static bool app_menu_should_use_browser_profile(const char *active_app_label) {
	if (active_app_label == NULL || active_app_label[0] == '\0') {
		return false;
	}
	return strcasestr(active_app_label, "Firefox") != NULL ||
		strcasestr(active_app_label, "Browser") != NULL ||
		strcasestr(active_app_label, "Chromium") != NULL ||
		strcasestr(active_app_label, "Chrome") != NULL ||
		strcasestr(active_app_label, "Brave") != NULL;
}

static const char *fallback_app_menu_tab_label(
		int tab,
		const char *active_app_label) {
	if (tab == ORANGE_APP_MENU_TAB_APP) {
		return active_app_label != NULL && active_app_label[0] != '\0' ?
			active_app_label : "Files";
	}
	if (app_menu_should_use_files_profile(active_app_label)) {
		static const char *files_labels[ORANGE_APP_MENU_TAB_COUNT] = {
			"", "File", "Edit", "View", "Go", "Window", "", "Help",
		};
		return tab >= 0 && tab < ORANGE_APP_MENU_TAB_COUNT ?
			files_labels[tab] : "";
	}
	if (app_menu_should_use_browser_profile(active_app_label)) {
		static const char *browser_labels[ORANGE_APP_MENU_TAB_COUNT] = {
			"", "File", "Edit", "View", "History", "Bookmarks", "Tools", "Help",
		};
		return tab >= 0 && tab < ORANGE_APP_MENU_TAB_COUNT ?
			browser_labels[tab] : "";
	}
	static const char *generic_labels[ORANGE_APP_MENU_TAB_COUNT] = {
		"", "File", "Edit", "View", "", "Window", "", "Help",
	};
	return tab >= 0 && tab < ORANGE_APP_MENU_TAB_COUNT ?
		generic_labels[tab] : "";
}

static const char *app_menu_tab_label(
		const struct orange_app_menu_model *app_menu,
		int tab,
		const char *active_app_label) {
	if (tab == ORANGE_APP_MENU_TAB_APP) {
		return fallback_app_menu_tab_label(tab, active_app_label);
	}
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

bool orange_menubar_context_menu_uses_icons(
		enum orange_context_menu_kind kind) {
	return kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH;
}

const char *orange_menubar_menu_shortcut_label(int index) {
	switch (index) {
	case 4:
		return "Ctrl+Alt+Esc";
	case 8:
		return "Ctrl+Alt+Q";
	case 9:
		return "Shift+Ctrl+Q";
	default:
		return NULL;
	}
}

bool orange_menubar_menu_has_submenu(int index) {
	(void)index;
	return false;
}

const char *orange_menubar_context_menu_shortcut_label(
		enum orange_context_menu_kind kind,
		int index) {
	switch (kind) {
	case ORANGE_CONTEXT_MENU_APP:
		switch (index) {
		case 3:
			return "Ctrl+H";
		case 4:
			return "Ctrl+Alt+H";
		case 6:
			return "Ctrl+Q";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_APP_FILE:
		switch (index) {
		case 0:
			return "Ctrl+N";
		case 1:
			return "Ctrl+O";
		case 3:
			return "Ctrl+W";
		case 4:
			return "Ctrl+S";
		case 5:
			return "Shift+Ctrl+S";
		case 6:
			return "Ctrl+P";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_APP_EDIT:
		switch (index) {
		case 0:
			return "Ctrl+Z";
		case 1:
			return "Shift+Ctrl+Z";
		case 2:
			return "Ctrl+X";
		case 3:
			return "Ctrl+C";
		case 4:
			return "Ctrl+V";
		case 5:
			return "Ctrl+A";
		case 6:
			return "Ctrl+F";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_APP_VIEW:
		switch (index) {
		case 0:
			return "Ctrl++";
		case 1:
			return "Ctrl+-";
		case 2:
			return "Ctrl+0";
		case 4:
			return "Ctrl+Alt+F";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_APP_GO:
	case ORANGE_CONTEXT_MENU_APP_HISTORY:
		switch (index) {
		case 0:
			return kind == ORANGE_CONTEXT_MENU_APP_HISTORY ?
				"Ctrl+[" : "Shift+Ctrl+H";
		case 1:
			return kind == ORANGE_CONTEXT_MENU_APP_HISTORY ?
				"Ctrl+]" : "Shift+Ctrl+D";
		case 2:
			return kind == ORANGE_CONTEXT_MENU_APP_HISTORY ?
				"Shift+Ctrl+H" : "Shift+Ctrl+O";
		case 3:
			return kind == ORANGE_CONTEXT_MENU_APP_GO ? "Ctrl+Alt+L" : NULL;
		case 4:
			return kind == ORANGE_CONTEXT_MENU_APP_GO ? "Shift+Ctrl+A" : NULL;
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_APP_WINDOW:
		switch (index) {
		case 0:
			return "Ctrl+M";
		case 2:
			return "Ctrl+`";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DOCK_RUNNING:
		switch (index) {
		case 1:
			return "Ctrl+H";
		case 3:
			return "Ctrl+Q";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DOCK_TRASH:
		return index == 1 ? "Shift+Ctrl+Del" : NULL;
	case ORANGE_CONTEXT_MENU_DESKTOP:
		switch (index) {
		case 0:
			return "Shift+Ctrl+N";
		case 1:
			return "Ctrl+V";
		case 2:
			return "Ctrl+I";
		case 8:
			return "Ctrl+J";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DESKTOP_ICON:
		switch (index) {
		case 2:
			return "Ctrl+C";
		case 3:
			return "Ctrl+I";
		case 5:
			return "Ctrl+D";
		case 6:
			return "Space";
		case 8:
			return "Ctrl+Del";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DESKTOP_VOLUME:
		switch (index) {
		case 1:
			return "Ctrl+I";
		case 2:
			return "Ctrl+E";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE:
		switch (index) {
		case 3:
			return "Ctrl+C";
		case 4:
			return "Ctrl+I";
		case 6:
			return "Ctrl+D";
		case 7:
			return "Space";
		case 9:
			return "Ctrl+Del";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DESKTOP_SELECTION:
		switch (index) {
		case 1:
			return "Ctrl+C";
		case 2:
			return "Ctrl+I";
		default:
			return NULL;
		}
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION:
		switch (index) {
		case 2:
			return "Ctrl+C";
		case 3:
			return "Ctrl+I";
		case 4:
			return "Space";
		case 6:
			return "Ctrl+Del";
		default:
			return NULL;
		}
	default:
		return NULL;
	}
}

bool orange_menubar_context_menu_has_submenu(
		enum orange_context_menu_kind kind,
		int index) {
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE && index == 1) {
		return true;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK && index == 1) {
		return true;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING && index == 2) {
		return true;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR &&
			(index == 2 || index == 3)) {
		return true;
	}
	return false;
}

static bool native_app_menu_item_enabled(
		const struct orange_shell_state *state,
		enum orange_context_menu_kind kind,
		int index) {
	int tab = app_menu_tab_for_context_kind(kind);
	if (state == NULL || !state->app_menu.available ||
			tab < 0 || tab >= state->app_menu.tab_count ||
			index < 0 || index >= state->app_menu.item_counts[tab]) {
		return true;
	}
	return state->app_menu.items[tab][index].enabled;
}

static enum orange_widget_size widget_size_for_target(
		const struct orange_config *config,
		int target) {
	if (target == 0) {
		return config != NULL ?
			config->calendar_widget_size : ORANGE_WIDGET_SIZE_SMALL;
	}
	if (target == 1) {
		return config != NULL ?
			config->weather_widget_size : ORANGE_WIDGET_SIZE_SMALL;
	}
	return ORANGE_WIDGET_SIZE_SMALL;
}

static const char *desktop_sort_detail_name(
		enum orange_desktop_sort_by sort) {
	switch (sort) {
	case ORANGE_DESKTOP_SORT_NONE:
		return "None";
	case ORANGE_DESKTOP_SORT_SNAP_TO_GRID:
		return "Grid";
	case ORANGE_DESKTOP_SORT_NAME:
		return "Name";
	case ORANGE_DESKTOP_SORT_DATE_ADDED:
		return "Date Added";
	case ORANGE_DESKTOP_SORT_DATE_MODIFIED:
		return "Date Modified";
	case ORANGE_DESKTOP_SORT_SIZE:
		return "Size";
	case ORANGE_DESKTOP_SORT_KIND:
		return "Kind";
	default:
		return "";
	}
}

static bool context_menu_item_checked(
		enum orange_context_menu_kind kind,
		int index,
		const struct orange_shell_layout *layout,
		const struct orange_config *config) {
	if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		enum orange_widget_size size = widget_size_for_target(config,
			layout != NULL ? layout->context_menu_index : -1);
		return (index == 1 && size == ORANGE_WIDGET_SIZE_SMALL) ||
			(index == 2 && size == ORANGE_WIDGET_SIZE_MEDIUM) ||
			(index == 3 && size == ORANGE_WIDGET_SIZE_LARGE);
	}
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP && index == 5) {
		return config != NULL && config->desktop_use_stacks;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_OPTIONS && index == 0) {
		return layout == NULL || !layout->context_menu_dock_temporary;
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING) {
		enum orange_minimize_effect effect = config != NULL ?
			config->minimize_effect : ORANGE_MINIMIZE_EFFECT_GENIE;
		return (index == 0 && effect == ORANGE_MINIMIZE_EFFECT_GENIE) ||
			(index == 1 && effect == ORANGE_MINIMIZE_EFFECT_SCALE);
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_POSITION) {
		enum orange_dock_position position = config != NULL ?
			config->dock_position : ORANGE_DOCK_POSITION_BOTTOM;
		return (index == 0 && position == ORANGE_DOCK_POSITION_LEFT) ||
			(index == 1 && position == ORANGE_DOCK_POSITION_BOTTOM) ||
			(index == 2 && position == ORANGE_DOCK_POSITION_RIGHT);
	}
	return false;
}

static const char *context_menu_detail_for_draw(
		enum orange_context_menu_kind kind,
		int index,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		char *buffer,
		size_t buffer_size) {
	(void)layout;
	if (buffer == NULL || buffer_size == 0) {
		return NULL;
	}
	buffer[0] = '\0';
	const char *shortcut =
		orange_menubar_context_menu_shortcut_label(kind, index);
	if (shortcut != NULL) {
		return shortcut;
	}
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP && index == 6 &&
			config != NULL) {
		snprintf(buffer, buffer_size, "%s",
			desktop_sort_detail_name(config->desktop_sort_by));
		return buffer;
	}
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP && index == 7) {
		snprintf(buffer, buffer_size, "%s", "Grid");
		return buffer;
	}
	if (state != NULL && kind == ORANGE_CONTEXT_MENU_STATUS_WIFI &&
			index == 0 && state->status.network_label[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", state->status.network_label);
		return buffer;
	}
	if (state != NULL && kind == ORANGE_CONTEXT_MENU_STATUS_SOUND &&
			index == 0 && state->status.sound_label[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", state->status.sound_label);
		return buffer;
	}
	if (state != NULL && kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY &&
			index == 0 && state->status.battery_label[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", state->status.battery_label);
		return buffer;
	}
	return NULL;
}

static double text_advance(cairo_t *cr,
		const char *text,
		double font_size,
		bool bold) {
	if (text == NULL || text[0] == '\0') {
		return 0.0;
	}
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, text, &extents);
	return extents.x_advance;
}

static double menu_item_baseline(cairo_t *cr,
		struct orange_rect item,
		double font_size,
		bool bold) {
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	cairo_font_extents_t fe;
	cairo_font_extents(cr, &fe);
	return item.y + ((double)item.height - fe.ascent - fe.descent) * 0.5 +
		fe.ascent;
}

static void draw_submenu_chevron(cairo_t *cr,
		double center_x,
		double center_y,
		double s,
		const struct menu_palette *palette,
		double alpha) {
	double w = scaled_i(6, s);
	double h = scaled_i(10, s);
	cairo_save(cr);
	set_source_rgba255(cr, palette->text_r, palette->text_g,
		palette->text_b, alpha);
	cairo_set_line_width(cr, fmax(1.25, 2.0 * s));
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(cr, center_x - w * 0.5, center_y - h * 0.5);
	cairo_line_to(cr, center_x + w * 0.5, center_y);
	cairo_line_to(cr, center_x - w * 0.5, center_y + h * 0.5);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_menu_check(cairo_t *cr,
		struct orange_rect item,
		double s,
		const struct menu_palette *palette,
		double alpha) {
	double x = item.x + scaled_i(12, s);
	double y = item.y + item.height / 2.0;
	cairo_save(cr);
	set_source_rgba255(cr, palette->text_r, palette->text_g,
		palette->text_b, alpha);
	cairo_set_line_width(cr, fmax(1.35, 2.4 * s));
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(cr, x, y + scaled_i(1, s));
	cairo_line_to(cr, x + scaled_i(5, s), y + scaled_i(6, s));
	cairo_line_to(cr, x + scaled_i(15, s), y - scaled_i(7, s));
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_menu_item_highlight(cairo_t *cr,
		struct orange_rect item,
		double s,
		const struct menu_palette *palette) {
	cairo_save(cr);
	rounded_rect(cr,
		item.x + scaled_i(2, s),
		item.y + scaled_i(3, s),
		item.width - scaled_i(4, s),
		item.height - scaled_i(6, s),
		scaled_i(5, s));
	set_source_rgba255(cr, palette->text_r, palette->text_g,
		palette->text_b, 0.15);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, palette->text_r, palette->text_g,
		palette->text_b, 0.18);
	cairo_set_line_width(cr, fmax(1.0, 1.0 * s));
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_menu_item_text(cairo_t *cr,
		struct orange_rect item,
		double s,
		const struct menu_palette *palette,
		const char *label,
		const char *detail,
		bool submenu,
		bool checked,
		bool bold,
		bool enabled,
		bool icon_leading) {
	if (label == NULL || label[0] == '\0') {
		return;
	}
	double font_size = 22.0 * s;
	double detail_size = 20.0 * s;
	double text_alpha = enabled ? palette->text_alpha : 0.38;
	double detail_alpha = enabled ? palette->text_alpha * 0.72 : 0.32;
	double baseline = menu_item_baseline(cr, item, font_size, bold);
	int left_pad = icon_leading ? scaled_i(42, s) : scaled_i(34, s);
	double label_x = item.x + left_pad;
	double right_edge = item.x + item.width - scaled_i(26, s);
	double center_y = item.y + item.height / 2.0;

	if (checked && !icon_leading) {
		draw_menu_check(cr, item, s, palette, text_alpha);
	}

	if (submenu) {
		draw_submenu_chevron(cr, right_edge - scaled_i(5, s), center_y, s,
			palette, detail_alpha);
		right_edge -= scaled_i(20, s);
	}

	double detail_left = right_edge;
	if (detail != NULL && detail[0] != '\0') {
		double advance = text_advance(cr, detail, detail_size, false);
		detail_left = right_edge - advance;
		double detail_baseline = menu_item_baseline(cr, item,
			detail_size, false);
		cairo_save(cr);
		cairo_rectangle(cr, item.x, item.y, item.width, item.height);
		cairo_clip(cr);
		draw_text(cr, detail, detail_left, detail_baseline,
			detail_size, palette->text_r, palette->text_g,
			palette->text_b, detail_alpha, false);
		cairo_restore(cr);
	}

	double label_right = detail != NULL && detail[0] != '\0' ?
		detail_left - scaled_i(16, s) : right_edge;
	if (label_right < label_x + scaled_i(48, s)) {
		label_right = label_x + scaled_i(48, s);
	}
	cairo_save(cr);
	cairo_rectangle(cr, label_x, item.y,
		fmax(1.0, label_right - label_x), item.height);
	cairo_clip(cr);
	draw_text(cr, label, label_x, baseline, font_size,
		palette->text_r, palette->text_g, palette->text_b,
		text_alpha, bold);
	cairo_restore(cr);
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
	draw_status_notifier_items(cr, layout, state, s);

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
		if (is_square_icon_name(wifi_name)) {
			draw_image_cover(cr, wifi_icon, wifi_rect);
		} else {
			draw_tinted_image_cover(cr, wifi_icon, wifi_rect, 0.92, 255, 255, 255);
		}
	}
	const char *sound_name = state->status.sound_icon[0] != '\0' ?
		state->status.sound_icon : "audio-volume-high";
	cairo_surface_t *sound_icon = assets != NULL ?
		orange_assets_icon(assets, variant, sound_name) : NULL;
	if (sound_icon != NULL) {
		if (is_square_icon_name(sound_name)) {
			draw_image_cover(cr, sound_icon, sound_rect);
		} else {
			draw_tinted_image_cover(cr, sound_icon, sound_rect, 0.92, 255, 255, 255);
		}
	}
	const char *battery_name = state->status.battery_icon[0] != '\0' ?
		state->status.battery_icon : "battery";
	cairo_surface_t *battery_icon = assets != NULL ?
		orange_assets_icon(assets, variant, battery_name) : NULL;
	if (battery_icon != NULL) {
		if (is_square_icon_name(battery_name)) {
			draw_image_cover(cr, battery_icon, battery_rect);
		} else {
			draw_tinted_image_cover(cr, battery_icon, battery_rect, 0.92, 255, 255, 255);
		}
	} else {
		draw_status_battery(cr, battery_rect);
	}
	cairo_surface_t *search_icon = assets != NULL ?
		orange_assets_icon(assets, variant, "edit-find") : NULL;
	if (search_icon != NULL) {
		if (is_square_icon_name("edit-find")) {
			draw_image_cover(cr, search_icon, search_rect);
		} else {
			draw_tinted_image_cover(cr, search_icon, search_rect, 0.92, 255, 255, 255);
		}
	}
}

static void draw_system_menu(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_menu_scale(layout);
	(void)state;
	if (layout->system_menu_item_count == 0) {
		return;
	}

	struct orange_rect panel = layout->system_menu_panel;
	bool dark = is_dark_config(config);
	struct menu_palette palette = menu_palette_for_config(config);
	draw_menu_glass(cr, &panel, menu_glass_radius(layout), dark);
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
		draw_menu_item_text(cr, item, s, &palette,
			orange_menubar_menu_label(i),
			orange_menubar_menu_shortcut_label(i),
			orange_menubar_menu_has_submenu(i),
			false,
			i == 0,
			true,
			false);
	}
}

static const char *context_menu_label_for_draw(
		enum orange_context_menu_kind kind,
		int index,
		const struct orange_shell_layout *layout,
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
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH) {
		if (state != NULL && index >= 0 &&
				index < state->open_with_app_count &&
				index < ORANGE_OPEN_WITH_APP_MAX &&
				state->open_with_app_labels[index][0] != '\0') {
			return state->open_with_app_labels[index];
		}
		return "No Applications Available";
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR && config != NULL) {
		if (index == 0) {
			return config->dock_auto_hide ?
				"Turn Hiding Off" : "Turn Hiding On";
		}
		if (index == 1) {
			return config->dock_magnification ?
				"Turn Magnification Off" : "Turn Magnification On";
		}
	}
	if (kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING) {
		bool alt = layout != NULL && layout->context_menu_alt_pressed;
		switch (index) {
		case 1:
			return alt ? "Hide Others" : "Hide";
		case 3:
			return alt ? "Force Quit" : "Quit";
		default:
			break;
		}
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

static const char *context_menu_icon_name_for_draw(
		enum orange_context_menu_kind kind,
		int index,
		const struct orange_shell_state *state) {
	if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH) {
		if (state != NULL && index >= 0 &&
				index < state->open_with_app_count &&
				index < ORANGE_OPEN_WITH_APP_MAX &&
				state->open_with_app_icons[index][0] != '\0') {
			return state->open_with_app_icons[index];
		}
		return "application-x-executable";
	}
	return orange_menubar_context_menu_icon_name(kind, index);
}

static bool context_menu_kind_is_dock(enum orange_context_menu_kind kind) {
	return kind == ORANGE_CONTEXT_MENU_DOCK ||
		kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING ||
		kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR ||
		kind == ORANGE_CONTEXT_MENU_DOCK_LAUNCHER ||
		kind == ORANGE_CONTEXT_MENU_DOCK_TRASH;
}

static bool dock_context_anchor_rect(
		const struct orange_shell_layout *layout,
		struct orange_rect *out_anchor) {
	if (layout == NULL || out_anchor == NULL) {
		return false;
	}
	if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR &&
			layout->dock_separator.width > 0 &&
			layout->dock_separator.height > 0) {
		*out_anchor = layout->dock_separator;
		return true;
	}
	if (!context_menu_kind_is_dock(layout->context_menu_kind) ||
			layout->context_menu_index < 0 ||
			layout->context_menu_index >= layout->dock_item_count) {
		return false;
	}
	*out_anchor = layout->dock_items[layout->context_menu_index];
	return out_anchor->width > 0 && out_anchor->height > 0;
}

static void dock_context_bubble_path(cairo_t *cr,
		const struct orange_shell_layout *layout,
		struct orange_rect panel,
		double s) {
	struct orange_rect anchor = {0};
	if (!dock_context_anchor_rect(layout, &anchor)) {
		rounded_rect(cr, panel.x, panel.y, panel.width, panel.height, 10 * s);
		return;
	}

	double tail = layout->dock_position == ORANGE_DOCK_POSITION_BOTTOM ?
		scaled_i(24, s) : scaled_i(18, s);
	double half_mouth = layout->dock_position == ORANGE_DOCK_POSITION_BOTTOM ?
		scaled_i(21, s) : scaled_i(16, s);
	double half_throat = scaled_i(4.5, s);
	double cap = half_throat * 0.12;
	double inset = scaled_i(34, s);

	if (layout->dock_position == ORANGE_DOCK_POSITION_BOTTOM) {
		double r = clamp(10.0 * s, 0.0,
			fmin(panel.width, panel.height) / 2.0);
		double x = panel.x;
		double y = panel.y;
		double w = panel.width;
		double h = panel.height;
		double bottom = y + h;
		double outlet_y = bottom + tail - half_throat;
		double cx = clamp(anchor.x + anchor.width / 2.0,
			panel.x + inset, panel.x + panel.width - inset);

		cairo_new_sub_path(cr);
		cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
		cairo_arc(cr, x + w - r, bottom - r, r, 0.0, M_PI / 2.0);
		cairo_line_to(cr, cx + half_mouth, bottom);
		cairo_curve_to(cr, cx + half_mouth * 0.82, bottom + tail * 0.10,
			cx + half_throat, bottom + tail * 0.58,
			cx + half_throat, outlet_y);
		cairo_curve_to(cr, cx + half_throat, outlet_y + cap,
			cx + cap, outlet_y + half_throat,
			cx, outlet_y + half_throat);
		cairo_curve_to(cr, cx - cap, outlet_y + half_throat,
			cx - half_throat, outlet_y + cap,
			cx - half_throat, outlet_y);
		cairo_curve_to(cr, cx - half_throat, bottom + tail * 0.58,
			cx - half_mouth * 0.82, bottom + tail * 0.10,
			cx - half_mouth, bottom);
		cairo_line_to(cr, x + r, bottom);
		cairo_arc(cr, x + r, bottom - r, r, M_PI / 2.0, M_PI);
		cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
		cairo_close_path(cr);
	} else if (layout->dock_position == ORANGE_DOCK_POSITION_LEFT) {
		rounded_rect(cr, panel.x, panel.y, panel.width, panel.height, 10 * s);
		double mouth_x = panel.x + scaled_i(8, s);
		double edge_x = panel.x - scaled_i(1, s);
		double outlet_x = edge_x - tail + half_throat;
		double cy = clamp(panel.y + scaled_i(42, s),
			panel.y + inset, panel.y + panel.height - inset);
		cairo_new_sub_path(cr);
		cairo_move_to(cr, mouth_x, cy + half_mouth);
		cairo_curve_to(cr, edge_x - tail * 0.10, cy + half_mouth * 0.82,
			edge_x - tail * 0.52, cy + half_throat,
			outlet_x, cy + half_throat);
		cairo_curve_to(cr, outlet_x - cap, cy + half_throat,
			outlet_x - half_throat, cy + cap,
			outlet_x - half_throat, cy);
		cairo_curve_to(cr, outlet_x - half_throat, cy - cap,
			outlet_x - cap, cy - half_throat,
			outlet_x, cy - half_throat);
		cairo_curve_to(cr, edge_x - tail * 0.52, cy - half_throat,
			edge_x - tail * 0.10, cy - half_mouth * 0.82,
			mouth_x, cy - half_mouth);
		cairo_close_path(cr);
	} else if (layout->dock_position == ORANGE_DOCK_POSITION_RIGHT) {
		rounded_rect(cr, panel.x, panel.y, panel.width, panel.height, 10 * s);
		double mouth_x = panel.x + panel.width - scaled_i(8, s);
		double edge_x = panel.x + panel.width + scaled_i(1, s);
		double outlet_x = edge_x + tail - half_throat;
		double cy = clamp(panel.y + scaled_i(42, s),
			panel.y + inset, panel.y + panel.height - inset);
		cairo_new_sub_path(cr);
		cairo_move_to(cr, mouth_x, cy - half_mouth);
		cairo_curve_to(cr, edge_x + tail * 0.10, cy - half_mouth * 0.82,
			edge_x + tail * 0.52, cy - half_throat,
			outlet_x, cy - half_throat);
		cairo_curve_to(cr, outlet_x + cap, cy - half_throat,
			outlet_x + half_throat, cy - cap,
			outlet_x + half_throat, cy);
		cairo_curve_to(cr, outlet_x + half_throat, cy + cap,
			outlet_x + cap, cy + half_throat,
			outlet_x, cy + half_throat);
		cairo_curve_to(cr, edge_x + tail * 0.52, cy + half_throat,
			edge_x + tail * 0.10, cy + half_mouth * 0.82,
			mouth_x, cy + half_mouth);
		cairo_close_path(cr);
	} else {
		rounded_rect(cr, panel.x, panel.y, panel.width, panel.height, 10 * s);
	}
}

static struct orange_rect path_bounds(cairo_t *cr) {
	double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
	cairo_path_extents(cr, &x1, &y1, &x2, &y2);

	int x = (int)floor(x1) - 2;
	int y = (int)floor(y1) - 2;
	int right = (int)ceil(x2) + 2;
	int bottom = (int)ceil(y2) + 2;
	if (right < x) {
		right = x;
	}
	if (bottom < y) {
		bottom = y;
	}
	return (struct orange_rect){x, y, right - x, bottom - y};
}

static void draw_dock_context_menu_bubble(cairo_t *cr,
		const struct orange_shell_layout *layout,
		struct orange_rect panel,
		double s,
		bool dark) {
	cairo_new_path(cr);
	dock_context_bubble_path(cr, layout, panel, s);
	struct orange_rect bounds = path_bounds(cr);
	cairo_path_t *path = cairo_copy_path(cr);
	if (path == NULL || path->status != CAIRO_STATUS_SUCCESS) {
		if (path != NULL) {
			cairo_path_destroy(path);
		}
		draw_menu_glass(cr, &panel, menu_glass_radius(layout), dark);
		return;
	}
	cairo_new_path(cr);

	draw_menu_glass_path(cr, &bounds, path, menu_glass_radius(layout), dark);

	cairo_path_destroy(path);
}

static void draw_context_submenu(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		const struct menu_palette *palette) {
	bool open_with = layout->open_with_submenu_open;
	bool dock_options = layout->dock_options_submenu_open;
	bool dock_minimize = layout->dock_minimize_submenu_open;
	bool dock_position = layout->dock_position_submenu_open;
	if ((!open_with && !dock_options && !dock_minimize && !dock_position) ||
			layout->context_submenu_item_count <= 0 ||
			layout->context_submenu_panel.width <= 0 ||
			layout->context_submenu_panel.height <= 0) {
		return;
	}

	enum orange_context_menu_kind submenu_kind =
		ORANGE_CONTEXT_MENU_DOCK_OPTIONS;
	if (open_with) {
		submenu_kind = ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH;
	} else if (dock_minimize) {
		submenu_kind = ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING;
	} else if (dock_position) {
		submenu_kind = ORANGE_CONTEXT_MENU_DOCK_POSITION;
	}

	double s = layout_menu_scale(layout);
	struct orange_rect panel = layout->context_submenu_panel;
	bool dark = is_dark_config(config);
	draw_menu_glass(cr, &panel, menu_glass_radius(layout), dark);

	bool use_icons = open_with;
	int icon_size = scaled_i(20, s);
	for (int i = 0; i < layout->context_submenu_item_count; i++) {
		struct orange_rect item = layout->context_submenu_items[i];
		if (use_icons && state != NULL && state->assets != NULL) {
			const char *icon_name = context_menu_icon_name_for_draw(
				submenu_kind, i, state);
			if (icon_name != NULL) {
				cairo_surface_t *icon = orange_assets_icon(
					state->assets, palette->icon_variant, icon_name);
				if (icon != NULL) {
					struct orange_rect icon_rect = {
						item.x + scaled_i(11, s),
						item.y + (item.height - icon_size) / 2,
						icon_size,
						icon_size,
					};
					draw_image_cover(cr, icon, icon_rect);
				}
			}
		}

		char label_buffer[128];
		const char *label = context_menu_label_for_draw(
			submenu_kind, i, layout, state, config,
			label_buffer, sizeof(label_buffer));
		bool enabled = open_with ? layout->open_with_app_count > 0 : true;
		bool checked = !open_with && context_menu_item_checked(
			submenu_kind, i, layout, config);
		draw_menu_item_text(cr, item, s, palette, label, NULL,
			false, checked, false, enabled, use_icons);
	}
}

static void draw_context_menu(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = layout_menu_scale(layout);
	if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_NONE ||
			layout->context_menu_item_count == 0) {
		return;
	}

	struct orange_rect panel = layout->context_menu_panel;
	bool dark = is_dark_config(config);
	struct menu_palette palette = menu_palette_for_config(config);
	bool dock_menu = context_menu_kind_is_dock(layout->context_menu_kind);
	if (dock_menu) {
		draw_dock_context_menu_bubble(cr, layout, panel, s, dark);
	} else {
		draw_menu_glass(cr, &panel, menu_glass_radius(layout), dark);
	}
	bool use_icons =
		orange_menubar_context_menu_uses_icons(layout->context_menu_kind);
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
		if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE &&
				layout->open_with_submenu_open && i == 1) {
			draw_menu_item_highlight(cr, item, s, &palette);
		}
		if (((layout->context_menu_kind == ORANGE_CONTEXT_MENU_DOCK &&
					i == 1) ||
				(layout->context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING &&
					i == 2)) &&
				layout->dock_options_submenu_open) {
			draw_menu_item_highlight(cr, item, s, &palette);
		}
		if (layout->context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR &&
				((i == 2 && layout->dock_minimize_submenu_open) ||
				 (i == 3 && layout->dock_position_submenu_open))) {
			draw_menu_item_highlight(cr, item, s, &palette);
		}
		if (use_icons && state != NULL && state->assets != NULL) {
			const char *icon_name = context_menu_icon_name_for_draw(
				layout->context_menu_kind, i, state);
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
					draw_image_cover(cr, icon, icon_rect);
				}
			}
		}
		char label_buffer[128];
		char detail_buffer[128];
		const char *label = context_menu_label_for_draw(
			layout->context_menu_kind, i, layout, state, config,
			label_buffer, sizeof(label_buffer));
		const char *detail = context_menu_detail_for_draw(
			layout->context_menu_kind, i, layout, state, config,
			detail_buffer, sizeof(detail_buffer));
		bool enabled = native_app_menu_item_enabled(state,
			layout->context_menu_kind, i);
		bool checked = context_menu_item_checked(
			layout->context_menu_kind, i, layout, config);
		draw_menu_item_text(cr, item, s, &palette, label, detail,
			orange_menubar_context_menu_has_submenu(
				layout->context_menu_kind, i),
			checked, false, enabled, use_icons);
	}
	draw_context_submenu(cr, layout, state, config, &palette);
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

	warm_icon_array(assets, status_wifi_context_icon_names,
		(int)(sizeof(status_wifi_context_icon_names) /
			sizeof(status_wifi_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, status_sound_context_icon_names,
		(int)(sizeof(status_sound_context_icon_names) /
			sizeof(status_sound_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));
	warm_icon_array(assets, status_battery_context_icon_names,
		(int)(sizeof(status_battery_context_icon_names) /
			sizeof(status_battery_context_icon_names[0])),
		surface_seen, &surface_seen_count,
		(int)(sizeof(surface_seen) / sizeof(surface_seen[0])));

	static const enum orange_context_menu_kind preload_kinds[] = {
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
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE: {
		if (index >= 0 &&
				index < (int)(sizeof(desktop_file_context_labels) /
					sizeof(desktop_file_context_labels[0]))) {
			return desktop_file_context_labels[index];
		}
		break;
	}
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH:
		return index >= 0 ? "No Applications Available" : NULL;
	case ORANGE_CONTEXT_MENU_DESKTOP_SELECTION:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_selection_context_labels) /
					sizeof(desktop_selection_context_labels[0]))) {
			return desktop_selection_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_file_selection_context_labels) /
					sizeof(desktop_file_selection_context_labels[0]))) {
			return desktop_file_selection_context_labels[index];
		}
		break;
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
	case ORANGE_CONTEXT_MENU_DOCK_LAUNCHER:
		if (index >= 0 &&
				index < (int)(sizeof(dock_launcher_context_labels) /
					sizeof(dock_launcher_context_labels[0]))) {
			return dock_launcher_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_TRASH:
		if (index >= 0 &&
				index < (int)(sizeof(dock_trash_context_labels) /
					sizeof(dock_trash_context_labels[0]))) {
			return dock_trash_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_OPTIONS:
		if (index >= 0 &&
				index < (int)(sizeof(dock_options_context_labels) /
					sizeof(dock_options_context_labels[0]))) {
			return dock_options_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING:
		if (index >= 0 &&
				index < (int)(sizeof(dock_minimize_using_context_labels) /
					sizeof(dock_minimize_using_context_labels[0]))) {
			return dock_minimize_using_context_labels[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_POSITION:
		if (index >= 0 &&
				index < (int)(sizeof(dock_position_context_labels) /
					sizeof(dock_position_context_labels[0]))) {
			return dock_position_context_labels[index];
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
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE: {
		if (index >= 0 &&
				index < (int)(sizeof(desktop_file_context_icon_names) /
					sizeof(desktop_file_context_icon_names[0]))) {
			return desktop_file_context_icon_names[index];
		}
		break;
	}
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH:
		return index >= 0 ? "application-x-executable" : NULL;
	case ORANGE_CONTEXT_MENU_DESKTOP_SELECTION:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_selection_context_icon_names) /
					sizeof(desktop_selection_context_icon_names[0]))) {
			return desktop_selection_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_file_selection_context_icon_names) /
					sizeof(desktop_file_selection_context_icon_names[0]))) {
			return desktop_file_selection_context_icon_names[index];
		}
		break;
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
	case ORANGE_CONTEXT_MENU_DOCK_LAUNCHER:
		if (index >= 0 &&
				index < (int)(sizeof(dock_launcher_context_icon_names) /
					sizeof(dock_launcher_context_icon_names[0]))) {
			return dock_launcher_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_TRASH:
		if (index >= 0 &&
				index < (int)(sizeof(dock_trash_context_icon_names) /
					sizeof(dock_trash_context_icon_names[0]))) {
			return dock_trash_context_icon_names[index];
		}
		break;
	case ORANGE_CONTEXT_MENU_DOCK_OPTIONS:
		if (index >= 0 &&
				index < (int)(sizeof(dock_options_context_icon_names) /
					sizeof(dock_options_context_icon_names[0]))) {
			return dock_options_context_icon_names[index];
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
