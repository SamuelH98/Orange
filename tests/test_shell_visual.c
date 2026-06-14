#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/shell.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static cairo_surface_t *solid_icon_surface(void) {
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 0.95, 0.12, 0.18, 1.0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_flush(surface);
	return surface;
}

static struct color pixel_at(
		uint32_t *pixels,
		int stride,
		int x,
		int y) {
	return pixel_from_u32(
		pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
}

static bool is_solid_icon_pixel(struct color color) {
	return color.a > 220 && color.r > 200 && color.g < 80 && color.b < 100;
}

static void test_context_menu_glass_and_scaling(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout small;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1440, 900, false, &config, 0, &small);
	orange_shell_layout_set_context_menu(&small,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450);
	orange_shell_layout_compute(VISUAL_WIDTH, VISUAL_HEIGHT, false, &config, 0,
		&large);
	orange_shell_layout_set_context_menu(&large,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900);
	assert(large.context_menu_panel.width > small.context_menu_panel.width);
	assert(large.context_menu_items[0].height >
		small.context_menu_items[0].height);
	assert(large.context_menu_item_count == 8);

	int stride = VISUAL_WIDTH * 4;
	uint32_t *pixels = calloc((size_t)VISUAL_HEIGHT, (size_t)stride);
	assert(pixels != NULL);
	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP,
		.context_menu_index = -1,
		.context_menu_cursor_x = 1440,
		.context_menu_cursor_y = 900,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, VISUAL_WIDTH, VISUAL_HEIGHT,
		stride, &state, &options);

	struct orange_rect panel = large.context_menu_panel;
	bool found_translucent_glass = false;
	for (int y = panel.y + 12; y < panel.y + panel.height - 12; y++) {
		for (int x = panel.x + panel.width - 30;
				x < panel.x + panel.width - 10; x++) {
			struct color c = pixel_from_u32(
				pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
			if (c.a > 25 && c.a < 245) {
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

static void test_dark_context_menu_uses_dark_material(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_DARK;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(VISUAL_WIDTH, VISUAL_HEIGHT, false, &config, 0,
		&layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900);

	int stride = VISUAL_WIDTH * 4;
	uint32_t *pixels = calloc((size_t)VISUAL_HEIGHT, (size_t)stride);
	assert(pixels != NULL);
	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP,
		.context_menu_index = -1,
		.context_menu_cursor_x = 1440,
		.context_menu_cursor_y = 900,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, VISUAL_WIDTH, VISUAL_HEIGHT,
		stride, &state, &options);

	struct orange_rect panel = layout.context_menu_panel;
	struct color center = pixel_at(pixels, stride,
		panel.x + panel.width - 20,
		panel.y + panel.height / 2);
	assert(center.a > 120);
	assert(center.r < 80);
	assert(center.g < 85);
	assert(center.b < 95);
	free(pixels);
}

static void test_missing_desktop_icon_draws_visible_fallback(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_desktop_entry entry = {
		.id = "org.example.MissingIcon",
		.name = "Missing Icon",
		.icon = "definitely-not-installed-orange-test-icon",
		.exec = "true",
		.terminal = false,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 1, &layout);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.desktop_entries = &entry,
		.desktop_entry_count = 1,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect item = layout.desktop_items[0];
	int icon_size = (int)((double)item.width * (116.0 / 194.0) + 0.5);
	struct color center = pixel_at(pixels, stride,
		item.x + item.width / 2,
		item.y + icon_size / 2);
	assert(center.a > 120);
	assert(center.r > 30 || center.g > 30 || center.b > 30);
	free(pixels);
}

static void test_dock_magnification_wave_paints_above_base_icons(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	/* Set up dock apps for the test */
	const char *dock_ids[] = {
		"__launcher__", "test1", "test2", "test3", "test4",
		"test5", "test6", "__trash__",
	};
	int dock_count = sizeof(dock_ids) / sizeof(dock_ids[0]);
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (i < dock_count) {
			snprintf(config.dock_apps[i], 128, "%s", dock_ids[i]);
		} else {
			config.dock_apps[i][0] = '\0';
		}
	}

	/* Create matching desktop entries */
	struct orange_desktop_entry entries[8];
	int entry_count = 0;
	for (int i = 0; i < dock_count; i++) {
		if (dock_ids[i][0] == '_') {
			continue;
		}
		snprintf(entries[entry_count].id, 256, "%s", dock_ids[i]);
		snprintf(entries[entry_count].name, 512, "Test %d", i);
		snprintf(entries[entry_count].icon, 512, "folder");
		snprintf(entries[entry_count].exec, 512, "true");
		entries[entry_count].terminal = false;
		entry_count++;
	}

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, &layout);
	int hot = 5;
	assert(layout.dock_item_count > hot + 1);
	struct orange_rect hot_base = layout.dock_items[hot];
	struct orange_rect neighbor_base = layout.dock_items[hot + 1];

	cairo_surface_t *icon = solid_icon_surface();
	struct orange_assets assets = {0};
	orange_assets_init(&assets);

	snprintf(assets.icons[assets.icon_count].name,
		sizeof(assets.icons[assets.icon_count].name), "%s", "folder");
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_LIGHT] =
		cairo_surface_reference(icon);
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_DARK] =
		cairo_surface_reference(icon);
	assets.icon_count++;

	snprintf(assets.icons[assets.icon_count].name,
		sizeof(assets.icons[assets.icon_count].name), "%s", "preferences-system");
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_LIGHT] =
		cairo_surface_reference(icon);
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_DARK] =
		cairo_surface_reference(icon);
	assets.icon_count++;

	snprintf(assets.icons[assets.icon_count].name,
		sizeof(assets.icons[assets.icon_count].name), "%s", "user-trash");
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_LIGHT] =
		cairo_surface_reference(icon);
	assets.icons[assets.icon_count].surface[ORANGE_ASSET_ICON_DARK] =
		cairo_surface_reference(icon);
	assets.icon_count++;

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = hot,
		.dock_pointer_x = hot_base.x + hot_base.width / 2,
		.dock_pointer_y = hot_base.y + hot_base.height / 2,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = &assets,
		.config = &config,
		.desktop_entries = entries,
		.desktop_entry_count = entry_count,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};

	config.dock_magnification = false;
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);
	struct color hot_plain = pixel_at(pixels, stride,
		hot_base.x + hot_base.width / 2, hot_base.y - 3);
	struct color neighbor_plain = pixel_at(pixels, stride,
		neighbor_base.x + neighbor_base.width / 2, neighbor_base.y - 3);

	memset(pixels, 0, (size_t)height * (size_t)stride);
	config.dock_magnification = true;
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);
	struct color hot_magnified = pixel_at(pixels, stride,
		hot_base.x + hot_base.width / 2, hot_base.y - 3);
	struct color neighbor_magnified = pixel_at(pixels, stride,
		neighbor_base.x + neighbor_base.width / 2, neighbor_base.y - 3);

	assert(!is_solid_icon_pixel(hot_plain));
	assert(!is_solid_icon_pixel(neighbor_plain));
	assert(is_solid_icon_pixel(hot_magnified));
	assert(is_solid_icon_pixel(neighbor_magnified));

	cairo_surface_destroy(icon);
	orange_assets_finish(&assets);
	free(pixels);
}

int main(void) {
	test_context_menu_glass_and_scaling();
	test_dark_context_menu_uses_dark_material();
	test_missing_desktop_icon_draws_visible_fallback();
	test_dock_magnification_wave_paints_above_base_icons();
	puts("shell visual tests passed");
	return 0;
}
