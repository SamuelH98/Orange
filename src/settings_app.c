#include "tahoe/config.h"

#include <gtk/gtk.h>
#include <stdio.h>

struct settings_app {
	GtkApplication *app;
	struct tahoe_config config;
	const char *config_path;
};

static void save_config(struct settings_app *settings) {
	tahoe_config_save(&settings->config, settings->config_path);
}

static GtkWidget *row_new(const char *title, GtkWidget *control) {
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
	gtk_widget_set_margin_top(row, 10);
	gtk_widget_set_margin_bottom(row, 10);
	gtk_widget_set_margin_start(row, 14);
	gtk_widget_set_margin_end(row, 14);

	GtkWidget *label = gtk_label_new(title);
	gtk_widget_set_hexpand(label, true);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_append(GTK_BOX(row), label);
	gtk_box_append(GTK_BOX(row), control);
	return row;
}

static GtkWidget *scale_new(double value, double min, double max, double step) {
	GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		min, max, step);
	gtk_range_set_value(GTK_RANGE(scale), value);
	gtk_widget_set_size_request(scale, 220, -1);
	gtk_scale_set_draw_value(GTK_SCALE(scale), true);
	return scale;
}

static void on_dark_mode(GtkSwitch *widget, GParamSpec *pspec, gpointer data) {
	(void)pspec;
	struct settings_app *settings = data;
	settings->config.appearance = gtk_switch_get_active(widget) ?
		TAHOE_APPEARANCE_DARK : TAHOE_APPEARANCE_LIGHT;
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
		(enum tahoe_widget_size)(int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_weather_size(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.weather_widget_size =
		(enum tahoe_widget_size)(int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_cursor_size(GtkRange *range, gpointer data) {
	struct settings_app *settings = data;
	settings->config.cursor_size = (int)gtk_range_get_value(range);
	save_config(settings);
}

static void on_cursor_theme(GtkEditable *editable, gpointer data) {
	struct settings_app *settings = data;
	const char *text = gtk_editable_get_text(editable);
	snprintf(settings->config.cursor_theme,
		sizeof(settings->config.cursor_theme),
		"%s",
		text != NULL ? text : "");
	save_config(settings);
}

static GtkWidget *section_label(const char *title) {
	GtkWidget *label = gtk_label_new(title);
	gtk_widget_set_margin_top(label, 18);
	gtk_widget_set_margin_bottom(label, 8);
	gtk_widget_set_margin_start(label, 14);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_widget_add_css_class(label, "heading");
	return label;
}

static void activate(GtkApplication *app, gpointer data) {
	struct settings_app *settings = data;
	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "System Settings");
	gtk_window_set_default_size(GTK_WINDOW(window), 680, 560);

	GtkWidget *header = gtk_header_bar_new();
	gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), true);
	gtk_window_set_titlebar(GTK_WINDOW(window), header);

	GtkWidget *scroller = gtk_scrolled_window_new();
	gtk_window_set_child(GTK_WINDOW(window), scroller);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), content);

	GtkWidget *dark = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(dark),
		settings->config.appearance == TAHOE_APPEARANCE_DARK);
	g_signal_connect(dark, "notify::active", G_CALLBACK(on_dark_mode), settings);
	gtk_box_append(GTK_BOX(content), row_new("Dark Appearance", dark));

	gtk_box_append(GTK_BOX(content), section_label("Desktop Icons"));
	GtkWidget *visible = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(visible), settings->config.desktop_icons_visible);
	g_signal_connect(visible, "notify::active",
		G_CALLBACK(on_desktop_visible), settings);
	gtk_box_append(GTK_BOX(content), row_new("Show Desktop Icons", visible));

	GtkWidget *grid = scale_new(settings->config.desktop_grid_spacing, 8, 96, 1);
	g_signal_connect(grid, "value-changed", G_CALLBACK(on_desktop_grid), settings);
	gtk_box_append(GTK_BOX(content), row_new("Grid Spacing", grid));

	GtkWidget *label = scale_new(settings->config.desktop_label_size, 10, 28, 1);
	g_signal_connect(label, "value-changed", G_CALLBACK(on_desktop_label), settings);
	gtk_box_append(GTK_BOX(content), row_new("Label Size", label));

	GtkWidget *desktop_scale =
		scale_new(settings->config.desktop_icon_scale, 0.60, 2.00, 0.05);
	g_signal_connect(desktop_scale, "value-changed",
		G_CALLBACK(on_desktop_scale), settings);
	gtk_box_append(GTK_BOX(content), row_new("Shortcut Scale", desktop_scale));

	gtk_box_append(GTK_BOX(content), section_label("Widgets"));
	GtkWidget *calendar_visible = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(calendar_visible),
		settings->config.calendar_widget_visible);
	g_signal_connect(calendar_visible, "notify::active",
		G_CALLBACK(on_calendar_visible), settings);
	gtk_box_append(GTK_BOX(content), row_new("Show Calendar", calendar_visible));

	GtkWidget *weather_visible = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(weather_visible),
		settings->config.weather_widget_visible);
	g_signal_connect(weather_visible, "notify::active",
		G_CALLBACK(on_weather_visible), settings);
	gtk_box_append(GTK_BOX(content), row_new("Show Weather", weather_visible));

	GtkWidget *calendar_size =
		scale_new(settings->config.calendar_widget_size, 0, 2, 1);
	g_signal_connect(calendar_size, "value-changed",
		G_CALLBACK(on_calendar_size), settings);
	gtk_box_append(GTK_BOX(content), row_new("Calendar Size", calendar_size));

	GtkWidget *weather_size =
		scale_new(settings->config.weather_widget_size, 0, 2, 1);
	g_signal_connect(weather_size, "value-changed",
		G_CALLBACK(on_weather_size), settings);
	gtk_box_append(GTK_BOX(content), row_new("Weather Size", weather_size));

	gtk_box_append(GTK_BOX(content), section_label("Dock"));
	GtkWidget *dock_scale = scale_new(settings->config.dock_scale, 0.60, 1.60, 0.05);
	g_signal_connect(dock_scale, "value-changed", G_CALLBACK(on_dock_scale), settings);
	gtk_box_append(GTK_BOX(content), row_new("Dock Size", dock_scale));

	GtkWidget *dock_icon =
		scale_new(settings->config.dock_icon_scale, 0.60, 1.80, 0.05);
	g_signal_connect(dock_icon, "value-changed",
		G_CALLBACK(on_dock_icon_scale), settings);
	gtk_box_append(GTK_BOX(content), row_new("Icon Scale", dock_icon));

	GtkWidget *magnification = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(magnification), settings->config.dock_magnification);
	g_signal_connect(magnification, "notify::active",
		G_CALLBACK(on_dock_magnification), settings);
	gtk_box_append(GTK_BOX(content), row_new("Magnification", magnification));

	GtkWidget *mag_scale =
		scale_new(settings->config.dock_magnification_scale, 1.00, 2.20, 0.05);
	g_signal_connect(mag_scale, "value-changed",
		G_CALLBACK(on_dock_mag_scale), settings);
	gtk_box_append(GTK_BOX(content), row_new("Magnification Strength", mag_scale));

	GtkWidget *indicators = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(indicators), settings->config.dock_show_indicators);
	g_signal_connect(indicators, "notify::active",
		G_CALLBACK(on_dock_indicators), settings);
	gtk_box_append(GTK_BOX(content), row_new("Active Indicator Dots", indicators));

	gtk_box_append(GTK_BOX(content), section_label("Cursor"));
	GtkWidget *cursor_theme = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(cursor_theme), settings->config.cursor_theme);
	gtk_widget_set_size_request(cursor_theme, 220, -1);
	g_signal_connect(cursor_theme, "changed", G_CALLBACK(on_cursor_theme), settings);
	gtk_box_append(GTK_BOX(content), row_new("Cursor Theme", cursor_theme));

	GtkWidget *cursor_size = scale_new(settings->config.cursor_size, 16, 96, 1);
	g_signal_connect(cursor_size, "value-changed", G_CALLBACK(on_cursor_size), settings);
	gtk_box_append(GTK_BOX(content), row_new("Cursor Size", cursor_size));

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	const char *config_path = argc > 1 ? argv[1] : "tahoe.conf";
	struct settings_app settings = {
		.config_path = config_path,
	};
	tahoe_config_load(&settings.config, config_path);

	settings.app = gtk_application_new("dev.tahoe.Settings",
		G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(settings.app, "activate", G_CALLBACK(activate), &settings);
	int status = g_application_run(G_APPLICATION(settings.app), argc, argv);
	g_object_unref(settings.app);
	return status;
}
