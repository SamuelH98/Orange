#include "tahoe/config.h"
#include "tahoe/shell.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define VISUAL_WIDTH 2880
#define VISUAL_HEIGHT 1800

struct color {
	int r;
	int g;
	int b;
	int a;
};

static struct color pixel_from_u32(uint32_t pixel) {
	return (struct color){
		.r = (pixel >> 16) & 0xff,
		.g = (pixel >> 8) & 0xff,
		.b = pixel & 0xff,
		.a = (pixel >> 24) & 0xff,
	};
}

static void test_context_menu_glass_and_scaling(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);

	struct tahoe_shell_layout small;
	struct tahoe_shell_layout large;
	tahoe_shell_layout_compute(1440, 900, false, &config, 0, &small);
	tahoe_shell_layout_set_context_menu(&small,
		TAHOE_CONTEXT_MENU_DESKTOP, -1, 720, 450);
	tahoe_shell_layout_compute(VISUAL_WIDTH, VISUAL_HEIGHT, false, &config, 0,
		&large);
	tahoe_shell_layout_set_context_menu(&large,
		TAHOE_CONTEXT_MENU_DESKTOP, -1, 1440, 900);
	assert(large.context_menu_panel.width > small.context_menu_panel.width);
	assert(large.context_menu_items[0].height >
		small.context_menu_items[0].height);
	assert(large.context_menu_item_count == 8);

	int stride = VISUAL_WIDTH * 4;
	uint32_t *pixels = calloc((size_t)VISUAL_HEIGHT, (size_t)stride);
	assert(pixels != NULL);
	struct tahoe_shell_state state = {
		.apple_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.context_menu_kind = TAHOE_CONTEXT_MENU_DESKTOP,
		.context_menu_index = -1,
		.context_menu_cursor_x = 1440,
		.context_menu_cursor_y = 900,
	};
	const struct tahoe_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	tahoe_shell_draw_with_options(pixels, VISUAL_WIDTH, VISUAL_HEIGHT,
		stride, &state, &options);

	struct tahoe_rect panel = large.context_menu_panel;
	bool found_translucent_glass = false;
	for (int y = panel.y + 12; y < panel.y + panel.height - 12; y++) {
		for (int x = panel.x + panel.width - 30;
				x < panel.x + panel.width - 10; x++) {
			struct color c = pixel_from_u32(
				pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
			if (c.a > 25 && c.a < 220) {
				found_translucent_glass = true;
				break;
			}
		}
		if (found_translucent_glass) {
			break;
		}
	}
	assert(found_translucent_glass);
	free(pixels);
}

int main(void) {
	test_context_menu_glass_and_scaling();
	puts("shell visual tests passed");
	return 0;
}
