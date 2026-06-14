#include "orange/assets.h"

#include <assert.h>
#include <cairo/cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void mkdir_p(const char *path) {
	char partial[512];
	snprintf(partial, sizeof(partial), "%s", path);
	for (char *p = partial + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(partial, 0700);
			*p = '/';
		}
	}
	mkdir(partial, 0700);
}

static void write_png(const char *path, double r, double g, double b) {
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_paint(cr);
	cairo_destroy(cr);
	assert(cairo_surface_write_to_png(surface, path) == CAIRO_STATUS_SUCCESS);
	cairo_surface_destroy(surface);
}

static void assert_pixel_rgb(cairo_surface_t *surface,
		int expected_r,
		int expected_g,
		int expected_b) {
	assert(surface != NULL);
	cairo_surface_flush(surface);
	uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(surface);
	assert(pixels != NULL);
	uint32_t pixel = pixels[0];
	int r = (pixel >> 16) & 0xff;
	int g = (pixel >> 8) & 0xff;
	int b = pixel & 0xff;
	assert(abs(r - expected_r) <= 2);
	assert(abs(g - expected_g) <= 2);
	assert(abs(b - expected_b) <= 2);
}

static void write_index(const char *path, const char *inherits) {
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	fprintf(file,
		"[Icon Theme]\n"
		"Name=Test\n"
		"Comment=Test\n"
		"Directories=128x128/apps\n"
		"%s%s"
		"\n"
		"[128x128/apps]\n"
		"Size=128\n"
		"Type=Fixed\n"
		"Context=Applications\n",
		inherits != NULL ? "Inherits=" : "",
		inherits != NULL ? inherits : "");
	fclose(file);
}

int main(void) {
	mkdir_p("/tmp/orange-assets-test/assets");
	mkdir_p("/tmp/orange-assets-test/data/icons/TestTheme/128x128/apps");
	mkdir_p("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps");
	assert(setenv("XDG_DATA_HOME", "/tmp/orange-assets-test/data", 1) == 0);
	write_index("/tmp/orange-assets-test/data/icons/TestTheme/index.theme", NULL);
	write_index("/tmp/orange-assets-test/data/icons/hicolor/index.theme", NULL);
	write_png("/tmp/orange-assets-test/data/icons/TestTheme/128x128/apps/test-app.png",
		1.0, 0.2, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/fallback-app.png",
		0.1, 0.2, 1.0);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/start-here.png",
		0.2, 0.8, 0.2);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/view-app-grid.png",
		0.8, 0.1, 0.2);

	struct orange_assets assets;
	orange_assets_init(&assets);
	orange_assets_load(&assets, "/tmp/orange-assets-test/assets", "TestTheme");
	assert(assets.icon_count == 0);

	cairo_surface_t *icon = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "test-app");
	assert(icon != NULL);
	assert(assets.icon_count == 1);
	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "test-app") == icon);

	cairo_surface_t *fallback = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "fallback-app");
	assert(fallback != NULL);
	assert(fallback != icon);

	cairo_surface_t *menu = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "orange-menu");
	assert(menu != NULL);
	assert(menu != icon);
	assert_pixel_rgb(menu, 51, 204, 51);

	cairo_surface_t *launcher = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "view-app-grid");
	assert(launcher != NULL);
	assert_pixel_rgb(launcher, 204, 26, 51);

	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "missing-app") == NULL);
	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "missing-app") == NULL);

	orange_assets_finish(&assets);
	puts("asset tests passed");
	return 0;
}
