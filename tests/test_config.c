#include "orange/config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_DARK;
	config.accent_color = ORANGE_ACCENT_GREEN;
	config.text_highlight_automatic = false;
	config.icon_widget_style = ORANGE_ICON_WIDGET_STYLE_TINTED;
	config.folder_color_automatic = false;
	config.sidebar_icon_size = ORANGE_SIDEBAR_ICON_SIZE_LARGE;
	config.tint_window_background = true;
	config.show_scroll_bars = ORANGE_SCROLL_BARS_ALWAYS;
	config.desktop_icons_visible = false;
	config.desktop_grid_spacing = 40;
	config.desktop_label_size = 18;
	config.desktop_icon_scale = 1.25;
	config.desktop_positions[1] =
		(struct orange_desktop_icon_position){true, 720, 140};
	config.dock_scale = 1.20;
	config.dock_icon_scale = 1.15;
	config.dock_magnification = false;
	config.dock_magnification_scale = 1.50;
	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	config.minimize_effect = ORANGE_MINIMIZE_EFFECT_SCALE;
	config.dock_show_indicators = false;
	config.animate_opening_applications = false;
	config.calendar_widget_visible = false;
	config.weather_widget_visible = true;
	config.calendar_widget_size = ORANGE_WIDGET_SIZE_MEDIUM;
	config.weather_widget_size = ORANGE_WIDGET_SIZE_LARGE;
	snprintf(config.cursor_theme, sizeof(config.cursor_theme), "%s", "Bibata-Modern-Ice");
	config.cursor_size = 36;
	snprintf(config.icon_theme, sizeof(config.icon_theme), "%s", "Adwaita");
	snprintf(config.gtk_theme_light, sizeof(config.gtk_theme_light), "%s", "Adwaita");
	snprintf(config.gtk_theme_dark, sizeof(config.gtk_theme_dark), "%s", "Adwaita-dark");
	snprintf(config.dock_apps[0], 128, "%s", "test-app.desktop");
	snprintf(config.dock_apps[1], 128, "%s", "__trash__");

	const char *path = "/tmp/orange-config-test.conf";
	assert(orange_config_save(&config, path));

	struct orange_config loaded;
	assert(orange_config_load(&loaded, path));
	assert(loaded.appearance == ORANGE_APPEARANCE_DARK);
	assert(loaded.accent_color == ORANGE_ACCENT_GREEN);
	assert(!loaded.text_highlight_automatic);
	assert(loaded.icon_widget_style == ORANGE_ICON_WIDGET_STYLE_TINTED);
	assert(!loaded.folder_color_automatic);
	assert(loaded.sidebar_icon_size == ORANGE_SIDEBAR_ICON_SIZE_LARGE);
	assert(loaded.tint_window_background);
	assert(loaded.show_scroll_bars == ORANGE_SCROLL_BARS_ALWAYS);
	assert(!loaded.desktop_icons_visible);
	assert(loaded.desktop_grid_spacing == 40);
	assert(loaded.desktop_label_size == 18);
	assert(loaded.desktop_icon_scale > 1.24 && loaded.desktop_icon_scale < 1.26);
	assert(loaded.desktop_positions[1].valid);
	assert(loaded.desktop_positions[1].x == 720);
	assert(loaded.desktop_positions[1].y == 140);
	assert(loaded.dock_scale > 1.19 && loaded.dock_scale < 1.21);
	assert(loaded.dock_icon_scale > 1.14 && loaded.dock_icon_scale < 1.16);
	assert(!loaded.dock_magnification);
	assert(loaded.dock_magnification_scale > 1.49 &&
		loaded.dock_magnification_scale < 1.51);
	assert(loaded.dock_position == ORANGE_DOCK_POSITION_LEFT);
	assert(loaded.minimize_effect == ORANGE_MINIMIZE_EFFECT_SCALE);
	assert(!loaded.dock_show_indicators);
	assert(!loaded.animate_opening_applications);
	assert(!loaded.calendar_widget_visible);
	assert(loaded.weather_widget_visible);
	assert(loaded.calendar_widget_size == ORANGE_WIDGET_SIZE_MEDIUM);
	assert(loaded.weather_widget_size == ORANGE_WIDGET_SIZE_LARGE);
	assert(loaded.cursor_size == 36);
	assert(strcmp(loaded.cursor_theme, "Bibata-Modern-Ice") == 0);
	assert(strcmp(loaded.icon_theme, "Adwaita") == 0);
	assert(strcmp(loaded.gtk_theme_light, "Adwaita") == 0);
	assert(strcmp(loaded.gtk_theme_dark, "Adwaita-dark") == 0);
	assert(strcmp(loaded.dock_apps[0], "test-app.desktop") == 0);
	assert(strcmp(loaded.dock_apps[1], "__trash__") == 0);

	puts("config tests passed");
	return 0;
}
