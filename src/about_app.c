#include <cairo/cairo.h>
#include <gtk/gtk.h>

#define ABOUT_DESIGN_WIDTH 560
#define ABOUT_DESIGN_HEIGHT 580
#define ABOUT_MAX_SCALE 1.0
#define TRAFFIC_LIGHT_DESIGN_SIZE 12
#define TRAFFIC_LIGHT_DESIGN_Y 14
#define TRAFFIC_LIGHT_1_X 16
#define TRAFFIC_LIGHT_2_X 44
#define TRAFFIC_LIGHT_3_X 72
#define DRAG_AREA_DESIGN_HEIGHT 52

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
		.width = 1280,
		.height = 720,
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

static void apply_css(double scale) {
	GtkCssProvider *provider = gtk_css_provider_new();
	char *css = g_strdup_printf(
		"window { background: transparent; }"
		".about-root {"
		"  background: #fbfbfb;"
		"  color: #252527;"
		"  border-radius: 12px;"
		"  overflow: hidden;"
		"}"
		".model-title {"
		"  color: #252527;"
		"  font-size: %dpx;"
		"  font-weight: 800;"
		"  letter-spacing: 0;"
		"}"
		".model-year {"
		"  color: #b8b8ba;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".spec-key {"
		"  color: #242426;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".spec-value {"
		"  color: #7f7f83;"
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
		"  background: #eeeeef;"
		"  background-image: none;"
		"  box-shadow: none;"
		"  color: #272729;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"}"
		"button.more-info:hover { background: #e7e7e8; }"
		".traffic-light {"
		"  min-width: %dpx;"
		"  min-height: %dpx;"
		"  padding: 0;"
		"  border: 0;"
		"  border-radius: 50%%;"
		"  background-image: none;"
		"  box-shadow: none;"
		"}"
		".traffic-light.close { background: #ff5f57; }"
		".traffic-light.close:hover { background: #e0403a; }"
		".traffic-light.minimize { background: #b8b8ba; }"
		".traffic-light.minimize:hover { background: #9a9a9c; }"
		".traffic-light.maximize { background: #b8b8ba; }"
		".traffic-light.maximize:hover { background: #9a9a9c; }"
		".regulatory {"
		"  color: #b8b8ba;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}"
		".copyright {"
		"  color: #b8b8ba;"
		"  font-size: %dpx;"
		"  font-weight: 400;"
		"  letter-spacing: 0;"
		"}",
		scaled_px(scale, 42),
		scaled_px(scale, 21),
		scaled_px(scale, 21),
		scaled_px(scale, 21),
		scaled_px(scale, 170),
		scaled_px(scale, 44),
		scaled_px(scale, 10),
		scaled_px(scale, 24),
		scaled_px(scale, 19),
		scaled_px(scale, 19),
		scaled_px(scale, TRAFFIC_LIGHT_DESIGN_SIZE),
		scaled_px(scale, TRAFFIC_LIGHT_DESIGN_SIZE));
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
	(void)data;

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
	cairo_pattern_add_color_stop_rgb(base, 0.0, 0.86, 0.87, 0.87);
	cairo_pattern_add_color_stop_rgb(base, 0.45, 0.62, 0.63, 0.63);
	cairo_pattern_add_color_stop_rgb(base, 1.0, 0.92, 0.92, 0.92);
	cairo_set_source(cr, base);
	cairo_fill(cr);
	cairo_pattern_destroy(base);

	cairo_rectangle(cr, base_x + 7.0, base_y + 5.0, base_w - 14.0, 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.42);
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

static void add_spec_row(
		GtkWidget *fixed,
		double scale,
		int y,
		const char *key,
		const char *value) {
	fixed_label(fixed, key, "spec-key", scale, 88, y, 155, 30, 1.0f);
	fixed_label(fixed, value, "spec-value", scale, 266, y, 220, 30, 0.0f);
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

static GtkWidget *traffic_light_new(
		const char *color,
		double scale,
		GCallback callback,
		gpointer data) {
	GtkWidget *button = gtk_button_new();
	gtk_widget_add_css_class(button, "traffic-light");
	gtk_widget_add_css_class(button, color);
	gtk_widget_set_size_request(button,
		scaled_px(scale, TRAFFIC_LIGHT_DESIGN_SIZE),
		scaled_px(scale, TRAFFIC_LIGHT_DESIGN_SIZE));
	g_signal_connect(button, "clicked", callback, data);
	return button;
}

struct window_drag_data {
	GtkWindow *window;
	double scale;
};

static gboolean is_traffic_light(double x, double y, double scale) {
	int s = scaled_px(scale, TRAFFIC_LIGHT_DESIGN_SIZE);
	int ty = scaled_px(scale, TRAFFIC_LIGHT_DESIGN_Y);
	int positions[] = {
		TRAFFIC_LIGHT_1_X,
		TRAFFIC_LIGHT_2_X,
		TRAFFIC_LIGHT_3_X,
	};
	for (int i = 0; i < 3; i++) {
		int tx = scaled_px(scale, positions[i]);
		if (x >= tx && x < tx + s && y >= ty && y < ty + s) {
			return TRUE;
		}
	}
	return FALSE;
}

static void on_drag_begin(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
	(void)n_press;
	struct window_drag_data *drag = data;
	if (y < scaled_px(drag->scale, DRAG_AREA_DESIGN_HEIGHT) && !is_traffic_light(x, y, drag->scale)) {
		guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
		GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
		guint32 time = gdk_event_get_time(event);
		GdkDevice *device = gdk_event_get_device(event);
		GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(drag->window));
		gdk_toplevel_begin_move(GDK_TOPLEVEL(surface), device, button, x, y, time);
	}
}

static void activate(GtkApplication *app, gpointer data) {
	(void)data;
	struct about_metrics metrics = about_metrics_for_display();
	apply_css(metrics.scale);

	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "About This Mac");
	gtk_window_set_default_size(GTK_WINDOW(window), metrics.width, metrics.height);
	gtk_window_set_resizable(GTK_WINDOW(window), false);

	GtkWidget *empty_titlebar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request(empty_titlebar, -1, 0);
	gtk_window_set_titlebar(GTK_WINDOW(window), empty_titlebar);

	GtkWidget *root = gtk_fixed_new();
	gtk_widget_add_css_class(root, "about-root");
	gtk_widget_set_size_request(root, metrics.width, metrics.height);
	gtk_widget_set_overflow(root, GTK_OVERFLOW_HIDDEN);
	gtk_window_set_child(GTK_WINDOW(window), root);

	GtkWidget *close_btn = traffic_light_new("close", metrics.scale,
		G_CALLBACK(on_close), window);
	gtk_fixed_put(GTK_FIXED(root), close_btn,
		scaled_px(metrics.scale, TRAFFIC_LIGHT_1_X),
		scaled_px(metrics.scale, TRAFFIC_LIGHT_DESIGN_Y));
	GtkWidget *minimize_btn = traffic_light_new("minimize", metrics.scale,
		G_CALLBACK(on_minimize), window);
	gtk_fixed_put(GTK_FIXED(root), minimize_btn,
		scaled_px(metrics.scale, TRAFFIC_LIGHT_2_X),
		scaled_px(metrics.scale, TRAFFIC_LIGHT_DESIGN_Y));
	GtkWidget *maximize_btn = traffic_light_new("maximize", metrics.scale,
		G_CALLBACK(on_maximize), window);
	gtk_fixed_put(GTK_FIXED(root), maximize_btn,
		scaled_px(metrics.scale, TRAFFIC_LIGHT_3_X),
		scaled_px(metrics.scale, TRAFFIC_LIGHT_DESIGN_Y));

	struct window_drag_data *drag_data = g_new(struct window_drag_data, 1);
	drag_data->window = GTK_WINDOW(window);
	drag_data->scale = metrics.scale;
	GtkGesture *drag = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
	g_signal_connect(drag, "pressed", G_CALLBACK(on_drag_begin), drag_data);
	gtk_widget_add_controller(root, GTK_EVENT_CONTROLLER(drag));

	GtkWidget *laptop = gtk_drawing_area_new();
	gtk_widget_set_size_request(laptop,
		scaled_px(metrics.scale, 360),
		scaled_px(metrics.scale, 190));
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(laptop), draw_laptop, NULL, NULL);
	gtk_fixed_put(GTK_FIXED(root), laptop,
		scaled_px(metrics.scale, 100),
		scaled_px(metrics.scale, 36));

	fixed_label(root,
		"MacBook Pro",
		"model-title",
		metrics.scale,
		0,
		256,
		ABOUT_DESIGN_WIDTH,
		50,
		0.5f);
	fixed_label(root,
		"16-inch, 2021",
		"model-year",
		metrics.scale,
		0,
		304,
		ABOUT_DESIGN_WIDTH,
		28,
		0.5f);

	add_spec_row(root, metrics.scale, 346, "Chip", "Apple M1 Pro");
	add_spec_row(root, metrics.scale, 374, "Memory", "16 GB");
	add_spec_row(root, metrics.scale, 402, "Serial number", "XXXXXXXXXX");
	add_spec_row(root, metrics.scale, 430, "macOS", "Tahoe 26.0");

	GtkWidget *more_info = gtk_button_new_with_label("More Info...");
	gtk_widget_add_css_class(more_info, "more-info");
	gtk_widget_set_size_request(more_info,
		scaled_px(metrics.scale, 170),
		scaled_px(metrics.scale, 44));
	g_signal_connect(more_info, "clicked", G_CALLBACK(on_more_info), NULL);
	gtk_fixed_put(GTK_FIXED(root), more_info,
		scaled_px(metrics.scale, 195),
		scaled_px(metrics.scale, 474));

	GtkWidget *regulatory = fixed_label(root,
		NULL,
		"regulatory",
		metrics.scale,
		0,
		528,
		ABOUT_DESIGN_WIDTH,
		24,
		0.5f);
	gtk_label_set_markup(GTK_LABEL(regulatory), "<u>Regulatory Certification</u>");

	GtkWidget *copyright = fixed_label(root,
		NULL,
		"copyright",
		metrics.scale,
		0,
		550,
		ABOUT_DESIGN_WIDTH,
		30,
		0.5f);
	gtk_label_set_markup(GTK_LABEL(copyright),
		"&#8482; and &#169; 1983-2025 Apple Inc.\nAll Rights Reserved.");
	gtk_label_set_justify(GTK_LABEL(copyright), GTK_JUSTIFY_CENTER);

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	struct about_app about = {
		.app = gtk_application_new("dev.tahoe.About",
			G_APPLICATION_DEFAULT_FLAGS),
	};
	g_signal_connect(about.app, "activate", G_CALLBACK(activate), &about);
	int status = g_application_run(G_APPLICATION(about.app), argc, argv);
	g_object_unref(about.app);
	return status;
}
