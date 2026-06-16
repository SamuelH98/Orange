#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <sys/sysinfo.h>

#define ABOUT_DESIGN_WIDTH 560
#define ABOUT_DESIGN_HEIGHT 974
#define ABOUT_MAX_SCALE 0.72
#ifndef ORANGE_PROJECT_VERSION
#define ORANGE_PROJECT_VERSION "0.1.0"
#endif
#define TRAFFIC_LIGHT_DESIGN_SIZE 16
#define TRAFFIC_LIGHT_DESIGN_Y 18
#define TRAFFIC_LIGHT_1_X 18
#define TRAFFIC_LIGHT_STEP 23
#define TRAFFIC_LIGHT_2_X (TRAFFIC_LIGHT_1_X + TRAFFIC_LIGHT_STEP)
#define TRAFFIC_LIGHT_3_X (TRAFFIC_LIGHT_2_X + TRAFFIC_LIGHT_STEP)

struct about_app {
	GtkApplication *app;
};

struct about_metrics {
	double scale;
	int width;
	int height;
};

static int scaled_px(double scale, double value) {
	int scaled = (int)(value * scale + 0.5);
	return scaled > 1 ? scaled : 1;
}

static void get_monitor_geometry(GdkRectangle *geometry) {
	*geometry = (GdkRectangle) {
		.x = 0,
		.y = 0,
		.width = 1920,
		.height = 1080,
	};

	GdkDisplay *display = gdk_display_get_default();
	if (display == NULL) {
		return;
	}

	GListModel *monitors = gdk_display_get_monitors(display);
	if (monitors == NULL || g_list_model_get_n_items(monitors) == 0) {
		return;
	}

	GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
	if (monitor == NULL) {
		return;
	}
	gdk_monitor_get_geometry(monitor, geometry);
	g_object_unref(monitor);
}

static struct about_metrics about_metrics_for_display(void) {
	GdkRectangle geometry;
	get_monitor_geometry(&geometry);

	double max_width = geometry.width > 120 ? (double)geometry.width - 80.0 :
		(double)geometry.width;
	double max_height = geometry.height > 160 ? (double)geometry.height - 120.0 :
		(double)geometry.height;
	double scale = MIN(ABOUT_MAX_SCALE, MIN(max_width / ABOUT_DESIGN_WIDTH,
		max_height / ABOUT_DESIGN_HEIGHT));
	if (scale <= 0.0) {
		scale = 1.0;
	}

	return (struct about_metrics) {
		.scale = scale,
		.width = scaled_px(scale, ABOUT_DESIGN_WIDTH),
		.height = scaled_px(scale, ABOUT_DESIGN_HEIGHT),
	};
}

static bool about_should_use_dark(void) {
	const char *appearance = g_getenv("ORANGE_APPEARANCE");
	if (appearance != NULL && appearance[0] != '\0') {
		return g_ascii_strcasecmp(appearance, "dark") == 0;
	}

	const char *theme = g_getenv("GTK_THEME");
	if (theme != NULL &&
			(g_strrstr(theme, "Dark") != NULL ||
			 g_strrstr(theme, "dark") != NULL)) {
		return true;
	}

	GtkSettings *settings = gtk_settings_get_default();
	if (settings != NULL &&
			g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
				"gtk-application-prefer-dark-theme") != NULL) {
		gboolean prefer_dark = FALSE;
		g_object_get(settings,
			"gtk-application-prefer-dark-theme",
			&prefer_dark,
			NULL);
		return prefer_dark;
	}
	return false;
}

static void set_gtk_setting_string_from_env(GtkSettings *settings,
		const char *property,
		const char *env_name) {
	const char *value = g_getenv(env_name);
	if (settings == NULL || value == NULL || value[0] == '\0' ||
			g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
				property) == NULL) {
		return;
	}
	g_object_set(settings, property, value, NULL);
}

static void apply_env_gtk_settings(void) {
	GtkSettings *settings = gtk_settings_get_default();
	set_gtk_setting_string_from_env(settings, "gtk-theme-name", "GTK_THEME");
	set_gtk_setting_string_from_env(settings,
		"gtk-icon-theme-name",
		"GTK_ICON_THEME");
}

static void apply_css(double scale, bool dark) {
	GtkCssProvider *provider = gtk_css_provider_new();
	const char *root_bg = dark ? "#333333" : "#fbfbfb";
	const char *primary = dark ? "#dadada" : "#252527";
	const char *secondary = dark ? "#9a9996" : "#b8b8ba";
	const char *spec_key = dark ? "#dadada" : "#242426";
	const char *spec_value = dark ? "#b8b8ba" : "#7f7f83";
	const char *button_bg = dark ? "#242424" : "#eeeeef";
	const char *button_hover = dark ? "#3d3d3d" : "#e7e7e8";
	const char *button_fg = dark ? "#dadada" : "#272729";
	char *css = g_strdup_printf(
		"window.orange-about-window { background: transparent; }"
		".about-root {"
		"  background: %s;"
		"  color: %s;"
		"  border-radius: inherit;"
		"  overflow: hidden;"
		"}"
		".model-title {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 800;"
		"  letter-spacing: 0;"
		"}"
		".model-year {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".spec-key {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".spec-value {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		"button.more-info {"
		"  min-width: %dpx;"
		"  min-height: %dpx;"
		"  padding: 0;"
		"  border: 0;"
		"  border-radius: %dpx;"
		"  background: %s;"
		"  background-image: none;"
		"  box-shadow: none;"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"}"
		"button.more-info:hover { background: %s; }"
		".traffic-light {"
		"  min-width: %dpx;"
		"  min-height: %dpx;"
		"  padding: 0;"
		"  border: 0;"
		"  border-radius: 999px;"
		"  background-image: none;"
		"  background: transparent;"
		"  box-shadow: none;"
		"}"
		".regulatory {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".copyright {"
		"  color: %s;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}",
		root_bg,
		primary,
		primary,
		scaled_px(scale, 42),
		secondary,
		scaled_px(scale, 21),
		spec_key,
		scaled_px(scale, 21),
		spec_value,
		scaled_px(scale, 21),
		scaled_px(scale, 184),
		scaled_px(scale, 48),
		scaled_px(scale, 10),
		button_bg,
		button_fg,
		scaled_px(scale, 24),
		button_hover,
		TRAFFIC_LIGHT_DESIGN_SIZE,
		TRAFFIC_LIGHT_DESIGN_SIZE,
		secondary,
		scaled_px(scale, 12),
		secondary,
		scaled_px(scale, 12));
	gtk_css_provider_load_from_string(provider, css);
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);
	g_object_unref(provider);
}

static void rounded_rectangle(
		cairo_t *cr,
		double x,
		double y,
		double width,
		double height,
		double radius) {
	double right = x + width;
	double bottom = y + height;
	cairo_new_sub_path(cr);
	cairo_arc(cr, right - radius, y + radius, radius, -G_PI / 2.0, 0.0);
	cairo_arc(cr, right - radius, bottom - radius, radius, 0.0, G_PI / 2.0);
	cairo_arc(cr, x + radius, bottom - radius, radius, G_PI / 2.0, G_PI);
	cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3.0 * G_PI / 2.0);
	cairo_close_path(cr);
}

static void draw_laptop(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
	(void)area;
	bool dark = GPOINTER_TO_INT(data) != 0;

	const double design_w = 420.0;
	const double design_h = 222.0;
	double scale = MIN((double)width / design_w, (double)height / design_h);
	cairo_translate(cr,
		((double)width - design_w * scale) / 2.0,
		((double)height - design_h * scale) / 2.0);
	cairo_scale(cr, scale, scale);

	const double screen_x = (design_w - 270.0) / 2.0;
	const double screen_y = 8.0;
	const double screen_w = 270.0;
	const double screen_h = 188.0;
	const double base_x = (design_w - 344.0) / 2.0;
	const double base_y = 194.0;
	const double base_w = 344.0;

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	cairo_save(cr);
	cairo_translate(cr, screen_x + screen_w / 2.0, screen_y + screen_h + 13.0);
	cairo_scale(cr, 1.0, 0.16);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
	cairo_arc(cr, 0.0, 0.0, 172.0, 0.0, 2.0 * G_PI);
	cairo_fill(cr);
	cairo_restore(cr);

	rounded_rectangle(cr, screen_x - 2.0, screen_y + 2.0, screen_w + 4.0, screen_h + 5.0, 5.0);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
	cairo_fill(cr);

	rounded_rectangle(cr, screen_x, screen_y, screen_w, screen_h, 4.0);
	cairo_set_source_rgb(cr, 0.02, 0.02, 0.02);
	cairo_fill(cr);

	rounded_rectangle(cr, screen_x + 5.0, screen_y + 5.0, screen_w - 10.0, screen_h - 13.0, 2.0);
	cairo_pattern_t *screen = cairo_pattern_create_linear(0.0, screen_y + 5.0, 0.0, screen_y + screen_h);
	cairo_pattern_add_color_stop_rgb(screen, 0.0, 0.21, 0.58, 0.82);
	cairo_pattern_add_color_stop_rgb(screen, 1.0, 0.38, 0.74, 0.96);
	cairo_set_source(cr, screen);
	cairo_fill(cr);
	cairo_pattern_destroy(screen);

	rounded_rectangle(cr, screen_x + 120.0, screen_y, 30.0, 7.5, 2.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_fill(cr);

	cairo_move_to(cr, base_x + 41.0, base_y - 7.0);
	cairo_line_to(cr, base_x + base_w - 41.0, base_y - 7.0);
	cairo_line_to(cr, base_x + base_w - 16.0, base_y + 1.0);
	cairo_line_to(cr, base_x + 16.0, base_y + 1.0);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 0.02, 0.02, 0.02, 0.83);
	cairo_fill(cr);

	cairo_move_to(cr, base_x + 63.0, base_y - 3.0);
	cairo_line_to(cr, base_x + 146.0, base_y - 3.0);
	cairo_line_to(cr, base_x + 132.0, base_y + 1.0);
	cairo_line_to(cr, base_x + 49.0, base_y + 1.0);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.72);
	cairo_fill(cr);

	cairo_move_to(cr, base_x + base_w - 146.0, base_y - 3.0);
	cairo_line_to(cr, base_x + base_w - 63.0, base_y - 3.0);
	cairo_line_to(cr, base_x + base_w - 49.0, base_y + 1.0);
	cairo_line_to(cr, base_x + base_w - 132.0, base_y + 1.0);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.72);
	cairo_fill(cr);

	rounded_rectangle(cr, base_x, base_y, base_w, 17.0, 3.0);
	cairo_pattern_t *base = cairo_pattern_create_linear(0.0, base_y, 0.0, base_y + 17.0);
	if (dark) {
		cairo_pattern_add_color_stop_rgb(base, 0.0, 0.56, 0.57, 0.60);
		cairo_pattern_add_color_stop_rgb(base, 0.45, 0.30, 0.31, 0.34);
		cairo_pattern_add_color_stop_rgb(base, 1.0, 0.68, 0.69, 0.72);
	} else {
		cairo_pattern_add_color_stop_rgb(base, 0.0, 0.86, 0.87, 0.87);
		cairo_pattern_add_color_stop_rgb(base, 0.45, 0.62, 0.63, 0.63);
		cairo_pattern_add_color_stop_rgb(base, 1.0, 0.92, 0.92, 0.92);
	}
	cairo_set_source(cr, base);
	cairo_fill(cr);
	cairo_pattern_destroy(base);

	cairo_rectangle(cr, base_x + 7.0, base_y + 5.0, base_w - 14.0, 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.22 : 0.42);
	cairo_fill(cr);

	cairo_move_to(cr, base_x + 149.0, base_y + 8.0);
	cairo_curve_to(cr, base_x + 161.0, base_y + 10.0,
		base_x + 183.0, base_y + 10.0,
		base_x + 195.0, base_y + 8.0);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.35);
	cairo_stroke(cr);

	cairo_rectangle(cr, base_x + 180.0, base_y + 9.5, 1.2, 1.2);
	cairo_rectangle(cr, base_x + 205.0, base_y + 9.5, 1.2, 1.2);
	cairo_set_source_rgba(cr, 0.12, 0.12, 0.12, 0.35);
	cairo_fill(cr);
}

static GtkWidget *fixed_label(
		GtkWidget *fixed,
		const char *text,
		const char *css_class,
		double scale,
		int x,
		int y,
		int width,
		int height,
		float align) {
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_add_css_class(label, css_class);
	gtk_widget_set_size_request(label,
		scaled_px(scale, width),
		scaled_px(scale, height));
	gtk_label_set_xalign(GTK_LABEL(label), align);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	gtk_fixed_put(GTK_FIXED(fixed), label,
		scaled_px(scale, x),
		scaled_px(scale, y));
	return label;
}

static char *normalize_cpu_label(const char *value) {
	GString *normalized = g_string_new(NULL);
	bool previous_space = false;
	for (const char *p = value; *p != '\0'; p++) {
		if (g_ascii_isspace((guchar)*p)) {
			if (!previous_space && normalized->len > 0) {
				g_string_append_c(normalized, ' ');
			}
			previous_space = true;
		} else {
			g_string_append_c(normalized, *p);
			previous_space = false;
		}
	}
	return g_string_free(normalized, false);
}

static char *read_cpu_label(void) {
	char *contents = NULL;
	if (!g_file_get_contents("/proc/cpuinfo", &contents, NULL, NULL)) {
		return g_strdup("Unknown CPU");
	}

	const char *keys[] = {
		"model name",
		"Hardware",
		"Processor",
		"cpu model",
	};
	char *result = NULL;
	char **lines = g_strsplit(contents, "\n", -1);
	for (int key = 0; key < (int)(sizeof(keys) / sizeof(keys[0])) && result == NULL;
			key++) {
		for (char **line = lines; *line != NULL; line++) {
			char *colon = strchr(*line, ':');
			if (colon == NULL) {
				continue;
			}
			*colon = '\0';
			char *name = g_strstrip(*line);
			char *value = g_strstrip(colon + 1);
			if (g_ascii_strcasecmp(name, keys[key]) == 0 && value[0] != '\0') {
				result = normalize_cpu_label(value);
				break;
			}
		}
	}
	g_strfreev(lines);
	g_free(contents);
	return result != NULL ? result : g_strdup("Unknown CPU");
}

static char *read_memory_label(void) {
	struct sysinfo info;
	if (sysinfo(&info) != 0 || info.totalram == 0) {
		return g_strdup("Unknown");
	}

	long double bytes = (long double)info.totalram *
		(long double)(info.mem_unit == 0 ? 1 : info.mem_unit);
	long double gib = bytes / (1024.0L * 1024.0L * 1024.0L);
	if (gib >= 10.0L) {
		return g_strdup_printf("%.0Lf GB", gib);
	}
	return g_strdup_printf("%.1Lf GB", gib);
}

static void add_spec_row(
		GtkWidget *fixed,
		double scale,
		int y,
		const char *key,
		const char *value) {
	fixed_label(fixed, key, "spec-key", scale, 88, y, 155, 30, 1.0f);
	GtkWidget *value_label = fixed_label(fixed, value, "spec-value", scale,
		266, y, 232, 30, 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode(GTK_LABEL(value_label), TRUE);
}

static void on_more_info(GtkButton *button, gpointer data) {
	(void)button;
	(void)data;
	g_spawn_command_line_async(
		"sh -c 'command -v hardinfo2 >/dev/null && hardinfo2 || "
		"command -v hardinfo >/dev/null && hardinfo || true'",
		NULL);
}

static void on_close(GtkButton *button, gpointer data) {
	(void)button;
	gtk_window_destroy(GTK_WINDOW(data));
}

struct drag_data {
	GtkWindow *window;
	int threshold;
};

static void on_drag_begin(GtkGestureDrag *gesture, double x, double y,
		gpointer user_data) {
	struct drag_data *dd = user_data;
	if (y > dd->threshold) {
		return;
	}
	GtkWidget *widget = gtk_event_controller_get_widget(
		GTK_EVENT_CONTROLLER(gesture));
	GdkDevice *device = gtk_gesture_get_device(GTK_GESTURE(gesture));
	GdkSurface *surface = gtk_native_get_surface(
		gtk_widget_get_native(widget));
	if (surface == NULL) {
		return;
	}
	GdkToplevel *toplevel = GDK_TOPLEVEL(surface);
	GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
	guint32 timestamp = gdk_event_get_time(event);
	gdk_toplevel_begin_move(toplevel, device, 1, x, y, timestamp);
}

static void free_drag_data(gpointer data, GClosure *closure) {
	(void)closure;
	g_free(data);
}

static void on_minimize(GtkButton *button, gpointer data) {
	(void)button;
	gtk_window_minimize(GTK_WINDOW(data));
}

static void on_maximize(GtkButton *button, gpointer data) {
	(void)button;
	GtkWindow *window = GTK_WINDOW(data);
	if (gtk_window_is_maximized(window)) {
		gtk_window_unmaximize(window);
	} else {
		gtk_window_maximize(window);
	}
}

enum traffic_light_kind {
	TRAFFIC_LIGHT_CLOSE,
	TRAFFIC_LIGHT_MINIMIZE,
	TRAFFIC_LIGHT_MAXIMIZE,
};

struct traffic_light_data {
	GtkWidget *button;
	GtkWidget *face;
	GtkWindow *window;
	enum traffic_light_kind kind;
	gulong active_handler;
	bool hover;
};

static void traffic_light_queue(struct traffic_light_data *td) {
	if (td->face != NULL) {
		gtk_widget_queue_draw(td->face);
	}
}

static void set_source_hex(cairo_t *cr, unsigned int rgb, double alpha) {
	cairo_set_source_rgba(cr,
		((rgb >> 16) & 0xff) / 255.0,
		((rgb >> 8) & 0xff) / 255.0,
		(rgb & 0xff) / 255.0,
		alpha);
}

static void draw_traffic_glyph(cairo_t *cr, enum traffic_light_kind kind) {
	set_source_hex(cr, 0x000000, 0.50);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	switch (kind) {
	case TRAFFIC_LIGHT_CLOSE:
		cairo_set_line_width(cr, 1.45);
		cairo_move_to(cr, 5.35, 5.35);
		cairo_line_to(cr, 10.65, 10.65);
		cairo_move_to(cr, 10.65, 5.35);
		cairo_line_to(cr, 5.35, 10.65);
		cairo_stroke(cr);
		break;
	case TRAFFIC_LIGHT_MINIMIZE:
		rounded_rectangle(cr, 4.0, 7.0, 8.0, 2.0, 1.0);
		cairo_fill(cr);
		break;
	case TRAFFIC_LIGHT_MAXIMIZE:
		cairo_move_to(cr, 6.35, 5.0);
		cairo_line_to(cr, 11.0, 9.65);
		cairo_line_to(cr, 11.0, 6.05);
		cairo_curve_to(cr, 11.0, 5.45, 10.55, 5.0, 9.95, 5.0);
		cairo_close_path(cr);
		cairo_move_to(cr, 5.0, 6.35);
		cairo_line_to(cr, 5.0, 9.95);
		cairo_curve_to(cr, 5.0, 10.55, 5.45, 11.0, 6.05, 11.0);
		cairo_line_to(cr, 9.65, 11.0);
		cairo_close_path(cr);
		cairo_fill(cr);
		break;
	}
}

static void draw_traffic_light(GtkDrawingArea *area, cairo_t *cr,
		int width, int height, gpointer data) {
	(void)area;
	struct traffic_light_data *td = data;
	bool active_window = td->window == NULL || gtk_window_is_active(td->window);
	GtkStateFlags flags = td->button != NULL ?
		gtk_widget_get_state_flags(td->button) : GTK_STATE_FLAG_NORMAL;
	bool active_button = td->kind == TRAFFIC_LIGHT_CLOSE;
	bool pressed = active_button && (flags & GTK_STATE_FLAG_ACTIVE) != 0;
	bool show_glyph = active_button && (td->hover || pressed);
	double scale = MIN((double)width / 16.0, (double)height / 16.0);
	cairo_translate(cr, ((double)width - 16.0 * scale) / 2.0,
		((double)height - 16.0 * scale) / 2.0);
	cairo_scale(cr, scale, scale);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	unsigned int outer = 0xbababa;
	unsigned int inner = 0xcecece;
	switch (td->kind) {
	case TRAFFIC_LIGHT_CLOSE:
		if (active_window) {
			outer = 0xcb4e43;
			inner = 0xfe6254;
		}
		break;
	case TRAFFIC_LIGHT_MINIMIZE:
		if (active_window) {
			outer = 0xbababa;
			inner = 0xcecece;
		}
		break;
	case TRAFFIC_LIGHT_MAXIMIZE:
		if (active_window) {
			outer = 0xbababa;
			inner = 0xcecece;
		}
		break;
	}

	cairo_arc(cr, 8.0, 8.0, 7.0, 0.0, 2.0 * G_PI);
	set_source_hex(cr, outer, 1.0);
	cairo_fill(cr);
	if (!pressed) {
		cairo_arc(cr, 8.0, 8.0, 6.5, 0.0, 2.0 * G_PI);
		set_source_hex(cr, inner, 1.0);
		cairo_fill(cr);
	}
	if (show_glyph) {
		draw_traffic_glyph(cr, td->kind);
	}
}

static void traffic_light_enter(GtkEventControllerMotion *controller,
		double x, double y, gpointer data) {
	(void)controller;
	(void)x;
	(void)y;
	struct traffic_light_data *td = data;
	td->hover = true;
	traffic_light_queue(td);
}

static void traffic_light_leave(GtkEventControllerMotion *controller,
		gpointer data) {
	(void)controller;
	struct traffic_light_data *td = data;
	td->hover = false;
	traffic_light_queue(td);
}

static void traffic_light_window_active_changed(GObject *object,
		GParamSpec *pspec, gpointer data) {
	(void)object;
	(void)pspec;
	traffic_light_queue(data);
}

static void free_traffic_light_data(gpointer data, GObject *object) {
	(void)object;
	struct traffic_light_data *td = data;
	if (td->window != NULL && td->active_handler != 0) {
		g_signal_handler_disconnect(td->window, td->active_handler);
	}
	g_free(td);
}

static GtkWidget *traffic_light_new(
		const char *color,
		enum traffic_light_kind kind,
		GtkWindow *window,
		GCallback callback,
		gpointer data) {
	GtkWidget *button = gtk_button_new();
	gtk_widget_add_css_class(button, "traffic-light");
	gtk_widget_add_css_class(button, color);
	gtk_widget_set_size_request(button,
		TRAFFIC_LIGHT_DESIGN_SIZE,
		TRAFFIC_LIGHT_DESIGN_SIZE);
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);

	struct traffic_light_data *td = g_new0(struct traffic_light_data, 1);
	td->button = button;
	td->window = window;
	td->kind = kind;
	GtkWidget *face = gtk_drawing_area_new();
	td->face = face;
	gtk_widget_set_size_request(face,
		TRAFFIC_LIGHT_DESIGN_SIZE,
		TRAFFIC_LIGHT_DESIGN_SIZE);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(face),
		draw_traffic_light, td, NULL);
	gtk_button_set_child(GTK_BUTTON(button), face);
	g_object_weak_ref(G_OBJECT(button), free_traffic_light_data, td);

	GtkEventController *motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "enter", G_CALLBACK(traffic_light_enter), td);
	g_signal_connect(motion, "leave", G_CALLBACK(traffic_light_leave), td);
	gtk_widget_add_controller(button, motion);

	td->active_handler = g_signal_connect(window, "notify::is-active",
		G_CALLBACK(traffic_light_window_active_changed), td);
	g_signal_connect(button, "clicked", callback, data);
	return button;
}

static void activate(GtkApplication *app, gpointer data) {
	(void)data;
	struct about_metrics metrics = about_metrics_for_display();
	bool dark = about_should_use_dark();
	apply_env_gtk_settings();
	apply_css(metrics.scale, dark);

	GtkWidget *window = gtk_application_window_new(app);
	gtk_widget_add_css_class(window, "background");
	gtk_widget_add_css_class(window, "csd");
	gtk_widget_add_css_class(window, "orange-about-window");
	gtk_window_set_title(GTK_WINDOW(window), "About Orange");
	gtk_window_set_icon_name(GTK_WINDOW(window), "help-about");
	gtk_window_set_default_size(GTK_WINDOW(window), metrics.width, metrics.height);
	gtk_window_set_resizable(GTK_WINDOW(window), false);

	GtkWidget *root = gtk_fixed_new();
	gtk_widget_add_css_class(root, "about-root");
	gtk_widget_set_size_request(root, metrics.width, metrics.height);
	gtk_widget_set_overflow(root, GTK_OVERFLOW_HIDDEN);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_child(GTK_WINDOW(window), root);

	struct drag_data *dd = g_new(struct drag_data, 1);
	dd->window = GTK_WINDOW(window);
	dd->threshold = metrics.height / 2;
	GtkGesture *drag = gtk_gesture_drag_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
	g_signal_connect_data(drag, "drag-begin", G_CALLBACK(on_drag_begin), dd,
		free_drag_data, 0);
	gtk_widget_add_controller(root, GTK_EVENT_CONTROLLER(drag));

	GtkWidget *close_btn = traffic_light_new("close", TRAFFIC_LIGHT_CLOSE,
		GTK_WINDOW(window),
		G_CALLBACK(on_close), window);
	gtk_fixed_put(GTK_FIXED(root), close_btn,
		TRAFFIC_LIGHT_1_X,
		TRAFFIC_LIGHT_DESIGN_Y);
	GtkWidget *minimize_btn = traffic_light_new("minimize",
		TRAFFIC_LIGHT_MINIMIZE, GTK_WINDOW(window),
		G_CALLBACK(on_minimize), window);
	gtk_fixed_put(GTK_FIXED(root), minimize_btn,
		TRAFFIC_LIGHT_2_X,
		TRAFFIC_LIGHT_DESIGN_Y);
	GtkWidget *maximize_btn = traffic_light_new("maximize",
		TRAFFIC_LIGHT_MAXIMIZE, GTK_WINDOW(window),
		G_CALLBACK(on_maximize), window);
	gtk_fixed_put(GTK_FIXED(root), maximize_btn,
		TRAFFIC_LIGHT_3_X,
		TRAFFIC_LIGHT_DESIGN_Y);

	GtkWidget *laptop = gtk_drawing_area_new();
	gtk_widget_set_size_request(laptop,
		scaled_px(metrics.scale, 360),
		scaled_px(metrics.scale, 190));
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(laptop), draw_laptop,
		GINT_TO_POINTER(dark), NULL);
	gtk_fixed_put(GTK_FIXED(root), laptop,
		scaled_px(metrics.scale, 100),
		scaled_px(metrics.scale, 156));

	fixed_label(root,
		"MacBook Pro",
		"model-title",
		metrics.scale,
		0,
		470,
		ABOUT_DESIGN_WIDTH,
		50,
		0.5f);
	fixed_label(root,
		"16-inch, 2021",
		"model-year",
		metrics.scale,
		0,
		528,
		ABOUT_DESIGN_WIDTH,
		28,
		0.5f);

	char *chip = read_cpu_label();
	char *memory = read_memory_label();
	add_spec_row(root, metrics.scale, 592, "Chip", chip);
	add_spec_row(root, metrics.scale, 624, "Memory", memory);
	add_spec_row(root, metrics.scale, 656, "Orange", ORANGE_PROJECT_VERSION);
	g_free(chip);
	g_free(memory);

	GtkWidget *more_info = gtk_button_new_with_label("More Info...");
	gtk_widget_add_css_class(more_info, "more-info");
	gtk_widget_set_size_request(more_info,
		scaled_px(metrics.scale, 184),
		scaled_px(metrics.scale, 48));
	g_signal_connect(more_info, "clicked", G_CALLBACK(on_more_info), NULL);
	gtk_fixed_put(GTK_FIXED(root), more_info,
		scaled_px(metrics.scale, 188),
		scaled_px(metrics.scale, 752));

	GtkWidget *regulatory = fixed_label(root,
		NULL,
		"regulatory",
		metrics.scale,
		0,
		834,
		ABOUT_DESIGN_WIDTH,
		24,
		0.5f);
	gtk_label_set_markup(GTK_LABEL(regulatory), "<u>Regulatory Certification</u>");

	GtkWidget *copyright = fixed_label(root,
		NULL,
		"copyright",
		metrics.scale,
		0,
		864,
		ABOUT_DESIGN_WIDTH,
		30,
		0.5f);
	gtk_label_set_markup(GTK_LABEL(copyright),
		"&#169; 2026 Orange Project\nAll Rights Reserved.");
	gtk_label_set_justify(GTK_LABEL(copyright), GTK_JUSTIFY_CENTER);

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	struct about_app about = {
		.app = gtk_application_new("dev.orange.About",
			G_APPLICATION_DEFAULT_FLAGS),
	};
	gtk_window_set_default_icon_name("help-about");
	g_signal_connect(about.app, "activate", G_CALLBACK(activate), &about);
	int status = g_application_run(G_APPLICATION(about.app), argc, argv);
	g_object_unref(about.app);
	return status;
}
