#ifndef ORANGE_ASSETS_H
#define ORANGE_ASSETS_H

#include <cairo/cairo.h>
#include <stdbool.h>

#define ORANGE_ASSET_ICON_MAX 1024
#define ORANGE_ASSET_ICON_NAME_MAX 128
#define ORANGE_THEME_NAME_MAX 128

enum orange_asset_icon_variant {
	ORANGE_ASSET_ICON_LIGHT,
	ORANGE_ASSET_ICON_DARK,
	ORANGE_ASSET_ICON_VARIANTS,
};

struct orange_named_icon {
	char name[ORANGE_ASSET_ICON_NAME_MAX];
	cairo_surface_t *surface[ORANGE_ASSET_ICON_VARIANTS];
	bool resolved[ORANGE_ASSET_ICON_VARIANTS];
};

struct orange_assets {
	cairo_surface_t *wallpaper;
	cairo_surface_t *wallpaper_dark;
	cairo_surface_t *wallpaper_scaled;
	cairo_surface_t *wallpaper_dark_scaled;
	bool wallpaper_checked;
	bool wallpaper_dark_checked;
	int wallpaper_scaled_width;
	int wallpaper_scaled_height;
	int wallpaper_dark_scaled_width;
	int wallpaper_dark_scaled_height;
	struct orange_named_icon icons[ORANGE_ASSET_ICON_MAX];
	int icon_count;
	char asset_root[4096];
	char icon_theme[ORANGE_THEME_NAME_MAX];
};

void orange_assets_init(struct orange_assets *assets);
bool orange_assets_load(
	struct orange_assets *assets,
	const char *asset_root,
	const char *icon_theme);
void orange_assets_finish(struct orange_assets *assets);
cairo_surface_t *orange_assets_wallpaper(
	struct orange_assets *assets,
	bool dark,
	int width,
	int height);
cairo_surface_t *orange_assets_icon(
	struct orange_assets *assets,
	enum orange_asset_icon_variant variant,
	const char *name);

#endif
