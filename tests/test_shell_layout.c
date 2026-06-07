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

static void test_reference_menu_bar_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	assert(layout.menu_bar.x == 0);
	assert(layout.menu_bar.y == 0);
	assert(layout.menu_bar.width == 2048);
	assert(layout.menu_bar.height == 32);
}

static void test_reference_apple_menu_button_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	assert(layout.apple_menu_button.x == 18);
	assert(layout.apple_menu_button.y == 0);
	assert(layout.apple_menu_button.width == 43);
	assert(layout.apple_menu_button.height == 32);
}

static void test_reference_apple_menu_panel_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, true, &config, 2, &layout);
	assert(layout.apple_menu_item_count == 10);
	assert(layout.apple_menu_panel.x == 18);
	assert(layout.apple_menu_panel.y == 36);
	assert(layout.apple_menu_panel.width == 260);
	assert(layout.apple_menu_panel.height == 358);
	/* item rects */
	assert(layout.apple_menu_items[0].x == 26);
	assert(layout.apple_menu_items[0].y == 45);
	assert(layout.apple_menu_items[0].width == 244);
	assert(layout.apple_menu_items[0].height == 32);
	/* item spacing */
	assert(layout.apple_menu_items[1].y - layout.apple_menu_items[0].y == 34);
	assert(layout.apple_menu_items[9].y == 45 + 9 * 34);
	/* separator flags */
	assert(layout.apple_menu_separator[0] == false);
	assert(layout.apple_menu_separator[3] == true);
	assert(layout.apple_menu_separator[4] == true);
	assert(layout.apple_menu_separator[5] == true);
	assert(layout.apple_menu_separator[8] == true);
	assert(layout.apple_menu_separator[9] == false);
}

static void test_reference_widget_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	assert(layout.calendar_widget.x == 23);
	assert(layout.calendar_widget.y == 66);
	assert(layout.calendar_widget.width == 233);
	assert(layout.calendar_widget.height == 234);
	assert(layout.weather_widget.x == 272);
	assert(layout.weather_widget.y == 66);
	assert(layout.weather_widget.width == 232);
	assert(layout.weather_widget.height == 232);
}

static void test_reference_desktop_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	assert(layout.desktop_item_count == 2);
	assert(layout.desktop_items[0].x == 1884);
	assert(layout.desktop_items[0].y == 74);
	assert(layout.desktop_items[0].width == 138);
	assert(layout.desktop_items[0].height == 120);
	assert(layout.desktop_items[1].x == 1879);
	assert(layout.desktop_items[1].y == 220);
	assert(layout.desktop_items[1].width == 138);
	assert(layout.desktop_items[1].height == 120);
}

static void test_reference_context_menu_dock_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	tahoe_shell_layout_set_context_menu(&layout, TAHOE_CONTEXT_MENU_DOCK, 0, 0, 0);
	assert(layout.context_menu_kind == TAHOE_CONTEXT_MENU_DOCK);
	assert(layout.context_menu_item_count == 5);
	assert(layout.context_menu_panel.x == 112);
	assert(layout.context_menu_panel.y == 889);
	assert(layout.context_menu_panel.width == 184);
	assert(layout.context_menu_panel.height == 167);
	assert(layout.context_menu_items[0].x == 119);
	assert(layout.context_menu_items[0].y == 895);
	assert(layout.context_menu_items[0].width == 170);
	assert(layout.context_menu_items[0].height == 31);
	assert(layout.context_menu_items[1].y - layout.context_menu_items[0].y == 31);
	assert(layout.context_menu_separator[2] == true);
	assert(layout.context_menu_separator[4] == true);
}

static void test_reference_context_menu_desktop_icon_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	tahoe_shell_layout_set_context_menu(&layout, TAHOE_CONTEXT_MENU_DESKTOP_ICON, 0, 0, 0);
	assert(layout.context_menu_kind == TAHOE_CONTEXT_MENU_DESKTOP_ICON);
	assert(layout.context_menu_item_count == 9);
	assert(layout.context_menu_panel.width == 220);
	assert(layout.context_menu_panel.y == 94);
	assert(layout.context_menu_items[0].y == 100);
	assert(layout.context_menu_separator[2] == true);
	assert(layout.context_menu_separator[6] == true);
	assert(layout.context_menu_separator[8] == true);
}

static void test_reference_context_menu_desktop_measurements(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	tahoe_shell_layout_set_context_menu(&layout, TAHOE_CONTEXT_MENU_DESKTOP, -1, 960, 540);
	assert(layout.context_menu_kind == TAHOE_CONTEXT_MENU_DESKTOP);
	assert(layout.context_menu_item_count == 8);
	assert(layout.context_menu_panel.width == 220);
	assert(layout.context_menu_panel.x == 850);
	assert(layout.context_menu_panel.y == 532);
	assert(layout.context_menu_separator[2] == true);
	assert(layout.context_menu_separator[5] == true);
}

static void test_desktop_background_hit(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	/* click on empty desktop area below menu bar */
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(&layout, 500, 500);
	assert(hit.kind == TAHOE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_context_menu_hit(void) {
	struct tahoe_config config;
	tahoe_config_set_defaults(&config);
	struct tahoe_shell_layout layout;
	tahoe_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	tahoe_shell_layout_set_context_menu(&layout, TAHOE_CONTEXT_MENU_DOCK, 0, 0, 0);
	assert(layout.context_menu_item_count == 5);
	/* index 1 is "Show in Finder" (first non-separator item after index 0) */
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
	test_reference_menu_bar_measurements();
	test_reference_apple_menu_button_measurements();
	test_reference_apple_menu_panel_measurements();
	test_reference_widget_measurements();
	test_reference_desktop_measurements();
	test_reference_context_menu_dock_measurements();
	test_reference_context_menu_desktop_icon_measurements();
	test_reference_context_menu_desktop_measurements();
	test_context_menu_hit();
	test_desktop_background_hit();
	test_render_smoke();
	test_dark_render_smoke();
	test_widget_layer_exists();
	puts("shell layout tests passed");
	return 0;
}
