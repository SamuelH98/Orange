#include "tahoe/shell.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void test_dock_hit(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
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
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	struct tahoe_rect item = layout.desktop_items[1];
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == TAHOE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
}

static void test_desktop_requires_entries(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 0, &layout);
	assert(layout.desktop_item_count == 0);
}

static void test_desktop_custom_position(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	config.desktop_positions[1] =
		(struct tahoe_desktop_icon_position){true, 512, 144};
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.desktop_items[1].x == 512);
	assert(layout.desktop_items[1].y == 144);
}

static void test_apple_menu_hit(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout closed;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &closed);
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(&closed, 36, 16);
	assert(hit.kind == TAHOE_HIT_APPLE_MENU);

	struct tahoe_shell_layout open;
	tahoe_shell_layout_compute(1920, 1080, true, &config, 2, &open);
	assert(open.apple_menu_item_count > 4);
	struct tahoe_rect item = open.apple_menu_items[3];
	hit = tahoe_shell_hit_test(&open, item.x + 8, item.y + 10);
	assert(hit.kind == TAHOE_HIT_APPLE_MENU_ITEM);
	assert(hit.index == 3);
	assert(tahoe_shell_menu_label(hit.index) != NULL);
}

static void test_small_width_dock_fits(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(900, 700, false, &config, 2, &layout);
	assert(layout.dock.x >= 0);
	assert(layout.dock.x + layout.dock.width <= layout.width);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].x >= layout.dock.x);
		assert(layout.dock_items[i].x + layout.dock_items[i].width <=
			layout.dock.x + layout.dock.width);
	}
}

static void test_resolution_scaling(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout base;
	struct tahoe_shell_layout large;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &base);
	tahoe_shell_layout_compute(3840, 2160, false, &config, 2, &large);

	assert(large.menu_bar.height > base.menu_bar.height);
	assert(large.calendar_widget.width > base.calendar_widget.width);
	assert(large.weather_widget.height > base.weather_widget.height);
	assert(large.dock_items[0].width > base.dock_items[0].width);
	assert(large.dock.x + large.dock.width <= large.width);
}

static void test_reference_dock_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	assert(layout.dock.x == 153);
	assert(layout.dock.y == 1055);
	assert(layout.dock.width == 1742);
	assert(layout.dock.height == 98);
	assert(layout.dock_items[0].width == 62);
	assert(layout.dock_items[0].x == 173);
	assert(layout.dock_items[0].y == 1066);
	assert(layout.dock_items[1].x - layout.dock_items[0].x == 82);
}

static void test_context_menu_hit(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	tahoe_shell_layout_set_context_menu(&layout, TAHOE_CONTEXT_MENU_DOCK, 0);
	assert(layout.context_menu_item_count == 3);
	struct tahoe_rect item = layout.context_menu_items[1];
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == TAHOE_HIT_CONTEXT_MENU_ITEM);
	assert(hit.index == 1);
	assert(tahoe_shell_context_menu_label(TAHOE_CONTEXT_MENU_DOCK, hit.index) != NULL);
}

static void test_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_state state = {
		.apple_menu_open = true,
		.hot_dock_index = 0,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
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

static void test_dark_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	config.appearance = TAHOE_APPEARANCE_DARK;

	struct tahoe_shell_state state = {
		.apple_menu_open = false,
		.hot_dock_index = -1,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
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

static void test_widget_layer_exists(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.widget_count == 2);
	assert(layout.widgets[0].type == TAHOE_WIDGET_CALENDAR);
	assert(layout.widgets[1].type == TAHOE_WIDGET_WEATHER);
	assert(layout.widgets[0].visible);
	assert(layout.widgets[1].visible);
}

int main(void) {
	test_dock_hit();
	test_desktop_hit();
	test_desktop_requires_entries();
	test_desktop_custom_position();
	test_apple_menu_hit();
	test_small_width_dock_fits();
	test_resolution_scaling();
	test_reference_dock_measurements();
	test_context_menu_hit();
	test_render_smoke();
	test_dark_render_smoke();
	test_widget_layer_exists();
	puts("shell layout tests passed");
	return 0;
}
