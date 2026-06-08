#ifndef TAHOE_SHELL_H
#define TAHOE_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "tahoe/assets.h"
#include "tahoe/config.h"
#include "tahoe/desktop_entry.h"

#define TAHOE_DOCK_MAX 24
#define TAHOE_DESKTOP_MAX 8
#define TAHOE_MENU_ITEM_MAX 20
#define TAHOE_WIDGET_MAX 16
#define TAHOE_CONTEXT_MENU_ITEM_MAX 16

struct tahoe_rect {
	int x;
	int y;
	int width;
	int height;
};

enum tahoe_shell_hit_kind {
	TAHOE_HIT_NONE,
	TAHOE_HIT_APPLE_MENU,
	TAHOE_HIT_APPLE_MENU_ITEM,
	TAHOE_HIT_DOCK_ITEM,
	TAHOE_HIT_WIDGET,
	TAHOE_HIT_DESKTOP_ITEM,
	TAHOE_HIT_DESKTOP,
	TAHOE_HIT_CONTEXT_MENU_ITEM,
};

struct tahoe_shell_hit {
	enum tahoe_shell_hit_kind kind;
	int index;
};

enum tahoe_widget_type {
	TAHOE_WIDGET_CALENDAR,
	TAHOE_WIDGET_WEATHER,
};

struct tahoe_widget {
	int id;
	enum tahoe_widget_type type;
	bool visible;
	struct tahoe_rect rect;
};

enum tahoe_context_menu_kind {
	TAHOE_CONTEXT_MENU_NONE,
	TAHOE_CONTEXT_MENU_DOCK,
	TAHOE_CONTEXT_MENU_WIDGET,
	TAHOE_CONTEXT_MENU_DESKTOP,
	TAHOE_CONTEXT_MENU_DESKTOP_ICON,
};

struct tahoe_shell_layout {
	int width;
	int height;

	struct tahoe_rect menu_bar;
	struct tahoe_rect apple_menu_button;
	struct tahoe_rect apple_menu_panel;
	struct tahoe_rect apple_menu_items[TAHOE_MENU_ITEM_MAX];
	bool apple_menu_separator[TAHOE_MENU_ITEM_MAX];
	int apple_menu_item_count;

	struct tahoe_rect calendar_widget;
	struct tahoe_rect weather_widget;
	struct tahoe_widget widgets[TAHOE_WIDGET_MAX];
	int widget_count;

	struct tahoe_rect desktop_items[TAHOE_DESKTOP_MAX];
	int desktop_item_count;

	struct tahoe_rect dock;
	struct tahoe_rect dock_items[TAHOE_DOCK_MAX];
	int dock_item_count;

	enum tahoe_context_menu_kind context_menu_kind;
	int context_menu_index;
	struct tahoe_rect context_menu_panel;
	struct tahoe_rect context_menu_items[TAHOE_CONTEXT_MENU_ITEM_MAX];
	bool context_menu_separator[TAHOE_CONTEXT_MENU_ITEM_MAX];
	int context_menu_item_count;
};

struct tahoe_shell_state {
	bool apple_menu_open;
	int hot_dock_index;
	time_t now;
	const struct tahoe_assets *assets;
	const struct tahoe_config *config;
	const struct tahoe_desktop_entry *desktop_entries;
	int desktop_entry_count;
	bool dock_open[TAHOE_DOCK_MAX];
	enum tahoe_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
};

struct tahoe_shell_draw_options {
	bool draw_wallpaper;
};

void tahoe_shell_layout_compute(
	int width,
	int height,
	bool apple_menu_open,
	const struct tahoe_config *config,
	int desktop_entry_count,
	struct tahoe_shell_layout *layout);
void tahoe_shell_layout_set_context_menu(
	struct tahoe_shell_layout *layout,
	enum tahoe_context_menu_kind kind,
	int index,
	int cursor_x,
	int cursor_y);

struct tahoe_shell_hit tahoe_shell_hit_test(
	const struct tahoe_shell_layout *layout,
	int x,
	int y);

void tahoe_shell_draw(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct tahoe_shell_state *state);
void tahoe_shell_draw_with_options(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct tahoe_shell_state *state,
	const struct tahoe_shell_draw_options *options);

const char *tahoe_shell_dock_label(int index);
const char *tahoe_shell_dock_command(int index);
const char *tahoe_shell_menu_label(int index);
const char *tahoe_shell_context_menu_label(
	enum tahoe_context_menu_kind kind,
	int index);

#endif
