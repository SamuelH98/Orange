#ifndef TAHOE_SHELL_H
#define TAHOE_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "tahoe/assets.h"

#define TAHOE_DOCK_MAX 24
#define TAHOE_DESKTOP_MAX 8
#define TAHOE_MENU_ITEM_MAX 8

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
	TAHOE_HIT_DESKTOP_ITEM,
};

struct tahoe_shell_hit {
	enum tahoe_shell_hit_kind kind;
	int index;
};

struct tahoe_shell_layout {
	int width;
	int height;

	struct tahoe_rect menu_bar;
	struct tahoe_rect apple_menu_button;
	struct tahoe_rect apple_menu_panel;
	struct tahoe_rect apple_menu_items[TAHOE_MENU_ITEM_MAX];
	int apple_menu_item_count;

	struct tahoe_rect calendar_widget;
	struct tahoe_rect weather_widget;

	struct tahoe_rect desktop_items[TAHOE_DESKTOP_MAX];
	int desktop_item_count;

	struct tahoe_rect dock;
	struct tahoe_rect dock_items[TAHOE_DOCK_MAX];
	int dock_item_count;
};

struct tahoe_shell_state {
	bool apple_menu_open;
	int hot_dock_index;
	time_t now;
	const struct tahoe_assets *assets;
};

void tahoe_shell_layout_compute(
	int width,
	int height,
	bool apple_menu_open,
	struct tahoe_shell_layout *layout);

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

const char *tahoe_shell_dock_label(int index);
const char *tahoe_shell_dock_command(int index);
const char *tahoe_shell_desktop_label(int index);
const char *tahoe_shell_desktop_command(int index);
const char *tahoe_shell_menu_label(int index);

#endif
