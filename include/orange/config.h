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

enum orange_desktop_sort_by {
	ORANGE_DESKTOP_SORT_NONE,
	ORANGE_DESKTOP_SORT_SNAP_TO_GRID,
	ORANGE_DESKTOP_SORT_NAME,
	ORANGE_DESKTOP_SORT_DATE_ADDED,
	ORANGE_DESKTOP_SORT_DATE_MODIFIED,
	ORANGE_DESKTOP_SORT_SIZE,
	ORANGE_DESKTOP_SORT_KIND,
};

enum orange_desktop_label_position {
	ORANGE_DESKTOP_LABEL_BOTTOM,
	ORANGE_DESKTOP_LABEL_RIGHT,
};

enum orange_accent_color {
	ORANGE_ACCENT_MULTICOLOR,
	ORANGE_ACCENT_BLUE,
	ORANGE_ACCENT_PURPLE,
	ORANGE_ACCENT_PINK,
	ORANGE_ACCENT_RED,
	ORANGE_ACCENT_ORANGE,
	ORANGE_ACCENT_YELLOW,
	ORANGE_ACCENT_GREEN,
	ORANGE_ACCENT_GRAPHITE,
	ORANGE_ACCENT_COLOR_COUNT,
};

enum orange_icon_widget_style {
	ORANGE_ICON_WIDGET_STYLE_DEFAULT,
	ORANGE_ICON_WIDGET_STYLE_DARK,
	ORANGE_ICON_WIDGET_STYLE_CLEAR,
	ORANGE_ICON_WIDGET_STYLE_TINTED,
	ORANGE_ICON_WIDGET_STYLE_COUNT,
};

enum orange_sidebar_icon_size {
	ORANGE_SIDEBAR_ICON_SIZE_SMALL,
	ORANGE_SIDEBAR_ICON_SIZE_MEDIUM,
	ORANGE_SIDEBAR_ICON_SIZE_LARGE,
};

enum orange_scroll_bar_visibility {
	ORANGE_SCROLL_BARS_AUTOMATIC,
	ORANGE_SCROLL_BARS_WHEN_SCROLLING,
	ORANGE_SCROLL_BARS_ALWAYS,
};

enum orange_dock_position {
	ORANGE_DOCK_POSITION_BOTTOM,
	ORANGE_DOCK_POSITION_LEFT,
	ORANGE_DOCK_POSITION_RIGHT,
};

enum orange_minimize_effect {
	ORANGE_MINIMIZE_EFFECT_GENIE,
	ORANGE_MINIMIZE_EFFECT_SCALE,
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

#define ORANGE_DESKTOP_VOLUME_MAX 12
#define ORANGE_VOLUME_LABEL_MAX 256
#define ORANGE_VOLUME_PATH_MAX 512
#define ORANGE_VOLUME_ICON_MAX 128

struct orange_volume_info {
	char label[ORANGE_VOLUME_LABEL_MAX];
	char mount_path[ORANGE_VOLUME_PATH_MAX];
	char icon_name[ORANGE_VOLUME_ICON_MAX];
	bool is_removable;
	bool is_internal;
};

struct orange_config {
	enum orange_appearance appearance;
	enum orange_accent_color accent_color;
	bool text_highlight_automatic;
	enum orange_icon_widget_style icon_widget_style;
	bool folder_color_automatic;
	enum orange_sidebar_icon_size sidebar_icon_size;
	bool tint_window_background;
	enum orange_scroll_bar_visibility show_scroll_bars;

	bool desktop_icons_visible;
	bool desktop_use_stacks;
	int desktop_grid_spacing;
	int desktop_label_size;
	double desktop_icon_scale;
	enum orange_desktop_sort_by desktop_sort_by;
	enum orange_desktop_label_position desktop_label_position;
	struct orange_desktop_icon_position desktop_positions[ORANGE_DESKTOP_POSITION_MAX];
	bool desktop_show_hard_disks;
	bool desktop_show_external_disks;
	bool desktop_show_removable_media;
	bool desktop_show_servers;

	double dock_scale;
	double dock_icon_scale;
	bool dock_magnification;
	double dock_magnification_scale;
	enum orange_dock_position dock_position;
	enum orange_minimize_effect minimize_effect;
	bool dock_show_indicators;
	bool animate_opening_applications;
	int dock_order[ORANGE_DOCK_MAX];

	bool calendar_widget_visible;
	bool weather_widget_visible;
	enum orange_widget_size calendar_widget_size;
	enum orange_widget_size weather_widget_size;

	char cursor_theme[ORANGE_CURSOR_THEME_MAX];
	int cursor_size;
	char icon_theme[ORANGE_CURSOR_THEME_MAX];
	char gtk_theme_light[ORANGE_CURSOR_THEME_MAX];
	char gtk_theme_dark[ORANGE_CURSOR_THEME_MAX];
	char dock_apps[ORANGE_DOCK_MAX][128];
};

void orange_config_set_defaults(struct orange_config *config);
bool orange_config_load(struct orange_config *config, const char *path);
bool orange_config_save(const struct orange_config *config, const char *path);
const char *orange_config_appearance_name(enum orange_appearance appearance);
enum orange_appearance orange_config_parse_appearance(const char *value);

#endif
