#ifndef ORANGE_SHELL_H
#define ORANGE_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"

#define ORANGE_DESKTOP_MAX 8
#define ORANGE_MENU_ITEM_MAX 20
#define ORANGE_WIDGET_MAX 16
#define ORANGE_CONTEXT_MENU_ITEM_MAX 16
#define ORANGE_STATUS_TEXT_MAX 64

struct orange_rect {
	int x;
	int y;
	int width;
	int height;
};

enum orange_shell_hit_kind {
	ORANGE_HIT_NONE,
	ORANGE_HIT_SYSTEM_MENU,
	ORANGE_HIT_SYSTEM_MENU_ITEM,
	ORANGE_HIT_APP_MENU,
	ORANGE_HIT_STATUS_AREA,
	ORANGE_HIT_DOCK_ITEM,
	ORANGE_HIT_WIDGET,
	ORANGE_HIT_DESKTOP_ITEM,
	ORANGE_HIT_DESKTOP,
	ORANGE_HIT_CONTEXT_MENU_ITEM,
};

struct orange_shell_hit {
	enum orange_shell_hit_kind kind;
	int index;
};

enum orange_widget_type {
	ORANGE_WIDGET_CALENDAR,
	ORANGE_WIDGET_WEATHER,
};

struct orange_widget {
	int id;
	enum orange_widget_type type;
	bool visible;
	struct orange_rect rect;
};

enum orange_context_menu_kind {
	ORANGE_CONTEXT_MENU_NONE,
	ORANGE_CONTEXT_MENU_DOCK,
	ORANGE_CONTEXT_MENU_WIDGET,
	ORANGE_CONTEXT_MENU_DESKTOP,
	ORANGE_CONTEXT_MENU_DESKTOP_ICON,
	ORANGE_CONTEXT_MENU_STATUS,
};

struct orange_status_state {
	char network_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char network_label[ORANGE_STATUS_TEXT_MAX];
	bool network_connected;
	bool network_available;

	char sound_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char sound_label[ORANGE_STATUS_TEXT_MAX];
	bool sound_muted;
	int sound_percent;

	char battery_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char battery_label[ORANGE_STATUS_TEXT_MAX];
	bool battery_present;
	bool battery_charging;
	int battery_percent;
};

struct orange_shell_layout {
	int width;
	int height;

	struct orange_rect menu_bar;
	struct orange_rect system_menu_button;
	struct orange_rect app_menu_button;
	struct orange_rect status_area;
	struct orange_rect system_menu_panel;
	struct orange_rect system_menu_items[ORANGE_MENU_ITEM_MAX];
	bool system_menu_separator[ORANGE_MENU_ITEM_MAX];
	int system_menu_item_count;

	struct orange_rect calendar_widget;
	struct orange_rect weather_widget;
	struct orange_widget widgets[ORANGE_WIDGET_MAX];
	int widget_count;

	struct orange_rect desktop_items[ORANGE_DESKTOP_MAX];
	int desktop_item_count;

	struct orange_rect dock;
	struct orange_rect dock_items[ORANGE_DOCK_MAX];
	int dock_launcher_indices[ORANGE_DOCK_MAX];
	int dock_item_count;

	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	struct orange_rect context_menu_panel;
	struct orange_rect context_menu_items[ORANGE_CONTEXT_MENU_ITEM_MAX];
	bool context_menu_separator[ORANGE_CONTEXT_MENU_ITEM_MAX];
	int context_menu_item_count;
};

struct orange_shell_state {
	bool system_menu_open;
	int hot_dock_index;
	int dock_pointer_x;
	int dock_pointer_y;
	time_t now;
	struct orange_assets *assets;
	const struct orange_config *config;
	struct orange_status_state status;
	const struct orange_desktop_entry *desktop_entries;
	int desktop_entry_count;
	bool dock_open[ORANGE_DOCK_MAX];
	int dock_drag_index;
	int dock_drag_insert_before;
	int dock_drag_x;
	int dock_drag_y;
	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
};

struct orange_shell_draw_options {
	bool draw_wallpaper;
};

void orange_shell_layout_compute(
	int width,
	int height,
	bool system_menu_open,
	const struct orange_config *config,
	int desktop_entry_count,
	struct orange_shell_layout *layout);
void orange_shell_layout_set_context_menu(
	struct orange_shell_layout *layout,
	enum orange_context_menu_kind kind,
	int index,
	int cursor_x,
	int cursor_y);

struct orange_shell_hit orange_shell_hit_test(
	const struct orange_shell_layout *layout,
	int x,
	int y);

void orange_shell_draw(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct orange_shell_state *state);
void orange_shell_draw_with_options(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct orange_shell_state *state,
	const struct orange_shell_draw_options *options);

int orange_shell_dock_count(const struct orange_config *config);
int orange_shell_dock_launcher_index(
	const struct orange_shell_layout *layout,
	int visible_index);
const char *orange_shell_dock_label(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_shell_dock_command(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_shell_menu_label(int index);
const char *orange_shell_context_menu_label(
	enum orange_context_menu_kind kind,
	int index);
const char *orange_shell_context_menu_icon_name(
	enum orange_context_menu_kind kind,
	int index);

#endif
