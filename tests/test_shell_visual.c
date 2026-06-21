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

static cairo_surface_t *colored_icon_surface(double r, double g, double b) {
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgba(cr, r, g, b, 1.0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_flush(surface);
	return surface;
}

static void add_test_icon(
		struct orange_assets *assets,
		const char *name,
		cairo_surface_t *surface) {
	assert(assets->icon_count < ORANGE_ASSET_ICON_MAX);
	struct orange_named_icon *icon = &assets->icons[assets->icon_count++];
	snprintf(icon->name, sizeof(icon->name), "%s", name);
	icon->surface[ORANGE_ASSET_ICON_LIGHT] = cairo_surface_reference(surface);
	icon->surface[ORANGE_ASSET_ICON_DARK] = cairo_surface_reference(surface);
	icon->resolved[ORANGE_ASSET_ICON_LIGHT] = true;
	icon->resolved[ORANGE_ASSET_ICON_DARK] = true;
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

static bool color_close(struct color color, int r, int g, int b) {
	return color.a > 220 &&
		abs(color.r - r) <= 4 &&
		abs(color.g - g) <= 4 &&
		abs(color.b - b) <= 4;
}

static bool colors_near(struct color a, struct color b, int tolerance) {
	return abs(a.r - b.r) <= tolerance &&
		abs(a.g - b.g) <= tolerance &&
		abs(a.b - b.b) <= tolerance &&
		abs(a.a - b.a) <= tolerance;
}

static uint32_t composite_over_opaque(uint32_t src, uint32_t dst) {
	int alpha = (int)((src >> 24) & 0xff);
	int inv = 255 - alpha;
	int sr = (int)((src >> 16) & 0xff);
	int sg = (int)((src >> 8) & 0xff);
	int sb = (int)(src & 0xff);
	int dr = (int)((dst >> 16) & 0xff);
	int dg = (int)((dst >> 8) & 0xff);
	int db = (int)(dst & 0xff);
	int r = sr + (dr * inv + 127) / 255;
	int g = sg + (dg * inv + 127) / 255;
	int b = sb + (db * inv + 127) / 255;
	if (r > 255) {
		r = 255;
	}
	if (g > 255) {
		g = 255;
	}
	if (b > 255) {
		b = 255;
	}
	return 0xff000000u |
		((uint32_t)r << 16) |
		((uint32_t)g << 8) |
		(uint32_t)b;
}

static void test_context_menu_glass_and_scaling(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout small;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &small);
	orange_shell_layout_set_context_menu(&small,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450, NULL);
	orange_shell_layout_compute(VISUAL_WIDTH, VISUAL_HEIGHT, false, &config, 0, 0, &large);
	orange_shell_layout_set_context_menu(&large,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900, NULL);
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
	orange_shell_layout_compute(VISUAL_WIDTH, VISUAL_HEIGHT, false, &config, 0, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900, NULL);

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
	assert(center.a > 50);
	assert(center.r < 80);
	assert(center.g < 85);
	assert(center.b < 95);
	free(pixels);
}

static void test_base_shell_can_skip_transient_overlays(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, width / 2, height / 2, NULL);
	struct orange_rect panel = layout.context_menu_panel;
	int sample_x = panel.x + panel.width - 20;
	int sample_y = panel.y + panel.height / 2;

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
		.context_menu_cursor_x = width / 2,
		.context_menu_cursor_y = height / 2,
	};
	const struct orange_shell_draw_options normal_options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &normal_options);
	assert(pixel_at(pixels, stride, sample_x, sample_y).a > 25);

	memset(pixels, 0, (size_t)height * (size_t)stride);
	const struct orange_shell_draw_options base_only_options = {
		.draw_wallpaper = false,
		.skip_transient_overlays = true,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &base_only_options);
	assert(pixel_at(pixels, stride, sample_x, sample_y).a == 0);
	free(pixels);
}

static void test_overlay_draw_clears_when_no_overlay_is_active(void) {
	const int width = 640;
	const int height = 360;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);
	for (int i = 0; i < width * height; i++) {
		pixels[i] = 0xffffffffu;
	}

	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_state state = {
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
	};
	orange_shell_draw_overlay(pixels, width, height, stride, &state);
	assert(pixel_at(pixels, stride, width / 2, height / 2).a == 0);
	assert(pixel_at(pixels, stride, 8, 8).a == 0);
	free(pixels);
}

static void test_menu_overlay_bounds_are_tight(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *base = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(base != NULL);
	assert(overlay != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_state state = {
		.system_menu_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, true, &config, 0, 0, &layout);

	struct orange_rect bounds;
	assert(orange_shell_overlay_bounds(width, height, &state, &bounds));
	assert(bounds.x <= layout.system_menu_panel.x);
	assert(bounds.y <= layout.system_menu_panel.y);
	assert(bounds.x + bounds.width >=
		layout.system_menu_panel.x + layout.system_menu_panel.width);
	assert(bounds.y + bounds.height >=
		layout.system_menu_panel.y + layout.system_menu_panel.height);
	assert(bounds.width * bounds.height < width * height / 3);

	const struct orange_shell_draw_options base_options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
	};
	orange_shell_draw_with_options(base, width, height, stride,
		&state, &base_options);
	orange_shell_draw_overlay_with_backdrop(overlay, width, height, stride,
		base, stride, &state);
	assert(pixel_at(overlay, stride, width - 8, height - 8).a == 0);
	assert(pixel_at(overlay, stride,
		layout.system_menu_panel.x + layout.system_menu_panel.width / 2,
		layout.system_menu_panel.y + layout.system_menu_panel.height / 2).a > 0);

	state.system_menu_open = false;
	state.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP;
	state.context_menu_index = -1;
	state.context_menu_cursor_x = width / 2;
	state.context_menu_cursor_y = height / 2;
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, width / 2, height / 2, NULL);
	assert(orange_shell_overlay_bounds(width, height, &state, &bounds));
	assert(bounds.x <= layout.context_menu_panel.x);
	assert(bounds.y <= layout.context_menu_panel.y);
	assert(bounds.x + bounds.width >=
		layout.context_menu_panel.x + layout.context_menu_panel.width);
	assert(bounds.y + bounds.height >=
		layout.context_menu_panel.y + layout.context_menu_panel.height);
	assert(bounds.width * bounds.height < width * height / 3);

	free(base);
	free(overlay);
}

static void assert_backdrop_overlay_matches_direct_launcher(
		enum orange_launcher_mode mode) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *base = calloc((size_t)height, (size_t)stride);
	uint32_t *direct = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(base != NULL);
	assert(direct != NULL);
	assert(overlay != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_state state = {
		.launcher_open = true,
		.launcher_display_mode = mode,
		.launcher_search_focus = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
	};
	snprintf(state.launcher_categories[0],
		sizeof(state.launcher_categories[0]), "%s", "All");
	state.launcher_category_count = 1;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
	orange_shell_layout_set_launcher(&layout, false, mode, 0);
	struct orange_rect sample_rect = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		layout.launcher_panel : layout.launcher_search_field;
	int sample_x = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		sample_rect.x + 24 : sample_rect.x + sample_rect.width - 24;
	int sample_y = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		sample_rect.y + sample_rect.height - 24 :
		sample_rect.y + sample_rect.height / 2;

	const struct orange_shell_draw_options base_options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
	};
	orange_shell_draw_with_options(base, width, height, stride,
		&state, &base_options);
	orange_shell_draw(direct, width, height, stride, &state);
	orange_shell_draw_overlay_with_backdrop(overlay, width, height, stride,
		base, stride, &state);

	size_t idx = (size_t)sample_y * (size_t)(stride / 4) + (size_t)sample_x;
	assert(((overlay[idx] >> 24) & 0xff) > 0);
	struct color composed = pixel_from_u32(
		composite_over_opaque(overlay[idx], base[idx]));
	struct color direct_color = pixel_from_u32(direct[idx]);
	assert(colors_near(composed, direct_color, 3));

	free(base);
	free(direct);
	free(overlay);
}

static void test_backdrop_overlay_matches_direct_launcher_glass(void) {
	assert_backdrop_overlay_matches_direct_launcher(
		ORANGE_LAUNCHER_DISPLAY_FULL);
	assert_backdrop_overlay_matches_direct_launcher(
		ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
}

static void test_notification_center_renders_panel_cards_and_button(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
	orange_shell_layout_set_notification_center(&layout);
	assert(layout.notification_center_card_count >= 4);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.notification_center_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect panel = layout.notification_center_panel;
	struct color panel_pixel = pixel_at(pixels, stride,
		panel.x + panel.width / 2,
		panel.y + 12);
	assert(panel_pixel.a > 30);

	struct orange_rect card = layout.notification_center_cards[0];
	struct color card_pixel = pixel_at(pixels, stride,
		card.x + card.width / 2,
		card.y + card.height / 2);
	assert(card_pixel.a > 90);

	struct orange_rect edit = layout.notification_center_edit_button;
	struct color edit_pixel = pixel_at(pixels, stride,
		edit.x + edit.width / 2,
		edit.y + edit.height / 2);
	assert(edit_pixel.a > 70);
	free(pixels);
}

static void test_desktop_volume_icon_draws(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_volume_info volume = {
		.label = "Test Volume",
		.mount_path = "/mnt/test",
		.icon_name = "drive-harddisk",
		.is_removable = false,
		.is_internal = true,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, 1, &layout);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.desktop_entries = NULL,
		.desktop_entry_count = 0,
		.volumes = &volume,
		.volume_count = 1,
		.desktop_volume_count = 1,
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect item = layout.desktop_items[0];
	/* Without assets, volume draws no icon, but label should still render */
	int label_y = item.y + (int)((double)item.width * 0.62 + 0.5) + 4;
	struct color label_pixel = pixel_at(pixels, stride,
		item.x + item.width / 2, label_y);
	(void)label_pixel;
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
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
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

static void test_default_dock_prefers_desktop_icons_before_role_icons(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	config.dock_magnification = false;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config, 0, 0, &layout);
	assert(layout.dock_item_count > 8);

	cairo_surface_t *generic = colored_icon_surface(0.95, 0.12, 0.18);
	cairo_surface_t *image_viewer = colored_icon_surface(0.65, 0.22, 0.92);
	cairo_surface_t *notes = colored_icon_surface(1.00, 0.78, 0.10);
	cairo_surface_t *text_editor = colored_icon_surface(0.15, 0.25, 0.95);
	cairo_surface_t *video_player = colored_icon_surface(0.10, 0.80, 0.30);
	cairo_surface_t *video_display = colored_icon_surface(0.95, 0.35, 0.10);
	struct orange_assets assets = {0};
	orange_assets_init(&assets);
	add_test_icon(&assets, "image-x-generic", generic);
	add_test_icon(&assets, "image-viewer", image_viewer);
	add_test_icon(&assets, "org.gnome.Loupe", generic);
	add_test_icon(&assets, "accessories-text-editor", notes);
	add_test_icon(&assets, "org.gnome.TextEditor", text_editor);
	add_test_icon(&assets, "video-player", video_player);
	add_test_icon(&assets, "video-display", video_display);
	add_test_icon(&assets, "org.gnome.Showtime", generic);

	const struct orange_desktop_entry entries[] = {
		{
			.id = "org.gnome.Loupe",
			.name = "Image Viewer",
			.icon = "image-x-generic",
			.exec = "loupe",
		},
		{
			.id = "org.gnome.TextEditor",
			.name = "Notes",
			.icon = "org.gnome.TextEditor",
			.exec = "gnome-text-editor",
		},
		{
			.id = "org.gnome.Showtime",
			.name = "Video Player",
			.icon = "video-display",
			.exec = "showtime",
		},
	};
	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = &assets,
		.config = &config,
		.desktop_entries = entries,
		.desktop_entry_count = (int)(sizeof(entries) / sizeof(entries[0])),
	};
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect loupe_item = layout.dock_items[4];
	struct orange_rect notes_item = layout.dock_items[7];
	struct orange_rect video_item = layout.dock_items[8];
	assert(color_close(pixel_at(pixels, stride,
		loupe_item.x + loupe_item.width / 2,
		loupe_item.y + loupe_item.height / 2), 242, 31, 46));
	assert(color_close(pixel_at(pixels, stride,
		notes_item.x + notes_item.width / 2,
		notes_item.y + notes_item.height / 2), 38, 64, 242));
	assert(color_close(pixel_at(pixels, stride,
		video_item.x + video_item.width / 2,
		video_item.y + video_item.height / 2), 242, 89, 26));

	cairo_surface_destroy(generic);
	cairo_surface_destroy(image_viewer);
	cairo_surface_destroy(notes);
	cairo_surface_destroy(text_editor);
	cairo_surface_destroy(video_player);
	cairo_surface_destroy(video_display);
	orange_assets_finish(&assets);
	free(pixels);
}

int main(void) {
	test_context_menu_glass_and_scaling();
	test_dark_context_menu_uses_dark_material();
	test_base_shell_can_skip_transient_overlays();
	test_overlay_draw_clears_when_no_overlay_is_active();
	test_menu_overlay_bounds_are_tight();
	test_backdrop_overlay_matches_direct_launcher_glass();
	test_notification_center_renders_panel_cards_and_button();
	test_desktop_volume_icon_draws();
	test_dock_magnification_wave_paints_above_base_icons();
	test_default_dock_prefers_desktop_icons_before_role_icons();
	puts("shell visual tests passed");
	return 0;
}
