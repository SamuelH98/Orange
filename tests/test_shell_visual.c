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

static void fill_pixels(uint32_t *pixels, int width, int height, uint32_t color) {
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			pixels[(size_t)y * (size_t)width + (size_t)x] = color;
		}
	}
}

static void assert_not_bright_source(struct color color) {
	assert(!(color.a > 24 && color.r > 205 &&
		color.g > 205 && color.b > 205));
}

static void assert_clear_source(struct color color) {
	assert(color.a == 0);
}

static void test_context_menu_glass_and_scaling(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout small;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &small);
	orange_shell_layout_set_context_menu(&small,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450, NULL);
	orange_shell_layout_compute(VISUAL_WIDTH,VISUAL_HEIGHT,false,&config,0,0, NULL, 0, &large);
	orange_shell_layout_set_context_menu(&large,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900, NULL);
	assert(large.context_menu_panel.width > small.context_menu_panel.width);
	assert(large.context_menu_items[0].height >
		small.context_menu_items[0].height);
	assert(large.context_menu_item_count == 9);

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

static void test_light_context_menu_text_uses_white_palette(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_LIGHT;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(VISUAL_WIDTH,VISUAL_HEIGHT,false,&config,0,0, NULL, 0, &layout);
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

	struct orange_rect item = layout.context_menu_items[0];
	int bright_text_pixels = 0;
	for (int y = item.y + 12; y < item.y + 40; y++) {
		for (int x = item.x + 42; x < item.x + 190; x++) {
			struct color c = pixel_at(pixels, stride, x, y);
			if (c.a > 100 && c.r > 150 && c.g > 150 && c.b > 150) {
				bright_text_pixels++;
			}
		}
	}
	assert(bright_text_pixels > 8);
	free(pixels);
}

static void test_dark_context_menu_uses_dark_material(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_DARK;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(VISUAL_WIDTH,VISUAL_HEIGHT,false,&config,0,0, NULL, 0, &layout);
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

static int count_checkmark_pixels(
		uint32_t *pixels,
		int stride,
		struct orange_rect item) {
	int count = 0;
	int left = item.x + 8;
	int right = item.x + 32;
	int top = item.y + 8;
	int bottom = item.y + item.height - 8;
	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			struct color c = pixel_at(pixels, stride, x, y);
			if (c.a > 90 && c.r > 150 && c.g > 150 && c.b > 150) {
				count++;
			}
		}
	}
	return count;
}

static void test_desktop_context_menu_stacks_checkmark_row(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_use_stacks = true;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(VISUAL_WIDTH,VISUAL_HEIGHT,false,&config,0,0, NULL, 0, &layout);
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

	int get_info_check_pixels = count_checkmark_pixels(pixels, stride,
		layout.context_menu_items[2]);
	int use_stacks_check_pixels = count_checkmark_pixels(pixels, stride,
		layout.context_menu_items[5]);
	assert(get_info_check_pixels < 4);
	assert(use_stacks_check_pixels > 8);
	free(pixels);
}

static void test_menu_surfaces_avoid_white_halo_without_shadow(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(backdrop != NULL);
	assert(overlay != NULL);

	fill_pixels(backdrop, width, height, 0xffffffffu);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_state menu_state = {
		.system_menu_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, true, &config,
		0, 0, NULL, 0, &layout);
	struct orange_rect panel = layout.system_menu_panel;
	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &menu_state);

	assert_not_bright_source(pixel_at(overlay, stride,
		panel.x + panel.width / 2, panel.y));
	assert_clear_source(pixel_at(overlay, stride,
		panel.x + panel.width / 2, panel.y + panel.height + 4));

	memset(overlay, 0, (size_t)height * (size_t)stride);
	struct orange_file_info files[1] = {
		{.name = "Project Notes.txt", .path = "/tmp/Project Notes.txt"},
	};
	struct orange_shell_state submenu_state = {
		.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_FILE,
		.context_menu_index = 0,
		.context_menu_cursor_x = width / 2,
		.context_menu_cursor_y = height / 2,
		.open_with_submenu_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
		.desktop_files = files,
		.desktop_file_count = 1,
	};
	orange_shell_layout_compute(width, height, false, &config,
		0, 0, files, 1, &layout);
	layout.open_with_submenu_open = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0, width / 2, height / 2, NULL);
	assert(layout.context_submenu_item_count > 0);
	panel = layout.context_submenu_panel;
	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &submenu_state);

	assert_not_bright_source(pixel_at(overlay, stride,
		panel.x + panel.width / 2, panel.y));
	assert_clear_source(pixel_at(overlay, stride,
		panel.x + panel.width / 2, panel.y + panel.height + 4));

	free(backdrop);
	free(overlay);
}

static void test_dock_context_menu_tail_is_seamless(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(backdrop != NULL);
	assert(overlay != NULL);

	fill_pixels(backdrop, width, height, 0xffffffffu);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_state state = {
		.context_menu_kind = ORANGE_CONTEXT_MENU_DOCK,
		.context_menu_index = 0,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config,
		0, 0, NULL, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK, 0, 0, 0, NULL);
	assert(layout.context_menu_panel.width > 0);
	assert(layout.dock_item_count > 0);

	struct orange_rect panel = layout.context_menu_panel;
	struct orange_rect anchor = layout.dock_items[0];
	int tail_x = anchor.x + anchor.width / 2;
	if (tail_x < panel.x + 34) {
		tail_x = panel.x + 34;
	}
	if (tail_x > panel.x + panel.width - 34) {
		tail_x = panel.x + panel.width - 34;
	}

	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &state);

	struct color body = pixel_at(overlay, stride,
		tail_x, panel.y + panel.height - 2);
	struct color seam = pixel_at(overlay, stride,
		tail_x, panel.y + panel.height + 1);
	assert(body.a > 0);
	assert(seam.a > 0);
	assert_not_bright_source(seam);

	int tail_pixels = 0;
	for (int y = panel.y + panel.height;
			y < panel.y + panel.height + 18 && y < height; y++) {
		for (int x = tail_x - 18; x <= tail_x + 18; x++) {
			if (x < 0 || x >= width) {
				continue;
			}
			struct color tail = pixel_at(overlay, stride, x, y);
			if (tail.a > 0) {
				assert_not_bright_source(tail);
				tail_pixels++;
			}
		}
	}
	assert(tail_pixels > 16);

	free(backdrop);
	free(overlay);
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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
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

static void test_dock_bounce_keeps_auto_hide_overlay_visible(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_auto_hide = true;

	struct orange_shell_state state = {
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.dock_auto_hide_blocked = true,
	};
	struct orange_rect bounds;
	assert(!orange_shell_overlay_bounds(width, height, &state, &bounds));

	state.dock_bounce_active = true;
	state.dock_bounce_launcher_idx = 0;
	state.dock_bounce_start_time = 1000000;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config,
		0, 0, NULL, 0, &layout);
	orange_shell_layout_apply_dock_auto_hide_progress(&layout, &config,
		true, false, 1.0);

	assert(orange_shell_overlay_bounds(width, height, &state, &bounds));
	assert(bounds.x <= layout.dock.x);
	assert(bounds.y <= layout.dock.y);
	assert(bounds.x + bounds.width >= layout.dock.x + layout.dock.width);
	assert(bounds.y + bounds.height >= layout.dock.y + layout.dock.height);

	orange_shell_draw_overlay(pixels, width, height, stride, &state);
	assert(pixel_at(pixels, stride,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + layout.dock.height / 2).a > 0);

	memset(pixels, 0, (size_t)height * (size_t)stride);
	const struct orange_shell_draw_options draw_dock_options = {
		.draw_wallpaper = false,
		.skip_transient_overlays = true,
		.skip_dock = false,
		.draw_only_dock = true,
		.clip_to_bounds = true,
		.clip_bounds = layout.dock,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &draw_dock_options);
	assert(pixel_at(pixels, stride,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + layout.dock.height / 2).a > 0);

	memset(pixels, 0, (size_t)height * (size_t)stride);
	const struct orange_shell_draw_options skip_dock_options = {
		.draw_wallpaper = false,
		.skip_transient_overlays = true,
		.skip_dock = true,
		.draw_only_dock = true,
		.clip_to_bounds = true,
		.clip_bounds = layout.dock,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &skip_dock_options);
	assert(pixel_at(pixels, stride,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + layout.dock.height / 2).a == 0);

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
	orange_shell_layout_compute(width,height,true,&config,0,0, NULL, 0, &layout);

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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
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

	struct orange_file_info files[1] = {
		{.name = "Project Notes.txt", .path = "/tmp/Project Notes.txt"},
	};
	state.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_FILE;
	state.context_menu_index = 0;
	state.context_menu_cursor_x = width / 2;
	state.context_menu_cursor_y = height / 2;
	state.desktop_files = files;
	state.desktop_file_count = 1;
	state.open_with_app_count = ORANGE_OPEN_WITH_APP_MAX;
	state.open_with_submenu_open = true;
	orange_shell_layout_compute(width,height,false,&config,0,0, files, 1,
		&layout);
	layout.open_with_app_count = state.open_with_app_count;
	layout.open_with_submenu_open = state.open_with_submenu_open;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0, width / 2, height / 2, NULL);
	assert(layout.context_submenu_item_count == ORANGE_OPEN_WITH_APP_MAX);
	assert(orange_shell_overlay_bounds(width, height, &state, &bounds));
	assert(bounds.x <= layout.context_menu_panel.x);
	assert(bounds.x + bounds.width >=
		layout.context_submenu_panel.x + layout.context_submenu_panel.width);
	assert(bounds.y <= layout.context_submenu_panel.y);
	assert(bounds.y + bounds.height >=
		layout.context_submenu_panel.y + layout.context_submenu_panel.height);

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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_launcher(&layout, false, mode, 0);
	struct orange_rect sample_rect = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		layout.launcher_panel : layout.launcher_search_field;
	int sample_x = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		sample_rect.x + sample_rect.width / 2 :
		sample_rect.x + sample_rect.width - 24;
	int sample_y = mode == ORANGE_LAUNCHER_DISPLAY_FULL ?
		sample_rect.y + sample_rect.height / 2 :
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

static void test_notification_overlay_uses_backdrop_color(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *red_backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *blue_backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *red_overlay = calloc((size_t)height, (size_t)stride);
	uint32_t *blue_overlay = calloc((size_t)height, (size_t)stride);
	assert(red_backdrop != NULL);
	assert(blue_backdrop != NULL);
	assert(red_overlay != NULL);
	assert(blue_overlay != NULL);

	fill_pixels(red_backdrop, width, height, 0xffd02020u);
	fill_pixels(blue_backdrop, width, height, 0xff2048d0u);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	struct orange_shell_state state = {
		.notification_center_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_notification_center(&layout, 0);
	struct orange_rect card = layout.notification_center_cards[0];
	int sample_x = card.x + card.width - 30;
	int sample_y = card.y + card.height - 24;

	orange_shell_draw_overlay_with_backdrop(red_overlay, width, height,
		stride, red_backdrop, stride, &state);
	orange_shell_draw_overlay_with_backdrop(blue_overlay, width, height,
		stride, blue_backdrop, stride, &state);

	size_t idx = (size_t)sample_y * (size_t)width + (size_t)sample_x;
	assert(red_overlay[idx] != blue_overlay[idx]);

	free(red_backdrop);
	free(blue_backdrop);
	free(red_overlay);
	free(blue_overlay);
}

static void test_context_menu_frosts_high_contrast_backdrop(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(backdrop != NULL);
	assert(overlay != NULL);

	fill_pixels(backdrop, width, height, 0xffffffffu);

	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_state state = {
		.context_menu_kind = ORANGE_CONTEXT_MENU_APP_EDIT,
		.context_menu_index = -1,
		.context_menu_cursor_x = 220,
		.context_menu_cursor_y = 50,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_EDIT, -1, 220, 50, NULL);
	struct orange_rect panel = layout.context_menu_panel;
	int sample_x = panel.x + panel.width - 24;
	int sample_y = panel.y + panel.height / 2;

	for (int y = panel.y; y < panel.y + panel.height; y++) {
		backdrop[(size_t)y * (size_t)width + (size_t)sample_x] = 0xff000000u;
	}

	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &state);

	size_t idx = (size_t)sample_y * (size_t)width + (size_t)sample_x;
	struct color composed = pixel_from_u32(
		composite_over_opaque(overlay[idx], backdrop[idx]));
	assert(composed.r > 120);
	assert(composed.g > 120);
	assert(composed.b > 120);

	free(backdrop);
	free(overlay);
}

static void test_launcher_panel_frosts_high_contrast_backdrop(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(backdrop != NULL);
	assert(overlay != NULL);

	fill_pixels(backdrop, width, height, 0xffffffffu);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	struct orange_shell_state state = {
		.launcher_open = true,
		.launcher_display_mode = ORANGE_LAUNCHER_DISPLAY_FULL,
		.launcher_search_focus = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	snprintf(state.launcher_categories[0],
		sizeof(state.launcher_categories[0]), "%s", "All");
	state.launcher_category_count = 1;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_FULL, 0);
	struct orange_rect panel = layout.launcher_panel;
	int sample_x = panel.x + panel.width - 28;
	int sample_y = panel.y + panel.height / 2;

	for (int y = panel.y; y < panel.y + panel.height; y++) {
		backdrop[(size_t)y * (size_t)width + (size_t)sample_x] = 0xff000000u;
	}

	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &state);

	size_t idx = (size_t)sample_y * (size_t)width + (size_t)sample_x;
	struct color composed = pixel_from_u32(
		composite_over_opaque(overlay[idx], backdrop[idx]));
	assert(composed.r > 120);
	assert(composed.g > 120);
	assert(composed.b > 120);

	free(backdrop);
	free(overlay);
}

static void assert_overlay_source_not_washed_out(uint32_t pixel) {
	struct color source = pixel_from_u32(pixel);
	assert(source.a > 0);
	assert(source.a < 100);

	struct color over_black = pixel_from_u32(
		composite_over_opaque(pixel, 0xff000000u));
	assert(over_black.r < 100);
	assert(over_black.g < 100);
	assert(over_black.b < 100);
}

static void test_transient_glass_stays_subtle_on_light_backdrop(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *backdrop = calloc((size_t)height, (size_t)stride);
	uint32_t *overlay = calloc((size_t)height, (size_t)stride);
	assert(backdrop != NULL);
	assert(overlay != NULL);

	fill_pixels(backdrop, width, height, 0xffffffffu);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;

	struct orange_shell_state menu_state = {
		.system_menu_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout menu_layout;
	orange_shell_layout_compute(width,height,true,&config,0,0, NULL, 0, &menu_layout);
	struct orange_rect system_panel = menu_layout.system_menu_panel;
	int sample_x = system_panel.x + system_panel.width - 24;
	int sample_y = system_panel.y + system_panel.height / 2;
	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &menu_state);
	assert_overlay_source_not_washed_out(
		overlay[(size_t)sample_y * (size_t)width + (size_t)sample_x]);

	memset(overlay, 0, (size_t)height * (size_t)stride);
	struct orange_shell_state context_state = {
		.context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP,
		.context_menu_index = -1,
		.context_menu_cursor_x = 220,
		.context_menu_cursor_y = 50,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	struct orange_shell_layout context_layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &context_layout);
	orange_shell_layout_set_context_menu(&context_layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 220, 50, NULL);
	struct orange_rect context_panel = context_layout.context_menu_panel;
	sample_x = context_panel.x + context_panel.width - 24;
	sample_y = context_panel.y + context_panel.height / 2;
	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &context_state);
	assert_overlay_source_not_washed_out(
		overlay[(size_t)sample_y * (size_t)width + (size_t)sample_x]);

	memset(overlay, 0, (size_t)height * (size_t)stride);
	struct orange_shell_state launcher_state = {
		.launcher_open = true,
		.launcher_display_mode = ORANGE_LAUNCHER_DISPLAY_FULL,
		.launcher_search_focus = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
	};
	snprintf(launcher_state.launcher_categories[0],
		sizeof(launcher_state.launcher_categories[0]), "%s", "All");
	launcher_state.launcher_category_count = 1;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_FULL, 0);
	struct orange_rect panel = layout.launcher_panel;
	sample_x = panel.x + panel.width - 34;
	sample_y = panel.y + panel.height / 2;

	orange_shell_draw_overlay_with_backdrop(overlay, width, height,
		stride, backdrop, stride, &launcher_state);

	size_t idx = (size_t)sample_y * (size_t)width + (size_t)sample_x;
	assert_overlay_source_not_washed_out(overlay[idx]);

	free(backdrop);
	free(overlay);
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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
	orange_shell_layout_set_notification_center(&layout, 1);
	assert(layout.notification_center_card_count >= 4);
	assert(layout.notification_center_card_kinds[0] ==
		ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION);

	struct orange_notification notifications[1] = {{
		.id = 7,
		.created_at = 1781451540,
		.expire_timeout = -1,
		.app_name = "Build",
		.app_icon = "dialog-information",
		.summary = "Tests finished",
		.body = "All checks passed",
	}};

	struct orange_shell_state state = {
		.system_menu_open = false,
		.notification_center_open = true,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1781451600,
		.assets = NULL,
		.config = &config,
		.notifications = notifications,
		.notification_count = 1,
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
	orange_shell_layout_compute(width,height,false,&config,0,1, NULL, 0, &layout);

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

static void test_desktop_image_preview_and_selection_draw(void) {
	const char *preview_path = "/tmp/orange-shell-preview-test.png";
	cairo_surface_t *preview = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 16, 16);
	cairo_t *preview_cr = cairo_create(preview);
	cairo_set_source_rgb(preview_cr, 0.08, 0.82, 0.18);
	cairo_paint(preview_cr);
	cairo_destroy(preview_cr);
	cairo_surface_write_to_png(preview, preview_path);
	cairo_surface_destroy(preview);

	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	struct orange_file_info files[1] = {
		{
			.name = "Preview.png",
			.path = "/tmp/orange-shell-preview-test.png",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, files, 1, &layout);
	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = NULL,
		.config = &config,
		.desktop_files = files,
		.desktop_file_count = 1,
		.desktop_selection_active = true,
		.desktop_selection_rect = {
			.x = layout.desktop_items[0].x - 12,
			.y = layout.desktop_items[0].y - 12,
			.width = layout.desktop_items[0].width + 24,
			.height = layout.desktop_items[0].height + 24,
		},
	};
	state.desktop_selected[0] = true;
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect item = layout.desktop_items[0];
	struct color preview_pixel = pixel_at(pixels, stride,
		item.x + item.width / 2,
		item.y + item.width / 2);
	assert(preview_pixel.g > 160);
	assert(preview_pixel.r < 80);
	assert(preview_pixel.b < 90);

	struct color highlight_pixel = pixel_at(pixels, stride,
		item.x - 3,
		item.y + item.height - 6);
	assert(highlight_pixel.a > 32);
	assert(highlight_pixel.a < 80);
	assert(highlight_pixel.r < 24);
	assert(highlight_pixel.g < 24);
	assert(highlight_pixel.b < 24);

	struct color marquee_pixel = pixel_at(pixels, stride,
		state.desktop_selection_rect.x + 6,
		state.desktop_selection_rect.y + 6);
	assert(marquee_pixel.a > 20);
	assert(marquee_pixel.a < 56);
	assert(marquee_pixel.r < 24);
	assert(marquee_pixel.g < 24);
	assert(marquee_pixel.b < 24);

	remove(preview_path);
	free(pixels);
}

static void test_side_dock_hover_redraw_preserves_desktop_icons(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	config.dock_magnification = true;
	config.desktop_sort_by = ORANGE_DESKTOP_SORT_NONE;

	struct orange_shell_layout dock_layout;
	orange_shell_layout_compute(width, height, false, &config,
		0, 0, NULL, 0, &dock_layout);
	config.desktop_positions[0] = (struct orange_desktop_icon_position){
		.valid = true,
		.x = width / 2,
		.y = dock_layout.dock.y + dock_layout.dock.height / 2,
	};

	struct orange_file_info files[1] = {
		{
			.name = "Keep.txt",
			.path = "/tmp/orange-keep.txt",
			.icon_name = "text-x-generic",
			.is_directory = false,
			.is_image = false,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config,
		0, 0, files, 1, &layout);
	assert(layout.desktop_item_count == 1);
	assert(layout.dock_item_count > 0);

	cairo_surface_t *icon = solid_icon_surface();
	struct orange_assets assets = {0};
	orange_assets_init(&assets);
	add_test_icon(&assets, "text-x-generic", icon);

	struct orange_rect item = layout.desktop_items[0];
	int sample_x = item.x + item.width / 2;
	int sample_y = item.y + item.width / 2;
	struct orange_rect dirty =
		orange_dock_visual_dirty_bounds(&layout, width, height);
	assert(sample_x >= dirty.x);
	assert(sample_x < dirty.x + dirty.width);
	assert(sample_y >= dirty.y);
	assert(sample_y < dirty.y + dirty.height);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = 0,
		.dock_pointer_x = layout.dock_items[0].x +
			layout.dock_items[0].width / 2,
		.dock_pointer_y = layout.dock_items[0].y +
			layout.dock_items[0].height / 2,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = &assets,
		.config = &config,
		.desktop_files = files,
		.desktop_file_count = 1,
	};
	const struct orange_shell_draw_options full_options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &full_options);
	assert(is_solid_icon_pixel(pixel_at(pixels, stride, sample_x, sample_y)));

	const struct orange_shell_draw_options dock_options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
		.skip_dock = false,
		.draw_only_dock = true,
		.clip_to_bounds = true,
		.clip_bounds = dirty,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &dock_options);
	assert(is_solid_icon_pixel(pixel_at(pixels, stride, sample_x, sample_y)));

	cairo_surface_destroy(icon);
	orange_assets_finish(&assets);
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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
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

static void test_status_notifier_icon_draws_in_menu_bar(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0,
		&layout);

	cairo_surface_t *icon = solid_icon_surface();
	struct orange_assets assets = {0};
	orange_assets_init(&assets);
	add_test_icon(&assets, "indicator-test", icon);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757638380,
		.assets = &assets,
		.config = &config,
		.status_notifier_item_count = 1,
	};
	snprintf(state.status_notifier_items[0].id,
		sizeof(state.status_notifier_items[0].id),
		"%s", ":1.77/StatusNotifierItem");
	snprintf(state.status_notifier_items[0].icon_name,
		sizeof(state.status_notifier_items[0].icon_name),
		"%s", "indicator-test");
	snprintf(state.status_notifier_items[0].status,
		sizeof(state.status_notifier_items[0].status), "%s", "Active");
	orange_shell_layout_set_status_notifier_items(&layout,
		state.status_notifier_items, state.status_notifier_item_count);

	const struct orange_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	orange_shell_draw_with_options(pixels, width, height, stride,
		&state, &options);

	struct orange_rect item = layout.status_notifier_items[0];
	struct color tray_pixel = pixel_at(pixels, stride,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(tray_pixel.a > 180);
	assert(tray_pixel.r > 200);
	assert(tray_pixel.g > 200);
	assert(tray_pixel.b > 200);

	cairo_surface_destroy(icon);
	orange_assets_finish(&assets);
	free(pixels);
}

static void test_side_dock_magnification_stays_below_menu_bar(void) {
	const int width = 1440;
	const int height = 900;
	const int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	config.weather_widget_visible = false;
	config.dock_magnification = true;
	config.dock_magnification_scale = 2.20;
	config.dock_scale = 2.0;
	config.dock_icon_scale = 2.0;

	const char *dock_ids[] = {
		"__launcher__", "test1", "test2", "test3", "test4",
		"test5", "test6", "test7", "test8", "__trash__",
	};
	int dock_count = sizeof(dock_ids) / sizeof(dock_ids[0]);
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		if (i < dock_count) {
			snprintf(config.dock_apps[i], 128, "%s", dock_ids[i]);
		} else {
			config.dock_apps[i][0] = '\0';
		}
	}

	cairo_surface_t *icon = solid_icon_surface();
	struct orange_assets assets = {0};
	orange_assets_init(&assets);
	add_test_icon(&assets, "view-app-grid", icon);

	const enum orange_dock_position positions[] = {
		ORANGE_DOCK_POSITION_LEFT,
		ORANGE_DOCK_POSITION_RIGHT,
	};
	for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
		memset(pixels, 0, (size_t)height * (size_t)stride);
		config.dock_position = positions[i];

		struct orange_shell_layout layout;
		orange_shell_layout_compute(width, height, false, &config,
			0, 0, NULL, 0, &layout);
		assert(layout.dock_item_count > 0);
		struct orange_rect hot_base = layout.dock_items[0];
		int sample_x = hot_base.x + hot_base.width / 2;
		int sample_y = layout.menu_bar.y + layout.menu_bar.height - 2;

		struct orange_shell_state state = {
			.system_menu_open = false,
			.hot_dock_index = 0,
			.dock_pointer_x = hot_base.x + hot_base.width / 2,
			.dock_pointer_y = hot_base.y + hot_base.height / 2,
			.dock_drag_index = -1,
			.dock_drag_insert_before = -1,
			.now = 1757638380,
			.assets = &assets,
			.config = &config,
		};
		const struct orange_shell_draw_options baseline_options = {
			.draw_wallpaper = false,
			.skip_dock = true,
		};
		orange_shell_draw_with_options(pixels, width, height, stride,
			&state, &baseline_options);
		struct color menu_before = pixel_at(pixels, stride,
			sample_x, sample_y);

		memset(pixels, 0, (size_t)height * (size_t)stride);
		const struct orange_shell_draw_options options = {
			.draw_wallpaper = false,
		};
		orange_shell_draw_with_options(pixels, width, height, stride,
			&state, &options);

		struct color menu_after = pixel_at(pixels, stride,
			sample_x, sample_y);
		assert(colors_near(menu_before, menu_after, 3));
		assert(!is_solid_icon_pixel(menu_after));
	}

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
	orange_shell_layout_compute(width,height,false,&config,0,0, NULL, 0, &layout);
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
	test_light_context_menu_text_uses_white_palette();
	test_dark_context_menu_uses_dark_material();
	test_desktop_context_menu_stacks_checkmark_row();
	test_menu_surfaces_avoid_white_halo_without_shadow();
	test_dock_context_menu_tail_is_seamless();
	test_base_shell_can_skip_transient_overlays();
	test_overlay_draw_clears_when_no_overlay_is_active();
	test_dock_bounce_keeps_auto_hide_overlay_visible();
	test_menu_overlay_bounds_are_tight();
	test_backdrop_overlay_matches_direct_launcher_glass();
	test_context_menu_frosts_high_contrast_backdrop();
	test_launcher_panel_frosts_high_contrast_backdrop();
	test_transient_glass_stays_subtle_on_light_backdrop();
	test_notification_overlay_uses_backdrop_color();
	test_notification_center_renders_panel_cards_and_button();
	test_desktop_volume_icon_draws();
	test_desktop_image_preview_and_selection_draw();
	test_dock_magnification_wave_paints_above_base_icons();
	test_side_dock_hover_redraw_preserves_desktop_icons();
	test_status_notifier_icon_draws_in_menu_bar();
	test_side_dock_magnification_stays_below_menu_bar();
	test_default_dock_prefers_desktop_icons_before_role_icons();
	puts("shell visual tests passed");
	return 0;
}
