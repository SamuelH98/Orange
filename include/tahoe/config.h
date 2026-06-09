#ifndef TAHOE_CONFIG_H
#define TAHOE_CONFIG_H

#include <stdbool.h>

#define TAHOE_CURSOR_THEME_MAX 128
#define TAHOE_DESKTOP_POSITION_MAX 8
#define TAHOE_DOCK_MAX 24

enum tahoe_widget_size {
	TAHOE_WIDGET_SIZE_SMALL,
	TAHOE_WIDGET_SIZE_MEDIUM,
	TAHOE_WIDGET_SIZE_LARGE,
};

struct tahoe_desktop_icon_position {
	bool valid;
	int x;
	int y;
};

enum tahoe_appearance {
	TAHOE_APPEARANCE_LIGHT,
	TAHOE_APPEARANCE_DARK,
};

struct tahoe_config {
	enum tahoe_appearance appearance;

	bool desktop_icons_visible;
	int desktop_grid_spacing;
	int desktop_label_size;
	double desktop_icon_scale;
	struct tahoe_desktop_icon_position desktop_positions[TAHOE_DESKTOP_POSITION_MAX];

	double dock_scale;
	double dock_icon_scale;
	bool dock_magnification;
	double dock_magnification_scale;
	bool dock_show_indicators;
	int dock_order[TAHOE_DOCK_MAX];

	bool calendar_widget_visible;
	bool weather_widget_visible;
	enum tahoe_widget_size calendar_widget_size;
	enum tahoe_widget_size weather_widget_size;

	char cursor_theme[TAHOE_CURSOR_THEME_MAX];
	int cursor_size;
};

void tahoe_config_set_defaults(struct tahoe_config *config);
bool tahoe_config_load(struct tahoe_config *config, const char *path);
bool tahoe_config_save(const struct tahoe_config *config, const char *path);
const char *tahoe_config_appearance_name(enum tahoe_appearance appearance);
enum tahoe_appearance tahoe_config_parse_appearance(const char *value);

#endif
