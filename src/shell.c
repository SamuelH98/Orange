#include "tahoe/shell.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REFERENCE_WIDTH 2880.0
#define REFERENCE_HEIGHT 1800.0
#define MIN_UI_SCALE 0.50
#define MAX_UI_SCALE 1.60

struct launcher {
	const char *label;
	const char *command;
};

static const struct launcher dock_launchers[] = {
	{"Finder", "xdg-open \"$HOME\""},
	{"Launchpad", "${TAHOE_APP_PICKER:-wofi --show drun || rofi -show drun || true}"},
	{"Safari", "xdg-open \"https://www.apple.com/macos/\""},
	{"Messages", "xdg-open \"sms:\" || true"},
	{"Mail", "xdg-email || true"},
	{"Maps", "xdg-open \"https://maps.apple.com\""},
	{"Photos", "xdg-open \"$HOME/Pictures\""},
	{"FaceTime", "xdg-open \"facetime:\" || true"},
	{"Phone", "xdg-open \"tel:\" || true"},
	{"Calendar", "gnome-calendar || korganizer || xdg-open \"https://calendar.google.com\""},
	{"Contacts", "gnome-contacts || kaddressbook || true"},
	{"Reminders", "gnome-todo || endeavour || true"},
	{"Notes", "gnome-text-editor || gedit || mousepad || ${TAHOE_TERMINAL:-xterm} -e nano"},
	{"TV", "xdg-open \"https://tv.apple.com\""},
	{"Music", "xdg-open \"$HOME/Music\""},
	{"App Store", "gnome-software || plasma-discover || true"},
	{"Settings", "GSK_RENDERER=cairo build/tahoe-settings tahoe.conf || true"},
	{"Calculator", "gnome-calculator || kcalc || xcalc || true"},
	{"Trash", "gio open trash:// || xdg-open trash:// || true"},
};

static const char *menu_labels[] = {
	"About Tahoe",
	"System Settings...",
	"App Store...",
	"Recent Items",
	"Force Quit...",
	"Sleep",
	"Restart...",
	"Shut Down...",
	"Lock Screen",
	"Log Out",
};

/* Index of first separator after each group; -1 marks end.
   A separator is drawn before the item at this index. */
static const int menu_separator_before[] = {3, 4, 5, 8, -1};

static const char *menu_icon_names[] = {
	"info.circle",
	"gear",
	"bag",
	"clock",
	"xmark.octagon",
	"moon.zzz",
	"arrow.counterclockwise",
	"power",
	"lock",
	"arrow.right.to.line.alt",
};

static const char *dock_context_labels[] = {
	"Open",
	"Show in Finder",
	"Remove from Dock",
	"Open at Login",
	"Dock Settings",
};

static const char *dock_context_icon_names[] = {
	"arrow.up.doc",
	"magnifyingglass",
	"minus.circle",
	"person.crop.circle.badge.plus",
	"gear",
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
	"square.and.pencil",
	"rectangle.3.offgrid",
	"square.grid.2x2",
	"rectangle.grid.3x2",
	"minus.circle",
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
	"folder.badge.plus",
	"info.circle",
	"rectangle.stack",
	"arrow.up.arrow.down",
	"wand.and.stars",
	"sidebar.right",
	"photo",
	"square.grid.2x2",
};

static const int desktop_context_separator_before[] = {2, 5, -1};

static const char *desktop_icon_context_labels[] = {
	"Open",
	"Show in Finder",
	"Copy",
	"Get Info",
	"Rename",
	"Duplicate",
	"Quick Look",
	"Share",
	"Move to Trash",
};

static const char *desktop_icon_context_icon_names[] = {
	"arrow.up.doc",
	"magnifyingglass",
	"doc.on.doc",
	"info.circle",
	"square.and.pencil",
	"plus.square.on.square",
	"eye",
	"square.and.arrow.up",
	"trash",
};

static const int desktop_icon_context_separator_before[] = {2, 6, 8, -1};

static bool rect_contains(const struct tahoe_rect *rect, int x, int y) {
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

static struct tahoe_rect widget_rect_for_size(
		enum tahoe_widget_type type,
		enum tahoe_widget_size size,
		int x,
		int y,
		double s) {
	int small_w = type == TAHOE_WIDGET_CALENDAR ? 328 : 326;
	int small_h = type == TAHOE_WIDGET_CALENDAR ? 329 : 326;
	int width = small_w;
	int height = small_h;
	if (size == TAHOE_WIDGET_SIZE_MEDIUM) {
		width = 674;
		height = small_h;
	} else if (size == TAHOE_WIDGET_SIZE_LARGE) {
		width = 674;
		height = 674;
	}
	return (struct tahoe_rect){
		x,
		y,
		scaled_i(width, s),
		scaled_i(height, s),
	};
}

static double layout_scale(const struct tahoe_shell_layout *layout) {
	return clamp((double)layout->menu_bar.height / 48.0,
		MIN_UI_SCALE, MAX_UI_SCALE);
}

static const struct tahoe_config *state_config(
		const struct tahoe_shell_state *state,
		struct tahoe_config *fallback) {
	if (state != NULL && state->config != NULL) {
		return state->config;
	}
	tahoe_config_set_defaults(fallback);
	return fallback;
}

static bool is_dark_config(const struct tahoe_config *config) {
	return config != NULL && config->appearance == TAHOE_APPEARANCE_DARK;
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
		struct tahoe_rect r) {
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
		struct tahoe_rect r, double opacity) {
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
		struct tahoe_rect r, double opacity, int red, int green, int blue) {
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

static void stroke_status_line(cairo_t *cr, double width) {
	cairo_set_line_width(cr, width);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke(cr);
}

static void draw_status_keyboard(cairo_t *cr, struct tahoe_rect r) {
	double u = fmin(r.width, r.height) / 28.0;
	rounded_rect(cr, r.x, r.y, r.width, r.height, r.height * 0.18);
	set_source_rgba255(cr, 255, 255, 255, 0.92);
	cairo_set_line_width(cr, 2.0 * u);
	cairo_stroke(cr);
	for (int row = 0; row < 2; row++) {
		for (int col = 0; col < 3; col++) {
			cairo_rectangle(cr,
				r.x + 5 * u + col * 8 * u,
				r.y + 5 * u + row * 7 * u,
				4 * u,
				3 * u);
			cairo_fill(cr);
		}
	}
}

static void draw_status_battery(cairo_t *cr, struct tahoe_rect r) {
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

static void draw_status_search(cairo_t *cr, struct tahoe_rect r) {
	double u = fmin(r.width, r.height) / 30.0;
	set_source_rgba255(cr, 255, 255, 255, 0.92);
	cairo_arc(cr, r.x + r.width * 0.43, r.y + r.height * 0.40,
		r.width * 0.27, 0, 2.0 * M_PI);
	stroke_status_line(cr, 2.8 * u);
	cairo_move_to(cr, r.x + r.width * 0.62, r.y + r.height * 0.62);
	cairo_line_to(cr, r.x + r.width * 0.82, r.y + r.height * 0.82);
	stroke_status_line(cr, 2.8 * u);
}

static void draw_status_control_center(cairo_t *cr, struct tahoe_rect r) {
	double u = fmin(r.width, r.height) / 30.0;
	set_source_rgba255(cr, 255, 255, 255, 0.92);
	rounded_rect(cr, r.x + 2 * u, r.y + 3 * u,
		r.width - 4 * u, 8 * u, 4 * u);
	cairo_set_line_width(cr, 2.4 * u);
	cairo_stroke(cr);
	rounded_rect(cr, r.x + 2 * u, r.y + r.height - 11 * u,
		r.width - 4 * u, 8 * u, 4 * u);
	cairo_stroke(cr);
	cairo_arc(cr, r.x + r.width * 0.35, r.y + 7 * u, 3.0 * u, 0, 2.0 * M_PI);
	cairo_fill(cr);
	cairo_arc(cr, r.x + r.width * 0.68, r.y + r.height - 7 * u, 3.0 * u, 0, 2.0 * M_PI);
	cairo_fill(cr);
}

static void draw_status_location(cairo_t *cr, struct tahoe_rect r) {
	set_source_rgba255(cr, 255, 255, 255, 0.92);
	cairo_move_to(cr, r.x + r.width * 0.16, r.y + r.height * 0.20);
	cairo_line_to(cr, r.x + r.width * 0.86, r.y + r.height * 0.48);
	cairo_line_to(cr, r.x + r.width * 0.42, r.y + r.height * 0.61);
	cairo_line_to(cr, r.x + r.width * 0.28, r.y + r.height * 0.86);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void draw_status_siri(cairo_t *cr, struct tahoe_rect r) {
	cairo_pattern_t *pattern = cairo_pattern_create_radial(
		r.x + r.width * 0.35, r.y + r.height * 0.35, 2,
		r.x + r.width * 0.50, r.y + r.height * 0.50, r.width * 0.58);
	cairo_pattern_add_color_stop_rgb(pattern, 0.00, 1.00, 0.32, 0.45);
	cairo_pattern_add_color_stop_rgb(pattern, 0.35, 0.32, 0.65, 1.00);
	cairo_pattern_add_color_stop_rgb(pattern, 0.70, 1.00, 0.42, 0.74);
	cairo_pattern_add_color_stop_rgb(pattern, 1.00, 0.95, 0.95, 1.00);
	cairo_arc(cr, r.x + r.width / 2.0, r.y + r.height / 2.0,
		fmin(r.width, r.height) * 0.46, 0, 2.0 * M_PI);
	cairo_set_source(cr, pattern);
	cairo_fill(cr);
	cairo_pattern_destroy(pattern);
}

static struct tahoe_rect status_rect_before(
		int *right,
		int width,
		int height,
		int center_y,
		int gap) {
	*right -= width;
	struct tahoe_rect rect = {
		*right,
		center_y - height / 2,
		width,
		height,
	};
	*right -= gap;
	return rect;
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

static void draw_wallpaper(cairo_t *cr, int width, int height,
		const struct tahoe_assets *assets,
		const struct tahoe_config *config) {
	cairo_surface_t *wallpaper = NULL;
	if (assets != NULL) {
		if (is_dark_config(config) && assets->wallpaper_dark != NULL) {
			wallpaper = assets->wallpaper_dark;
		} else if (assets->wallpaper_beach_day != NULL) {
			wallpaper = assets->wallpaper_beach_day;
		} else {
			wallpaper = assets->wallpaper;
		}
	}
	if (wallpaper != NULL) {
		int sw = cairo_image_surface_get_width(wallpaper);
		int sh = cairo_image_surface_get_height(wallpaper);
		double scale = fmax((double)width / (double)sw, (double)height / (double)sh);
		double tx = ((double)width - sw * scale) * 0.5;
		double ty = ((double)height - sh * scale) * 0.5;
		cairo_save(cr);
		cairo_translate(cr, tx, ty);
		cairo_scale(cr, scale, scale);
		cairo_set_source_surface(cr, wallpaper, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
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

static void draw_liquid_panel(cairo_t *cr, const struct tahoe_rect *rect,
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

static void draw_dock_glass(cairo_t *cr, const struct tahoe_rect *rect,
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

static void draw_menu_glass(cairo_t *cr, const struct tahoe_rect *rect,
		double radius) {
	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 1.0, 1.0, 1.0, 0.92);
	cairo_pattern_add_color_stop_rgba(shade, 0.55, 0.96, 0.97, 0.98, 0.88);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.80, 0.84, 0.88, 0.83);
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);

	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	set_source_rgba255(cr, 255, 255, 255, 0.48);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, 0.18);
	cairo_stroke(cr);
}

static void draw_menu_bar(cairo_t *cr, const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	double s = layout_scale(layout);
	(void)config;
	int text_r = 255;
	int text_g = 255;
	int text_b = 255;
	int status_r = 252;
	int status_g = 252;
	int status_b = 252;

	if (state->assets != NULL && state->assets->apple_menu != NULL) {
		draw_tinted_image_fit(cr, state->assets->apple_menu,
			(struct tahoe_rect){
				scaled_i(39, s),
				scaled_i(13, s),
				scaled_i(31, s),
				scaled_i(31, s),
			},
			0.98,
			255, 255, 255);
	} else {
		draw_text(cr, "T", scaled_i(44, s), scaled_i(38, s),
			26 * s, 255, 255, 255, 0.98, true);
	}

	const char *menus[] = {
		"Finder", "File", "Edit", "View", "Go", "Window", "Help",
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

	const struct tahoe_assets *assets = state->assets;
	int center_y = scaled_i(29, s);
	int gap = scaled_i(16, s);
	int status_right = (int)clock_x - scaled_i(28, s);
	struct tahoe_rect siri_rect = status_rect_before(&status_right,
		scaled_i(30, s), scaled_i(30, s), center_y, gap);
	struct tahoe_rect location_rect = status_rect_before(&status_right,
		scaled_i(24, s), scaled_i(24, s), center_y, gap);
	struct tahoe_rect control_rect = status_rect_before(&status_right,
		scaled_i(29, s), scaled_i(29, s), center_y, gap);
	struct tahoe_rect search_rect = status_rect_before(&status_right,
		scaled_i(28, s), scaled_i(28, s), center_y, gap);
	struct tahoe_rect battery_rect = status_rect_before(&status_right,
		scaled_i(45, s), scaled_i(25, s), center_y, gap);
	struct tahoe_rect wifi_rect = status_rect_before(&status_right,
		scaled_i(31, s), scaled_i(27, s), center_y, gap);
	struct tahoe_rect keyboard_rect = status_rect_before(&status_right,
		scaled_i(32, s), scaled_i(24, s), center_y, gap);

	draw_status_keyboard(cr, keyboard_rect);
	if (assets != NULL && assets->status_icons[TAHOE_STATUS_ICON_WIFI] != NULL) {
		draw_tinted_image_fit(cr, assets->status_icons[TAHOE_STATUS_ICON_WIFI],
			wifi_rect, 0.92,
			status_r, status_g, status_b);
	}
	if (assets != NULL && assets->status_icons[TAHOE_STATUS_ICON_BATTERY] != NULL) {
		draw_tinted_image_fit(cr, assets->status_icons[TAHOE_STATUS_ICON_BATTERY],
			battery_rect, 0.92,
			status_r, status_g, status_b);
	} else {
		draw_status_battery(cr, battery_rect);
	}
	if (assets != NULL && assets->status_icons[TAHOE_STATUS_ICON_SEARCH] != NULL) {
		draw_tinted_image_fit(cr, assets->status_icons[TAHOE_STATUS_ICON_SEARCH],
			search_rect, 0.86,
			status_r, status_g, status_b);
	} else {
		draw_status_search(cr, search_rect);
	}
	if (assets != NULL &&
			assets->status_icons[TAHOE_STATUS_ICON_CONTROL_CENTER] != NULL) {
		draw_tinted_image_fit(cr, assets->status_icons[TAHOE_STATUS_ICON_CONTROL_CENTER],
			control_rect, 0.86,
			status_r, status_g, status_b);
	} else {
		draw_status_control_center(cr, control_rect);
	}
	draw_status_location(cr, location_rect);
	draw_status_siri(cr, siri_rect);
}

static void draw_calendar_widget(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct tahoe_rect r = layout->calendar_widget;
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
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct tahoe_rect r = layout->weather_widget;
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
	if (state->assets != NULL &&
			state->assets->status_icons[TAHOE_STATUS_ICON_WEATHER] != NULL) {
		draw_image_fit(cr, state->assets->status_icons[TAHOE_STATUS_ICON_WEATHER],
			(struct tahoe_rect){
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

static struct tahoe_rect dock_item_with_magnification(
		struct tahoe_rect item,
		bool hot,
		const struct tahoe_config *config) {
	if (!hot || config == NULL || !config->dock_magnification) {
		return item;
	}

	double mag = config->dock_magnification_scale;
	int grow_x = (int)lrint((item.width * mag - item.width) / 2.0);
	int grow_y = (int)lrint((item.height * mag - item.height) / 2.0);
	item.x -= grow_x;
	item.y -= grow_y;
	item.width += grow_x * 2;
	item.height += grow_y * 2;
	return item;
}

static void draw_dock_hover_label(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	int hot = state->hot_dock_index;
	if (hot < 0 || hot >= layout->dock_item_count ||
			hot == state->dock_drag_index) {
		return;
	}

	int launcher_idx = config != NULL && hot < TAHOE_DOCK_MAX ?
		config->dock_order[hot] : hot;
	if (launcher_idx < 0 || launcher_idx >= tahoe_shell_dock_count()) {
		launcher_idx = hot;
	}
	const char *label = tahoe_shell_dock_label(launcher_idx);
	if (label == NULL) {
		return;
	}

	double s = layout_scale(layout);
	struct tahoe_rect item = dock_item_with_magnification(
		layout->dock_items[hot], true, config);
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

static void draw_dock(cairo_t *cr, const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	struct tahoe_rect r = layout->dock;
	set_source_rgba255(cr, 0, 0, 0, 0.18);
	rounded_rect(cr, r.x + scaled_i(2, s), r.y + scaled_i(7, s),
		r.width, r.height, 34 * s);
	cairo_fill(cr);
	draw_dock_glass(cr, &r, 34 * s, dark);

	for (int i = 0; i < layout->dock_item_count; i++) {
		int launcher_idx = config != NULL && i < TAHOE_DOCK_MAX ?
			config->dock_order[i] : i;
		if (launcher_idx < 0 || launcher_idx >= TAHOE_DOCK_MAX) {
			launcher_idx = i;
		}

		if (i == state->dock_drag_index) {
			continue;
		}

		struct tahoe_rect item = layout->dock_items[i];
		bool drew_icon = false;
		item = dock_item_with_magnification(item,
			i == state->hot_dock_index, config);
		int variant = dark ? TAHOE_ASSET_ICON_DARK : TAHOE_ASSET_ICON_LIGHT;
		if (state->assets != NULL &&
				launcher_idx < state->assets->dock_icon_count &&
				(state->assets->dock_icons[variant][launcher_idx] != NULL ||
				 state->assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][launcher_idx] != NULL)) {
			cairo_surface_t *icon_surface = state->assets->dock_icons[variant][launcher_idx];
			if (icon_surface == NULL && variant == TAHOE_ASSET_ICON_DARK) {
				icon_surface = state->assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][launcher_idx];
			}
			draw_image_cover(cr, icon_surface, item);
			drew_icon = true;
		}
		if (i == 9) {
			time_t now = state->now != 0 ? state->now : time(NULL);
			struct tm local;
			localtime_r(&now, &local);
			char weekday[8];
			char day[8];
			strftime(weekday, sizeof(weekday), "%a", &local);
			snprintf(day, sizeof(day), "%d", local.tm_mday);
			struct tahoe_rect base_item = layout->dock_items[i];
			if (drew_icon) {
				rounded_rect(cr,
					base_item.x + base_item.width * 0.15,
					base_item.y + base_item.height * 0.11,
					base_item.width * 0.70,
					base_item.height * 0.76,
					base_item.width * 0.11);
				set_source_rgba255(cr, 248, 248, 246, 0.96);
				cairo_fill(cr);
			}
			double item_s = (double)base_item.width / 106.0;
			draw_text(cr, weekday,
				base_item.x + base_item.width * 0.31,
				base_item.y + base_item.height * 0.33,
				18 * item_s, 230, 52, 54, 0.96, true);
			draw_text(cr, day,
				base_item.x + base_item.width * 0.37,
				base_item.y + base_item.height * 0.73,
				47 * item_s, 42, 43, 48, 0.92, true);
		}
		if ((config == NULL || config->dock_show_indicators) &&
				launcher_idx >= 0 && launcher_idx < TAHOE_DOCK_MAX &&
				state->dock_open[launcher_idx]) {
			double item_s = (double)item.width / 106.0;
			set_source_rgba255(cr, 34, 37, 42, 0.70);
			cairo_arc(cr, item.x + item.width / 2.0,
				r.y + r.height - scaled_i(9, s),
				2.1 * item_s, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		if (i == layout->dock_item_count - 2) {
			set_source_rgba255(cr, 30, 34, 38, 0.32);
			cairo_rectangle(cr, item.x + item.width + scaled_i(30, s),
				r.y + scaled_i(17, s), fmax(1.0, 1.5 * s),
				r.height - scaled_i(34, s));
			cairo_fill(cr);
		}
	}

	draw_dock_hover_label(cr, layout, state, config);

	if (state->dock_drag_index >= 0 && state->dock_drag_index < layout->dock_item_count) {
		int launcher_idx = config != NULL ? config->dock_order[state->dock_drag_index] : state->dock_drag_index;
		if (launcher_idx < 0 || launcher_idx >= TAHOE_DOCK_MAX) {
			launcher_idx = state->dock_drag_index;
		}
		struct tahoe_rect drag_source = layout->dock_items[state->dock_drag_index];
		struct tahoe_rect ghost_item = {
			state->dock_drag_x - drag_source.width / 2,
			state->dock_drag_y - drag_source.height / 2,
			drag_source.width,
			drag_source.height,
		};
		cairo_push_group(cr);
		int variant = dark ? TAHOE_ASSET_ICON_DARK : TAHOE_ASSET_ICON_LIGHT;
		if (state->assets != NULL &&
				launcher_idx < state->assets->dock_icon_count &&
				(state->assets->dock_icons[variant][launcher_idx] != NULL ||
				 state->assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][launcher_idx] != NULL)) {
			cairo_surface_t *icon_surface = state->assets->dock_icons[variant][launcher_idx];
			if (icon_surface == NULL && variant == TAHOE_ASSET_ICON_DARK) {
				icon_surface = state->assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][launcher_idx];
			}
			draw_image_cover(cr, icon_surface, ghost_item);
		}
		cairo_pop_group_to_source(cr);
		cairo_paint_with_alpha(cr, 0.70);
	}
}

static void draw_centered_label(cairo_t *cr,
		const char *label,
		struct tahoe_rect bounds,
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

static void draw_desktop_items(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	if (config != NULL && !config->desktop_icons_visible) {
		return;
	}
	bool dark = is_dark_config(config);
	int variant = dark ? TAHOE_ASSET_ICON_DARK : TAHOE_ASSET_ICON_LIGHT;
	double s = layout_scale(layout);
	for (int i = 0; i < layout->desktop_item_count; i++) {
		const struct tahoe_desktop_entry *entry = NULL;
		if (state->desktop_entries != NULL && i < state->desktop_entry_count) {
			entry = &state->desktop_entries[i];
		}
		if (entry == NULL) {
			continue;
		}
		struct tahoe_rect r = layout->desktop_items[i];
		int icon_size = (int)lrint(r.width * (116.0 / 194.0));
		struct tahoe_rect icon_rect = {
			r.x + (r.width - icon_size) / 2,
			r.y,
			icon_size,
			icon_size,
		};
		cairo_surface_t *icon = tahoe_assets_desktop_icon(
			state->assets, variant, entry->icon);
		if (icon != NULL) {
			draw_image_fit(cr, icon, icon_rect, 1.0);
		}

		struct tahoe_rect label_rect = {
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

static void draw_apple_menu(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state) {
	double s = layout_scale(layout);
	if (layout->apple_menu_item_count == 0) {
		return;
	}

	struct tahoe_rect panel = layout->apple_menu_panel;
	draw_menu_glass(cr, &panel, 14 * s);
	int icon_size = scaled_i(18, s);
	for (int i = 0; i < layout->apple_menu_item_count; i++) {
		struct tahoe_rect item = layout->apple_menu_items[i];
		if (i > 0 && layout->apple_menu_separator[i]) {
			set_source_rgba255(cr, 42, 42, 46, 0.18);
			cairo_rectangle(cr, panel.x + scaled_i(12, s),
				item.y - 1,
				panel.width - scaled_i(24, s), 1);
			cairo_fill(cr);
		}
		if (state != NULL && state->assets != NULL) {
			cairo_surface_t *icon = tahoe_assets_context_menu_icon(
				state->assets, menu_icon_names[i]);
			if (icon != NULL) {
				struct tahoe_rect icon_rect = {
					item.x + scaled_i(10, s),
					item.y + (item.height - icon_size) / 2,
					icon_size,
					icon_size,
				};
				draw_image_fit(cr, icon, icon_rect, 0.85);
			}
		}
		draw_text(cr, tahoe_shell_menu_label(i),
			item.x + scaled_i(36, s),
			item.y + scaled_i(28, s),
			20 * s, 23, 26, 30, 0.92, i == 0);
	}
}

static void draw_context_menu(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state) {
	double s = layout_scale(layout);
	if (layout->context_menu_kind == TAHOE_CONTEXT_MENU_NONE ||
			layout->context_menu_item_count == 0) {
		return;
	}

	struct tahoe_rect panel = layout->context_menu_panel;
	draw_menu_glass(cr, &panel, 13 * s);
	int icon_size = scaled_i(18, s);
	for (int i = 0; i < layout->context_menu_item_count; i++) {
		struct tahoe_rect item = layout->context_menu_items[i];
		if (i > 0 && layout->context_menu_separator[i]) {
			set_source_rgba255(cr, 32, 36, 42, 0.13);
			cairo_rectangle(cr, panel.x + scaled_i(11, s),
				item.y - 1,
				panel.width - scaled_i(22, s), 1);
			cairo_fill(cr);
		}
		if (state != NULL && state->assets != NULL) {
			const char *icon_name = tahoe_shell_context_menu_icon_name(
				layout->context_menu_kind, i);
			if (icon_name != NULL) {
				cairo_surface_t *icon = tahoe_assets_context_menu_icon(
					state->assets, icon_name);
				if (icon != NULL) {
					struct tahoe_rect icon_rect = {
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
			tahoe_shell_context_menu_label(layout->context_menu_kind, i),
			item.x + scaled_i(36, s),
			item.y + scaled_i(27, s),
			19 * s,
			22, 25, 30, 0.92, false);
	}
}

void tahoe_shell_layout_compute(
		int width,
		int height,
		bool apple_menu_open,
		const struct tahoe_config *config,
		int desktop_entry_count,
		struct tahoe_shell_layout *layout) {
	memset(layout, 0, sizeof(*layout));
	struct tahoe_config defaults;
	if (config == NULL) {
		tahoe_config_set_defaults(&defaults);
		config = &defaults;
	}
	double s = ui_scale_for_size(width, height);
	layout->width = width;
	layout->height = height;
	layout->menu_bar = (struct tahoe_rect){0, 0, width, scaled_i(48, s)};
	layout->apple_menu_button = (struct tahoe_rect){
		scaled_i(31, s), 0, scaled_i(52, s), layout->menu_bar.height};

	layout->calendar_widget = widget_rect_for_size(TAHOE_WIDGET_CALENDAR,
		config->calendar_widget_size, scaled_i(32, s), scaled_i(92, s), s);
	layout->weather_widget = widget_rect_for_size(TAHOE_WIDGET_WEATHER,
		config->weather_widget_size, scaled_i(383, s), scaled_i(92, s), s);
	layout->widget_count = 2;
	layout->widgets[0] = (struct tahoe_widget){
		.id = 1,
		.type = TAHOE_WIDGET_CALENDAR,
		.visible = config->calendar_widget_visible,
		.rect = layout->calendar_widget,
	};
	layout->widgets[1] = (struct tahoe_widget){
		.id = 2,
		.type = TAHOE_WIDGET_WEATHER,
		.visible = config->weather_widget_visible,
		.rect = layout->weather_widget,
	};

	layout->desktop_item_count = config->desktop_icons_visible ?
		desktop_entry_count : 0;
	if (layout->desktop_item_count > TAHOE_DESKTOP_MAX) {
		layout->desktop_item_count = TAHOE_DESKTOP_MAX;
	}
	int item_x = width - scaled_i(231, s);
	int item_step = scaled_i(200 + config->desktop_grid_spacing, s);
	int item_w = scaled_i(194 * config->desktop_icon_scale, s);
	int item_h = scaled_i(212 * config->desktop_icon_scale, s);
	layout->desktop_items[0] = (struct tahoe_rect){
		item_x, scaled_i(96, s), item_w, item_h};
	for (int i = 1; i < layout->desktop_item_count; i++) {
		layout->desktop_items[i] = (struct tahoe_rect){
			item_x - scaled_i(7, s),
			scaled_i(96, s) + i * item_step,
			item_w,
			item_h,
		};
	}
	for (int i = 0; i < layout->desktop_item_count &&
			i < TAHOE_DESKTOP_POSITION_MAX; i++) {
		if (config->desktop_positions[i].valid) {
			layout->desktop_items[i].x = config->desktop_positions[i].x;
			layout->desktop_items[i].y = config->desktop_positions[i].y;
		}
	}

	double dock_s = s * config->dock_scale;
	int icon = scaled_i(106, s * config->dock_icon_scale * config->dock_scale);
	int gap = scaled_i(18, dock_s);
	int separator_extra = scaled_i(66, dock_s);
	int left_pad = scaled_i(34, dock_s);
	int right_pad = scaled_i(17, dock_s);
	int top_pad = scaled_i(27, dock_s);
	int bottom_pad = scaled_i(16, dock_s);
	int bottom_margin = scaled_i(12, dock_s);
	int dock_count = tahoe_shell_dock_count();
	int dock_width = dock_count * icon + (dock_count - 1) * gap +
		separator_extra + left_pad + right_pad;
	while (dock_width > width - scaled_i(80, s) && icon > scaled_i(40, s)) {
		icon -= 2;
		gap = gap > 6 ? gap - 1 : gap;
		separator_extra = separator_extra > scaled_i(18, s) ?
			separator_extra - 2 : separator_extra;
		left_pad = left_pad > scaled_i(8, s) ? left_pad - 1 : left_pad;
		right_pad = right_pad > scaled_i(4, s) ? right_pad - 1 : right_pad;
		dock_width = dock_count * icon + (dock_count - 1) * gap +
			separator_extra + left_pad + right_pad;
	}
	int dock_height = icon + top_pad + bottom_pad;
	layout->dock_item_count = dock_count;
	layout->dock = (struct tahoe_rect){
		(width - dock_width) / 2,
		height - dock_height - bottom_margin,
		dock_width,
		dock_height,
	};
	int x = layout->dock.x + left_pad;
	int y = layout->dock.y + top_pad;
	for (int i = 0; i < dock_count; i++) {
		int launcher_idx = config->dock_order[i];
		if (launcher_idx < 0 || launcher_idx >= dock_count) {
			launcher_idx = i;
		}
		layout->dock_items[i] = (struct tahoe_rect){x, y, icon, icon};
		x += icon + gap;
		if (i == dock_count - 2) {
			x += separator_extra;
		}
	}

	if (apple_menu_open) {
		layout->apple_menu_item_count =
			(int)(sizeof(menu_labels) / sizeof(menu_labels[0]));
		layout->apple_menu_panel = (struct tahoe_rect){
			scaled_i(18, s), layout->menu_bar.height + scaled_i(4, s),
			scaled_i(290, s),
			scaled_i(18, s) + layout->apple_menu_item_count * scaled_i(42, s)};
		memset(layout->apple_menu_separator, 0,
			sizeof(layout->apple_menu_separator));
		for (int si = 0; menu_separator_before[si] >= 0; si++) {
			int idx = menu_separator_before[si];
			if (idx < layout->apple_menu_item_count) {
				layout->apple_menu_separator[idx] = true;
			}
		}
		for (int i = 0; i < layout->apple_menu_item_count; i++) {
			layout->apple_menu_items[i] = (struct tahoe_rect){
				layout->apple_menu_panel.x + scaled_i(8, s),
				layout->apple_menu_panel.y + scaled_i(9, s) + i * scaled_i(42, s),
				layout->apple_menu_panel.width - scaled_i(16, s),
				scaled_i(40, s),
			};
		}
	}
}

void tahoe_shell_layout_set_context_menu(
		struct tahoe_shell_layout *layout,
		enum tahoe_context_menu_kind kind,
		int index,
		int cursor_x,
		int cursor_y) {
	layout->context_menu_kind = TAHOE_CONTEXT_MENU_NONE;
	layout->context_menu_index = -1;
	layout->context_menu_item_count = 0;
	if (kind == TAHOE_CONTEXT_MENU_NONE) {
		return;
	}

	double s = layout_scale(layout);
	int item_count = 0;
	struct tahoe_rect anchor = {0};
	const int *separator_before = NULL;

	if (kind == TAHOE_CONTEXT_MENU_DOCK &&
			index >= 0 && index < layout->dock_item_count) {
		anchor = layout->dock_items[index];
		item_count = (int)(sizeof(dock_context_labels) /
			sizeof(dock_context_labels[0]));
		separator_before = dock_context_separator_before;
	} else if (kind == TAHOE_CONTEXT_MENU_WIDGET &&
			index >= 0 && index < layout->widget_count &&
			layout->widgets[index].visible) {
		anchor = layout->widgets[index].rect;
		item_count = (int)(sizeof(widget_context_labels) /
			sizeof(widget_context_labels[0]));
		separator_before = widget_context_separator_before;
	} else if (kind == TAHOE_CONTEXT_MENU_DESKTOP_ICON &&
			index >= 0 && index < layout->desktop_item_count) {
		anchor = layout->desktop_items[index];
		item_count = (int)(sizeof(desktop_icon_context_labels) /
			sizeof(desktop_icon_context_labels[0]));
		separator_before = desktop_icon_context_separator_before;
	} else if (kind == TAHOE_CONTEXT_MENU_DESKTOP) {
		item_count = (int)(sizeof(desktop_context_labels) /
			sizeof(desktop_context_labels[0]));
		separator_before = desktop_context_separator_before;
	} else {
		return;
	}
	if (item_count > TAHOE_CONTEXT_MENU_ITEM_MAX) {
		item_count = TAHOE_CONTEXT_MENU_ITEM_MAX;
	}

	int item_h = scaled_i(40, s);
	int panel_width_base = 250;
	if (kind == TAHOE_CONTEXT_MENU_DOCK) {
		panel_width_base = 214;
	} else if (kind == TAHOE_CONTEXT_MENU_WIDGET) {
		panel_width_base = 232;
	} else if (kind == TAHOE_CONTEXT_MENU_DESKTOP) {
		panel_width_base = 366;
	}
	int panel_w = scaled_i(panel_width_base, s);
	int panel_h = scaled_i(12, s) + item_count * item_h;
	int x, y;
	if (kind == TAHOE_CONTEXT_MENU_DESKTOP ||
			kind == TAHOE_CONTEXT_MENU_DESKTOP_ICON) {
		x = cursor_x;
		y = cursor_y;
	} else if (kind == TAHOE_CONTEXT_MENU_DOCK) {
		x = anchor.x + anchor.width / 2 - panel_w / 2;
		y = layout->dock.y - panel_h - scaled_i(10, s);
	} else if (kind == TAHOE_CONTEXT_MENU_WIDGET) {
		x = cursor_x - panel_w / 2;
		y = cursor_y - scaled_i(8, s);
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
	layout->context_menu_panel = (struct tahoe_rect){x, y, panel_w, panel_h};
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
		layout->context_menu_items[i] = (struct tahoe_rect){
			x + scaled_i(7, s),
			y + scaled_i(6, s) + i * item_h,
			panel_w - scaled_i(14, s),
			item_h,
		};
	}
}

struct tahoe_shell_hit tahoe_shell_hit_test(
		const struct tahoe_shell_layout *layout,
		int x,
		int y) {
	if (layout->context_menu_item_count > 0 &&
			rect_contains(&layout->context_menu_panel, x, y)) {
		for (int i = 0; i < layout->context_menu_item_count; i++) {
			if (rect_contains(&layout->context_menu_items[i], x, y)) {
				return (struct tahoe_shell_hit){TAHOE_HIT_CONTEXT_MENU_ITEM, i};
			}
		}
		return (struct tahoe_shell_hit){TAHOE_HIT_NONE, -1};
	}
	if (layout->apple_menu_item_count > 0 &&
			rect_contains(&layout->apple_menu_panel, x, y)) {
		for (int i = 0; i < layout->apple_menu_item_count; i++) {
			if (rect_contains(&layout->apple_menu_items[i], x, y)) {
				return (struct tahoe_shell_hit){TAHOE_HIT_APPLE_MENU_ITEM, i};
			}
		}
		return (struct tahoe_shell_hit){TAHOE_HIT_APPLE_MENU, -1};
	}
	if (rect_contains(&layout->apple_menu_button, x, y)) {
		return (struct tahoe_shell_hit){TAHOE_HIT_APPLE_MENU, -1};
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		if (rect_contains(&layout->dock_items[i], x, y)) {
			return (struct tahoe_shell_hit){TAHOE_HIT_DOCK_ITEM, i};
		}
	}
	for (int i = 0; i < layout->widget_count; i++) {
		if (layout->widgets[i].visible &&
				rect_contains(&layout->widgets[i].rect, x, y)) {
			return (struct tahoe_shell_hit){TAHOE_HIT_WIDGET, i};
		}
	}
	for (int i = 0; i < layout->desktop_item_count; i++) {
		if (rect_contains(&layout->desktop_items[i], x, y)) {
			return (struct tahoe_shell_hit){TAHOE_HIT_DESKTOP_ITEM, i};
		}
	}
	if (y > layout->menu_bar.height) {
		return (struct tahoe_shell_hit){TAHOE_HIT_DESKTOP, -1};
	}
	return (struct tahoe_shell_hit){TAHOE_HIT_NONE, -1};
}

static void draw_widget_layer(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state,
		const struct tahoe_config *config) {
	for (int i = 0; i < layout->widget_count; i++) {
		if (!layout->widgets[i].visible) {
			continue;
		}
		switch (layout->widgets[i].type) {
		case TAHOE_WIDGET_CALENDAR:
			draw_calendar_widget(cr, layout, state, config);
			break;
		case TAHOE_WIDGET_WEATHER:
			draw_weather_widget(cr, layout, state, config);
			break;
		}
	}
}

void tahoe_shell_draw(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct tahoe_shell_state *state) {
	const struct tahoe_shell_draw_options options = {
		.draw_wallpaper = true,
	};
	tahoe_shell_draw_with_options(pixels, width, height, stride, state, &options);
}

void tahoe_shell_draw_with_options(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct tahoe_shell_state *state,
		const struct tahoe_shell_draw_options *options) {
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_t *cr = cairo_create(surface);

	struct tahoe_config fallback_config;
	const struct tahoe_config *config = state_config(state, &fallback_config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(width, height, state->apple_menu_open, config,
		state->desktop_entry_count, &layout);
	tahoe_shell_layout_set_context_menu(&layout,
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
	if (state->apple_menu_open) {
		draw_apple_menu(cr, &layout, state);
	}
	draw_context_menu(cr, &layout, state);

	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

const char *tahoe_shell_dock_label(int index) {
	if (index < 0 || index >= (int)(sizeof(dock_launchers) / sizeof(dock_launchers[0]))) {
		return NULL;
	}
	return dock_launchers[index].label;
}

const char *tahoe_shell_dock_command(int index) {
	if (index < 0 || index >= (int)(sizeof(dock_launchers) / sizeof(dock_launchers[0]))) {
		return NULL;
	}
	return dock_launchers[index].command;
}

int tahoe_shell_dock_count(void) {
	return (int)(sizeof(dock_launchers) / sizeof(dock_launchers[0]));
}

const char *tahoe_shell_menu_label(int index) {
	if (index < 0 || index >= (int)(sizeof(menu_labels) / sizeof(menu_labels[0]))) {
		return NULL;
	}
	return menu_labels[index];
}

const char *tahoe_shell_context_menu_label(
		enum tahoe_context_menu_kind kind,
		int index) {
	switch (kind) {
	case TAHOE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_labels) /
					sizeof(dock_context_labels[0]))) {
			return dock_context_labels[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_WIDGET:
		if (index >= 0 &&
				index < (int)(sizeof(widget_context_labels) /
					sizeof(widget_context_labels[0]))) {
			return widget_context_labels[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_DESKTOP:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_context_labels) /
					sizeof(desktop_context_labels[0]))) {
			return desktop_context_labels[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_DESKTOP_ICON:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_icon_context_labels) /
					sizeof(desktop_icon_context_labels[0]))) {
			return desktop_icon_context_labels[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}

const char *tahoe_shell_context_menu_icon_name(
		enum tahoe_context_menu_kind kind,
		int index) {
	switch (kind) {
	case TAHOE_CONTEXT_MENU_DOCK:
		if (index >= 0 &&
				index < (int)(sizeof(dock_context_icon_names) /
					sizeof(dock_context_icon_names[0]))) {
			return dock_context_icon_names[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_WIDGET:
		if (index >= 0 &&
				index < (int)(sizeof(widget_context_icon_names) /
					sizeof(widget_context_icon_names[0]))) {
			return widget_context_icon_names[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_DESKTOP:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_context_icon_names) /
					sizeof(desktop_context_icon_names[0]))) {
			return desktop_context_icon_names[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_DESKTOP_ICON:
		if (index >= 0 &&
				index < (int)(sizeof(desktop_icon_context_icon_names) /
					sizeof(desktop_icon_context_icon_names[0]))) {
			return desktop_icon_context_icon_names[index];
		}
		break;
	case TAHOE_CONTEXT_MENU_NONE:
	default:
		break;
	}
	return NULL;
}
