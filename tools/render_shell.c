#include "tahoe/assets.h"
#include "tahoe/config.h"
#include "tahoe/desktop_entry.h"
#include "tahoe/shell.h"

#include <cairo/cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
	fprintf(stderr,
		"usage: %s [--width N] [--height N] [--assets PATH] [--config PATH] [--desktop-dir PATH] output.png\n",
		argv0);
}

int main(int argc, char **argv) {
	int width = 2048;
	int height = 1153;
	const char *asset_root = "assets";
	const char *config_path = "tahoe.conf";
	const char *desktop_dir = "assets/desktop";
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

	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	if (pixels == NULL) {
		return 1;
	}

	struct tahoe_assets assets;
	tahoe_assets_init(&assets);
	tahoe_assets_load(&assets, asset_root);
	struct tahoe_config config;
	tahoe_config_load(&config, config_path);
	struct tahoe_desktop_entry desktop_entries[TAHOE_DESKTOP_MAX];
	size_t desktop_entry_count = 0;
	tahoe_desktop_entry_load_all(desktop_dir, desktop_entries,
		TAHOE_DESKTOP_MAX, &desktop_entry_count);

	struct tahoe_shell_state state = {
		.apple_menu_open = false,
		.hot_dock_index = -1,
		.now = 1780795380,
		.assets = &assets,
		.config = &config,
		.desktop_entries = desktop_entries,
		.desktop_entry_count = (int)desktop_entry_count,
	};
	tahoe_shell_draw(pixels, width, height, stride, &state);

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_status_t status = cairo_surface_write_to_png(surface, output);
	cairo_surface_destroy(surface);
	tahoe_assets_finish(&assets);
	free(pixels);

	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to write %s: %s\n",
			output, cairo_status_to_string(status));
		return 1;
	}

	return 0;
}
