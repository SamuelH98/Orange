#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/shell.h"

#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(const char *argv0) {
	fprintf(stderr,
		"usage: %s [--width N] [--height N] [--assets PATH] [--config PATH] [--desktop-dir PATH] [--timestamp N] [--foreground-only] [--context-menu KIND] [--context-index N] [--context-x N] [--context-y N] output.png\n",
		argv0);
}

static bool parse_context_menu_kind(
		const char *value,
		enum orange_context_menu_kind *kind) {
	if (strcmp(value, "none") == 0) {
		*kind = ORANGE_CONTEXT_MENU_NONE;
	} else if (strcmp(value, "dock") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK;
	} else if (strcmp(value, "widget") == 0) {
		*kind = ORANGE_CONTEXT_MENU_WIDGET;
	} else if (strcmp(value, "desktop") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP;
	} else if (strcmp(value, "desktop-icon") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP_ICON;
	} else {
		return false;
	}
	return true;
}

int main(int argc, char **argv) {
	int width = 2880;
	int height = 1800;
	const char *asset_root = "assets";
	const char *config_path = "orange.conf";
	const char *desktop_dir = "assets/desktop";
	time_t now = 1757638380;
	bool foreground_only = false;
	enum orange_context_menu_kind context_kind = ORANGE_CONTEXT_MENU_NONE;
	int context_index = -1;
	int context_x = -1;
	int context_y = -1;
	const char *output = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
			width = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
			height = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
			asset_root = argv[++i];
		} else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
			config_path = argv[++i];
		} else if (strcmp(argv[i], "--desktop-dir") == 0 && i + 1 < argc) {
			desktop_dir = argv[++i];
		} else if (strcmp(argv[i], "--timestamp") == 0 && i + 1 < argc) {
			now = (time_t)strtoll(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "--foreground-only") == 0) {
			foreground_only = true;
		} else if (strcmp(argv[i], "--context-menu") == 0 && i + 1 < argc) {
			if (!parse_context_menu_kind(argv[++i], &context_kind)) {
				usage(argv[0]);
				return 2;
			}
		} else if (strcmp(argv[i], "--context-index") == 0 && i + 1 < argc) {
			context_index = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--context-x") == 0 && i + 1 < argc) {
			context_x = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--context-y") == 0 && i + 1 < argc) {
			context_y = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else if (output == NULL) {
			output = argv[i];
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	if (output == NULL || width <= 0 || height <= 0) {
		usage(argv[0]);
		return 2;
	}
	if (context_kind != ORANGE_CONTEXT_MENU_NONE) {
		if (context_x < 0) {
			context_x = width / 2;
		}
		if (context_y < 0) {
			context_y = height / 2;
		}
		if (context_index < 0 && context_kind != ORANGE_CONTEXT_MENU_DESKTOP) {
			context_index = 0;
		}
	}

	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	if (pixels == NULL) {
		return 1;
	}

	struct orange_assets assets;
	orange_assets_init(&assets);
	orange_assets_load(&assets, asset_root);
	struct orange_config config;
	orange_config_load(&config, config_path);
	struct orange_desktop_entry desktop_entries[ORANGE_DESKTOP_MAX];
	size_t desktop_entry_count = 0;
	orange_desktop_entry_load_all(desktop_dir, desktop_entries,
		ORANGE_DESKTOP_MAX, &desktop_entry_count);

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = now,
		.assets = &assets,
		.config = &config,
		.desktop_entries = desktop_entries,
		.desktop_entry_count = (int)desktop_entry_count,
		.context_menu_kind = context_kind,
		.context_menu_index = context_index,
		.context_menu_cursor_x = context_x,
		.context_menu_cursor_y = context_y,
	};
	if (foreground_only) {
		const struct orange_shell_draw_options options = {
			.draw_wallpaper = false,
		};
		orange_shell_draw_with_options(pixels, width, height, stride,
			&state, &options);
	} else {
		orange_shell_draw(pixels, width, height, stride, &state);
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_status_t status = cairo_surface_write_to_png(surface, output);
	cairo_surface_destroy(surface);
	orange_assets_finish(&assets);
	free(pixels);

	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to write %s: %s\n",
			output, cairo_status_to_string(status));
		return 1;
	}

	return 0;
}
