#include "tahoe/assets.h"

#include <stdio.h>
#include <string.h>

static cairo_surface_t *load_png(const char *path) {
	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

void tahoe_assets_init(struct tahoe_assets *assets) {
	assets->wallpaper = NULL;
	assets->apple_menu = NULL;
}

bool tahoe_assets_load(struct tahoe_assets *assets, const char *root) {
	if (root == NULL || root[0] == '\0') {
		return false;
	}

	char path[4096];
	bool loaded = false;

	snprintf(path, sizeof(path), "%s/wallpaper.png", root);
	assets->wallpaper = load_png(path);
	loaded = loaded || assets->wallpaper != NULL;

	snprintf(path, sizeof(path), "%s/apple-menu.png", root);
	assets->apple_menu = load_png(path);
	loaded = loaded || assets->apple_menu != NULL;

	return loaded;
}

void tahoe_assets_finish(struct tahoe_assets *assets) {
	if (assets->wallpaper != NULL) {
		cairo_surface_destroy(assets->wallpaper);
		assets->wallpaper = NULL;
	}
	if (assets->apple_menu != NULL) {
		cairo_surface_destroy(assets->apple_menu);
		assets->apple_menu = NULL;
	}
}
