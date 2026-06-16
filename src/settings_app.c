#include "orange/config.h"

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LEFT_DECORATION_LAYOUT "close,minimize,maximize:"
#define SETTINGS_WINDOW_WIDTH 980
#define SETTINGS_WINDOW_HEIGHT 720
#define SIDEBAR_WIDTH 240
#define TRAFFIC_LIGHT_DESIGN_SIZE 16
#define SWATCH_DESIGN_SIZE 30

struct settings_app {
	GtkApplication *app;
	GtkWindow *window;
	struct orange_config config;
	const char *config_path;
	GtkStack *stack;
	GtkListBox *sidebar_list;
	GtkSearchEntry *search_entry;
	GtkWidget *title_label;
	GtkWidget *accent_buttons[ORANGE_ACCENT_COLOR_COUNT];
	GtkWidget *icon_style_buttons[ORANGE_ICON_WIDGET_STYLE_COUNT];
};

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

struct swatch_data {
	GtkWidget *button;
	GtkWidget *face;
	struct settings_app *settings;
	enum orange_accent_color color;
};

struct nav_item {
	const char *panel_id;
	const char *title;
	const char *icon_name;
	bool group_start;
};

enum theme_dropdown_kind {
	THEME_DROPDOWN_GTK,
	THEME_DROPDOWN_ICON,
	THEME_DROPDOWN_CURSOR,
};

struct dropdown_options {
	char **labels;
	char **values;
	unsigned count;
};

static const struct nav_item nav_items[] = {
	{"family", "Family", "avatar-default", false},
	{"wifi", "Wi-Fi", "network-wireless", true},
	{"bluetooth", "Bluetooth", "bluetooth", false},
	{"network", "Network", "preferences-system-network", false},
	{"vpn", "VPN", "preferences-system-network-vpn", false},
	{"energy", "Energy", "preferences-system-power", false},
	{"general", "General", "preferences-system", true},
	{"accessibility", "Accessibility", "preferences-desktop-accessibility", false},
	{"appearance", "Appearance", "preferences-desktop-theme-global", false},
	{"menu-bar", "Menu Bar", "view-grid", false},
	{"intelligence", "Apple Intelligence & Siri", "preferences-system-search", false},
	{"desktop-dock", "Desktop & Dock", "preferences-desktop", false},
	{"displays", "Displays", "preferences-desktop-display", false},
	{"spotlight", "Spotlight", "system-search", false},
	{"widgets", "Widgets", "applications-graphics", false},
};

static const char *settings_css =
	"window.orange-settings-window {"
	"  background: transparent;"
	"}"
	".settings-root {"
	"  background: rgba(247, 247, 248, 0.96);"
	"  border: 1px solid rgba(170, 170, 176, 0.58);"
	"  border-radius: inherit;"
	"  overflow: hidden;"
	"}"
	".settings-sidebar {"
	"  background: rgba(241, 241, 242, 0.86);"
	"  border: 1px solid rgba(215, 215, 219, 0.84);"
	"  border-radius: 24px;"
	"  box-shadow: 0 18px 42px rgba(38, 38, 44, 0.18), inset 0 1px rgba(255, 255, 255, 0.58);"
	"  overflow: hidden;"
	"}"
	".settings-sidebar-chrome {"
	"  min-height: 42px;"
	"  padding: 12px 18px 0 18px;"
	"}"
	"button.traffic-light {"
	"  min-width: 16px;"
	"  min-height: 16px;"
	"  padding: 0;"
	"  border: 0;"
	"  border-radius: 999px;"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".settings-search {"
	"  margin: 0 14px 10px 14px;"
	"  min-height: 28px;"
	"  border-radius: 16px;"
	"  background: rgba(255, 255, 255, 0.52);"
	"  border: 1px solid rgba(190, 190, 194, 0.35);"
	"}"
	".settings-account {"
	"  padding: 8px 14px 10px 14px;"
	"}"
	".account-avatar {"
	"  min-width: 42px;"
	"  min-height: 42px;"
	"  border-radius: 21px;"
	"  background: linear-gradient(135deg, #edb48a, #b87962);"
	"  color: white;"
	"  font-weight: 700;"
	"}"
	".account-name {"
	"  font-weight: 700;"
	"}"
	".account-kind {"
	"  color: #77777c;"
	"  font-size: 12px;"
	"}"
	".settings-sidebar-list {"
	"  background: transparent;"
	"}"
	".settings-sidebar-list row {"
	"  margin: 1px 10px;"
	"  border-radius: 8px;"
	"}"
	".settings-sidebar-list row.group-start {"
	"  margin-top: 12px;"
	"}"
	".settings-sidebar-row {"
	"  padding: 5px 8px;"
	"}"
	".settings-sidebar-list row:selected {"
	"  background: #0a72e8;"
	"}"
	".settings-sidebar-list row:selected label {"
	"  color: white;"
	"}"
	".sidebar-theme-icon {"
	"  min-width: 28px;"
	"  min-height: 28px;"
	"  -gtk-icon-size: 28px;"
	"}"
	".sidebar-title {"
	"  font-weight: 600;"
	"}"
	".settings-main {"
	"  background: rgba(248, 248, 249, 0.96);"
	"}"
	".panel-toolbar {"
	"  min-height: 52px;"
	"  padding: 10px 28px 4px 20px;"
	"}"
	".panel-title {"
	"  font-weight: 700;"
	"  font-size: 21px;"
	"}"
	".toolbar-nav button {"
	"  min-width: 32px;"
	"  min-height: 28px;"
	"  padding: 0;"
	"  border: 0;"
	"  border-radius: 14px;"
	"  background: transparent;"
	"  box-shadow: none;"
	"}"
	".toolbar-nav {"
	"  padding: 2px;"
	"  border-radius: 18px;"
	"  background: rgba(255, 255, 255, 0.56);"
	"  border: 1px solid rgba(210, 210, 214, 0.52);"
	"}"
	".toolbar-nav separator {"
	"  min-width: 1px;"
	"  margin-top: 5px;"
	"  margin-bottom: 5px;"
	"  background: rgba(160, 160, 166, 0.30);"
	"}"
	".settings-content {"
	"  background: transparent;"
	"}"
	".panel-body {"
	"  padding: 8px 28px 34px 28px;"
	"}"
	".section-heading {"
	"  margin: 18px 0 8px 14px;"
	"  color: #262629;"
	"  font-weight: 700;"
	"}"
	".settings-card {"
	"  background: white;"
	"  border: 1px solid #e5e5e8;"
	"  border-radius: 10px;"
	"}"
	".settings-row {"
	"  min-height: 45px;"
	"  padding: 8px 14px;"
	"}"
	".settings-row-title {"
	"  color: #242428;"
	"  font-size: 15px;"
	"}"
	".settings-row-detail {"
	"  color: #77777d;"
	"  font-size: 12px;"
	"}"
	".settings-value {"
	"  color: #303036;"
	"  font-size: 15px;"
	"}"
	".settings-separator {"
	"  margin-left: 14px;"
	"  background: #e9e9ec;"
	"  min-height: 1px;"
	"}"
	".appearance-choice {"
	"  padding: 6px;"
	"  border-radius: 10px;"
	"  background: transparent;"
	"  background-image: none;"
	"  border: 0;"
	"  box-shadow: none;"
	"}"
	".appearance-choice:checked {"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".appearance-choice-label {"
	"  font-weight: 600;"
	"}"
	".appearance-preview {"
	"  min-width: 90px;"
	"  min-height: 54px;"
	"  border-radius: 8px;"
	"  border: 1px solid #d7d7db;"
	"}"
	".appearance-choice:checked .appearance-preview {"
	"  border: 3px solid #0a84ff;"
	"}"
	".swatch-row {"
	"  background: transparent;"
	"}"
	"button.color-swatch {"
	"  min-width: 30px;"
	"  min-height: 30px;"
	"  padding: 0;"
	"  border: 0;"
	"  border-radius: 999px;"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".swatch-face {"
	"  min-width: 30px;"
	"  min-height: 30px;"
	"}"
	".style-choice {"
	"  min-width: 64px;"
	"  padding: 5px 6px;"
	"  border-radius: 9px;"
	"  border: 0;"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".style-choice:checked {"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".style-preview {"
	"  border-radius: 8px;"
	"}"
	".style-choice:checked .style-preview {"
	"  border: 3px solid #0a84ff;"
	"}"
	".settings-entry {"
	"  min-width: 220px;"
	"}"
	".settings-dropdown {"
	"  min-height: 28px;"
	"  border: 0;"
	"  border-radius: 7px;"
	"  background: transparent;"
	"  background-image: none;"
	"  box-shadow: none;"
	"}"
	".settings-scale {"
	"  min-width: 210px;"
	"}";

static bool settings_should_use_dark(const struct settings_app *settings) {
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

	GtkSettings *gtk_settings = gtk_settings_get_default();
	if (gtk_settings != NULL &&
			g_object_class_find_property(G_OBJECT_GET_CLASS(gtk_settings),
				"gtk-application-prefer-dark-theme") != NULL) {
		gboolean prefer_dark = FALSE;
		g_object_get(gtk_settings,
			"gtk-application-prefer-dark-theme",
			&prefer_dark,
			NULL);
		if (prefer_dark) {
			return true;
		}
	}

	return settings != NULL &&
		settings->config.appearance == ORANGE_APPEARANCE_DARK;
}

static void set_gtk_setting_string_if_supported(GtkSettings *gtk_settings,
		const char *property,
		const char *value) {
	if (gtk_settings == NULL ||
			g_object_class_find_property(G_OBJECT_GET_CLASS(gtk_settings),
				property) == NULL) {
		return;
	}
	if (value == NULL || value[0] == '\0') {
		gtk_settings_reset_property(gtk_settings, property);
	} else {
		g_object_set(gtk_settings, property, value, NULL);
	}
}

static void apply_configured_gtk_settings(
		const struct settings_app *settings) {
	GtkSettings *gtk_settings = gtk_settings_get_default();
	if (gtk_settings == NULL || settings == NULL) {
		return;
	}

	const char *gtk_theme = settings_should_use_dark(settings) ?
		settings->config.gtk_theme_dark : settings->config.gtk_theme_light;
	set_gtk_setting_string_if_supported(gtk_settings,
		"gtk-theme-name",
		gtk_theme);
	set_gtk_setting_string_if_supported(gtk_settings,
		"gtk-icon-theme-name",
		settings->config.icon_theme);
}

static void install_css(const struct settings_app *settings) {
	GdkDisplay *display = gdk_display_get_default();
	if (display == NULL) {
		return;
	}

	GtkCssProvider *provider = gtk_css_provider_new();
	char *css = NULL;
	if (settings_should_use_dark(settings)) {
		const char *dark_css =
			".settings-root {"
			"  background: #333333;"
			"  border-color: #4a4a4d;"
			"  color: #dadada;"
			"}"
			".settings-sidebar {"
			"  background: rgba(36, 36, 36, 0.88);"
			"  border-color: rgba(78, 78, 82, 0.86);"
			"  box-shadow: 0 18px 42px rgba(0, 0, 0, 0.40), inset 0 1px rgba(255, 255, 255, 0.08);"
			"}"
			".settings-main { background: #333333; }"
			".settings-search {"
			"  background: #242424;"
			"  border-color: #444448;"
			"  color: #dadada;"
			"}"
			".account-name, .sidebar-title, .settings-row-title, .section-heading, .panel-title {"
			"  color: #dadada;"
			"}"
			".account-kind, .settings-row-detail, .settings-value {"
			"  color: #a5a5a8;"
			"}"
			".settings-card {"
			"  background: #242424;"
			"  border-color: #3f3f42;"
			"}"
			".settings-separator { background: #3a3a3d; }"
			".toolbar-nav {"
			"  background: #242424;"
			"  border-color: #444448;"
			"}"
			".toolbar-nav separator { background: #444448; }"
			".settings-dropdown { color: #dadada; }"
			".appearance-preview { border-color: #454548; }";
		css = g_strconcat(settings_css, dark_css, NULL);
	} else {
		css = g_strdup(settings_css);
	}
	gtk_css_provider_load_from_string(provider, css);
	gtk_style_context_add_provider_for_display(display,
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);
	g_object_unref(provider);
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

static int fit_window_dimension(int preferred, int monitor_size, int margin) {
	int available = monitor_size > margin ? monitor_size - margin : monitor_size;
	return available > 0 && available < preferred ? available : preferred;
}

static void set_left_decoration_layout(void) {
	GtkSettings *settings = gtk_settings_get_default();
	if (settings == NULL) {
		return;
	}
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
			"gtk-decoration-layout") != NULL) {
		g_object_set(settings,
			"gtk-decoration-layout",
			LEFT_DECORATION_LAYOUT,
			NULL);
	}
}

static void save_config(struct settings_app *settings) {
	orange_config_save(&settings->config, settings->config_path);
}

static void on_window_close(GtkButton *button, gpointer data) {
	(void)button;
	struct settings_app *settings = data;
	if (settings->window != NULL) {
		gtk_window_close(settings->window);
	}
}

static void on_window_minimize(GtkButton *button, gpointer data) {
	(void)button;
	struct settings_app *settings = data;
	if (settings->window != NULL) {
		gtk_window_minimize(settings->window);
	}
}

static void on_window_maximize(GtkButton *button, gpointer data) {
	(void)button;
	struct settings_app *settings = data;
	if (settings->window == NULL) {
		return;
	}
	if (gtk_window_is_maximized(settings->window)) {
		gtk_window_unmaximize(settings->window);
	} else {
		gtk_window_maximize(settings->window);
	}
}

static GtkWidget *traffic_light_new(enum traffic_light_kind kind,
		GtkWindow *window,
		GCallback callback,
		gpointer data);

static void on_titlebar_pressed(GtkGestureClick *gesture,
		int n_press,
		double x,
		double y,
		gpointer data) {
	(void)data;
	if (n_press != 1) {
		return;
	}

	GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GtkNative *native = gtk_widget_get_native(widget);
	if (native == NULL) {
		return;
	}

	GdkSurface *surface = gtk_native_get_surface(native);
	if (surface == NULL || !GDK_IS_TOPLEVEL(surface)) {
		return;
	}

	graphene_point_t widget_point = GRAPHENE_POINT_INIT((float)x, (float)y);
	graphene_point_t surface_point = GRAPHENE_POINT_INIT((float)x, (float)y);
	if (!gtk_widget_compute_point(widget, GTK_WIDGET(native),
			&widget_point, &surface_point)) {
		return;
	}

	GdkDevice *device = gtk_event_controller_get_current_event_device(
		GTK_EVENT_CONTROLLER(gesture));
	guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
	guint32 time = gtk_event_controller_get_current_event_time(
		GTK_EVENT_CONTROLLER(gesture));
	gdk_toplevel_begin_move(GDK_TOPLEVEL(surface),
		device,
		(int)button,
		surface_point.x,
		surface_point.y,
		time);
}

static void add_titlebar_drag(GtkWidget *widget) {
	GtkGesture *gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 1);
	g_signal_connect(gesture, "pressed", G_CALLBACK(on_titlebar_pressed), NULL);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(gesture));
}

static bool resize_edge_for_point(GtkWidget *widget,
		double x,
		double y,
		GdkSurfaceEdge *edge) {
	const double grip = 10.0;
	double width = gtk_widget_get_width(widget);
	double height = gtk_widget_get_height(widget);
	bool north = y <= grip;
	bool south = y >= height - grip;
	bool west = x <= grip;
	bool east = x >= width - grip;

	if (north && west) {
		*edge = GDK_SURFACE_EDGE_NORTH_WEST;
	} else if (north && east) {
		*edge = GDK_SURFACE_EDGE_NORTH_EAST;
	} else if (south && west) {
		*edge = GDK_SURFACE_EDGE_SOUTH_WEST;
	} else if (south && east) {
		*edge = GDK_SURFACE_EDGE_SOUTH_EAST;
	} else if (north) {
		*edge = GDK_SURFACE_EDGE_NORTH;
	} else if (south) {
		*edge = GDK_SURFACE_EDGE_SOUTH;
	} else if (west) {
		*edge = GDK_SURFACE_EDGE_WEST;
	} else if (east) {
		*edge = GDK_SURFACE_EDGE_EAST;
	} else {
		return false;
	}
	return true;
}

static void on_resize_pressed(GtkGestureClick *gesture,
		int n_press,
		double x,
		double y,
		gpointer data) {
	(void)data;
	if (n_press != 1) {
		return;
	}

	GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GdkSurfaceEdge edge;
	if (!resize_edge_for_point(widget, x, y, &edge)) {
		return;
	}

	GtkNative *native = gtk_widget_get_native(widget);
	if (native == NULL) {
		return;
	}
	GdkSurface *surface = gtk_native_get_surface(native);
	if (surface == NULL || !GDK_IS_TOPLEVEL(surface)) {
		return;
	}

	GdkDevice *device = gtk_event_controller_get_current_event_device(
		GTK_EVENT_CONTROLLER(gesture));
	guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
	guint32 time = gtk_event_controller_get_current_event_time(
		GTK_EVENT_CONTROLLER(gesture));
	gdk_toplevel_begin_resize(GDK_TOPLEVEL(surface),
		edge,
		device,
		(int)button,
		x,
		y,
		time);
	gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void add_resize_edges(GtkWidget *widget) {
	GtkGesture *gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 1);
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(gesture),
		GTK_PHASE_CAPTURE);
	g_signal_connect(gesture, "pressed", G_CALLBACK(on_resize_pressed), NULL);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(gesture));
}

static GtkWidget *sidebar_chrome_new(struct settings_app *settings) {
	GtkWidget *chrome = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_add_css_class(chrome, "settings-sidebar-chrome");
	add_titlebar_drag(chrome);

	gtk_box_append(GTK_BOX(chrome), traffic_light_new(TRAFFIC_LIGHT_CLOSE,
		settings->window, G_CALLBACK(on_window_close), settings));
	gtk_box_append(GTK_BOX(chrome), traffic_light_new(TRAFFIC_LIGHT_MINIMIZE,
		settings->window, G_CALLBACK(on_window_minimize), settings));
	gtk_box_append(GTK_BOX(chrome), traffic_light_new(TRAFFIC_LIGHT_MAXIMIZE,
		settings->window, G_CALLBACK(on_window_maximize), settings));

	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spacer, true);
	gtk_box_append(GTK_BOX(chrome), spacer);
	return chrome;
}

static void rounded_rect(cairo_t *cr,
		double x,
		double y,
		double width,
		double height,
		double radius) {
	double r = radius;
	if (r > width / 2.0) {
		r = width / 2.0;
	}
	if (r > height / 2.0) {
		r = height / 2.0;
	}
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + width - r, y + r, r, -1.57079632679, 0.0);
	cairo_arc(cr, x + width - r, y + height - r, r, 0.0, 1.57079632679);
	cairo_arc(cr, x + r, y + height - r, r, 1.57079632679, 3.14159265359);
	cairo_arc(cr, x + r, y + r, r, 3.14159265359, 4.71238898038);
	cairo_close_path(cr);
}

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

static unsigned int accent_color_rgb(enum orange_accent_color color) {
	switch (color) {
	case ORANGE_ACCENT_BLUE:
		return 0x0a84ff;
	case ORANGE_ACCENT_PURPLE:
		return 0x8e44ad;
	case ORANGE_ACCENT_PINK:
		return 0xec4899;
	case ORANGE_ACCENT_RED:
		return 0xe52d3a;
	case ORANGE_ACCENT_ORANGE:
		return 0xff7a1a;
	case ORANGE_ACCENT_YELLOW:
		return 0xffc62b;
	case ORANGE_ACCENT_GREEN:
		return 0x58b947;
	case ORANGE_ACCENT_GRAPHITE:
		return 0x8e8e93;
	case ORANGE_ACCENT_MULTICOLOR:
	case ORANGE_ACCENT_COLOR_COUNT:
		return 0x0a84ff;
	}
	return 0x0a84ff;
}

static void draw_multicolor_swatch(cairo_t *cr, double center, double radius) {
	static const unsigned int colors[] = {
		0xff3b30, 0xff9500, 0xffcc00, 0x34c759,
		0x0a84ff, 0x8e44ad, 0xff2d55,
	};
	const double step = 2.0 * G_PI / (double)G_N_ELEMENTS(colors);
	for (size_t i = 0; i < G_N_ELEMENTS(colors); i++) {
		cairo_move_to(cr, center, center);
		cairo_arc(cr, center, center, radius,
			-G_PI / 2.0 + (double)i * step,
			-G_PI / 2.0 + (double)(i + 1) * step);
		cairo_close_path(cr);
		set_source_hex(cr, colors[i], 1.0);
		cairo_fill(cr);
	}
}

static void draw_accent_swatch(GtkDrawingArea *area,
		cairo_t *cr,
		int width,
		int height,
		gpointer data) {
	(void)area;
	struct swatch_data *sd = data;
	const double size = MIN(width, height);
	const double center_x = width / 2.0;
	const double center_y = height / 2.0;
	const double selected_radius = MIN(14.0, size / 2.0 - 1.0);
	const double inner_gap_radius = MAX(0.0, selected_radius - 3.0);
	const double fill_radius = selected_radius - 4.8;
	const bool selected = sd->settings != NULL &&
		sd->settings->config.accent_color == sd->color;

	cairo_save(cr);
	cairo_translate(cr, center_x - size / 2.0, center_y - size / 2.0);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	if (selected) {
		cairo_arc(cr, size / 2.0, size / 2.0, selected_radius,
			0.0, 2.0 * G_PI);
		set_source_hex(cr, 0x0a84ff, 1.0);
		cairo_fill(cr);

		cairo_arc(cr, size / 2.0, size / 2.0, inner_gap_radius,
			0.0, 2.0 * G_PI);
		set_source_hex(cr,
			settings_should_use_dark(sd->settings) ? 0x242424 : 0xffffff,
			1.0);
		cairo_fill(cr);
	}

	if (sd->color == ORANGE_ACCENT_MULTICOLOR) {
		draw_multicolor_swatch(cr, size / 2.0,
			selected ? fill_radius : selected_radius - 1.0);
	} else {
		cairo_arc(cr, size / 2.0, size / 2.0,
			selected ? fill_radius : selected_radius - 1.0,
			0.0, 2.0 * G_PI);
		set_source_hex(cr, accent_color_rgb(sd->color), 1.0);
		cairo_fill(cr);
	}

	cairo_arc(cr, size / 2.0, size / 2.0,
		selected ? fill_radius : selected_radius - 1.0,
		0.0, 2.0 * G_PI);
	set_source_hex(cr, 0x000000, settings_should_use_dark(sd->settings) ? 0.24 : 0.12);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	cairo_restore(cr);
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
		rounded_rect(cr, 4.0, 7.0, 8.0, 2.0, 1.0);
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
	if (active_window) {
		switch (td->kind) {
		case TRAFFIC_LIGHT_CLOSE:
			outer = 0xcb4e43;
			inner = 0xfe6254;
			break;
		case TRAFFIC_LIGHT_MINIMIZE:
			outer = 0xc99422;
			inner = 0xfdbc2e;
			break;
		case TRAFFIC_LIGHT_MAXIMIZE:
			outer = 0x22a43a;
			inner = 0x28c840;
			break;
		}
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

static GtkWidget *traffic_light_new(enum traffic_light_kind kind,
		GtkWindow *window,
		GCallback callback,
		gpointer data) {
	GtkWidget *button = gtk_button_new();
	gtk_widget_add_css_class(button, "traffic-light");
	gtk_widget_set_size_request(button,
		TRAFFIC_LIGHT_DESIGN_SIZE,
		TRAFFIC_LIGHT_DESIGN_SIZE);
	gtk_widget_set_focusable(button, false);
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

	if (window != NULL) {
		td->active_handler = g_signal_connect(window, "notify::is-active",
			G_CALLBACK(traffic_light_window_active_changed), td);
	}
	g_signal_connect(button, "clicked", callback, data);
	return button;
}

static GtkWidget *value_label_new(const char *text) {
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 32);
	gtk_widget_add_css_class(label, "settings-value");
	return label;
}

static GtkWidget *disabled_switch_new(bool active) {
	GtkWidget *toggle = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(toggle), active);
	gtk_widget_set_sensitive(toggle, false);
	return toggle;
}

static GtkWidget *disabled_scale_new(double value) {
	GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		0.0, 1.0, 0.01);
	gtk_range_set_value(GTK_RANGE(scale), value);
	gtk_scale_set_draw_value(GTK_SCALE(scale), false);
	gtk_widget_add_css_class(scale, "settings-scale");
	gtk_widget_set_sensitive(scale, false);
	gtk_widget_set_size_request(scale, 220, -1);
	return scale;
}

static void draw_preview_chrome(cairo_t *cr,
		int width,
		int height,
		bool dark) {
	cairo_pattern_t *background = cairo_pattern_create_linear(0, 0, width, height);
	if (dark) {
		cairo_pattern_add_color_stop_rgb(background, 0.0, 0.02, 0.05, 0.14);
		cairo_pattern_add_color_stop_rgb(background, 0.55, 0.03, 0.11, 0.26);
		cairo_pattern_add_color_stop_rgb(background, 1.0, 0.13, 0.32, 0.58);
	} else {
		cairo_pattern_add_color_stop_rgb(background, 0.0, 0.94, 0.98, 1.00);
		cairo_pattern_add_color_stop_rgb(background, 0.55, 0.72, 0.88, 1.00);
		cairo_pattern_add_color_stop_rgb(background, 1.0, 0.35, 0.65, 0.94);
	}
	rounded_rect(cr, 0.5, 0.5, width - 1.0, height - 1.0, 8.0);
	cairo_set_source(cr, background);
	cairo_fill(cr);
	cairo_pattern_destroy(background);

	rounded_rect(cr, 11.0, 10.0, width - 22.0, height - 18.0, 7.0);
	cairo_set_source_rgba(cr,
		dark ? 0.05 : 1.0,
		dark ? 0.07 : 1.0,
		dark ? 0.12 : 1.0,
		dark ? 0.76 : 0.84);
	cairo_fill(cr);

	rounded_rect(cr, 18.0, 17.0, width * 0.34, 8.0, 2.0);
	cairo_set_source_rgba(cr,
		dark ? 0.24 : 0.72,
		dark ? 0.35 : 0.88,
		dark ? 0.66 : 1.0,
		0.95);
	cairo_fill(cr);

	for (int i = 0; i < 3; i++) {
		cairo_arc(cr, width - 36.0 + i * 10.0, height - 14.0, 3.0,
			0.0, 6.28318530718);
		if (i == 0) {
			cairo_set_source_rgb(cr, 1.0, 0.38, 0.30);
		} else if (i == 1) {
			cairo_set_source_rgb(cr, 1.0, 0.78, 0.22);
		} else {
			cairo_set_source_rgb(cr, 0.24, 0.80, 0.36);
		}
		cairo_fill(cr);
	}

	rounded_rect(cr, 0.5, 0.5, width - 1.0, height - 1.0, 8.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.16 : 0.42);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}

static void draw_appearance_preview(GtkDrawingArea *area,
		cairo_t *cr,
		int width,
		int height,
		gpointer data) {
	(void)area;
	int style = GPOINTER_TO_INT(data);
	if (style == 0) {
		cairo_save(cr);
		cairo_rectangle(cr, 0, 0, width / 2.0, height);
		cairo_clip(cr);
		draw_preview_chrome(cr, width, height, false);
		cairo_restore(cr);
		cairo_save(cr);
		cairo_rectangle(cr, width / 2.0, 0, width / 2.0, height);
		cairo_clip(cr);
		draw_preview_chrome(cr, width, height, true);
		cairo_restore(cr);
		return;
	}
	draw_preview_chrome(cr, width, height, style == 2);
}

static void draw_style_preview(GtkDrawingArea *area,
		cairo_t *cr,
		int width,
		int height,
		gpointer data) {
	(void)area;
	int style = GPOINTER_TO_INT(data);
	bool dark = style == ORANGE_ICON_WIDGET_STYLE_DARK;
	bool clear = style == ORANGE_ICON_WIDGET_STYLE_CLEAR;
	bool tinted = style == ORANGE_ICON_WIDGET_STYLE_TINTED;

	cairo_pattern_t *tile = cairo_pattern_create_linear(0, 0, width, height);
	if (dark) {
		cairo_pattern_add_color_stop_rgb(tile, 0.0, 0.05, 0.06, 0.08);
		cairo_pattern_add_color_stop_rgb(tile, 1.0, 0.18, 0.19, 0.22);
	} else if (clear) {
		cairo_pattern_add_color_stop_rgba(tile, 0.0, 1.0, 1.0, 1.0, 0.28);
		cairo_pattern_add_color_stop_rgba(tile, 1.0, 0.72, 0.78, 0.84, 0.22);
	} else if (tinted) {
		cairo_pattern_add_color_stop_rgb(tile, 0.0, 0.10, 0.61, 0.95);
		cairo_pattern_add_color_stop_rgb(tile, 1.0, 0.32, 0.74, 1.00);
	} else {
		cairo_pattern_add_color_stop_rgb(tile, 0.0, 0.10, 0.56, 0.96);
		cairo_pattern_add_color_stop_rgb(tile, 1.0, 0.20, 0.73, 1.00);
	}
	rounded_rect(cr, 7.0, 3.0, width - 14.0, height - 6.0, 9.0);
	cairo_set_source(cr, tile);
	cairo_fill(cr);
	cairo_pattern_destroy(tile);

	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.52 : 0.86);
	cairo_arc(cr, width / 2.0 - 8.0, height / 2.0 + 4.0, 8.0, 3.14, 6.28);
	cairo_arc(cr, width / 2.0 + 1.0, height / 2.0, 10.0, 3.14, 6.28);
	cairo_arc(cr, width / 2.0 + 12.0, height / 2.0 + 5.0, 7.0, 3.14, 6.28);
	cairo_rectangle(cr, width / 2.0 - 17.0, height / 2.0 + 4.0, 36.0, 9.0);
	cairo_fill(cr);

	cairo_set_source_rgb(cr, 1.0, 0.80, 0.20);
	cairo_arc(cr, width / 2.0 + 13.0, height / 2.0 - 8.0, 6.0, 0.0, 6.28);
	cairo_fill(cr);
}

static void draw_account_avatar(GtkDrawingArea *area,
		cairo_t *cr,
		int width,
		int height,
		gpointer data) {
	(void)area;
	(void)data;
	double size = MIN(width, height);
	double scale = size / 42.0;
	cairo_translate(cr, ((double)width - size) / 2.0,
		((double)height - size) / 2.0);
	cairo_scale(cr, scale, scale);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	cairo_arc(cr, 21.0, 21.0, 20.0, 0.0, 2.0 * G_PI);
	cairo_set_source_rgb(cr, 0.86, 0.55, 0.38);
	cairo_fill(cr);

	cairo_arc(cr, 21.0, 18.0, 10.0, 0.0, 2.0 * G_PI);
	cairo_set_source_rgb(cr, 0.94, 0.70, 0.57);
	cairo_fill(cr);

	rounded_rect(cr, 10.5, 26.0, 21.0, 12.0, 8.0);
	cairo_set_source_rgb(cr, 0.37, 0.21, 0.16);
	cairo_fill(cr);

	cairo_arc(cr, 16.7, 18.0, 1.3, 0.0, 2.0 * G_PI);
	cairo_arc(cr, 25.3, 18.0, 1.3, 0.0, 2.0 * G_PI);
	cairo_set_source_rgb(cr, 0.16, 0.10, 0.08);
	cairo_fill(cr);

	cairo_set_source_rgba(cr, 0.35, 0.13, 0.10, 0.68);
	cairo_set_line_width(cr, 1.2);
	cairo_arc(cr, 21.0, 21.5, 4.5, 0.18 * G_PI, 0.82 * G_PI);
	cairo_stroke(cr);
}

static GtkWidget *section_label(const char *title) {
	GtkWidget *label = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_widget_add_css_class(label, "section-heading");
	return label;
}

static GtkWidget *card_new(void) {
	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(card, "settings-card");
	return card;
}

static void card_append(GtkWidget *card, GtkWidget *row) {
	if (gtk_widget_get_first_child(card) != NULL) {
		GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_add_css_class(separator, "settings-separator");
		gtk_box_append(GTK_BOX(card), separator);
	}
	gtk_box_append(GTK_BOX(card), row);
}

static GtkWidget *row_new(const char *title,
		const char *detail,
		GtkWidget *control) {
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_widget_add_css_class(row, "settings-row");

	GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_hexpand(labels, true);
	gtk_widget_set_valign(labels, GTK_ALIGN_CENTER);

	GtkWidget *label = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_add_css_class(label, "settings-row-title");
	gtk_box_append(GTK_BOX(labels), label);

	if (detail != NULL && detail[0] != '\0') {
		GtkWidget *detail_label = gtk_label_new(detail);
		gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0);
		gtk_label_set_wrap(GTK_LABEL(detail_label), true);
		gtk_label_set_wrap_mode(GTK_LABEL(detail_label), PANGO_WRAP_WORD_CHAR);
		gtk_widget_add_css_class(detail_label, "settings-row-detail");
		gtk_box_append(GTK_BOX(labels), detail_label);
	}

	gtk_box_append(GTK_BOX(row), labels);

	if (control != NULL) {
		gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), control);
	}

	return row;
}

static GtkWidget *scale_new(double value, double min, double max, double step) {
	GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		min, max, step);
	gtk_range_set_value(GTK_RANGE(scale), value);
	gtk_widget_set_size_request(scale, 220, -1);
	gtk_scale_set_draw_value(GTK_SCALE(scale), false);
	gtk_widget_add_css_class(scale, "settings-scale");
	return scale;
}

static GtkWidget *dropdown_new(const char * const *strings,
		unsigned selected,
		int width) {
	GtkWidget *dropdown = gtk_drop_down_new_from_strings(strings);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), selected);
	gtk_widget_add_css_class(dropdown, "settings-dropdown");
	gtk_widget_set_size_request(dropdown, width, -1);
	return dropdown;
}

static bool string_array_contains(GPtrArray *array, const char *value) {
	for (unsigned i = 0; i < array->len; i++) {
		if (g_strcmp0(g_ptr_array_index(array, i), value) == 0) {
			return true;
		}
	}
	return false;
}

static void string_array_add_unique(GPtrArray *array, const char *value) {
	if (value == NULL || value[0] == '\0' ||
			string_array_contains(array, value)) {
		return;
	}
	g_ptr_array_add(array, g_strdup(value));
}

static int compare_string_ptrs(gconstpointer a, gconstpointer b) {
	const char * const *left = a;
	const char * const *right = b;
	return g_ascii_strcasecmp(*left, *right);
}

static void root_array_add_unique(GPtrArray *roots, const char *path) {
	if (path == NULL || path[0] == '\0' ||
			!g_file_test(path, G_FILE_TEST_IS_DIR) ||
			string_array_contains(roots, path)) {
		return;
	}
	g_ptr_array_add(roots, g_strdup(path));
}

static void root_array_add_child(GPtrArray *roots,
		const char *base,
		const char *child) {
	if (base == NULL || base[0] == '\0') {
		return;
	}
	char *path = g_build_filename(base, child, NULL);
	root_array_add_unique(roots, path);
	g_free(path);
}

static char *theme_path_expand_home(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return NULL;
	}
	if (path[0] != '~' ||
			(path[1] != '\0' && path[1] != G_DIR_SEPARATOR)) {
		return g_strdup(path);
	}

	const char *home = g_get_home_dir();
	if (home == NULL || home[0] == '\0') {
		return g_strdup(path);
	}
	if (path[1] == '\0') {
		return g_strdup(home);
	}
	return g_build_filename(home, path + 2, NULL);
}

static GPtrArray *theme_roots_new(const char *subdir) {
	GPtrArray *roots = g_ptr_array_new_with_free_func(g_free);
	root_array_add_child(roots, g_get_user_data_dir(), subdir);

	const char *home = g_get_home_dir();
	if (home != NULL && home[0] != '\0') {
		char *local_share = g_build_filename(home, ".local", "share", NULL);
		root_array_add_child(roots, local_share, subdir);
		g_free(local_share);
		if (strcmp(subdir, "themes") == 0) {
			root_array_add_child(roots, home, ".themes");
		} else if (strcmp(subdir, "icons") == 0) {
			root_array_add_child(roots, home, ".icons");
		}
	}

	const char * const *system_dirs = g_get_system_data_dirs();
	for (int i = 0; system_dirs != NULL && system_dirs[i] != NULL; i++) {
		root_array_add_child(roots, system_dirs[i], subdir);
	}
	return roots;
}

static void cursor_roots_add_xcursor_entry(GPtrArray *roots,
		const char *entry) {
	if (entry == NULL || entry[0] == '\0') {
		return;
	}

	char *path = theme_path_expand_home(entry);
	if (path == NULL) {
		return;
	}

	char *cursors = g_build_filename(path, "cursors", NULL);
	if (g_file_test(cursors, G_FILE_TEST_IS_DIR)) {
		char *parent = g_path_get_dirname(path);
		root_array_add_unique(roots, parent);
		g_free(parent);
		g_free(cursors);
		g_free(path);
		return;
	}
	g_free(cursors);

	char *basename = g_path_get_basename(path);
	if (strcmp(basename, "cursors") == 0) {
		char *theme_dir = g_path_get_dirname(path);
		char *parent = g_path_get_dirname(theme_dir);
		root_array_add_unique(roots, parent);
		g_free(parent);
		g_free(theme_dir);
	} else {
		root_array_add_unique(roots, path);
	}
	g_free(basename);
	g_free(path);
}

static void cursor_roots_add_local_paths(GPtrArray *roots) {
	root_array_add_child(roots, g_get_user_data_dir(), "cursors");

	const char *home = g_get_home_dir();
	if (home != NULL && home[0] != '\0') {
		char *local_share = g_build_filename(home, ".local", "share", NULL);
		root_array_add_child(roots, local_share, "cursors");
		g_free(local_share);
		root_array_add_child(roots, home, ".cursors");
	}

	const char * const *system_dirs = g_get_system_data_dirs();
	for (int i = 0; system_dirs != NULL && system_dirs[i] != NULL; i++) {
		root_array_add_child(roots, system_dirs[i], "cursors");
	}
}

static void cursor_roots_add_xcursor_path(GPtrArray *roots) {
	const char *xcursor_path = g_getenv("XCURSOR_PATH");
	if (xcursor_path == NULL || xcursor_path[0] == '\0') {
		return;
	}

	char **paths = g_strsplit(xcursor_path, ":", -1);
	for (char **path = paths; path != NULL && *path != NULL; path++) {
		cursor_roots_add_xcursor_entry(roots, *path);
	}
	g_strfreev(paths);
}

static bool theme_file_exists(const char *theme_path,
		const char *subdir,
		const char *file) {
	char *path = g_build_filename(theme_path, subdir, file, NULL);
	bool exists = g_file_test(path, G_FILE_TEST_IS_REGULAR);
	g_free(path);
	return exists;
}

static bool is_gtk_theme_dir(const char *path) {
	return theme_file_exists(path, "gtk-4.0", "gtk.css") ||
		theme_file_exists(path, "gtk-4.0", "gtk-dark.css");
}

static bool is_icon_theme_dir(const char *path) {
	return theme_file_exists(path, ".", "index.theme");
}

static bool is_cursor_theme_dir(const char *path, const char *name) {
	char *cursors = g_build_filename(path, "cursors", NULL);
	bool valid = g_file_test(cursors, G_FILE_TEST_IS_DIR);
	g_free(cursors);
	return valid ||
		(strcmp(name, "default") == 0 && is_icon_theme_dir(path));
}

static void scan_theme_root(GPtrArray *names,
		const char *root,
		enum theme_dropdown_kind kind) {
	GDir *dir = g_dir_open(root, 0, NULL);
	if (dir == NULL) {
		return;
	}

	const char *name = NULL;
	while ((name = g_dir_read_name(dir)) != NULL) {
		char *path = g_build_filename(root, name, NULL);
		bool valid = g_file_test(path, G_FILE_TEST_IS_DIR);
		if (valid && kind == THEME_DROPDOWN_GTK) {
			valid = is_gtk_theme_dir(path);
		} else if (valid && kind == THEME_DROPDOWN_ICON) {
			valid = is_icon_theme_dir(path);
		} else if (valid && kind == THEME_DROPDOWN_CURSOR) {
			valid = is_cursor_theme_dir(path, name);
		}
		if (valid) {
			string_array_add_unique(names, name);
		}
		g_free(path);
	}
	g_dir_close(dir);
}

static struct dropdown_options *dropdown_options_new(
		enum theme_dropdown_kind kind,
		const char *current_value) {
	GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
	string_array_add_unique(names, current_value);

	GPtrArray *roots = kind == THEME_DROPDOWN_GTK ?
		theme_roots_new("themes") : theme_roots_new("icons");
	if (kind == THEME_DROPDOWN_CURSOR) {
		cursor_roots_add_local_paths(roots);
		cursor_roots_add_xcursor_path(roots);
	}
	for (unsigned i = 0; i < roots->len; i++) {
		scan_theme_root(names, g_ptr_array_index(roots, i), kind);
	}
	g_ptr_array_free(roots, true);
	g_ptr_array_sort(names, compare_string_ptrs);

	struct dropdown_options *options = g_new0(struct dropdown_options, 1);
	options->count = names->len + 1;
	options->labels = g_new0(char *, options->count + 1);
	options->values = g_new0(char *, options->count + 1);
	options->labels[0] = g_strdup("System Default");
	options->values[0] = g_strdup("");
	for (unsigned i = 0; i < names->len; i++) {
		const char *name = g_ptr_array_index(names, i);
		options->labels[i + 1] = g_strdup(name);
		options->values[i + 1] = g_strdup(name);
	}
	g_ptr_array_free(names, true);
	return options;
}

static void dropdown_options_free(gpointer data) {
	struct dropdown_options *options = data;
	if (options == NULL) {
		return;
	}
	g_strfreev(options->labels);
	g_strfreev(options->values);
	g_free(options);
}

static unsigned dropdown_options_index_for_value(
		const struct dropdown_options *options,
		const char *value) {
	for (unsigned i = 0; i < options->count; i++) {
		if (g_strcmp0(options->values[i], value != NULL ? value : "") == 0) {
			return i;
		}
	}
	return 0;
}

static GtkWidget *theme_dropdown_new(enum theme_dropdown_kind kind,
		const char *current_value,
		int width) {
	struct dropdown_options *options = dropdown_options_new(kind, current_value);
	GtkWidget *dropdown = gtk_drop_down_new_from_strings(
		(const char * const *)options->labels);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown),
		dropdown_options_index_for_value(options, current_value));
	gtk_widget_add_css_class(dropdown, "settings-dropdown");
	gtk_widget_set_size_request(dropdown, width, -1);
	g_object_set_data_full(G_OBJECT(dropdown),
		"orange-dropdown-options",
		options,
		dropdown_options_free);
	return dropdown;
}

static const char *theme_dropdown_selected_value(GtkDropDown *dropdown) {
	struct dropdown_options *options = g_object_get_data(G_OBJECT(dropdown),
		"orange-dropdown-options");
	if (options == NULL) {
		return "";
	}
	unsigned selected = gtk_drop_down_get_selected(dropdown);
	if (selected >= options->count) {
		return "";
	}
	return options->values[selected];
}

static void copy_config_string(char *destination,
		size_t destination_size,
		const char *text) {
	snprintf(destination, destination_size, "%s", text != NULL ? text : "");
}

static GtkWidget *panel_scroller_new(GtkWidget **body_out) {
	GtkWidget *scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
		GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_widget_add_css_class(scroller, "settings-content");

	GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(body, "panel-body");
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), body);
	*body_out = body;
	return scroller;
}

static void on_appearance_toggled(GtkToggleButton *widget, gpointer data) {
	if (!gtk_toggle_button_get_active(widget)) {
		return;
	}
	struct settings_app *settings = data;
	int stored = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "appearance")) - 1;
	settings->config.appearance = (enum orange_appearance)stored;
	install_css(settings);
	save_config(settings);
}

static void update_accent_buttons(struct settings_app *settings) {
	for (int i = 0; i < ORANGE_ACCENT_COLOR_COUNT; i++) {
		if (settings->accent_buttons[i] == NULL) {
			continue;
		}
		gtk_widget_queue_draw(settings->accent_buttons[i]);
		GtkWidget *child = gtk_button_get_child(
			GTK_BUTTON(settings->accent_buttons[i]));
		if (child != NULL) {
			gtk_widget_queue_draw(child);
		}
	}
}

static void on_accent_clicked(GtkButton *button, gpointer data) {
	struct settings_app *settings = data;
	int stored = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "accent")) - 1;
	settings->config.accent_color = (enum orange_accent_color)stored;
	update_accent_buttons(settings);
	save_config(settings);
}

static void on_text_highlight(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.text_highlight_automatic =
		gtk_drop_down_get_selected(widget) == 0;
	save_config(settings);
}

static void on_folder_color(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.folder_color_automatic =
		gtk_drop_down_get_selected(widget) == 0;
	save_config(settings);
}

static void on_sidebar_icon_size(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.sidebar_icon_size = (enum orange_sidebar_icon_size)
		gtk_drop_down_get_selected(widget);
	save_config(settings);
}

static void on_tint_window_background(GtkSwitch *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.tint_window_background = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_show_scroll_bars(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.show_scroll_bars = (enum orange_scroll_bar_visibility)
		gtk_drop_down_get_selected(widget);
	save_config(settings);
}

static void on_icon_style_toggled(GtkToggleButton *widget, gpointer data) {
	if (!gtk_toggle_button_get_active(widget)) {
		return;
	}
	struct settings_app *settings = data;
	int stored = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "icon-style")) - 1;
	settings->config.icon_widget_style = (enum orange_icon_widget_style)stored;
	save_config(settings);
}

static void on_dark_mode(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.appearance = gtk_switch_get_active(widget) ?
		ORANGE_APPEARANCE_DARK : ORANGE_APPEARANCE_LIGHT;
	install_css(settings);
	save_config(settings);
}

static void on_desktop_visible(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_icons_visible = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_dock_magnification(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.dock_magnification = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_dock_indicators(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.dock_show_indicators = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_calendar_visible(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.calendar_widget_visible = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_weather_visible(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.weather_widget_visible = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_desktop_grid(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.desktop_grid_spacing = (int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_desktop_label(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.desktop_label_size = (int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_desktop_scale(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.desktop_icon_scale = gtk_range_get_value(range);
	save_config(settings);
}

static void on_dock_scale(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.dock_scale = gtk_range_get_value(range);
	save_config(settings);
}

static void on_dock_icon_scale(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.dock_icon_scale = gtk_range_get_value(range);
	save_config(settings);
}

static void on_dock_mag_scale(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.dock_magnification_scale = gtk_range_get_value(range);
	save_config(settings);
}

static void on_calendar_size(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.calendar_widget_size =
		(enum orange_widget_size)(int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_weather_size(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.weather_widget_size =
		(enum orange_widget_size)(int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_cursor_size(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.cursor_size = (int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_desktop_sort_by(GtkDropDown *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_sort_by = (enum orange_desktop_sort_by)
		gtk_drop_down_get_selected(widget);
	save_config(settings);
}

static void on_desktop_label_position(GtkDropDown *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_label_position = (enum orange_desktop_label_position)
		gtk_drop_down_get_selected(widget);
	save_config(settings);
}

static void on_desktop_show_hard_disks(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_show_hard_disks = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_desktop_show_external(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_show_external_disks = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_desktop_show_removable(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_show_removable_media = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_desktop_show_servers(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.desktop_show_servers = gtk_switch_get_active(widget);
	save_config(settings);
}

static void on_cursor_theme(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	copy_config_string(settings->config.cursor_theme,
		sizeof(settings->config.cursor_theme),
		theme_dropdown_selected_value(widget));
	save_config(settings);
}

static void on_icon_theme(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	copy_config_string(settings->config.icon_theme,
		sizeof(settings->config.icon_theme),
		theme_dropdown_selected_value(widget));
	apply_configured_gtk_settings(settings);
	save_config(settings);
}

static void on_gtk_theme_light(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	copy_config_string(settings->config.gtk_theme_light,
		sizeof(settings->config.gtk_theme_light),
		theme_dropdown_selected_value(widget));
	apply_configured_gtk_settings(settings);
	save_config(settings);
}

static void on_gtk_theme_dark(GtkDropDown *widget,
		GParamSpec *pspec,
		gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	copy_config_string(settings->config.gtk_theme_dark,
		sizeof(settings->config.gtk_theme_dark),
		theme_dropdown_selected_value(widget));
	apply_configured_gtk_settings(settings);
	save_config(settings);
}

static GtkWidget *switch_new(bool active,
		GCallback callback,
		struct settings_app *settings) {
	GtkWidget *toggle = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(toggle), active);
	g_signal_connect(toggle, "notify::active", callback, settings);
	return toggle;
}

static GtkWidget *appearance_choice_new(const char *label,
		int preview_style,
		bool sensitive) {
	GtkWidget *button = gtk_toggle_button_new();
	gtk_widget_add_css_class(button, "appearance-choice");
	gtk_widget_set_sensitive(button, sensitive);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
	GtkWidget *preview = gtk_drawing_area_new();
	gtk_widget_add_css_class(preview, "appearance-preview");
	gtk_widget_set_size_request(preview, 90, 54);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(preview),
		draw_appearance_preview,
		GINT_TO_POINTER(preview_style),
		NULL);
	gtk_box_append(GTK_BOX(box), preview);

	GtkWidget *text = gtk_label_new(label);
	gtk_widget_add_css_class(text, "appearance-choice-label");
	gtk_box_append(GTK_BOX(box), text);
	gtk_button_set_child(GTK_BUTTON(button), box);
	return button;
}

static GtkWidget *appearance_selector_new(struct settings_app *settings) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_halign(box, GTK_ALIGN_END);

	GtkWidget *auto_button = appearance_choice_new("Auto", 0, false);
	gtk_widget_set_tooltip_text(auto_button,
		"Automatic appearance will be backed by a scheduler later.");
	gtk_box_append(GTK_BOX(box), auto_button);

	GtkWidget *light_button = appearance_choice_new("Light", 1, true);
	g_object_set_data(G_OBJECT(light_button), "appearance",
		GINT_TO_POINTER(ORANGE_APPEARANCE_LIGHT + 1));
	gtk_box_append(GTK_BOX(box), light_button);

	GtkWidget *dark_button = appearance_choice_new("Dark", 2, true);
	gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(dark_button),
		GTK_TOGGLE_BUTTON(light_button));
	g_object_set_data(G_OBJECT(dark_button), "appearance",
		GINT_TO_POINTER(ORANGE_APPEARANCE_DARK + 1));
	gtk_box_append(GTK_BOX(box), dark_button);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
			settings->config.appearance == ORANGE_APPEARANCE_DARK ?
			dark_button : light_button), true);
	g_signal_connect(light_button, "toggled",
		G_CALLBACK(on_appearance_toggled), settings);
	g_signal_connect(dark_button, "toggled",
		G_CALLBACK(on_appearance_toggled), settings);
	return box;
}

static GtkWidget *swatch_button_new(struct settings_app *settings,
		enum orange_accent_color color,
		const char *tooltip) {
	GtkWidget *button = gtk_button_new();
	gtk_widget_add_css_class(button, "color-swatch");
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
	gtk_widget_set_size_request(button, SWATCH_DESIGN_SIZE, SWATCH_DESIGN_SIZE);
	gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
	gtk_widget_set_tooltip_text(button, tooltip);

	struct swatch_data *sd = g_new0(struct swatch_data, 1);
	sd->button = button;
	sd->settings = settings;
	sd->color = color;
	GtkWidget *face = gtk_drawing_area_new();
	sd->face = face;
	gtk_widget_add_css_class(face, "swatch-face");
	gtk_widget_set_size_request(face, SWATCH_DESIGN_SIZE, SWATCH_DESIGN_SIZE);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(face),
		draw_accent_swatch, sd, g_free);
	gtk_button_set_child(GTK_BUTTON(button), face);

	g_object_set_data(G_OBJECT(button), "accent", GINT_TO_POINTER(color + 1));
	g_signal_connect(button, "clicked", G_CALLBACK(on_accent_clicked), settings);
	settings->accent_buttons[color] = button;
	return button;
}

static GtkWidget *accent_swatch_row_new(struct settings_app *settings) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
	gtk_widget_add_css_class(box, "swatch-row");
	gtk_widget_set_halign(box, GTK_ALIGN_END);
	gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

	static const struct {
		enum orange_accent_color color;
		const char *tooltip;
	} swatches[] = {
		{ORANGE_ACCENT_MULTICOLOR, "Multicolor"},
		{ORANGE_ACCENT_BLUE, "Blue"},
		{ORANGE_ACCENT_PURPLE, "Purple"},
		{ORANGE_ACCENT_PINK, "Pink"},
		{ORANGE_ACCENT_RED, "Red"},
		{ORANGE_ACCENT_ORANGE, "Orange"},
		{ORANGE_ACCENT_YELLOW, "Yellow"},
		{ORANGE_ACCENT_GREEN, "Green"},
		{ORANGE_ACCENT_GRAPHITE, "Graphite"},
	};

	for (size_t i = 0; i < sizeof(swatches) / sizeof(swatches[0]); i++) {
		GtkWidget *button = swatch_button_new(settings,
			swatches[i].color,
			swatches[i].tooltip);
		if (swatches[i].color == ORANGE_ACCENT_MULTICOLOR) {
			GtkWidget *labeled = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
			gtk_widget_set_halign(labeled, GTK_ALIGN_CENTER);
			gtk_box_append(GTK_BOX(labeled), button);
			GtkWidget *label = gtk_label_new("Multicolor");
			gtk_widget_add_css_class(label, "settings-row-detail");
			gtk_box_append(GTK_BOX(labeled), label);
			gtk_box_append(GTK_BOX(box), labeled);
		} else {
			gtk_box_append(GTK_BOX(box), button);
		}
	}
	update_accent_buttons(settings);
	return box;
}

static GtkWidget *style_choice_new(struct settings_app *settings,
		enum orange_icon_widget_style style,
		const char *title,
		GtkToggleButton *group) {
	GtkWidget *button = gtk_toggle_button_new();
	gtk_widget_add_css_class(button, "style-choice");
	if (group != NULL) {
		gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(button), group);
	}

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
	GtkWidget *preview = gtk_drawing_area_new();
	gtk_widget_add_css_class(preview, "style-preview");
	gtk_widget_set_size_request(preview, 48, 38);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(preview),
		draw_style_preview,
		GINT_TO_POINTER(style),
		NULL);
	gtk_box_append(GTK_BOX(box), preview);

	GtkWidget *label = gtk_label_new(title);
	gtk_widget_add_css_class(label, "appearance-choice-label");
	gtk_box_append(GTK_BOX(box), label);
	gtk_button_set_child(GTK_BUTTON(button), box);

	g_object_set_data(G_OBJECT(button), "icon-style", GINT_TO_POINTER(style + 1));
	settings->icon_style_buttons[style] = button;
	return button;
}

static GtkWidget *icon_style_selector_new(struct settings_app *settings) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(box, GTK_ALIGN_END);

	GtkWidget *default_button = style_choice_new(settings,
		ORANGE_ICON_WIDGET_STYLE_DEFAULT, "Default", NULL);
	gtk_box_append(GTK_BOX(box), default_button);
	gtk_box_append(GTK_BOX(box), style_choice_new(settings,
		ORANGE_ICON_WIDGET_STYLE_DARK, "Dark", GTK_TOGGLE_BUTTON(default_button)));
	gtk_box_append(GTK_BOX(box), style_choice_new(settings,
		ORANGE_ICON_WIDGET_STYLE_CLEAR, "Clear", GTK_TOGGLE_BUTTON(default_button)));
	gtk_box_append(GTK_BOX(box), style_choice_new(settings,
		ORANGE_ICON_WIDGET_STYLE_TINTED, "Tinted", GTK_TOGGLE_BUTTON(default_button)));

	if (settings->config.icon_widget_style >= 0 &&
			settings->config.icon_widget_style < ORANGE_ICON_WIDGET_STYLE_COUNT &&
			settings->icon_style_buttons[settings->config.icon_widget_style] != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
			settings->icon_style_buttons[settings->config.icon_widget_style]), true);
	}
	for (int i = 0; i < ORANGE_ICON_WIDGET_STYLE_COUNT; i++) {
		if (settings->icon_style_buttons[i] != NULL) {
			g_signal_connect(settings->icon_style_buttons[i], "toggled",
				G_CALLBACK(on_icon_style_toggled), settings);
		}
	}
	return box;
}

static GtkWidget *build_appearance_panel(struct settings_app *settings) {
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	GtkWidget *appearance_card = card_new();
	card_append(appearance_card,
		row_new("Appearance", NULL, appearance_selector_new(settings)));
	gtk_box_append(GTK_BOX(body), appearance_card);

	gtk_box_append(GTK_BOX(body), section_label("Theme"));
	GtkWidget *theme_card = card_new();
	card_append(theme_card,
		row_new("Color", NULL, accent_swatch_row_new(settings)));

	const char * const color_modes[] = {"Automatic", "Accent Color", NULL};
	GtkWidget *highlight = dropdown_new(color_modes,
		settings->config.text_highlight_automatic ? 0 : 1,
		150);
	g_signal_connect(highlight, "notify::selected",
		G_CALLBACK(on_text_highlight), settings);
	card_append(theme_card, row_new("Text highlight color", NULL, highlight));

	card_append(theme_card,
		row_new("Icon & widget style", NULL, icon_style_selector_new(settings)));

	GtkWidget *folder = dropdown_new(color_modes,
		settings->config.folder_color_automatic ? 0 : 1,
		150);
	g_signal_connect(folder, "notify::selected",
		G_CALLBACK(on_folder_color), settings);
	card_append(theme_card, row_new("Folder color", NULL, folder));
	gtk_box_append(GTK_BOX(body), theme_card);

	gtk_box_append(GTK_BOX(body), section_label("Windows"));
	GtkWidget *windows_card = card_new();
	const char * const icon_sizes[] = {"Small", "Medium", "Large", NULL};
	GtkWidget *sidebar_size = dropdown_new(icon_sizes,
		settings->config.sidebar_icon_size,
		150);
	g_signal_connect(sidebar_size, "notify::selected",
		G_CALLBACK(on_sidebar_icon_size), settings);
	card_append(windows_card, row_new("Sidebar icon size", NULL, sidebar_size));

	card_append(windows_card,
		row_new("Tint window background with wallpaper color", NULL,
			switch_new(settings->config.tint_window_background,
				G_CALLBACK(on_tint_window_background), settings)));

	const char * const scroll_bar_modes[] = {
		"Automatic", "When Scrolling", "Always", NULL,
	};
	GtkWidget *scroll_bars = dropdown_new(scroll_bar_modes,
		settings->config.show_scroll_bars,
		150);
	g_signal_connect(scroll_bars, "notify::selected",
		G_CALLBACK(on_show_scroll_bars), settings);
	card_append(windows_card, row_new("Show Scroll Bars", NULL, scroll_bars));
	gtk_box_append(GTK_BOX(body), windows_card);

	gtk_box_append(GTK_BOX(body), section_label("GTK Clients"));
	GtkWidget *gtk_card = card_new();
	GtkWidget *icon_theme = theme_dropdown_new(THEME_DROPDOWN_ICON,
		settings->config.icon_theme,
		220);
	g_signal_connect(icon_theme, "notify::selected",
		G_CALLBACK(on_icon_theme), settings);
	card_append(gtk_card, row_new("Icon Theme", NULL, icon_theme));

	GtkWidget *light_theme = theme_dropdown_new(THEME_DROPDOWN_GTK,
		settings->config.gtk_theme_light,
		220);
	g_signal_connect(light_theme, "notify::selected",
		G_CALLBACK(on_gtk_theme_light), settings);
	card_append(gtk_card,
		row_new("Light GTK Theme", NULL, light_theme));

	GtkWidget *dark_theme = theme_dropdown_new(THEME_DROPDOWN_GTK,
		settings->config.gtk_theme_dark,
		220);
	g_signal_connect(dark_theme, "notify::selected",
		G_CALLBACK(on_gtk_theme_dark), settings);
	card_append(gtk_card,
		row_new("Dark GTK Theme", NULL, dark_theme));
	gtk_box_append(GTK_BOX(body), gtk_card);

	return scroller;
}

static GtkWidget *build_desktop_dock_panel(struct settings_app *settings) {
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Desktop Icons"));
	GtkWidget *desktop_card = card_new();
	card_append(desktop_card,
		row_new("Show Desktop Icons", NULL,
			switch_new(settings->config.desktop_icons_visible,
				G_CALLBACK(on_desktop_visible), settings)));

	const char * const sort_names[] = {
		"None", "Snap to Grid", "Name", "Date Added",
		"Date Modified", "Size", "Kind", NULL,
	};
	GtkWidget *sort_by = dropdown_new(sort_names,
		settings->config.desktop_sort_by,
		220);
	g_signal_connect(sort_by, "notify::selected",
		G_CALLBACK(on_desktop_sort_by), settings);
	card_append(desktop_card, row_new("Sort By", NULL, sort_by));

	const char * const label_pos_names[] = {"Bottom", "Right", NULL};
	GtkWidget *label_pos = dropdown_new(label_pos_names,
		settings->config.desktop_label_position,
		220);
	g_signal_connect(label_pos, "notify::selected",
		G_CALLBACK(on_desktop_label_position), settings);
	card_append(desktop_card, row_new("Label Position", NULL, label_pos));

	GtkWidget *grid = scale_new(settings->config.desktop_grid_spacing, 8, 96, 1);
	g_signal_connect(grid, "value-changed", G_CALLBACK(on_desktop_grid), settings);
	card_append(desktop_card, row_new("Grid Spacing", NULL, grid));

	GtkWidget *label = scale_new(settings->config.desktop_label_size, 10, 28, 1);
	g_signal_connect(label, "value-changed", G_CALLBACK(on_desktop_label), settings);
	card_append(desktop_card, row_new("Label Size", NULL, label));

	GtkWidget *desktop_scale =
		scale_new(settings->config.desktop_icon_scale, 0.60, 2.00, 0.05);
	g_signal_connect(desktop_scale, "value-changed",
		G_CALLBACK(on_desktop_scale), settings);
	card_append(desktop_card, row_new("Icon Scale", NULL, desktop_scale));

	card_append(desktop_card,
		row_new("Hard Disks", NULL,
			switch_new(settings->config.desktop_show_hard_disks,
				G_CALLBACK(on_desktop_show_hard_disks), settings)));
	card_append(desktop_card,
		row_new("External Disks", NULL,
			switch_new(settings->config.desktop_show_external_disks,
				G_CALLBACK(on_desktop_show_external), settings)));
	card_append(desktop_card,
		row_new("Removable Media", NULL,
			switch_new(settings->config.desktop_show_removable_media,
				G_CALLBACK(on_desktop_show_removable), settings)));
	card_append(desktop_card,
		row_new("Shared Servers", NULL,
			switch_new(settings->config.desktop_show_servers,
				G_CALLBACK(on_desktop_show_servers), settings)));
	gtk_box_append(GTK_BOX(body), desktop_card);

	gtk_box_append(GTK_BOX(body), section_label("Dock"));
	GtkWidget *dock_card = card_new();
	GtkWidget *dock_scale = scale_new(settings->config.dock_scale, 0.60, 1.60, 0.05);
	g_signal_connect(dock_scale, "value-changed", G_CALLBACK(on_dock_scale), settings);
	card_append(dock_card, row_new("Dock Size", NULL, dock_scale));

	GtkWidget *dock_icon =
		scale_new(settings->config.dock_icon_scale, 0.60, 1.80, 0.05);
	g_signal_connect(dock_icon, "value-changed",
		G_CALLBACK(on_dock_icon_scale), settings);
	card_append(dock_card, row_new("Icon Scale", NULL, dock_icon));

	card_append(dock_card,
		row_new("Magnification", NULL,
			switch_new(settings->config.dock_magnification,
				G_CALLBACK(on_dock_magnification), settings)));

	GtkWidget *mag_scale =
		scale_new(settings->config.dock_magnification_scale, 1.00, 2.20, 0.05);
	g_signal_connect(mag_scale, "value-changed",
		G_CALLBACK(on_dock_mag_scale), settings);
	card_append(dock_card, row_new("Magnification Strength", NULL, mag_scale));

	card_append(dock_card,
		row_new("Active Indicator Dots", NULL,
			switch_new(settings->config.dock_show_indicators,
				G_CALLBACK(on_dock_indicators), settings)));
	gtk_box_append(GTK_BOX(body), dock_card);

	return scroller;
}

static GtkWidget *build_widgets_panel(struct settings_app *settings) {
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Desktop Widgets"));
	GtkWidget *widget_card = card_new();
	card_append(widget_card,
		row_new("Show Calendar", NULL,
			switch_new(settings->config.calendar_widget_visible,
				G_CALLBACK(on_calendar_visible), settings)));
	GtkWidget *calendar_size =
		scale_new(settings->config.calendar_widget_size, 0, 2, 1);
	g_signal_connect(calendar_size, "value-changed",
		G_CALLBACK(on_calendar_size), settings);
	card_append(widget_card, row_new("Calendar Size", NULL, calendar_size));

	card_append(widget_card,
		row_new("Show Weather", NULL,
			switch_new(settings->config.weather_widget_visible,
				G_CALLBACK(on_weather_visible), settings)));
	GtkWidget *weather_size =
		scale_new(settings->config.weather_widget_size, 0, 2, 1);
	g_signal_connect(weather_size, "value-changed",
		G_CALLBACK(on_weather_size), settings);
	card_append(widget_card, row_new("Weather Size", NULL, weather_size));
	gtk_box_append(GTK_BOX(body), widget_card);

	return scroller;
}

static GtkWidget *build_accessibility_panel(struct settings_app *settings) {
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Pointer"));
	GtkWidget *cursor_card = card_new();
	GtkWidget *cursor_theme = theme_dropdown_new(THEME_DROPDOWN_CURSOR,
		settings->config.cursor_theme,
		220);
	g_signal_connect(cursor_theme, "notify::selected",
		G_CALLBACK(on_cursor_theme), settings);
	card_append(cursor_card, row_new("Cursor Theme", NULL, cursor_theme));

	GtkWidget *cursor_size = scale_new(settings->config.cursor_size, 16, 96, 1);
	g_signal_connect(cursor_size, "value-changed", G_CALLBACK(on_cursor_size), settings);
	card_append(cursor_card, row_new("Cursor Size", NULL, cursor_size));
	gtk_box_append(GTK_BOX(body), cursor_card);

	return scroller;
}

static GtkWidget *build_general_panel(struct settings_app *settings) {
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Orange"));
	GtkWidget *card = card_new();
	GtkWidget *path = gtk_label_new(settings->config_path);
	gtk_label_set_xalign(GTK_LABEL(path), 1.0);
	gtk_label_set_ellipsize(GTK_LABEL(path), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_size_request(path, 220, -1);
	card_append(card, row_new("Configuration File", NULL, path));

	GtkWidget *appearance = switch_new(
		settings->config.appearance == ORANGE_APPEARANCE_DARK,
		G_CALLBACK(on_dark_mode),
		settings);
	card_append(card, row_new("Dark Appearance", NULL, appearance));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_family_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Family"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Family Sharing", NULL, value_label_new("Off")));
	card_append(card, row_new("Purchase Sharing", NULL, value_label_new("Off")));
	card_append(card, row_new("Screen Time", NULL, value_label_new("Off")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_wifi_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Wi-Fi"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Wi-Fi", NULL, disabled_switch_new(false)));
	card_append(card, row_new("Network Name", NULL, value_label_new("Not Connected")));
	card_append(card, row_new("Known Networks", NULL, value_label_new("None")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_bluetooth_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Bluetooth"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Bluetooth", NULL, disabled_switch_new(false)));
	card_append(card, row_new("Devices", NULL, value_label_new("None")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_network_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Network"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Ethernet", NULL, value_label_new("Not Connected")));
	card_append(card, row_new("DNS", NULL, value_label_new("Automatic")));
	card_append(card, row_new("Firewall", NULL, value_label_new("Off")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_vpn_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("VPN"));
	GtkWidget *card = card_new();
	card_append(card, row_new("VPN Configurations", NULL, value_label_new("None")));
	card_append(card, row_new("Connect On Demand", NULL, disabled_switch_new(false)));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_energy_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Energy"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Low Power Mode", NULL, disabled_switch_new(false)));
	card_append(card, row_new("Display Sleep", NULL, value_label_new("Never")));
	card_append(card, row_new("Wake For Network Access", NULL, disabled_switch_new(false)));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_menu_bar_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Menu Bar"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Wi-Fi", NULL, value_label_new("Show in Menu Bar")));
	card_append(card, row_new("Sound", NULL, value_label_new("Show in Menu Bar")));
	card_append(card, row_new("Battery", NULL, value_label_new("Show in Menu Bar")));
	card_append(card, row_new("Clock", NULL, value_label_new("Always Shown")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_intelligence_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Apple Intelligence & Siri"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Apple Intelligence", NULL, value_label_new("Not Available")));
	card_append(card, row_new("Siri", NULL, disabled_switch_new(false)));
	card_append(card, row_new("Language", NULL, value_label_new("English")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_displays_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Displays"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Brightness", NULL, disabled_scale_new(0.72)));
	card_append(card, row_new("Resolution", NULL, value_label_new("Default")));
	card_append(card, row_new("Refresh Rate", NULL, value_label_new("Automatic")));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static GtkWidget *build_spotlight_panel(struct settings_app *settings) {
	(void)settings;
	GtkWidget *body = NULL;
	GtkWidget *scroller = panel_scroller_new(&body);

	gtk_box_append(GTK_BOX(body), section_label("Spotlight"));
	GtkWidget *card = card_new();
	card_append(card, row_new("Applications", NULL, disabled_switch_new(true)));
	card_append(card, row_new("Documents", NULL, disabled_switch_new(true)));
	card_append(card, row_new("System Settings", NULL, disabled_switch_new(true)));
	gtk_box_append(GTK_BOX(body), card);

	return scroller;
}

static bool string_contains_casefold(const char *haystack, const char *needle) {
	if (needle == NULL || needle[0] == '\0') {
		return true;
	}
	if (haystack == NULL) {
		return false;
	}

	char *lower_haystack = g_ascii_strdown(haystack, -1);
	char *lower_needle = g_ascii_strdown(needle, -1);
	bool contains = strstr(lower_haystack, lower_needle) != NULL;
	g_free(lower_haystack);
	g_free(lower_needle);
	return contains;
}

static gboolean filter_sidebar_row(GtkListBoxRow *row, gpointer data) {
	struct settings_app *settings = data;
	const char *query = gtk_editable_get_text(GTK_EDITABLE(settings->search_entry));
	const char *title = g_object_get_data(G_OBJECT(row), "search-title");
	return string_contains_casefold(title, query);
}

static void on_search_changed(GtkSearchEntry *entry, gpointer data) {
	(void)entry;
	struct settings_app *settings = data;
	gtk_list_box_invalidate_filter(settings->sidebar_list);
}

static void on_sidebar_row_selected(GtkListBox *box,
		GtkListBoxRow *row,
		gpointer data) {
	(void)box;
	if (row == NULL) {
		return;
	}
	struct settings_app *settings = data;
	const char *panel_id = g_object_get_data(G_OBJECT(row), "panel-id");
	const char *title = g_object_get_data(G_OBJECT(row), "search-title");
	if (panel_id == NULL || title == NULL) {
		return;
	}
	gtk_stack_set_visible_child_name(settings->stack, panel_id);
	gtk_label_set_text(GTK_LABEL(settings->title_label), title);
}

static GtkWidget *account_row_new(void) {
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_add_css_class(row, "settings-account");

	const char *name = g_get_real_name();
	if (name == NULL || name[0] == '\0' || strcmp(name, "Unknown") == 0) {
		name = g_get_user_name();
	}
	if (name == NULL || name[0] == '\0') {
		name = "Orange User";
	}

	GtkWidget *avatar = gtk_drawing_area_new();
	gtk_widget_add_css_class(avatar, "account-avatar");
	gtk_widget_set_size_request(avatar, 42, 42);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(avatar),
		draw_account_avatar, NULL, NULL);
	gtk_widget_set_valign(avatar, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(row), avatar);

	GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	gtk_widget_set_valign(labels, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(labels, true);
	GtkWidget *name_label = gtk_label_new(name);
	gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
	gtk_widget_add_css_class(name_label, "account-name");
	gtk_box_append(GTK_BOX(labels), name_label);

	GtkWidget *kind_label = gtk_label_new("Local Account");
	gtk_label_set_xalign(GTK_LABEL(kind_label), 0.0);
	gtk_widget_add_css_class(kind_label, "account-kind");
	gtk_box_append(GTK_BOX(labels), kind_label);

	gtk_box_append(GTK_BOX(row), labels);
	return row;
}

static const char *first_available_icon_name(
		const char *primary,
		const char * const *fallbacks) {
	GdkDisplay *display = gdk_display_get_default();
	GtkIconTheme *theme = display != NULL ?
		gtk_icon_theme_get_for_display(display) : NULL;
	if (theme == NULL || gtk_icon_theme_has_icon(theme, primary)) {
		return primary;
	}
	for (int i = 0; fallbacks != NULL && fallbacks[i] != NULL; i++) {
		if (gtk_icon_theme_has_icon(theme, fallbacks[i])) {
			return fallbacks[i];
		}
	}
	return primary;
}

static const char *sidebar_icon_name_for_item(const struct nav_item *item) {
	if (strcmp(item->panel_id, "family") == 0) {
		const char * const fallbacks[] = {
			"avatar-default-symbolic", "system-users-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "wifi") == 0) {
		const char * const fallbacks[] = {
			"network-wireless-symbolic", "network-wireless-signal-good-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "bluetooth") == 0) {
		const char * const fallbacks[] = {
			"bluetooth-symbolic", "bluetooth-active-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "network") == 0) {
		const char * const fallbacks[] = {
			"network-workgroup", "preferences-system-network-symbolic",
			"network-workgroup-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "vpn") == 0) {
		const char * const fallbacks[] = {
			"network-vpn", "network-vpn-symbolic",
			"preferences-system-network-proxy-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "energy") == 0) {
		const char * const fallbacks[] = {
			"battery", "battery-symbolic", "battery-good-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "general") == 0) {
		const char * const fallbacks[] = {
			"preferences-system-symbolic", "applications-system", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "accessibility") == 0) {
		const char * const fallbacks[] = {
			"preferences-desktop-accessibility-symbolic",
			"preferences-system-parental-controls-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "appearance") == 0) {
		const char * const fallbacks[] = {
			"preferences-desktop-appearance-symbolic",
			"preferences-desktop-theme-applications",
			"application-x-theme", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "menu-bar") == 0) {
		const char * const fallbacks[] = {
			"view-grid-symbolic", "view-app-grid-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "intelligence") == 0) {
		const char * const fallbacks[] = {
			"preferences-system-search-symbolic", "system-search-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "desktop-dock") == 0) {
		const char * const fallbacks[] = {
			"preferences-desktop-symbolic", "user-desktop",
			"user-desktop-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "displays") == 0) {
		const char * const fallbacks[] = {
			"video-display", "video-display-symbolic",
			"preferences-desktop-display-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "spotlight") == 0) {
		const char * const fallbacks[] = {
			"system-search-symbolic", "preferences-system-search-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}
	if (strcmp(item->panel_id, "widgets") == 0) {
		const char * const fallbacks[] = {
			"applications-graphics-symbolic", "applications-system-symbolic", NULL,
		};
		return first_available_icon_name(item->icon_name, fallbacks);
	}

	const char * const fallbacks[] = {
		"preferences-system-symbolic", NULL,
	};
	return first_available_icon_name(item->icon_name, fallbacks);
}

static GtkWidget *sidebar_row_new(const struct nav_item *item) {
	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
	gtk_widget_add_css_class(content, "settings-sidebar-row");

	GtkWidget *icon = gtk_image_new_from_icon_name(
		sidebar_icon_name_for_item(item));
	gtk_image_set_pixel_size(GTK_IMAGE(icon), 28);
	gtk_widget_add_css_class(icon, "sidebar-theme-icon");
	gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(icon, 28, 28);
	gtk_box_append(GTK_BOX(content), icon);

	GtkWidget *label = gtk_label_new(item->title);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_add_css_class(label, "sidebar-title");
	gtk_widget_set_hexpand(label, true);
	gtk_box_append(GTK_BOX(content), label);

	GtkWidget *row = gtk_list_box_row_new();
	if (item->group_start) {
		gtk_widget_add_css_class(row, "group-start");
	}
	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), content);
	g_object_set_data_full(G_OBJECT(row), "panel-id",
		g_strdup(item->panel_id), g_free);
	g_object_set_data_full(G_OBJECT(row), "search-title",
		g_strdup(item->title), g_free);
	return row;
}

static GtkWidget *sidebar_new(struct settings_app *settings) {
	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(sidebar, "settings-sidebar");
	gtk_widget_set_size_request(sidebar, SIDEBAR_WIDTH, -1);
	gtk_widget_set_margin_start(sidebar, 10);
	gtk_widget_set_margin_top(sidebar, 10);
	gtk_widget_set_margin_bottom(sidebar, 10);
	gtk_widget_set_margin_end(sidebar, 10);
	gtk_widget_set_overflow(sidebar, GTK_OVERFLOW_HIDDEN);

	gtk_box_append(GTK_BOX(sidebar), sidebar_chrome_new(settings));

	settings->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
	gtk_search_entry_set_placeholder_text(settings->search_entry, "Search");
	gtk_widget_add_css_class(GTK_WIDGET(settings->search_entry), "settings-search");
	g_signal_connect(settings->search_entry, "search-changed",
		G_CALLBACK(on_search_changed), settings);
	gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(settings->search_entry));

	gtk_box_append(GTK_BOX(sidebar), account_row_new());

	GtkWidget *scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
		GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroller, true);

	settings->sidebar_list = GTK_LIST_BOX(gtk_list_box_new());
	gtk_widget_add_css_class(GTK_WIDGET(settings->sidebar_list),
		"settings-sidebar-list");
	gtk_list_box_set_selection_mode(settings->sidebar_list,
		GTK_SELECTION_SINGLE);
	gtk_list_box_set_filter_func(settings->sidebar_list,
		filter_sidebar_row,
		settings,
		NULL);
	g_signal_connect(settings->sidebar_list, "row-selected",
		G_CALLBACK(on_sidebar_row_selected), settings);

	for (size_t i = 0; i < sizeof(nav_items) / sizeof(nav_items[0]); i++) {
		gtk_list_box_append(settings->sidebar_list,
			sidebar_row_new(&nav_items[i]));
	}

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),
		GTK_WIDGET(settings->sidebar_list));
	gtk_box_append(GTK_BOX(sidebar), scroller);
	return sidebar;
}

static GtkWidget *toolbar_new(struct settings_app *settings) {
	GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_widget_add_css_class(toolbar, "panel-toolbar");

	GtkWidget *nav = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(nav, "toolbar-nav");
	GtkWidget *back = gtk_button_new_from_icon_name("go-previous-symbolic");
	GtkWidget *forward = gtk_button_new_from_icon_name("go-next-symbolic");
	gtk_widget_set_sensitive(back, false);
	gtk_widget_set_sensitive(forward, false);
	gtk_box_append(GTK_BOX(nav), back);
	gtk_box_append(GTK_BOX(nav), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
	gtk_box_append(GTK_BOX(nav), forward);
	gtk_box_append(GTK_BOX(toolbar), nav);

	settings->title_label = gtk_label_new("Appearance");
	gtk_label_set_xalign(GTK_LABEL(settings->title_label), 0.0);
	gtk_widget_add_css_class(settings->title_label, "panel-title");
	gtk_widget_set_hexpand(settings->title_label, true);
	add_titlebar_drag(settings->title_label);
	gtk_box_append(GTK_BOX(toolbar), settings->title_label);
	return toolbar;
}

static void add_panels(struct settings_app *settings) {
	gtk_stack_add_named(settings->stack,
		build_family_panel(settings),
		"family");
	gtk_stack_add_named(settings->stack,
		build_wifi_panel(settings),
		"wifi");
	gtk_stack_add_named(settings->stack,
		build_bluetooth_panel(settings),
		"bluetooth");
	gtk_stack_add_named(settings->stack,
		build_network_panel(settings),
		"network");
	gtk_stack_add_named(settings->stack,
		build_vpn_panel(settings),
		"vpn");
	gtk_stack_add_named(settings->stack,
		build_energy_panel(settings),
		"energy");
	gtk_stack_add_named(settings->stack,
		build_general_panel(settings),
		"general");
	gtk_stack_add_named(settings->stack,
		build_accessibility_panel(settings),
		"accessibility");
	gtk_stack_add_named(settings->stack,
		build_appearance_panel(settings),
		"appearance");
	gtk_stack_add_named(settings->stack,
		build_menu_bar_panel(settings),
		"menu-bar");
	gtk_stack_add_named(settings->stack,
		build_intelligence_panel(settings),
		"intelligence");
	gtk_stack_add_named(settings->stack,
		build_desktop_dock_panel(settings),
		"desktop-dock");
	gtk_stack_add_named(settings->stack,
		build_displays_panel(settings),
		"displays");
	gtk_stack_add_named(settings->stack,
		build_spotlight_panel(settings),
		"spotlight");
	gtk_stack_add_named(settings->stack,
		build_widgets_panel(settings),
		"widgets");
}

static void select_sidebar_panel(struct settings_app *settings,
		const char *panel_id) {
	for (unsigned i = 0; i < sizeof(nav_items) / sizeof(nav_items[0]); i++) {
		GtkListBoxRow *row = gtk_list_box_get_row_at_index(settings->sidebar_list,
			(int)i);
		if (row == NULL) {
			continue;
		}
		const char *row_panel = g_object_get_data(G_OBJECT(row), "panel-id");
		if (row_panel != NULL && strcmp(row_panel, panel_id) == 0) {
			gtk_list_box_select_row(settings->sidebar_list, row);
			return;
		}
	}
	gtk_stack_set_visible_child_name(settings->stack, panel_id);
}

static void activate(GtkApplication *app, gpointer data) {
	struct settings_app *settings = data;
	set_left_decoration_layout();
	apply_configured_gtk_settings(settings);
	install_css(settings);

	GdkRectangle geometry;
	get_monitor_geometry(&geometry);

	GtkWidget *window = gtk_application_window_new(app);
	settings->window = GTK_WINDOW(window);
	gtk_widget_add_css_class(window, "background");
	gtk_widget_add_css_class(window, "csd");
	gtk_widget_add_css_class(window, "orange-settings-window");
	gtk_window_set_title(GTK_WINDOW(window), "System Settings");
	gtk_window_set_icon_name(GTK_WINDOW(window), "preferences-system");
	gtk_window_set_default_size(GTK_WINDOW(window),
		fit_window_dimension(SETTINGS_WINDOW_WIDTH, geometry.width, 80),
		fit_window_dimension(SETTINGS_WINDOW_HEIGHT, geometry.height, 80));
	gtk_window_set_decorated(GTK_WINDOW(window), false);
	gtk_window_set_resizable(GTK_WINDOW(window), true);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(root, "settings-root");
	gtk_widget_set_overflow(root, GTK_OVERFLOW_HIDDEN);
	gtk_widget_set_focusable(root, true);
	add_resize_edges(root);
	gtk_window_set_child(GTK_WINDOW(window), root);

	gtk_box_append(GTK_BOX(root), sidebar_new(settings));

	GtkWidget *main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(main, "settings-main");
	gtk_widget_set_hexpand(main, true);
	gtk_widget_set_vexpand(main, true);
	gtk_box_append(GTK_BOX(root), main);

	gtk_box_append(GTK_BOX(main), toolbar_new(settings));

	settings->stack = GTK_STACK(gtk_stack_new());
	gtk_stack_set_transition_type(settings->stack,
		GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
	gtk_widget_set_hexpand(GTK_WIDGET(settings->stack), true);
	gtk_widget_set_vexpand(GTK_WIDGET(settings->stack), true);
	gtk_box_append(GTK_BOX(main), GTK_WIDGET(settings->stack));
	add_panels(settings);

	select_sidebar_panel(settings, "appearance");
	gtk_window_set_focus(GTK_WINDOW(window), root);
	gtk_window_set_focus_visible(GTK_WINDOW(window), false);

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	const char *config_path = argc > 1 ? argv[1] : "orange.conf";
	struct settings_app settings = {
		.config_path = config_path,
	};
	orange_config_load(&settings.config, config_path);

	settings.app = gtk_application_new("dev.orange.Settings",
		G_APPLICATION_DEFAULT_FLAGS);
	gtk_window_set_default_icon_name("preferences-system");
	g_signal_connect(settings.app, "activate", G_CALLBACK(activate), &settings);
	int app_argc = argc > 0 ? 1 : 0;
	int status = g_application_run(G_APPLICATION(settings.app), app_argc, argv);
	g_object_unref(settings.app);
	return status;
}
