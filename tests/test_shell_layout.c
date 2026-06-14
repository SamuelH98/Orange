#include "orange/shell.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_roughly_double(int large, int small) {
	assert(abs(large - small * 2) <= 2);
}

static void test_dock_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.dock_item_count >= 10);
	struct orange_rect first = layout.dock_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_ITEM);
	assert(hit.index == 0);
	struct orange_desktop_entry entries[1];
	assert(orange_shell_dock_command(hit.index, entries, 0, &config) != NULL);
}

static void test_reordered_dock_hit_resolves_launcher(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_order[0] = 1;
	config.dock_order[1] = 0;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.dock_item_count >= 2);
	struct orange_rect first = layout.dock_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_ITEM);
	assert(hit.index == 0);
	assert(orange_shell_dock_launcher_index(&layout, hit.index) == 1);

	struct orange_desktop_entry entries[1] = {{
		.id = "org.gnome.Nautilus",
		.name = "Files",
		.icon = "system-file-manager",
		.exec = "nautilus %U",
	}};
	assert(strcmp(orange_shell_dock_label(
		orange_shell_dock_launcher_index(&layout, hit.index),
		entries, 1, &config), "Files") == 0);
	assert(strcmp(orange_shell_dock_command(
		orange_shell_dock_launcher_index(&layout, hit.index),
		entries, 1, &config), "nautilus %U") == 0);
}

static void test_default_dock_apps_have_fallback_commands(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_shell_dock_count(&config);
	assert(dock_count > 0);
	for (int i = 0; i < dock_count; i++) {
		const char *command = orange_shell_dock_command(
			i, NULL, 0, &config);
		assert(command != NULL);
		assert(command[0] != '\0');
	}
}

static void test_dock_trash_spacing_is_balanced(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	int first_leading = layout.dock_items[0].x - layout.dock.x;
	int last = layout.dock_item_count - 1;
	int trailing = layout.dock.x + layout.dock.width -
		(layout.dock_items[last].x + layout.dock_items[last].width);
	assert(abs(first_leading - trailing) <= 1);

	snprintf(config.dock_apps[last], sizeof(config.dock_apps[last]), "%s", "");
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	first_leading = layout.dock_items[0].x - layout.dock.x;
	last = layout.dock_item_count - 1;
	trailing = layout.dock.x + layout.dock.width -
		(layout.dock_items[last].x + layout.dock_items[last].width);
	assert(abs(first_leading - trailing) <= 1);
}

static void test_desktop_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	struct orange_rect item = layout.desktop_items[1];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
}

static void test_desktop_requires_entries(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, &layout);
	assert(layout.desktop_item_count == 0);
}

static void test_desktop_custom_position(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[1] =
		(struct orange_desktop_icon_position){true, 512, 144};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.desktop_items[1].x == 512);
	assert(layout.desktop_items[1].y == 144);
}

static void test_desktop_custom_position_clamps_to_visible_area(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 50000, 50000};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 1, &layout);
	struct orange_rect item = layout.desktop_items[0];
	assert(item.x >= 0);
	assert(item.y >= layout.menu_bar.height);
	assert(item.x + item.width <= layout.width);
	assert(item.y + item.height <= layout.dock.y);
}

static void test_system_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout closed;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &closed);
	struct orange_shell_hit hit = orange_shell_hit_test(&closed, 36, 16);
	assert(hit.kind == ORANGE_HIT_SYSTEM_MENU);
	hit = orange_shell_hit_test(&closed,
		closed.app_menu_button.x + closed.app_menu_button.width / 2,
		closed.app_menu_button.y + closed.app_menu_button.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);

	struct orange_shell_layout open;
	orange_shell_layout_compute(1920, 1080, true, &config, 2, &open);
	assert(open.system_menu_item_count > 4);
	struct orange_rect item = open.system_menu_items[3];
	hit = orange_shell_hit_test(&open, item.x + 8, item.y + 10);
	assert(hit.kind == ORANGE_HIT_SYSTEM_MENU_ITEM);
	assert(hit.index == 3);
	assert(orange_shell_menu_label(hit.index) != NULL);
}

static void test_status_area_hit_and_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.status_area.x + layout.status_area.width / 2,
		layout.status_area.y + layout.status_area.height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_AREA);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_STATUS, -1, 0, 0);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_STATUS);
	assert(layout.context_menu_item_count == 10);
	assert(orange_shell_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS, 0) != NULL);
	assert(strcmp(orange_shell_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS, 9), "Control Center Settings...") == 0);
	assert(orange_shell_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_STATUS, 0) != NULL);
}

static void test_small_width_dock_fits(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(900, 700, false, &config, 2, &layout);
	assert(layout.dock.x >= 0);
	assert(layout.dock.x + layout.dock.width <= layout.width);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].x >= layout.dock.x);
		assert(layout.dock_items[i].x + layout.dock_items[i].width <=
			layout.dock.x + layout.dock.width);
	}
}

static void test_invalid_visible_dock_order_has_no_blank_slot(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_shell_dock_count(&config);
	assert(dock_count > 1);
	config.dock_order[dock_count - 1] = dock_count;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440, 900, false, &config, 2, &layout);
	assert(layout.dock_item_count == dock_count);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].width > 0);
		assert(layout.dock_items[i].height > 0);
		assert(orange_shell_dock_launcher_index(&layout, i) >= 0);
		assert(orange_shell_dock_launcher_index(&layout, i) < dock_count);
	}
}

static void test_resolution_scaling(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout base;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &base);
	orange_shell_layout_compute(3840, 2160, false, &config, 2, &large);

	assert(large.menu_bar.height > base.menu_bar.height);
	assert(large.calendar_widget.width > base.calendar_widget.width);
	assert(large.weather_widget.height > base.weather_widget.height);
	assert(large.dock_items[0].width > base.dock_items[0].width);
	assert(large.dock.x + large.dock.width <= large.width);
}

static void test_major_surfaces_scale_by_resolution(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout small;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1440, 900, true, &config, 2, &small);
	orange_shell_layout_compute(2880, 1800, true, &config, 2, &large);

	assert_roughly_double(large.menu_bar.height, small.menu_bar.height);
	assert_roughly_double(large.system_menu_button.width,
		small.system_menu_button.width);
	assert_roughly_double(large.system_menu_panel.width,
		small.system_menu_panel.width);
	assert_roughly_double(large.system_menu_items[0].height,
		small.system_menu_items[0].height);
	assert_roughly_double(large.desktop_items[0].width,
		small.desktop_items[0].width);
	assert_roughly_double(large.desktop_items[0].height,
		small.desktop_items[0].height);
	assert_roughly_double(large.dock.width, small.dock.width);
	assert_roughly_double(large.dock.height, small.dock.height);
	assert_roughly_double(large.dock_items[0].width,
		small.dock_items[0].width);

	orange_shell_layout_set_context_menu(&small,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450);
	orange_shell_layout_set_context_menu(&large,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900);
	assert_roughly_double(large.context_menu_panel.width,
		small.context_menu_panel.width);
	assert_roughly_double(large.context_menu_panel.height,
		small.context_menu_panel.height);
	assert_roughly_double(large.context_menu_items[0].height,
		small.context_menu_items[0].height);
}

static void test_desktop_background_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(2048, 1153, false, &config, 2, &layout);
	/* click on empty desktop area below menu bar */
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, 500, 500);
	assert(hit.kind == ORANGE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_context_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	orange_shell_layout_set_context_menu(&layout, ORANGE_CONTEXT_MENU_DOCK, 0, 0, 0);
	assert(layout.context_menu_item_count == 5);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	/* index 1 is "Show in Files" (first non-separator item after index 0) */
	struct orange_rect item = layout.context_menu_items[1];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_CONTEXT_MENU_ITEM);
	assert(hit.index == 1);
	assert(orange_shell_context_menu_label(ORANGE_CONTEXT_MENU_DOCK, hit.index) != NULL);

	orange_shell_layout_set_context_menu(&layout, ORANGE_CONTEXT_MENU_DOCK,
		layout.dock_item_count - 1, layout.width, layout.height);
	assert(layout.context_menu_panel.x >= 0);
	assert(layout.context_menu_panel.x + layout.context_menu_panel.width <=
		layout.width);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
}

static void test_widget_hit_and_context_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	struct orange_rect widget = layout.widgets[0].rect;
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2);
	assert(hit.kind == ORANGE_HIT_WIDGET);
	assert(hit.index == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_WIDGET,
		hit.index,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2);
	assert(layout.context_menu_item_count == 5);
	assert(strcmp(orange_shell_context_menu_label(
		ORANGE_CONTEXT_MENU_WIDGET, 0), "Edit Widget") == 0);
	assert(strcmp(orange_shell_context_menu_label(
		ORANGE_CONTEXT_MENU_WIDGET, 4), "Remove Widget") == 0);
}

static void test_hidden_widget_not_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	struct orange_rect widget = layout.widgets[0].rect;
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_state state = {
		.system_menu_open = true,
		.hot_dock_index = 0,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
	};
	orange_shell_draw(pixels, width, height, stride, &state);

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

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_DARK;

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
	};
	orange_shell_draw(pixels, width, height, stride, &state);

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
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, &layout);
	assert(layout.widget_count == 2);
	assert(layout.widgets[0].type == ORANGE_WIDGET_CALENDAR);
	assert(layout.widgets[1].type == ORANGE_WIDGET_WEATHER);
	assert(layout.widgets[0].visible);
	assert(layout.widgets[1].visible);
}

int main(void) {
	test_dock_hit();
	test_reordered_dock_hit_resolves_launcher();
	test_default_dock_apps_have_fallback_commands();
	test_dock_trash_spacing_is_balanced();
	test_desktop_hit();
	test_desktop_requires_entries();
	test_desktop_custom_position();
	test_desktop_custom_position_clamps_to_visible_area();
	test_system_menu_hit();
	test_status_area_hit_and_menu();
	test_small_width_dock_fits();
	test_invalid_visible_dock_order_has_no_blank_slot();
	test_resolution_scaling();
	test_major_surfaces_scale_by_resolution();
	test_context_menu_hit();
	test_widget_hit_and_context_menu();
	test_hidden_widget_not_hit();
	test_desktop_background_hit();
	test_render_smoke();
	test_dark_render_smoke();
	test_widget_layer_exists();
	puts("shell layout tests passed");
	return 0;
}
