#include "tahoe/shell.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void test_dock_hit(void) {
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &layout);
	assert(layout.dock_item_count >= 10);
	struct tahoe_rect first = layout.dock_items[0];
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(
		&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == TAHOE_HIT_DOCK_ITEM);
	assert(hit.index == 0);
	assert(tahoe_shell_dock_command(hit.index) != NULL);
}

static void test_desktop_hit(void) {
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &layout);
	struct tahoe_rect item = layout.desktop_items[1];
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == TAHOE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
	assert(tahoe_shell_desktop_command(hit.index) != NULL);
}

static void test_apple_menu_hit(void) {
	struct tahoe_shell_layout closed;
	tahoe_shell_layout_compute(1920, 1080, false, &closed);
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(&closed, 36, 16);
	assert(hit.kind == TAHOE_HIT_APPLE_MENU);

	struct tahoe_shell_layout open;
	tahoe_shell_layout_compute(1920, 1080, true, &open);
	assert(open.apple_menu_item_count > 4);
	struct tahoe_rect item = open.apple_menu_items[3];
	hit = tahoe_shell_hit_test(&open, item.x + 8, item.y + 10);
	assert(hit.kind == TAHOE_HIT_APPLE_MENU_ITEM);
	assert(hit.index == 3);
	assert(tahoe_shell_menu_label(hit.index) != NULL);
}

static void test_small_width_dock_fits(void) {
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(900, 700, false, &layout);
	assert(layout.dock.x >= 0);
	assert(layout.dock.x + layout.dock.width <= layout.width);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].x >= layout.dock.x);
		assert(layout.dock_items[i].x + layout.dock_items[i].width <=
			layout.dock.x + layout.dock.width);
	}
}

static void test_resolution_scaling(void) {
	struct tahoe_shell_layout base;
	struct tahoe_shell_layout large;
	tahoe_shell_layout_compute(1920, 1080, false, &base);
	tahoe_shell_layout_compute(3840, 2160, false, &large);

	assert(large.menu_bar.height > base.menu_bar.height);
	assert(large.calendar_widget.width > base.calendar_widget.width);
	assert(large.weather_widget.height > base.weather_widget.height);
	assert(large.dock_items[0].width > base.dock_items[0].width);
	assert(large.dock.x + large.dock.width <= large.width);
}

static void test_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct tahoe_shell_state state = {
		.apple_menu_open = true,
		.hot_dock_index = 0,
		.now = 1757641980,
		.assets = NULL,
	};
	tahoe_shell_draw(pixels, width, height, stride, &state);

	size_t non_zero = 0;
	for (size_t i = 0; i < (size_t)width * (size_t)height; i++) {
		if (pixels[i] != 0) {
			non_zero++;
		}
	}
	assert(non_zero > (size_t)width * (size_t)height / 2);
	free(pixels);
}

int main(void) {
	test_dock_hit();
	test_desktop_hit();
	test_apple_menu_hit();
	test_small_width_dock_fits();
	test_resolution_scaling();
	test_render_smoke();
	puts("shell layout tests passed");
	return 0;
}
