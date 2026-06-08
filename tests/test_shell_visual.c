#include "tahoe/assets.h"
#include "tahoe/config.h"
#include "tahoe/desktop_entry.h"
#include "tahoe/shell.h"

#include <assert.h>
#include <cairo/cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VISUAL_WIDTH 2880
#define VISUAL_HEIGHT 1800
#define DOCK_RUN_MAX 32

struct color {
	int r;
	int g;
	int b;
	int a;
};

struct bbox {
	int x0;
	int y0;
	int x1;
	int y1;
	int count;
};

struct run {
	int x0;
	int x1;
	int peak;
};

static struct color pixel_from_u32(uint32_t pixel) {
	return (struct color){
		.r = (pixel >> 16) & 0xff,
		.g = (pixel >> 8) & 0xff,
		.b = pixel & 0xff,
		.a = (pixel >> 24) & 0xff,
	};
}

static struct color pixel_from_surface(
		cairo_surface_t *surface,
		int x,
		int y) {
	uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	return pixel_from_u32(pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
}

static bool is_dock_icon_pixel(struct color c) {
	int max = c.r > c.g ? c.r : c.g;
	max = max > c.b ? max : c.b;
	int min = c.r < c.g ? c.r : c.g;
	min = min < c.b ? min : c.b;
	return c.a > 180 &&
		(max - min > 55 ||
		 (c.r > 210 && c.g > 210 && c.b > 210) ||
		 (c.r < 55 && c.g < 55 && c.b < 55));
}

static struct bbox alpha_bbox(
		uint32_t *pixels,
		int stride,
		int x0,
		int y0,
		int x1,
		int y1,
		int min_alpha) {
	struct bbox box = {VISUAL_WIDTH, VISUAL_HEIGHT, 0, 0, 0};
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			struct color c = pixel_from_u32(
				pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
			if (c.a < min_alpha) {
				continue;
			}
			if (x < box.x0) {
				box.x0 = x;
			}
			if (y < box.y0) {
				box.y0 = y;
			}
			if (x + 1 > box.x1) {
				box.x1 = x + 1;
			}
			if (y + 1 > box.y1) {
				box.y1 = y + 1;
			}
			box.count++;
		}
	}
	return box;
}

static int collect_runs_from_columns(
		const int *columns,
		int width,
		int min_column_pixels,
		int min_run_width,
		struct run *runs,
		int max_runs) {
	int count = 0;
	int start = -1;
	int peak = 0;
	for (int x = 0; x < width; x++) {
		bool active = columns[x] >= min_column_pixels;
		if (active && start < 0) {
			start = x;
			peak = columns[x];
		}
		if (active && columns[x] > peak) {
			peak = columns[x];
		}
		if ((!active || x == width - 1) && start >= 0) {
			int end = active && x == width - 1 ? x + 1 : x;
			if (end - start >= min_run_width && count < max_runs) {
				runs[count++] = (struct run){start, end, peak};
			}
			start = -1;
			peak = 0;
		}
	}
	return count;
}

static int collect_render_dock_runs(
		uint32_t *pixels,
		int stride,
		struct run *runs,
		int max_runs) {
	int columns[VISUAL_WIDTH] = {0};
	for (int x = 0; x < VISUAL_WIDTH; x++) {
		for (int y = 1640; y < 1785; y++) {
			struct color c = pixel_from_u32(
				pixels[(size_t)y * (size_t)(stride / 4) + (size_t)x]);
			if (is_dock_icon_pixel(c)) {
				columns[x]++;
			}
		}
	}
	return collect_runs_from_columns(columns, VISUAL_WIDTH, 20, 70,
		runs, max_runs);
}

static int collect_reference_dock_runs(
		cairo_surface_t *reference,
		struct run *runs,
		int max_runs) {
	int columns[VISUAL_WIDTH] = {0};
	for (int x = 0; x < VISUAL_WIDTH; x++) {
		for (int y = 1640; y < 1785; y++) {
			if (is_dock_icon_pixel(pixel_from_surface(reference, x, y))) {
				columns[x]++;
			}
		}
	}
	return collect_runs_from_columns(columns, VISUAL_WIDTH, 20, 70,
		runs, max_runs);
}

static void assert_close(const char *name, int actual, int expected, int tolerance) {
	if (actual < expected - tolerance || actual > expected + tolerance) {
		fprintf(stderr, "%s: got %d, expected %d +/- %d\n",
			name, actual, expected, tolerance);
		abort();
	}
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

static void test_dock_matches_reference_metrics(void) {
	if (access("../assets/apple/apple-menu.png", R_OK) != 0) {
		puts("SKIP: assets/apple not present");
		exit(77);
	}

	cairo_surface_t *reference =
		cairo_image_surface_create_from_png("../tahoe-desktop-reference.png");
	if (cairo_surface_status(reference) != CAIRO_STATUS_SUCCESS) {
		fputs("SKIP: tahoe-desktop-reference.png not readable\n", stderr);
		cairo_surface_destroy(reference);
		exit(77);
	}
	assert(cairo_image_surface_get_width(reference) == VISUAL_WIDTH);
	assert(cairo_image_surface_get_height(reference) == VISUAL_HEIGHT);

	struct tahoe_assets assets;
	tahoe_assets_init(&assets);
	assert(tahoe_assets_load(&assets, "../assets/apple"));

	struct tahoe_config config;
	tahoe_config_set_defaults(&config);

	struct tahoe_desktop_entry desktop_entries[TAHOE_DESKTOP_MAX];
	size_t desktop_entry_count = 0;
	tahoe_desktop_entry_load_all("../assets/desktop", desktop_entries,
		TAHOE_DESKTOP_MAX, &desktop_entry_count);

	int stride = VISUAL_WIDTH * 4;
	uint32_t *pixels = calloc((size_t)VISUAL_HEIGHT, (size_t)stride);
	assert(pixels != NULL);

	struct tahoe_shell_state state = {
		.apple_menu_open = false,
		.hot_dock_index = -1,
		.now = 1757638380,
		.assets = &assets,
		.config = &config,
		.desktop_entries = desktop_entries,
		.desktop_entry_count = (int)desktop_entry_count,
	};
	const struct tahoe_shell_draw_options options = {
		.draw_wallpaper = false,
	};
	tahoe_shell_draw_with_options(pixels, VISUAL_WIDTH, VISUAL_HEIGHT,
		stride, &state, &options);

	struct bbox dock = alpha_bbox(pixels, stride, 0, 1500,
		VISUAL_WIDTH, VISUAL_HEIGHT, 20);
	assert_close("dock glass x0", dock.x0, 87, 4);
	assert_close("dock glass y0", dock.y0, 1638, 4);
	assert_close("dock glass x1", dock.x1, 2793, 4);
	assert_close("dock glass y1", dock.y1, 1795, 3);
	assert(VISUAL_HEIGHT - dock.y1 >= 5);

	struct run reference_runs[DOCK_RUN_MAX];
	struct run render_runs[DOCK_RUN_MAX];
	int reference_count =
		collect_reference_dock_runs(reference, reference_runs, DOCK_RUN_MAX);
	int render_count =
		collect_render_dock_runs(pixels, stride, render_runs, DOCK_RUN_MAX);
	assert(reference_count >= 10);
	assert(render_count >= 10);

	for (int i = 0; i < 10; i++) {
		char label[64];
		int reference_center = (reference_runs[i].x0 + reference_runs[i].x1) / 2;
		int render_center = (render_runs[i].x0 + render_runs[i].x1) / 2;
		snprintf(label, sizeof(label), "dock icon %d center", i);
		assert_close(label, render_center, reference_center, 5);

		int reference_width = reference_runs[i].x1 - reference_runs[i].x0;
		int render_width = render_runs[i].x1 - render_runs[i].x0;
		snprintf(label, sizeof(label), "dock icon %d width", i);
		assert_close(label, render_width, reference_width, 6);
	}

	for (int i = 1; i < 10; i++) {
		int reference_step =
			(reference_runs[i].x0 + reference_runs[i].x1) / 2 -
			(reference_runs[i - 1].x0 + reference_runs[i - 1].x1) / 2;
		int render_step =
			(render_runs[i].x0 + render_runs[i].x1) / 2 -
			(render_runs[i - 1].x0 + render_runs[i - 1].x1) / 2;
		assert_close("dock icon spacing", render_step, reference_step, 3);
	}

	free(pixels);
	tahoe_assets_finish(&assets);
	cairo_surface_destroy(reference);
}

int main(void) {
	test_context_menu_glass_and_scaling();
	test_dock_matches_reference_metrics();
	puts("shell visual tests passed");
	return 0;
}
