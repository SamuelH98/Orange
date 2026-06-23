#include "orange/widgets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "orange/glass.h"
#include "orange/util.h"

static double widget_layout_scale(const struct orange_shell_layout *layout) {
	if (layout == NULL || layout->width <= 0 || layout->height <= 0) {
		return 1.0;
	}
	double sx = (double)layout->width / 1440.0;
	double sy = (double)layout->height / 900.0;
	double s = sx < sy ? sx : sy;
	if (s < 0.55) {
		s = 0.55;
	}
	if (s > 2.2) {
		s = 2.2;
	}
	return s;
}

static int widget_scaled_i(int value, double scale) {
	return (int)lrint((double)value * scale);
}

static bool widget_dark_config(const struct orange_config *config) {
	return config != NULL &&
		config->appearance == ORANGE_APPEARANCE_DARK;
}

static void widget_rounded_rect(cairo_t *cr,
		double x,
		double y,
		double width,
		double height,
		double radius) {
	double r = clamp(radius, 0.0, fmin(width, height) / 2.0);
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + width - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_arc(cr, x + width - r, y + height - r, r, 0.0, M_PI / 2.0);
	cairo_arc(cr, x + r, y + height - r, r, M_PI / 2.0, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static void widget_draw_image_fit(cairo_t *cr,
		cairo_surface_t *surface,
		struct orange_rect rect,
		double opacity) {
	if (surface == NULL) {
		return;
	}
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0 || rect.width <= 0 || rect.height <= 0) {
		return;
	}
	double scale = fmin((double)rect.width / (double)sw,
		(double)rect.height / (double)sh);
	double tx = rect.x + ((double)rect.width - sw * scale) * 0.5;
	double ty = rect.y + ((double)rect.height - sh * scale) * 0.5;
	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint_with_alpha(cr, opacity);
	cairo_restore(cr);
}

static void widget_draw_liquid_panel(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		double alpha,
		bool dark) {
	orange_glass_draw(cr, rect, radius, dark);

	cairo_save(cr);
	widget_rounded_rect(cr, rect->x, rect->y,
		rect->width, rect->height, radius);
	if (dark) {
		set_source_rgba255(cr, 11, 13, 18, alpha);
	} else {
		set_source_rgba255(cr, 255, 255, 255, alpha);
	}
	cairo_fill_preserve(cr);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.18 : 0.38);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	widget_rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.08 : 0.13);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static double widget_text_width(cairo_t *cr,
		const char *text,
		double size,
		bool bold) {
	if (text == NULL || text[0] == '\0') {
		return 0.0;
	}
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, text, &extents);
	return extents.x_advance;
}

static void widget_fit_text(cairo_t *cr,
		const char *text,
		double max_width,
		double size,
		bool bold,
		char *out,
		size_t out_size) {
	if (out == NULL || out_size == 0) {
		return;
	}
	snprintf(out, out_size, "%s", text != NULL ? text : "");
	if (widget_text_width(cr, out, size, bold) <= max_width) {
		return;
	}
	const char *suffix = "...";
	size_t suffix_len = strlen(suffix);
	size_t len = strlen(out);
	while (len > 0) {
		if (len + suffix_len + 1 <= out_size) {
			memcpy(out + len, suffix, suffix_len + 1);
		}
		if (widget_text_width(cr, out, size, bold) <= max_width) {
			return;
		}
		len--;
		out[len] = '\0';
	}
	snprintf(out, out_size, "%s", suffix);
}

static const struct orange_widget_data *widget_data_for_state(
		const struct orange_shell_state *state) {
	return state != NULL ? state->widget_data : NULL;
}

static int widget_days_in_month(const struct tm *local) {
	static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
	int result = days[local->tm_mon];
	if (result == 28) {
		int year = local->tm_year + 1900;
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
			result = 29;
		}
	}
	return result;
}

static void uppercase_ascii(char *text) {
	for (char *p = text; p != NULL && *p != '\0'; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p = (char)(*p - 'a' + 'A');
		}
	}
}

static void draw_weather_crescent(cairo_t *cr,
		double cx,
		double cy,
		double radius) {
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

static void draw_calendar_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = widget_layout_scale(layout);
	bool dark = widget_dark_config(config);
	struct orange_rect r = layout->calendar_widget;
	widget_draw_liquid_panel(cr, &r, 30 * s, dark ? 0.58 : 0.86, dark);
	int primary = dark ? 235 : 36;

	time_t now = state != NULL && state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);

	char month[32];
	strftime(month, sizeof(month), "%B", &local);
	uppercase_ascii(month);
	draw_text(cr, month, r.x + widget_scaled_i(36, s),
		r.y + widget_scaled_i(59, s),
		22 * s, 238, 70, 76, 1.0, true);

	draw_text(cr, "S   M   T   W   T   F   S",
		r.x + widget_scaled_i(46, s), r.y + widget_scaled_i(101, s),
		22 * s, primary, primary, primary + (dark ? 5 : 12), 0.78, true);

	struct tm first = local;
	first.tm_mday = 1;
	mktime(&first);
	int first_wday = first.tm_wday;
	int days_in_month = widget_days_in_month(&local);
	int cell = widget_scaled_i(40, s);
	int start_x = r.x + widget_scaled_i(24, s);
	int start_y = r.y + widget_scaled_i(126, s);
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
				widget_scaled_i(18, s), 0, 2.0 * M_PI);
			cairo_fill(cr);
			draw_text(cr, day_text, text_x, text_y,
				22 * s, 255, 255, 255, 1.0, true);
		} else {
			draw_text(cr, day_text, text_x, text_y,
				22 * s, primary, primary, primary + (dark ? 5 : 12),
				0.86, true);
		}
	}

	const struct orange_widget_data *widgets = widget_data_for_state(state);
	if (widgets != NULL && widgets->calendar.event_count > 0) {
		const struct orange_calendar_event *event =
			&widgets->calendar.events[0];
		char line[ORANGE_WIDGET_TEXT_MAX + ORANGE_WIDGET_LABEL_MAX + 4];
		snprintf(line, sizeof(line), "%s  %s",
			event->time_label, event->title);
		char fitted[sizeof(line)];
		widget_fit_text(cr, line, r.width - widget_scaled_i(60, s),
			18 * s, true, fitted, sizeof(fitted));
		draw_text(cr, fitted, r.x + widget_scaled_i(32, s),
			r.y + r.height - widget_scaled_i(31, s),
			18 * s, primary, primary, primary + (dark ? 5 : 12),
			0.82, true);
	}
}

static void draw_weather_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	double s = widget_layout_scale(layout);
	(void)config;
	struct orange_rect r = layout->weather_widget;
	widget_draw_liquid_panel(cr, &r, 32 * s, 0.24, true);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.06, 0.06, 0.15, 0.86);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.10, 0.14, 0.27, 0.74);
	widget_rounded_rect(cr, r.x, r.y, r.width, r.height, 32 * s);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	const struct orange_widget_data *widgets = widget_data_for_state(state);
	const struct orange_weather_data *weather =
		widgets != NULL ? &widgets->weather : NULL;
	const char *location = weather != NULL && weather->location[0] != '\0' ?
		weather->location : "Weather";
	const char *temperature = weather != NULL &&
		weather->temperature[0] != '\0' ? weather->temperature : "--";
	const char *condition = weather != NULL &&
		weather->condition[0] != '\0' ?
		weather->condition : "Current weather unavailable";
	const char *icon_name = weather != NULL && weather->icon_name[0] != '\0' ?
		weather->icon_name : "weather-none-available";
	char location_fit[ORANGE_WIDGET_TEXT_MAX];
	char condition_fit[ORANGE_WIDGET_TEXT_MAX];
	widget_fit_text(cr, location, r.width - widget_scaled_i(94, s),
		32 * s, true, location_fit, sizeof(location_fit));
	widget_fit_text(cr, condition, r.width - widget_scaled_i(94, s),
		26 * s, true, condition_fit, sizeof(condition_fit));

	draw_text(cr, location_fit, r.x + widget_scaled_i(47, s),
		r.y + widget_scaled_i(61, s),
		32 * s, 255, 255, 255, 0.96, true);
	draw_text(cr, temperature, r.x + widget_scaled_i(47, s),
		r.y + widget_scaled_i(151, s),
		90 * s, 255, 255, 255, 0.97, false);
	cairo_surface_t *weather_icon = state != NULL && state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT,
			icon_name) : NULL;
	if (weather_icon != NULL) {
		widget_draw_image_fit(cr, weather_icon,
			(struct orange_rect){
				r.x + widget_scaled_i(50, s),
				r.y + r.height - widget_scaled_i(86, s),
				widget_scaled_i(38, s),
				widget_scaled_i(38, s),
			},
			0.95);
	} else {
		draw_weather_crescent(cr,
			r.x + widget_scaled_i(69, s),
			r.y + r.height - widget_scaled_i(67, s),
			widget_scaled_i(16, s));
	}
	draw_text(cr, condition_fit, r.x + widget_scaled_i(47, s),
		r.y + r.height - widget_scaled_i(34, s), 26 * s,
		255, 255, 255, 0.88, true);
}

static void draw_widget_card_background(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		bool dark) {
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.22 : 0.12);
	widget_rounded_rect(cr, rect->x + 1.5, rect->y + 3.0,
		rect->width, rect->height, radius);
	cairo_fill(cr);
	widget_draw_liquid_panel(cr, rect, radius, dark ? 0.62 : 0.84, dark);
}

static void draw_notification_calendar_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = widget_layout_scale(layout);
	bool dark = widget_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 188 : 88;
	time_t now = state != NULL && state->now != 0 ? state->now : time(NULL);
	struct tm local;
	localtime_r(&now, &local);

	draw_widget_card_background(cr, &r, 24 * s, dark);
	draw_text(cr, "Calendar", r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(36, s),
		22 * s, primary, primary, primary, 0.96, true);
	draw_text(cr, "Today", r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(63, s),
		17 * s, 238, 70, 76, 0.95, true);

	const struct orange_widget_data *widgets = widget_data_for_state(state);
	const struct orange_calendar_data *calendar =
		widgets != NULL ? &widgets->calendar : NULL;
	int event_count = calendar != NULL ? calendar->event_count : 0;
	if (event_count <= 0) {
		draw_text(cr, "No more events today",
			r.x + widget_scaled_i(20, s), r.y + widget_scaled_i(99, s),
			18 * s, primary, primary, primary, 0.88, true);
		const char *source = calendar != NULL && calendar->source[0] != '\0' ?
			calendar->source : "GNOME Calendar";
		char source_fit[ORANGE_WIDGET_TEXT_MAX];
		widget_fit_text(cr, source, r.width - widget_scaled_i(210, s),
			16 * s, false, source_fit, sizeof(source_fit));
		draw_text(cr, source_fit,
			r.x + widget_scaled_i(20, s), r.y + widget_scaled_i(124, s),
			16 * s, secondary, secondary, secondary, 0.82, false);
	} else {
		int lines = event_count > 2 ? 2 : event_count;
		for (int i = 0; i < lines; i++) {
			const struct orange_calendar_event *event =
				&calendar->events[i];
			char line[ORANGE_WIDGET_TEXT_MAX + ORANGE_WIDGET_LABEL_MAX + 4];
			snprintf(line, sizeof(line), "%s  %s",
				event->time_label, event->title);
			char fitted[sizeof(line)];
			widget_fit_text(cr, line, r.width - widget_scaled_i(210, s),
				18 * s, false, fitted, sizeof(fitted));
			draw_text(cr, fitted,
				r.x + widget_scaled_i(20, s),
				r.y + widget_scaled_i(96 + i * 28, s),
				18 * s, primary, primary, primary,
				i == 0 ? 0.90 : 0.86, false);
		}
	}

	int mini_w = widget_scaled_i(156, s);
	int mini_x = r.x + r.width - mini_w - widget_scaled_i(18, s);
	int mini_y = r.y + widget_scaled_i(42, s);
	char month[24];
	strftime(month, sizeof(month), "%b", &local);
	uppercase_ascii(month);
	draw_text(cr, month, mini_x, r.y + widget_scaled_i(35, s),
		14 * s, 238, 70, 76, 0.95, true);
	int cell = widget_scaled_i(20, s);
	int days = widget_days_in_month(&local);
	for (int day = 1; day <= days; day++) {
		int col = (day - 1) % 7;
		int row = (day - 1) / 7;
		int cx = mini_x + col * cell + cell / 2;
		int cy = mini_y + row * cell;
		if (day == local.tm_mday) {
			set_source_rgba255(cr, 242, 78, 79, 1.0);
			cairo_arc(cr, cx, cy, widget_scaled_i(8, s),
				0, 2.0 * M_PI);
			cairo_fill(cr);
		}
		char text[16];
		snprintf(text, sizeof(text), "%d", day);
		draw_text(cr, text, cx - widget_scaled_i(day < 10 ? 3 : 7, s),
			cy + widget_scaled_i(5, s), 10 * s,
			day == local.tm_mday ? 255 : secondary,
			day == local.tm_mday ? 255 : secondary,
			day == local.tm_mday ? 255 : secondary,
			0.90, true);
	}
}

static void draw_notification_screen_time_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = widget_layout_scale(layout);
	bool dark = widget_dark_config(config);
	int primary = dark ? 244 : 28;
	int secondary = dark ? 188 : 88;
	const struct orange_widget_data *widgets = widget_data_for_state(state);
	const struct orange_screen_time_data *screen =
		widgets != NULL ? &widgets->screen_time : NULL;
	const char *duration = screen != NULL &&
		screen->duration_label[0] != '\0' ? screen->duration_label : "--";
	const char *detail = screen != NULL && screen->detail[0] != '\0' ?
		screen->detail : "Session time unavailable";
	uint64_t seconds = screen != NULL ? screen->seconds : 0;

	draw_widget_card_background(cr, &r, 24 * s, dark);
	draw_text(cr, "Screen Time", r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(34, s),
		19 * s, secondary, secondary, secondary, 0.84, true);
	draw_text(cr, duration, r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(78, s),
		40 * s, primary, primary, primary, 0.96, false);
	draw_text(cr, detail, r.x + widget_scaled_i(22, s),
		r.y + widget_scaled_i(111, s),
		16 * s, secondary, secondary, secondary, 0.82, false);

	int graph_x = r.x + widget_scaled_i(174, s);
	int graph_y = r.y + widget_scaled_i(58, s);
	int graph_w = r.width - widget_scaled_i(214, s);
	int graph_h = widget_scaled_i(20, s);
	widget_rounded_rect(cr, graph_x, graph_y, graph_w, graph_h,
		graph_h / 2.0);
	set_source_rgba255(cr, dark ? 255 : 30, dark ? 255 : 34,
		dark ? 255 : 38, dark ? 0.10 : 0.08);
	cairo_fill(cr);
	double ratio = (double)seconds / (12.0 * 60.0 * 60.0);
	if (ratio < 0.02 && seconds > 0) {
		ratio = 0.02;
	}
	if (ratio > 1.0) {
		ratio = 1.0;
	}
	widget_rounded_rect(cr, graph_x, graph_y, graph_w * ratio, graph_h,
		graph_h / 2.0);
	set_source_rgba255(cr, 31, 138, 255, 0.92);
	cairo_fill(cr);
}

static void draw_notification_weather_widget(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		struct orange_rect r) {
	double s = widget_layout_scale(layout);
	(void)config;

	draw_widget_card_background(cr, &r, 24 * s, true);
	cairo_pattern_t *shade = cairo_pattern_create_linear(
		r.x, r.y, r.x, r.y + r.height);
	cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.16, 0.45, 0.83, 0.86);
	cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.05, 0.11, 0.32, 0.82);
	widget_rounded_rect(cr, r.x, r.y, r.width, r.height, 24 * s);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	const struct orange_widget_data *widgets = widget_data_for_state(state);
	const struct orange_weather_data *weather =
		widgets != NULL ? &widgets->weather : NULL;
	const char *location = weather != NULL && weather->location[0] != '\0' ?
		weather->location : "Weather";
	const char *temperature = weather != NULL &&
		weather->temperature[0] != '\0' ? weather->temperature : "--";
	const char *condition = weather != NULL &&
		weather->condition[0] != '\0' ?
		weather->condition : "Current weather unavailable";
	const char *icon_name = weather != NULL && weather->icon_name[0] != '\0' ?
		weather->icon_name : "weather-none-available";
	char location_fit[ORANGE_WIDGET_TEXT_MAX];
	char condition_fit[ORANGE_WIDGET_TEXT_MAX];
	widget_fit_text(cr, location, r.width - widget_scaled_i(40, s),
		25 * s, true, location_fit, sizeof(location_fit));
	widget_fit_text(cr, condition, widget_scaled_i(160, s),
		17 * s, true, condition_fit, sizeof(condition_fit));

	draw_text(cr, "Weather", r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(35, s),
		18 * s, 255, 255, 255, 0.78, true);
	draw_text(cr, location_fit, r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(66, s),
		25 * s, 255, 255, 255, 0.96, true);
	draw_text(cr, temperature, r.x + widget_scaled_i(20, s),
		r.y + widget_scaled_i(125, s),
		56 * s, 255, 255, 255, 0.97, false);
	cairo_surface_t *weather_icon = state != NULL && state->assets != NULL ?
		orange_assets_icon(state->assets, ORANGE_ASSET_ICON_LIGHT,
			icon_name) : NULL;
	if (weather_icon != NULL) {
		widget_draw_image_fit(cr, weather_icon,
			(struct orange_rect){
				r.x + r.width - widget_scaled_i(82, s),
				r.y + widget_scaled_i(42, s),
				widget_scaled_i(52, s),
				widget_scaled_i(52, s),
			},
			0.95);
	} else {
		draw_weather_crescent(cr,
			r.x + r.width - widget_scaled_i(55, s),
			r.y + widget_scaled_i(70, s),
			widget_scaled_i(19, s));
	}
	draw_text(cr, condition_fit,
		r.x + r.width - widget_scaled_i(156, s),
		r.y + r.height - widget_scaled_i(26, s), 17 * s,
		255, 255, 255, 0.84, true);
}

void orange_widgets_draw_desktop_layer(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	for (int i = 0; layout != NULL && i < layout->widget_count; i++) {
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

void orange_widgets_draw_notification_card(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config,
		enum orange_notification_center_card_kind kind,
		struct orange_rect rect) {
	switch (kind) {
	case ORANGE_NOTIFICATION_CENTER_CARD_CALENDAR_WIDGET:
		draw_notification_calendar_widget(cr, layout, state, config, rect);
		break;
	case ORANGE_NOTIFICATION_CENTER_CARD_SCREEN_TIME_WIDGET:
		draw_notification_screen_time_widget(cr, layout, state, config, rect);
		break;
	case ORANGE_NOTIFICATION_CENTER_CARD_WEATHER_WIDGET:
		draw_notification_weather_widget(cr, layout, state, config, rect);
		break;
	case ORANGE_NOTIFICATION_CENTER_CARD_EMPTY:
	case ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION:
		break;
	}
}
