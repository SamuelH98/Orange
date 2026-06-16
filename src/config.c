#include "orange/config.h"

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

static enum orange_widget_size parse_widget_size(const char *value) {
	if (value != NULL && strcmp(value, "medium") == 0) {
		return ORANGE_WIDGET_SIZE_MEDIUM;
	}
	if (value != NULL && strcmp(value, "large") == 0) {
		return ORANGE_WIDGET_SIZE_LARGE;
	}
	return ORANGE_WIDGET_SIZE_SMALL;
}

static enum orange_desktop_sort_by parse_sort_by(const char *value) {
	if (value == NULL) {
		return ORANGE_DESKTOP_SORT_NONE;
	}
	if (strcmp(value, "snap_to_grid") == 0) {
		return ORANGE_DESKTOP_SORT_SNAP_TO_GRID;
	}
	if (strcmp(value, "name") == 0) {
		return ORANGE_DESKTOP_SORT_NAME;
	}
	if (strcmp(value, "date_added") == 0) {
		return ORANGE_DESKTOP_SORT_DATE_ADDED;
	}
	if (strcmp(value, "date_modified") == 0) {
		return ORANGE_DESKTOP_SORT_DATE_MODIFIED;
	}
	if (strcmp(value, "size") == 0) {
		return ORANGE_DESKTOP_SORT_SIZE;
	}
	if (strcmp(value, "kind") == 0) {
		return ORANGE_DESKTOP_SORT_KIND;
	}
	return ORANGE_DESKTOP_SORT_NONE;
}

static const char *sort_by_name(enum orange_desktop_sort_by sort) {
	switch (sort) {
	case ORANGE_DESKTOP_SORT_NONE:
		return "none";
	case ORANGE_DESKTOP_SORT_SNAP_TO_GRID:
		return "snap_to_grid";
	case ORANGE_DESKTOP_SORT_NAME:
		return "name";
	case ORANGE_DESKTOP_SORT_DATE_ADDED:
		return "date_added";
	case ORANGE_DESKTOP_SORT_DATE_MODIFIED:
		return "date_modified";
	case ORANGE_DESKTOP_SORT_SIZE:
		return "size";
	case ORANGE_DESKTOP_SORT_KIND:
		return "kind";
	}
	return "none";
}

static enum orange_desktop_label_position parse_label_pos(const char *value) {
	if (value != NULL && strcmp(value, "right") == 0) {
		return ORANGE_DESKTOP_LABEL_RIGHT;
	}
	return ORANGE_DESKTOP_LABEL_BOTTOM;
}

static const char *label_pos_name(enum orange_desktop_label_position pos) {
	switch (pos) {
	case ORANGE_DESKTOP_LABEL_RIGHT:
		return "right";
	case ORANGE_DESKTOP_LABEL_BOTTOM:
	default:
		return "bottom";
	}
}

static enum orange_accent_color parse_accent_color(const char *value) {
	if (value == NULL) {
		return ORANGE_ACCENT_MULTICOLOR;
	}
	if (strcmp(value, "blue") == 0) {
		return ORANGE_ACCENT_BLUE;
	}
	if (strcmp(value, "purple") == 0) {
		return ORANGE_ACCENT_PURPLE;
	}
	if (strcmp(value, "pink") == 0) {
		return ORANGE_ACCENT_PINK;
	}
	if (strcmp(value, "red") == 0) {
		return ORANGE_ACCENT_RED;
	}
	if (strcmp(value, "orange") == 0) {
		return ORANGE_ACCENT_ORANGE;
	}
	if (strcmp(value, "yellow") == 0) {
		return ORANGE_ACCENT_YELLOW;
	}
	if (strcmp(value, "green") == 0) {
		return ORANGE_ACCENT_GREEN;
	}
	if (strcmp(value, "graphite") == 0) {
		return ORANGE_ACCENT_GRAPHITE;
	}
	return ORANGE_ACCENT_MULTICOLOR;
}

static const char *accent_color_name(enum orange_accent_color color) {
	switch (color) {
	case ORANGE_ACCENT_BLUE:
		return "blue";
	case ORANGE_ACCENT_PURPLE:
		return "purple";
	case ORANGE_ACCENT_PINK:
		return "pink";
	case ORANGE_ACCENT_RED:
		return "red";
	case ORANGE_ACCENT_ORANGE:
		return "orange";
	case ORANGE_ACCENT_YELLOW:
		return "yellow";
	case ORANGE_ACCENT_GREEN:
		return "green";
	case ORANGE_ACCENT_GRAPHITE:
		return "graphite";
	case ORANGE_ACCENT_MULTICOLOR:
	default:
		return "multicolor";
	}
}

static enum orange_icon_widget_style parse_icon_widget_style(const char *value) {
	if (value == NULL) {
		return ORANGE_ICON_WIDGET_STYLE_DEFAULT;
	}
	if (strcmp(value, "dark") == 0) {
		return ORANGE_ICON_WIDGET_STYLE_DARK;
	}
	if (strcmp(value, "clear") == 0) {
		return ORANGE_ICON_WIDGET_STYLE_CLEAR;
	}
	if (strcmp(value, "tinted") == 0) {
		return ORANGE_ICON_WIDGET_STYLE_TINTED;
	}
	return ORANGE_ICON_WIDGET_STYLE_DEFAULT;
}

static const char *icon_widget_style_name(enum orange_icon_widget_style style) {
	switch (style) {
	case ORANGE_ICON_WIDGET_STYLE_DARK:
		return "dark";
	case ORANGE_ICON_WIDGET_STYLE_CLEAR:
		return "clear";
	case ORANGE_ICON_WIDGET_STYLE_TINTED:
		return "tinted";
	case ORANGE_ICON_WIDGET_STYLE_DEFAULT:
	default:
		return "default";
	}
}

static enum orange_sidebar_icon_size parse_sidebar_icon_size(const char *value) {
	if (value != NULL && strcmp(value, "small") == 0) {
		return ORANGE_SIDEBAR_ICON_SIZE_SMALL;
	}
	if (value != NULL && strcmp(value, "large") == 0) {
		return ORANGE_SIDEBAR_ICON_SIZE_LARGE;
	}
	return ORANGE_SIDEBAR_ICON_SIZE_MEDIUM;
}

static const char *sidebar_icon_size_name(enum orange_sidebar_icon_size size) {
	switch (size) {
	case ORANGE_SIDEBAR_ICON_SIZE_SMALL:
		return "small";
	case ORANGE_SIDEBAR_ICON_SIZE_LARGE:
		return "large";
	case ORANGE_SIDEBAR_ICON_SIZE_MEDIUM:
	default:
		return "medium";
	}
}

static enum orange_scroll_bar_visibility parse_scroll_bar_visibility(const char *value) {
	if (value != NULL && strcmp(value, "when_scrolling") == 0) {
		return ORANGE_SCROLL_BARS_WHEN_SCROLLING;
	}
	if (value != NULL && strcmp(value, "always") == 0) {
		return ORANGE_SCROLL_BARS_ALWAYS;
	}
	return ORANGE_SCROLL_BARS_AUTOMATIC;
}

static const char *scroll_bar_visibility_name(enum orange_scroll_bar_visibility value) {
	switch (value) {
	case ORANGE_SCROLL_BARS_WHEN_SCROLLING:
		return "when_scrolling";
	case ORANGE_SCROLL_BARS_ALWAYS:
		return "always";
	case ORANGE_SCROLL_BARS_AUTOMATIC:
	default:
		return "automatic";
	}
}

static const char *widget_size_name(enum orange_widget_size size) {
	switch (size) {
	case ORANGE_WIDGET_SIZE_MEDIUM:
		return "medium";
	case ORANGE_WIDGET_SIZE_LARGE:
		return "large";
	case ORANGE_WIDGET_SIZE_SMALL:
	default:
		return "small";
	}
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
			parsed < 0 || parsed >= ORANGE_DESKTOP_POSITION_MAX) {
		return false;
	}
	*index = (int)parsed;
	return true;
}

static void copy_cursor_theme(struct orange_config *config, const char *value) {
	if (value == NULL || value[0] == '\0') {
		config->cursor_theme[0] = '\0';
		return;
	}
	snprintf(config->cursor_theme, sizeof(config->cursor_theme), "%s", value);
}

static void copy_theme_name(char *destination,
		size_t destination_size,
		const char *value) {
	if (value == NULL || value[0] == '\0') {
		destination[0] = '\0';
		return;
	}
	snprintf(destination, destination_size, "%s", value);
}

static void apply_pair(struct orange_config *config,
		const char *key,
		const char *value) {
	int desktop_index = -1;
	if (strcmp(key, "appearance") == 0) {
		config->appearance = orange_config_parse_appearance(value);
	} else if (strcmp(key, "accent_color") == 0) {
		config->accent_color = parse_accent_color(value);
	} else if (strcmp(key, "text_highlight_automatic") == 0) {
		config->text_highlight_automatic = parse_bool(value);
	} else if (strcmp(key, "icon_widget_style") == 0) {
		config->icon_widget_style = parse_icon_widget_style(value);
	} else if (strcmp(key, "folder_color_automatic") == 0) {
		config->folder_color_automatic = parse_bool(value);
	} else if (strcmp(key, "sidebar_icon_size") == 0) {
		config->sidebar_icon_size = parse_sidebar_icon_size(value);
	} else if (strcmp(key, "tint_window_background") == 0) {
		config->tint_window_background = parse_bool(value);
	} else if (strcmp(key, "show_scroll_bars") == 0) {
		config->show_scroll_bars = parse_scroll_bar_visibility(value);
	} else if (strcmp(key, "desktop_icons_visible") == 0) {
		config->desktop_icons_visible = parse_bool(value);
	} else if (strcmp(key, "desktop_grid_spacing") == 0) {
		config->desktop_grid_spacing = clamp_int(atoi(value), 8, 96);
	} else if (strcmp(key, "desktop_label_size") == 0) {
		config->desktop_label_size = clamp_int(atoi(value), 10, 28);
	} else if (strcmp(key, "desktop_icon_scale") == 0) {
		config->desktop_icon_scale = clamp_double(strtod(value, NULL), 0.60, 2.00);
	} else if (strcmp(key, "desktop_sort_by") == 0) {
		config->desktop_sort_by = parse_sort_by(value);
	} else if (strcmp(key, "desktop_label_position") == 0) {
		config->desktop_label_position = parse_label_pos(value);
	} else if (strcmp(key, "desktop_show_hard_disks") == 0) {
		config->desktop_show_hard_disks = parse_bool(value);
	} else if (strcmp(key, "desktop_show_external_disks") == 0) {
		config->desktop_show_external_disks = parse_bool(value);
	} else if (strcmp(key, "desktop_show_removable_media") == 0) {
		config->desktop_show_removable_media = parse_bool(value);
	} else if (strcmp(key, "desktop_show_servers") == 0) {
		config->desktop_show_servers = parse_bool(value);
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
	} else if (strcmp(key, "dock_order") == 0) {
		const char *p = value;
		int idx = 0;
		while (*p != '\0' && idx < ORANGE_DOCK_MAX) {
			char *end = NULL;
			long n = strtol(p, &end, 10);
			if (end == p) {
				break;
			}
			if (n >= 0 && n < ORANGE_DOCK_MAX) {
				config->dock_order[idx++] = (int)n;
			}
			p = end;
			if (*p == ',') {
				p++;
			}
		}
		while (idx < ORANGE_DOCK_MAX) {
			config->dock_order[idx] = idx;
			idx++;
		}
	} else if (strcmp(key, "calendar_widget_visible") == 0) {
		config->calendar_widget_visible = parse_bool(value);
	} else if (strcmp(key, "weather_widget_visible") == 0) {
		config->weather_widget_visible = parse_bool(value);
	} else if (strcmp(key, "calendar_widget_size") == 0) {
		config->calendar_widget_size = parse_widget_size(value);
	} else if (strcmp(key, "weather_widget_size") == 0) {
		config->weather_widget_size = parse_widget_size(value);
	} else if (strcmp(key, "cursor_theme") == 0) {
		copy_cursor_theme(config, value);
	} else if (strcmp(key, "cursor_size") == 0) {
		config->cursor_size = clamp_int(atoi(value), 16, 96);
	} else if (strcmp(key, "icon_theme") == 0) {
		copy_theme_name(config->icon_theme, sizeof(config->icon_theme), value);
	} else if (strcmp(key, "gtk_theme_light") == 0) {
		copy_theme_name(config->gtk_theme_light,
			sizeof(config->gtk_theme_light), value);
	} else if (strcmp(key, "gtk_theme_dark") == 0) {
		copy_theme_name(config->gtk_theme_dark,
			sizeof(config->gtk_theme_dark), value);
	} else if (strcmp(key, "dock_apps") == 0) {
		const char *p = value;
		int idx = 0;
		while (*p != '\0' && idx < ORANGE_DOCK_MAX) {
			while (*p == ' ' || *p == ',') {
				p++;
			}
			if (*p == '\0') {
				break;
			}
			const char *start = p;
			while (*p != '\0' && *p != ',' && *p != ' ') {
				p++;
			}
			size_t len = (size_t)(p - start);
			if (len > 0 && len < 128) {
				memcpy(config->dock_apps[idx], start, len);
				config->dock_apps[idx][len] = '\0';
				idx++;
			}
		}
		while (idx < ORANGE_DOCK_MAX) {
			config->dock_apps[idx][0] = '\0';
			idx++;
		}
	}
}

void orange_config_set_defaults(struct orange_config *config) {
	memset(config, 0, sizeof(*config));
	config->appearance = ORANGE_APPEARANCE_LIGHT;
	config->accent_color = ORANGE_ACCENT_MULTICOLOR;
	config->text_highlight_automatic = true;
	config->icon_widget_style = ORANGE_ICON_WIDGET_STYLE_DEFAULT;
	config->folder_color_automatic = true;
	config->sidebar_icon_size = ORANGE_SIDEBAR_ICON_SIZE_MEDIUM;
	config->tint_window_background = false;
	config->show_scroll_bars = ORANGE_SCROLL_BARS_AUTOMATIC;
	config->desktop_icons_visible = true;
	config->desktop_grid_spacing = 24;
	config->desktop_label_size = 15;
	config->desktop_icon_scale = 1.0;
	config->desktop_sort_by = ORANGE_DESKTOP_SORT_NONE;
	config->desktop_label_position = ORANGE_DESKTOP_LABEL_BOTTOM;
	config->desktop_show_hard_disks = true;
	config->desktop_show_external_disks = true;
	config->desktop_show_removable_media = true;
	config->desktop_show_servers = true;
	for (int i = 0; i < ORANGE_DESKTOP_POSITION_MAX; i++) {
		config->desktop_positions[i] =
			(struct orange_desktop_icon_position){false, 0, 0};
	}
	config->dock_scale = 1.0;
	config->dock_icon_scale = 1.0;
	config->dock_magnification = true;
	config->dock_magnification_scale = 1.28;
	config->dock_show_indicators = true;
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		config->dock_order[i] = i;
	}
	config->calendar_widget_visible = true;
	config->weather_widget_visible = true;
	config->calendar_widget_size = ORANGE_WIDGET_SIZE_SMALL;
	config->weather_widget_size = ORANGE_WIDGET_SIZE_SMALL;
	config->cursor_theme[0] = '\0';
	config->cursor_size = 28;
	snprintf(config->icon_theme, sizeof(config->icon_theme), "%s", "MacTahoe");
	snprintf(config->gtk_theme_light, sizeof(config->gtk_theme_light),
		"%s", "MacTahoe-Light");
	snprintf(config->gtk_theme_dark, sizeof(config->gtk_theme_dark),
		"%s", "MacTahoe-Dark");
	/* Default dock — macOS Golden Gate order hybridized with GNOME core apps */
	const char *default_dock[] = {
		"__launcher__",
		"org.gnome.Nautilus.desktop",
		"firefox.desktop",
		"org.gnome.Maps.desktop",
		"org.gnome.Loupe.desktop",
		"org.gnome.Calendar.desktop",
		"org.gnome.Contacts.desktop",
		"org.gnome.TextEditor.desktop",
		"org.gnome.Showtime.desktop",
		"org.gnome.Decibels.desktop",
		"org.gnome.Software.desktop",
		"org.gnome.Settings.desktop",
		"__trash__",
	};
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (i < (int)(sizeof(default_dock) / sizeof(default_dock[0]))) {
			snprintf(config->dock_apps[i], 128, "%s", default_dock[i]);
		} else {
			config->dock_apps[i][0] = '\0';
		}
	}
}

bool orange_config_load(struct orange_config *config, const char *path) {
	orange_config_set_defaults(config);
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

bool orange_config_save(const struct orange_config *config, const char *path) {
	if (path == NULL || path[0] == '\0') {
		return false;
	}

	FILE *file = fopen(path, "w");
	if (file == NULL) {
		return false;
	}

	fprintf(file, "appearance=%s\n",
		orange_config_appearance_name(config->appearance));
	fprintf(file, "accent_color=%s\n",
		accent_color_name(config->accent_color));
	fprintf(file, "text_highlight_automatic=%s\n",
		config->text_highlight_automatic ? "true" : "false");
	fprintf(file, "icon_widget_style=%s\n",
		icon_widget_style_name(config->icon_widget_style));
	fprintf(file, "folder_color_automatic=%s\n",
		config->folder_color_automatic ? "true" : "false");
	fprintf(file, "sidebar_icon_size=%s\n",
		sidebar_icon_size_name(config->sidebar_icon_size));
	fprintf(file, "tint_window_background=%s\n",
		config->tint_window_background ? "true" : "false");
	fprintf(file, "show_scroll_bars=%s\n",
		scroll_bar_visibility_name(config->show_scroll_bars));
	fprintf(file, "desktop_icons_visible=%s\n",
		config->desktop_icons_visible ? "true" : "false");
	fprintf(file, "desktop_grid_spacing=%d\n", config->desktop_grid_spacing);
	fprintf(file, "desktop_label_size=%d\n", config->desktop_label_size);
	fprintf(file, "desktop_icon_scale=%.2f\n", config->desktop_icon_scale);
	fprintf(file, "desktop_sort_by=%s\n",
		sort_by_name(config->desktop_sort_by));
	fprintf(file, "desktop_label_position=%s\n",
		label_pos_name(config->desktop_label_position));
	fprintf(file, "desktop_show_hard_disks=%s\n",
		config->desktop_show_hard_disks ? "true" : "false");
	fprintf(file, "desktop_show_external_disks=%s\n",
		config->desktop_show_external_disks ? "true" : "false");
	fprintf(file, "desktop_show_removable_media=%s\n",
		config->desktop_show_removable_media ? "true" : "false");
	fprintf(file, "desktop_show_servers=%s\n",
		config->desktop_show_servers ? "true" : "false");
	for (int i = 0; i < ORANGE_DESKTOP_POSITION_MAX; i++) {
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
	fprintf(file, "dock_order=");
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (i > 0) {
			fprintf(file, ",");
		}
		fprintf(file, "%d", config->dock_order[i]);
	}
	fprintf(file, "\n");
	fprintf(file, "calendar_widget_visible=%s\n",
		config->calendar_widget_visible ? "true" : "false");
	fprintf(file, "weather_widget_visible=%s\n",
		config->weather_widget_visible ? "true" : "false");
	fprintf(file, "calendar_widget_size=%s\n",
		widget_size_name(config->calendar_widget_size));
	fprintf(file, "weather_widget_size=%s\n",
		widget_size_name(config->weather_widget_size));
	fprintf(file, "cursor_theme=%s\n", config->cursor_theme);
	fprintf(file, "cursor_size=%d\n", config->cursor_size);
	fprintf(file, "icon_theme=%s\n", config->icon_theme);
	fprintf(file, "gtk_theme_light=%s\n", config->gtk_theme_light);
	fprintf(file, "gtk_theme_dark=%s\n", config->gtk_theme_dark);
	fprintf(file, "dock_apps=");
	bool first = true;
	for (int i = 0; i < ORANGE_DOCK_MAX && config->dock_apps[i][0] != '\0'; i++) {
		if (!first) {
			fprintf(file, ",");
		}
		fprintf(file, "%s", config->dock_apps[i]);
		first = false;
	}
	fprintf(file, "\n");

	fclose(file);
	return true;
}

const char *orange_config_appearance_name(enum orange_appearance appearance) {
	switch (appearance) {
	case ORANGE_APPEARANCE_DARK:
		return "dark";
	case ORANGE_APPEARANCE_LIGHT:
	default:
		return "light";
	}
}

enum orange_appearance orange_config_parse_appearance(const char *value) {
	if (value != NULL && strcmp(value, "dark") == 0) {
		return ORANGE_APPEARANCE_DARK;
	}
	return ORANGE_APPEARANCE_LIGHT;
}
