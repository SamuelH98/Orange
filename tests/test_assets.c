#include "orange/assets.h"
#include "orange/menubar.h"

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
	(void)path;
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
	if (abs(r - expected_r) > 2 ||
			abs(g - expected_g) > 2 ||
			abs(b - expected_b) > 2) {
		fprintf(stderr, "pixel rgb %d,%d,%d expected %d,%d,%d\n",
			r, g, b, expected_r, expected_g, expected_b);
		assert(false);
	}
}

static void write_index(const char *path, const char *inherits) {
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	fprintf(file,
		"[Icon Theme]\n"
		"Name=Test\n"
		"Comment=Test\n"
		"Directories=128x128/apps,places/scalable\n"
		"%s%s"
		"\n"
		"[128x128/apps]\n"
		"Size=128\n"
		"Type=Fixed\n"
		"Context=Applications\n"
		"\n"
		"[places/scalable]\n"
		"Size=64\n"
		"Type=Scalable\n"
		"MinSize=16\n"
		"MaxSize=512\n"
		"Context=Places\n",
		inherits != NULL ? "Inherits=" : "",
		inherits != NULL ? inherits : "");
	fclose(file);
}

static void write_long_hicolor_index(const char *path) {
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	fprintf(file,
		"[Icon Theme]\n"
		"Name=Hicolor\n"
		"Comment=Fallback\n"
		"Directories=");
	for (int i = 0; i < 300; i++) {
		fprintf(file, "dummy%03d/apps,", i);
	}
	fprintf(file,
		"128x128/apps,scalable/apps\n"
		"\n"
		"[128x128/apps]\n"
		"Size=128\n"
		"Type=Fixed\n"
		"Context=Applications\n"
		"\n"
		"[scalable/apps]\n"
		"Size=128\n"
		"Type=Scalable\n"
		"MinSize=1\n"
		"MaxSize=512\n"
		"Context=Applications\n");
	fclose(file);
}

int main(void) {
	mkdir_p("/tmp/orange-assets-test/data/icons/TestTheme/128x128/apps");
	mkdir_p("/tmp/orange-assets-test/data/icons/TestTheme/places/scalable");
	mkdir_p("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps");
	mkdir_p("/tmp/orange-assets-test/data/icons/hicolor/scalable/apps");
	mkdir_p("/tmp/orange-assets-test/extra/icons/TestTheme/128x128/apps");
	assert(setenv("XDG_DATA_HOME", "/tmp/orange-assets-test/data", 1) == 0);
	assert(setenv("XDG_DATA_DIRS", "/tmp/orange-assets-test/extra", 1) == 0);
	write_index("/tmp/orange-assets-test/data/icons/TestTheme/index.theme", NULL);
	write_long_hicolor_index("/tmp/orange-assets-test/data/icons/hicolor/index.theme");
	write_png("/tmp/orange-assets-test/data/icons/TestTheme/128x128/apps/test-app.png",
		1.0, 0.2, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/TestTheme/places/scalable/folder.png",
		0.2, 0.6, 0.9);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/fallback-app.png",
		0.1, 0.2, 1.0);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/start-here.png",
		0.2, 0.8, 0.2);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/view-app-grid.png",
		0.8, 0.1, 0.2);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/accessories-text-editor.png",
		0.9, 0.1, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/org.gnome.TextEditor.png",
		0.2, 0.2, 0.2);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/org.gnome.Notes.png",
		1.0, 0.8, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/notes.png",
		0.1, 0.4, 0.9);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/org.gnome.Showtime.png",
		0.1, 0.4, 0.9);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/video-display.png",
		0.9, 0.1, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/video-player.png",
		0.1, 0.8, 0.3);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/applications-graphics.png",
		0.9, 0.1, 0.1);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/org.gnome.Loupe.png",
		0.1, 0.7, 0.8);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/128x128/apps/image-viewer.png",
		0.7, 0.2, 0.9);
	write_png("/tmp/orange-assets-test/data/icons/hicolor/scalable/apps/org.gnome.Maps.png",
		0.1, 0.65, 0.3);
	write_png("/tmp/orange-assets-test/extra/icons/TestTheme/128x128/apps/split-theme-app.png",
		0.95, 0.55, 0.1);
	write_png("/tmp/orange-assets-test/absolute-icon.png", 0.7, 0.1, 0.35);
	write_png("/tmp/orange-assets-test/gnome-light.png", 1.0, 0.0, 0.0);
	write_png("/tmp/orange-assets-test/gnome-dark.png", 0.0, 0.0, 1.0);

	struct orange_assets assets;
	orange_assets_init(&assets);
	orange_assets_load(&assets, "TestTheme");
	assert(assets.icon_count == 0);

	assert(orange_assets_set_wallpaper_settings(&assets,
		"/tmp/orange-assets-test/gnome-light.png",
		"/tmp/orange-assets-test/gnome-dark.png",
		"zoom",
		"#000000",
		"#ffffff",
		"solid",
		100));
	cairo_surface_t *wallpaper = orange_assets_wallpaper(
		&assets, false, 32, 32);
	assert_pixel_rgb(wallpaper, 255, 0, 0);
	cairo_surface_t *dark_wallpaper = orange_assets_wallpaper(
		&assets, true, 32, 32);
	assert_pixel_rgb(dark_wallpaper, 0, 0, 255);
	assert(orange_assets_set_wallpaper_settings(&assets,
		"",
		"",
		"none",
		"#112233",
		"#445566",
		"solid",
		100));
	cairo_surface_t *solid_wallpaper = orange_assets_wallpaper(
		&assets, false, 32, 32);
	assert_pixel_rgb(solid_wallpaper, 17, 34, 51);

	cairo_surface_t *icon = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "test-app");
	assert(icon != NULL);
	assert(assets.icon_count == 1);
	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "test-app") == icon);

	orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "fallback-app");

	cairo_surface_t *folder = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "folder");
	assert(folder != NULL);
	assert_pixel_rgb(folder, 51, 153, 230);

	cairo_surface_t *menu = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "orange-menu");
	assert(menu != NULL);
	assert(menu != icon);
	assert_pixel_rgb(menu, 51, 204, 51);

	cairo_surface_t *launcher = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "view-app-grid");
	assert(launcher != NULL);
	assert_pixel_rgb(launcher, 204, 26, 51);

	cairo_surface_t *notes = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "org.gnome.Notes");
	assert(notes != NULL);
	assert_pixel_rgb(notes, 255, 204, 26);

	cairo_surface_t *text_editor = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "org.gnome.TextEditor");
	assert(text_editor != NULL);
	assert_pixel_rgb(text_editor, 51, 51, 51);

	cairo_surface_t *showtime = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "org.gnome.Showtime");
	assert(showtime != NULL);
	assert_pixel_rgb(showtime, 26, 102, 230);

	cairo_surface_t *video = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "video-display");
	assert(video != NULL);
	assert_pixel_rgb(video, 230, 26, 26);

	cairo_surface_t *loupe = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "org.gnome.Loupe");
	assert(loupe != NULL);
	assert_pixel_rgb(loupe, 26, 179, 204);

	cairo_surface_t *image_viewer = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "image-x-generic");
	assert(image_viewer != NULL);
	assert_pixel_rgb(image_viewer, 179, 51, 230);

	cairo_surface_t *maps = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "org.gnome.Maps");
	assert(maps != NULL);
	assert_pixel_rgb(maps, 26, 166, 77);

	cairo_surface_t *split = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "split-theme-app");
	assert(split != NULL);
	assert_pixel_rgb(split, 242, 140, 26);

	cairo_surface_t *absolute = orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "/tmp/orange-assets-test/absolute-icon.png");
	assert(absolute != NULL);
	assert_pixel_rgb(absolute, 179, 26, 89);

	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "missing-app") == NULL);
	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "missing-app") == NULL);

	int icon_count_before_warm = assets.icon_count;
	orange_menubar_warm_assets(&assets);
	assert(assets.icon_count >= icon_count_before_warm);
	assert(orange_assets_icon(&assets,
		ORANGE_ASSET_ICON_LIGHT, "orange-menu") == menu);

	orange_assets_finish(&assets);
	puts("asset tests passed");
	return 0;
}
