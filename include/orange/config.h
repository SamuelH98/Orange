#ifndef ORANGE_CONFIG_H
#define ORANGE_CONFIG_H

#include <stdbool.h>

#define ORANGE_CURSOR_THEME_MAX 128
#define ORANGE_DESKTOP_POSITION_MAX 8
#define ORANGE_DOCK_MAX 24

enum orange_widget_size {
	ORANGE_WIDGET_SIZE_SMALL,
	ORANGE_WIDGET_SIZE_MEDIUM,
	ORANGE_WIDGET_SIZE_LARGE,
};

struct orange_desktop_icon_position {
	bool valid;
	int x;
	int y;
};

enum orange_appearance {
	ORANGE_APPEARANCE_LIGHT,
	ORANGE_APPEARANCE_DARK,
};

struct orange_config {
	enum orange_appearance appearance;

	bool desktop_icons_visible;
	int desktop_grid_spacing;
	int desktop_label_size;
	double desktop_icon_scale;
	struct orange_desktop_icon_position desktop_positions[ORANGE_DESKTOP_POSITION_MAX];

	double dock_scale;
	double dock_icon_scale;
	bool dock_magnification;
	double dock_magnification_scale;
	bool dock_show_indicators;
	int dock_order[ORANGE_DOCK_MAX];

	bool calendar_widget_visible;
	bool weather_widget_visible;
	enum orange_widget_size calendar_widget_size;
	enum orange_widget_size weather_widget_size;

	char cursor_theme[ORANGE_CURSOR_THEME_MAX];
	int cursor_size;
};

void orange_config_set_defaults(struct orange_config *config);
bool orange_config_load(struct orange_config *config, const char *path);
bool orange_config_save(const struct orange_config *config, const char *path);
const char *orange_config_appearance_name(enum orange_appearance appearance);
enum orange_appearance orange_config_parse_appearance(const char *value);

#endif
