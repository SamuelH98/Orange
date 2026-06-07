#ifndef TAHOE_CONFIG_H
#define TAHOE_CONFIG_H

#include <stdbool.h>

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

	double dock_scale;
	double dock_icon_scale;
	bool dock_magnification;
	double dock_magnification_scale;
	bool dock_show_indicators;
};

void tahoe_config_set_defaults(struct tahoe_config *config);
bool tahoe_config_load(struct tahoe_config *config, const char *path);
bool tahoe_config_save(const struct tahoe_config *config, const char *path);
const char *tahoe_config_appearance_name(enum tahoe_appearance appearance);
enum tahoe_appearance tahoe_config_parse_appearance(const char *value);

#endif
