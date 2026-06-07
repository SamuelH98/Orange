#include "tahoe/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double clamp_double(double value, double min_value, double max_value) {
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

static int clamp_int(int value, int min_value, int max_value) {
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

static char *trim(char *value) {
	while (isspace((unsigned char)*value)) {
		value++;
	}
	if (*value == '\0') {
		return value;
	}
	char *end = value + strlen(value) - 1;
	while (end > value && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}
	return value;
}

static bool parse_bool(const char *value) {
	return strcmp(value, "1") == 0 ||
		strcmp(value, "true") == 0 ||
		strcmp(value, "yes") == 0 ||
		strcmp(value, "on") == 0;
}

static bool parse_desktop_position_key(const char *key, int *index) {
	const char *prefix = "desktop_icon_";
	size_t prefix_len = strlen(prefix);
	if (strncmp(key, prefix, prefix_len) != 0) {
		return false;
	}
	char *end = NULL;
	long parsed = strtol(key + prefix_len, &end, 10);
	if (end == key + prefix_len || strcmp(end, "_position") != 0 ||
			parsed < 0 || parsed >= TAHOE_DESKTOP_POSITION_MAX) {
		return false;
	}
	*index = (int)parsed;
	return true;
}

static void copy_cursor_theme(struct tahoe_config *config, const char *value) {
	if (value == NULL || value[0] == '\0') {
		config->cursor_theme[0] = '\0';
		return;
	}
	snprintf(config->cursor_theme, sizeof(config->cursor_theme), "%s", value);
}

static void apply_pair(struct tahoe_config *config,
		const char *key,
		const char *value) {
	int desktop_index = -1;
	if (strcmp(key, "appearance") == 0) {
		config->appearance = tahoe_config_parse_appearance(value);
	} else if (strcmp(key, "desktop_icons_visible") == 0) {
		config->desktop_icons_visible = parse_bool(value);
	} else if (strcmp(key, "desktop_grid_spacing") == 0) {
		config->desktop_grid_spacing = clamp_int(atoi(value), 8, 96);
	} else if (strcmp(key, "desktop_label_size") == 0) {
		config->desktop_label_size = clamp_int(atoi(value), 10, 28);
	} else if (strcmp(key, "desktop_icon_scale") == 0) {
		config->desktop_icon_scale = clamp_double(strtod(value, NULL), 0.60, 2.00);
	} else if (parse_desktop_position_key(key, &desktop_index)) {
		int x = 0;
		int y = 0;
		if (sscanf(value, "%d,%d", &x, &y) == 2) {
			config->desktop_positions[desktop_index].valid = true;
			config->desktop_positions[desktop_index].x = clamp_int(x, 0, 16384);
			config->desktop_positions[desktop_index].y = clamp_int(y, 0, 16384);
		}
	} else if (strcmp(key, "dock_scale") == 0) {
		config->dock_scale = clamp_double(strtod(value, NULL), 0.60, 1.60);
	} else if (strcmp(key, "dock_icon_scale") == 0) {
		config->dock_icon_scale = clamp_double(strtod(value, NULL), 0.60, 1.80);
	} else if (strcmp(key, "dock_magnification") == 0) {
		config->dock_magnification = parse_bool(value);
	} else if (strcmp(key, "dock_magnification_scale") == 0) {
		config->dock_magnification_scale =
			clamp_double(strtod(value, NULL), 1.00, 2.20);
	} else if (strcmp(key, "dock_show_indicators") == 0) {
		config->dock_show_indicators = parse_bool(value);
	} else if (strcmp(key, "cursor_theme") == 0) {
		copy_cursor_theme(config, value);
	} else if (strcmp(key, "cursor_size") == 0) {
		config->cursor_size = clamp_int(atoi(value), 16, 96);
	}
}

void tahoe_config_set_defaults(struct tahoe_config *config) {
	memset(config, 0, sizeof(*config));
	config->appearance = TAHOE_APPEARANCE_LIGHT;
	config->desktop_icons_visible = true;
	config->desktop_grid_spacing = 24;
	config->desktop_label_size = 15;
	config->desktop_icon_scale = 1.0;
	for (int i = 0; i < TAHOE_DESKTOP_POSITION_MAX; i++) {
		config->desktop_positions[i] =
			(struct tahoe_desktop_icon_position){false, 0, 0};
	}
	config->dock_scale = 1.0;
	config->dock_icon_scale = 1.0;
	config->dock_magnification = true;
	config->dock_magnification_scale = 1.28;
	config->dock_show_indicators = true;
	config->cursor_theme[0] = '\0';
	config->cursor_size = 28;
}

bool tahoe_config_load(struct tahoe_config *config, const char *path) {
	tahoe_config_set_defaults(config);
	if (path == NULL || path[0] == '\0') {
		return false;
	}

	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		char *start = trim(line);
		if (start[0] == '\0' || start[0] == '#') {
			continue;
		}
		char *equals = strchr(start, '=');
		if (equals == NULL) {
			continue;
		}
		*equals = '\0';
		char *key = trim(start);
		char *value = trim(equals + 1);
		apply_pair(config, key, value);
	}

	fclose(file);
	return true;
}

bool tahoe_config_save(const struct tahoe_config *config, const char *path) {
	if (path == NULL || path[0] == '\0') {
		return false;
	}

	FILE *file = fopen(path, "w");
	if (file == NULL) {
		return false;
	}

	fprintf(file, "appearance=%s\n",
		tahoe_config_appearance_name(config->appearance));
	fprintf(file, "desktop_icons_visible=%s\n",
		config->desktop_icons_visible ? "true" : "false");
	fprintf(file, "desktop_grid_spacing=%d\n", config->desktop_grid_spacing);
	fprintf(file, "desktop_label_size=%d\n", config->desktop_label_size);
	fprintf(file, "desktop_icon_scale=%.2f\n", config->desktop_icon_scale);
	for (int i = 0; i < TAHOE_DESKTOP_POSITION_MAX; i++) {
		if (config->desktop_positions[i].valid) {
			fprintf(file, "desktop_icon_%d_position=%d,%d\n",
				i,
				config->desktop_positions[i].x,
				config->desktop_positions[i].y);
		}
	}
	fprintf(file, "dock_scale=%.2f\n", config->dock_scale);
	fprintf(file, "dock_icon_scale=%.2f\n", config->dock_icon_scale);
	fprintf(file, "dock_magnification=%s\n",
		config->dock_magnification ? "true" : "false");
	fprintf(file, "dock_magnification_scale=%.2f\n",
		config->dock_magnification_scale);
	fprintf(file, "dock_show_indicators=%s\n",
		config->dock_show_indicators ? "true" : "false");
	fprintf(file, "cursor_theme=%s\n", config->cursor_theme);
	fprintf(file, "cursor_size=%d\n", config->cursor_size);

	fclose(file);
	return true;
}

const char *tahoe_config_appearance_name(enum tahoe_appearance appearance) {
	switch (appearance) {
	case TAHOE_APPEARANCE_DARK:
		return "dark";
	case TAHOE_APPEARANCE_LIGHT:
	default:
		return "light";
	}
}

enum tahoe_appearance tahoe_config_parse_appearance(const char *value) {
	if (value != NULL && strcmp(value, "dark") == 0) {
		return TAHOE_APPEARANCE_DARK;
	}
	return TAHOE_APPEARANCE_LIGHT;
}
