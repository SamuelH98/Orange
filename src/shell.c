#include "tahoe/shell.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REFERENCE_WIDTH 2048.0
#define REFERENCE_HEIGHT 1153.0

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
	{"Rocket", "${TAHOE_APP_PICKER:-wofi --show drun || rofi -show drun || true}"},
	{"App Store", "gnome-software || plasma-discover || true"},
	{"Calculator", "gnome-calculator || kcalc || xcalc || true"},
	{"Settings", "gnome-control-center || systemsettings || xfce4-settings-manager || true"},
	{"Desktop Preview", "xdg-open \"$HOME/Pictures\""},
	{"Trash", "gio open trash:// || xdg-open trash:// || true"},
};

static const struct launcher desktop_launchers[] = {
	{"Images", "xdg-open \"$HOME/Pictures\""},
	{"PDF Documents", "xdg-open \"$HOME/Documents\""},
};

static const char *menu_labels[] = {
	"About Tahoe wlroots",
	"System Settings...",
	"App Launcher",
	"Terminal",
	"Close Focused Window",
	"Quit Tahoe wlroots",
};

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
	return clamp(scale, 0.58, 1.60);
}

static int scaled_i(double value, double scale) {
	return (int)lrint(value * scale);
}

static double layout_scale(const struct tahoe_shell_layout *layout) {
	return clamp((double)layout->menu_bar.height / 32.0, 0.58, 1.60);
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

static void set_source_rgba255(cairo_t *cr, int r, int g, int b, double a) {
	cairo_set_source_rgba(cr, r / 255.0, g / 255.0, b / 255.0, a);
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

static void cairo_ellipse(cairo_t *cr, double x, double y, double rx, double ry) {
	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_scale(cr, rx, ry);
	cairo_arc(cr, 0, 0, 1.0, 0, 2.0 * M_PI);
	cairo_fill(cr);
	cairo_restore(cr);
}

static void draw_wallpaper(cairo_t *cr, int width, int height,
		const struct tahoe_assets *assets) {
	if (assets != NULL && assets->wallpaper != NULL) {
		int sw = cairo_image_surface_get_width(assets->wallpaper);
		int sh = cairo_image_surface_get_height(assets->wallpaper);
		double scale = fmax((double)width / (double)sw, (double)height / (double)sh);
		double tx = ((double)width - sw * scale) * 0.5;
		double ty = ((double)height - sh * scale) * 0.5;
		cairo_save(cr);
		cairo_translate(cr, tx, ty);
		cairo_scale(cr, scale, scale);
		cairo_set_source_surface(cr, assets->wallpaper, 0, 0);
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
		double radius, double alpha) {
	cairo_save(cr);
	rounded_rect(cr, rect->x, rect->y, rect->width, rect->height, radius);
	set_source_rgba255(cr, 255, 255, 255, alpha);
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, 0.38);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, 0.13);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_menu_bar(cairo_t *cr, const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state) {
	double s = layout_scale(layout);
	struct tahoe_rect bar = layout->menu_bar;
	set_source_rgba255(cr, 255, 255, 255, 0.26);
	cairo_rectangle(cr, bar.x, bar.y, bar.width, bar.height);
	cairo_fill(cr);

	if (state->assets != NULL && state->assets->apple_menu != NULL) {
		cairo_save(cr);
		cairo_translate(cr, scaled_i(25, s), scaled_i(7, s));
		double sw = cairo_image_surface_get_width(state->assets->apple_menu);
		double scale = scaled_i(16, s) / sw;
		cairo_scale(cr, scale, scale);
		cairo_set_source_surface(cr, state->assets->apple_menu, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
	} else {
		set_source_rgba255(cr, 255, 255, 255, 0.95);
		cairo_arc(cr, scaled_i(36, s), scaled_i(16, s), scaled_i(7, s),
			0, 2.0 * M_PI);
		cairo_fill(cr);
		cairo_save(cr);
		cairo_translate(cr, scaled_i(42, s), scaled_i(8, s));
		cairo_rotate(cr, -0.5);
		cairo_ellipse(cr, 0, 0, scaled_i(4, s), scaled_i(7, s));
		cairo_restore(cr);
	}

	const char *menus[] = {
		"Finder", "File", "Edit", "View", "Go", "Window", "Help",
	};
	double x = scaled_i(76, s);
	for (size_t i = 0; i < sizeof(menus) / sizeof(menus[0]); i++) {
		draw_text(cr, menus[i], x, scaled_i(22, s), 15 * s,
			255, 255, 255, 0.95, i == 0);
		x += scaled_i(48, s) + strlen(menus[i]) * 7.1 * s;
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

	draw_text(cr, clock_text, layout->width - scaled_i(195, s), scaled_i(22, s), 15 * s,
		255, 255, 255, 0.95, true);
	draw_text(cr, "⌕", layout->width - scaled_i(310, s), scaled_i(22, s),
		17 * s, 255, 255, 255, 0.86, true);
	draw_text(cr, "◒  Wi-Fi  ◧", layout->width - scaled_i(430, s), scaled_i(22, s), 14 * s,
		255, 255, 255, 0.88, true);
}

static void draw_calendar_widget(cairo_t *cr,
		const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state) {
	double s = layout_scale(layout);
	struct tahoe_rect r = layout->calendar_widget;
	draw_liquid_panel(cr, &r, 30 * s, 0.86);

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
	draw_text(cr, month, r.x + scaled_i(27, s), r.y + scaled_i(38, s),
		14 * s, 238, 70, 76, 1.0, true);

	const char *days = "S   M   T   W   T   F   S";
	draw_text(cr, days, r.x + scaled_i(28, s), r.y + scaled_i(66, s),
		13 * s, 43, 44, 48, 0.78, true);

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

	int cell = scaled_i(27, s);
	int start_x = r.x + scaled_i(27, s);
	int start_y = r.y + scaled_i(92, s);
	for (int d = 1; d <= days_in_month; d++) {
		int n = first_wday + d - 1;
		int col = n % 7;
		int row = n / 7;
		int cx = start_x + col * cell + 8;
		int cy = start_y + row * cell;
		if (d == local.tm_mday) {
			set_source_rgba255(cr, 242, 78, 79, 1.0);
			cairo_arc(cr, cx + scaled_i(5, s), cy - scaled_i(4, s),
				scaled_i(13, s), 0, 2.0 * M_PI);
			cairo_fill(cr);
			draw_text(cr, "", 0, 0, 1, 0, 0, 0, 0, false);
			char day_text[16];
			snprintf(day_text, sizeof(day_text), "%d", d);
			draw_text(cr, day_text, cx - scaled_i(d >= 10 ? 4 : 0, layout_scale(layout)),
				cy + scaled_i(1, layout_scale(layout)),
				14 * layout_scale(layout), 255, 255, 255, 1.0, true);
		} else {
			char day_text[16];
			snprintf(day_text, sizeof(day_text), "%d", d);
			draw_text(cr, day_text, cx - scaled_i(d >= 10 ? 4 : 0, layout_scale(layout)),
				cy + scaled_i(1, layout_scale(layout)),
				14 * layout_scale(layout), 36, 37, 41, 0.86, true);
		}
	}
}

static void draw_weather_widget(cairo_t *cr,
		const struct tahoe_shell_layout *layout) {
	double s = layout_scale(layout);
	struct tahoe_rect r = layout->weather_widget;
	draw_liquid_panel(cr, &r, 32 * s, 0.24);

	cairo_pattern_t *shade = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.06, 0.06, 0.15, 0.86);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.10, 0.14, 0.27, 0.74);
	rounded_rect(cr, r.x, r.y, r.width, r.height, 32 * s);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	draw_text(cr, "Memphis", r.x + scaled_i(25, s), r.y + scaled_i(38, s),
		20 * s, 255, 255, 255, 0.96, true);
	draw_text(cr, "79°", r.x + scaled_i(25, s), r.y + scaled_i(96, s),
		58 * s, 255, 255, 255, 0.97, false);
	draw_text(cr, "☾", r.x + scaled_i(25, s), r.y + r.height - scaled_i(51, s), 26 * s,
		255, 255, 255, 0.96, true);
	draw_text(cr, "Air quality alert", r.x + scaled_i(25, s),
		r.y + r.height - scaled_i(27, s), 17 * s,
		255, 255, 255, 0.88, true);
}

static void draw_finder_icon(cairo_t *cr, struct tahoe_rect r) {
	cairo_save(cr);
	cairo_translate(cr, r.x, r.y);
	cairo_scale(cr, r.width / 64.0, r.height / 64.0);
	r = (struct tahoe_rect){0, 0, 64, 64};
	cairo_pattern_t *g = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgb(g, 0, 0.10, 0.72, 1.0);
	cairo_pattern_add_color_stop_rgb(g, 1, 0.65, 0.90, 1.0);
	rounded_rect(cr, r.x, r.y, r.width, r.height, 14);
	cairo_set_source(cr, g);
	cairo_fill(cr);
	cairo_pattern_destroy(g);
	set_source_rgba255(cr, 246, 253, 255, 0.94);
	rounded_rect(cr, r.x + r.width * 0.48, r.y + 3, r.width * 0.45, r.height - 6, 11);
	cairo_fill(cr);
	set_source_rgba255(cr, 10, 70, 130, 0.9);
	cairo_set_line_width(cr, 2);
	cairo_move_to(cr, r.x + 21, r.y + 21);
	cairo_line_to(cr, r.x + 20, r.y + 31);
	cairo_move_to(cr, r.x + 43, r.y + 21);
	cairo_line_to(cr, r.x + 42, r.y + 31);
	cairo_move_to(cr, r.x + 23, r.y + 43);
	cairo_curve_to(cr, r.x + 34, r.y + 50, r.x + 43, r.y + 49, r.x + 52, r.y + 41);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_symbol_icon(cairo_t *cr, struct tahoe_rect r, int index) {
	cairo_save(cr);
	cairo_translate(cr, r.x, r.y);
	cairo_scale(cr, r.width / 64.0, r.height / 64.0);
	r = (struct tahoe_rect){0, 0, 64, 64};
	int palette[][3] = {
		{245, 245, 246}, {45, 139, 239}, {58, 212, 93}, {42, 150, 255},
		{255, 204, 64}, {246, 75, 83}, {36, 214, 93}, {96, 211, 105},
		{230, 236, 240}, {244, 245, 246}, {248, 238, 82}, {255, 236, 122},
		{28, 28, 32}, {246, 55, 87}, {255, 92, 54}, {36, 130, 235},
		{108, 120, 224}, {210, 213, 216}, {78, 146, 213}, {230, 230, 229},
	};
	int *p = palette[index % 20];
	cairo_pattern_t *g = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(g, 0, p[0] / 255.0 + 0.05, p[1] / 255.0 + 0.05,
		p[2] / 255.0 + 0.05, 1.0);
	cairo_pattern_add_color_stop_rgba(g, 1, p[0] / 255.0 * 0.75, p[1] / 255.0 * 0.75,
		p[2] / 255.0 * 0.75, 1.0);
	rounded_rect(cr, r.x, r.y, r.width, r.height, 14);
	cairo_set_source(cr, g);
	cairo_fill(cr);
	cairo_pattern_destroy(g);

	set_source_rgba255(cr, 255, 255, 255, 0.86);
	cairo_set_line_width(cr, 4.0);
	double cx = r.x + r.width / 2.0;
	double cy = r.y + r.height / 2.0;
	switch (index % 10) {
	case 0:
		for (int i = 0; i < 9; i++) {
			cairo_rectangle(cr, r.x + 17 + (i % 3) * 12,
				r.y + 17 + (i / 3) * 12, 7, 7);
			cairo_fill(cr);
		}
		break;
	case 1:
		cairo_arc(cr, cx, cy, 20, 0, 2.0 * M_PI);
		cairo_stroke(cr);
		cairo_move_to(cr, cx, cy);
		cairo_line_to(cr, cx + 14, cy - 15);
		cairo_stroke(cr);
		break;
	case 2:
		rounded_rect(cr, r.x + 13, r.y + 18, r.width - 26, r.height - 28, 10);
		cairo_stroke(cr);
		cairo_move_to(cr, r.x + 16, r.y + 26);
		cairo_line_to(cr, r.x + r.width / 2, r.y + 39);
		cairo_line_to(cr, r.x + r.width - 16, r.y + 26);
		cairo_stroke(cr);
		break;
	case 3:
		cairo_arc(cr, cx, cy, 19, 0, 2.0 * M_PI);
		cairo_stroke(cr);
		cairo_move_to(cr, cx - 13, cy + 10);
		cairo_line_to(cr, cx + 15, cy - 12);
		cairo_stroke(cr);
		break;
	case 4:
		for (int i = 0; i < 8; i++) {
			double a = i * M_PI / 4.0;
			cairo_arc(cr, cx + cos(a) * 13, cy + sin(a) * 13, 8, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		break;
	case 5:
		rounded_rect(cr, r.x + 15, r.y + 18, r.width - 30, r.height - 30, 8);
		cairo_stroke(cr);
		cairo_rectangle(cr, r.x + 22, r.y + 48, 20, 4);
		cairo_fill(cr);
		break;
	case 6:
		cairo_arc(cr, cx, cy, 19, -0.8, 0.8);
		cairo_stroke(cr);
		cairo_move_to(cr, cx - 13, cy - 14);
		cairo_curve_to(cr, cx - 25, cy + 1, cx - 9, cy + 21, cx + 10, cy + 16);
		cairo_stroke(cr);
		break;
	case 7:
		draw_text(cr, "11", r.x + 19, r.y + 47, 30, 60, 60, 62, 0.95, true);
		break;
	case 8:
		cairo_move_to(cr, cx, r.y + 15);
		cairo_line_to(cr, r.x + 48, r.y + 50);
		cairo_line_to(cr, r.x + 16, r.y + 50);
		cairo_close_path(cr);
		cairo_stroke(cr);
		break;
	default:
		cairo_arc(cr, cx, cy, 18, 0, 2.0 * M_PI);
		cairo_stroke(cr);
		cairo_move_to(cr, cx - 18, cy);
		cairo_line_to(cr, cx + 18, cy);
		cairo_move_to(cr, cx, cy - 18);
		cairo_line_to(cr, cx, cy + 18);
		cairo_stroke(cr);
		break;
	}
	cairo_restore(cr);
}

static void draw_dock(cairo_t *cr, const struct tahoe_shell_layout *layout,
		const struct tahoe_shell_state *state) {
	double s = layout_scale(layout);
	struct tahoe_rect r = layout->dock;
	set_source_rgba255(cr, 0, 0, 0, 0.18);
	rounded_rect(cr, r.x + scaled_i(2, s), r.y + scaled_i(7, s),
		r.width, r.height, 30 * s);
	cairo_fill(cr);
	draw_liquid_panel(cr, &r, 30 * s, 0.34);

	for (int i = 0; i < layout->dock_item_count; i++) {
		struct tahoe_rect item = layout->dock_items[i];
		if (i == state->hot_dock_index) {
			set_source_rgba255(cr, 255, 255, 255, 0.18);
			cairo_arc(cr, item.x + item.width / 2.0, item.y + item.height / 2.0,
				item.width * 0.64, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		if (state->assets != NULL &&
				i < state->assets->dock_icon_count &&
				state->assets->dock_icons[i] != NULL) {
			draw_image_cover(cr, state->assets->dock_icons[i], item);
		} else if (i == 0) {
			draw_finder_icon(cr, item);
		} else {
			draw_symbol_icon(cr, item, i);
		}
		if (i < layout->dock_item_count - 2) {
			set_source_rgba255(cr, 34, 37, 42, 0.70);
			cairo_arc(cr, item.x + item.width / 2.0,
				r.y + r.height - scaled_i(11, s),
				2.1 * s, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		if (i == layout->dock_item_count - 3) {
			set_source_rgba255(cr, 30, 34, 38, 0.32);
			cairo_rectangle(cr, item.x + item.width + scaled_i(16, s),
				r.y + scaled_i(17, s), fmax(1.0, 1.5 * s),
				r.height - scaled_i(34, s));
			cairo_fill(cr);
		}
	}
}

static void draw_desktop_items(cairo_t *cr,
		const struct tahoe_shell_layout *layout) {
	for (int i = 0; i < layout->desktop_item_count; i++) {
		struct tahoe_rect r = layout->desktop_items[i];
		if (i == 0) {
			rounded_rect(cr, r.x + 22, r.y, 66, 46, 9);
			cairo_pattern_t *g = cairo_pattern_create_linear(r.x, r.y, r.x, r.y + 46);
			cairo_pattern_add_color_stop_rgb(g, 0, 0.78, 0.86, 0.96);
			cairo_pattern_add_color_stop_rgb(g, 1, 0.20, 0.46, 0.66);
			cairo_set_source(cr, g);
			cairo_fill(cr);
			cairo_pattern_destroy(g);
			set_source_rgba255(cr, 255, 255, 255, 0.8);
			cairo_move_to(cr, r.x + 27, r.y + 36);
			cairo_line_to(cr, r.x + 50, r.y + 18);
			cairo_line_to(cr, r.x + 68, r.y + 35);
			cairo_line_to(cr, r.x + 86, r.y + 16);
			cairo_stroke(cr);
		} else {
			for (int s = 0; s < 3; s++) {
				rounded_rect(cr, r.x + 36 + s * 3, r.y + s * 5, 49, 58, 5);
				set_source_rgba255(cr, 249, 250, 252, 0.94);
				cairo_fill(cr);
				set_source_rgba255(cr, 55, 120, 180, 0.62);
				cairo_rectangle(cr, r.x + 44 + s * 3, r.y + 13 + s * 5, 32, 3);
				cairo_fill(cr);
				set_source_rgba255(cr, 130, 138, 146, 0.42);
				for (int line = 0; line < 5; line++) {
					cairo_rectangle(cr, r.x + 44 + s * 3,
						r.y + 23 + s * 5 + line * 6, 31, 2);
					cairo_fill(cr);
				}
			}
		}
		draw_text(cr, tahoe_shell_desktop_label(i), r.x, r.y + r.height - 4,
			15, 255, 255, 255, 0.95, true);
	}
}

static void draw_apple_menu(cairo_t *cr,
		const struct tahoe_shell_layout *layout) {
	double s = layout_scale(layout);
	if (layout->apple_menu_item_count == 0) {
		return;
	}

	struct tahoe_rect panel = layout->apple_menu_panel;
	draw_liquid_panel(cr, &panel, 14 * s, 0.70);
	for (int i = 0; i < layout->apple_menu_item_count; i++) {
		struct tahoe_rect item = layout->apple_menu_items[i];
		if (i == 0 || i == 4) {
			set_source_rgba255(cr, 42, 42, 46, 0.18);
			cairo_rectangle(cr, panel.x + scaled_i(12, s),
				item.y + item.height - 1,
				panel.width - scaled_i(24, s), 1);
			cairo_fill(cr);
		}
		draw_text(cr, tahoe_shell_menu_label(i),
			item.x + scaled_i(14, s),
			item.y + scaled_i(23, s),
			14 * s, 23, 26, 30, 0.92, i == 0);
	}
}

void tahoe_shell_layout_compute(
		int width,
		int height,
		bool apple_menu_open,
		struct tahoe_shell_layout *layout) {
	memset(layout, 0, sizeof(*layout));
	double s = ui_scale_for_size(width, height);
	layout->width = width;
	layout->height = height;
	layout->menu_bar = (struct tahoe_rect){0, 0, width, scaled_i(32, s)};
	layout->apple_menu_button = (struct tahoe_rect){
		scaled_i(18, s), 0, scaled_i(43, s), layout->menu_bar.height};

	layout->calendar_widget = (struct tahoe_rect){
		scaled_i(23, s), scaled_i(66, s), scaled_i(233, s), scaled_i(234, s)};
	layout->weather_widget = (struct tahoe_rect){
		scaled_i(272, s), scaled_i(66, s), scaled_i(232, s), scaled_i(232, s)};

	layout->desktop_item_count = 2;
	int item_x = width - scaled_i(164, s);
	layout->desktop_items[0] = (struct tahoe_rect){
		item_x, scaled_i(74, s), scaled_i(126, s), scaled_i(106, s)};
	layout->desktop_items[1] = (struct tahoe_rect){
		item_x - scaled_i(5, s), scaled_i(222, s),
		scaled_i(138, s), scaled_i(116, s)};

	int icon = scaled_i(64, s);
	int gap = scaled_i(26, s);
	int dock_count = (int)(sizeof(dock_launchers) / sizeof(dock_launchers[0]));
	int dock_width = dock_count * icon + (dock_count - 1) * gap + scaled_i(44, s);
	while (dock_width > width - scaled_i(136, s) && icon > scaled_i(42, s)) {
		icon -= 2;
		gap = gap > 6 ? gap - 1 : gap;
		dock_width = dock_count * icon + (dock_count - 1) * gap + scaled_i(44, s);
	}
	layout->dock_item_count = dock_count;
	layout->dock = (struct tahoe_rect){
		(width - dock_width) / 2,
		height - icon - scaled_i(42, s),
		dock_width,
		icon + scaled_i(36, s),
	};
	int x = layout->dock.x + scaled_i(22, s);
	int y = layout->dock.y + scaled_i(20, s);
	for (int i = 0; i < dock_count; i++) {
		layout->dock_items[i] = (struct tahoe_rect){x, y, icon, icon};
		x += icon + gap;
	}

	if (apple_menu_open) {
		layout->apple_menu_item_count =
			(int)(sizeof(menu_labels) / sizeof(menu_labels[0]));
		layout->apple_menu_panel = (struct tahoe_rect){
			scaled_i(18, s), layout->menu_bar.height + scaled_i(4, s),
			scaled_i(246, s),
			scaled_i(18, s) + layout->apple_menu_item_count * scaled_i(34, s)};
		for (int i = 0; i < layout->apple_menu_item_count; i++) {
			layout->apple_menu_items[i] = (struct tahoe_rect){
				layout->apple_menu_panel.x + scaled_i(8, s),
				layout->apple_menu_panel.y + scaled_i(9, s) + i * scaled_i(34, s),
				layout->apple_menu_panel.width - scaled_i(16, s),
				scaled_i(32, s),
			};
		}
	}
}

struct tahoe_shell_hit tahoe_shell_hit_test(
		const struct tahoe_shell_layout *layout,
		int x,
		int y) {
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
	for (int i = 0; i < layout->desktop_item_count; i++) {
		if (rect_contains(&layout->desktop_items[i], x, y)) {
			return (struct tahoe_shell_hit){TAHOE_HIT_DESKTOP_ITEM, i};
		}
	}
	return (struct tahoe_shell_hit){TAHOE_HIT_NONE, -1};
}

void tahoe_shell_draw(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		const struct tahoe_shell_state *state) {
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_t *cr = cairo_create(surface);

	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(width, height, state->apple_menu_open, &layout);

	draw_wallpaper(cr, width, height, state->assets);
	draw_menu_bar(cr, &layout, state);
	draw_calendar_widget(cr, &layout, state);
	draw_weather_widget(cr, &layout);
	draw_desktop_items(cr, &layout);
	draw_dock(cr, &layout, state);
	if (state->apple_menu_open) {
		draw_apple_menu(cr, &layout);
	}

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

const char *tahoe_shell_desktop_label(int index) {
	if (index < 0 ||
			index >= (int)(sizeof(desktop_launchers) / sizeof(desktop_launchers[0]))) {
		return NULL;
	}
	return desktop_launchers[index].label;
}

const char *tahoe_shell_desktop_command(int index) {
	if (index < 0 ||
			index >= (int)(sizeof(desktop_launchers) / sizeof(desktop_launchers[0]))) {
		return NULL;
	}
	return desktop_launchers[index].command;
}

const char *tahoe_shell_menu_label(int index) {
	if (index < 0 || index >= (int)(sizeof(menu_labels) / sizeof(menu_labels[0]))) {
		return NULL;
	}
	return menu_labels[index];
}
