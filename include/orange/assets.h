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
	char *cached_path; /* strdup'd path of the resolved icon file */
};

struct orange_assets {
	cairo_surface_t *wallpaper;
	cairo_surface_t *wallpaper_dark;
	cairo_surface_t *wallpaper_scaled;
	cairo_surface_t *wallpaper_dark_scaled;
	cairo_surface_t *wallpaper_custom;
	cairo_surface_t *wallpaper_custom_dark;
	cairo_surface_t *wallpaper_custom_scaled;
	cairo_surface_t *wallpaper_custom_dark_scaled;
	bool wallpaper_checked;
	bool wallpaper_dark_checked;
	bool wallpaper_custom_checked;
	bool wallpaper_custom_dark_checked;
	int wallpaper_scaled_width;
	int wallpaper_scaled_height;
	int wallpaper_dark_scaled_width;
	int wallpaper_dark_scaled_height;
	int wallpaper_custom_scaled_width;
	int wallpaper_custom_scaled_height;
	int wallpaper_custom_dark_scaled_width;
	int wallpaper_custom_dark_scaled_height;
	struct orange_named_icon icons[ORANGE_ASSET_ICON_MAX];
	int icon_count;
	int icon_lookup[ORANGE_ASSET_ICON_MAX]; /* hash table for O(1) icon lookup */
	char asset_root[4096];
	char icon_theme[ORANGE_THEME_NAME_MAX];
	bool wallpaper_configured;
	char wallpaper_path[4096];
	char wallpaper_dark_path[4096];
	char wallpaper_options[32];
	char wallpaper_shading_type[32];
	char wallpaper_primary_color[32];
	char wallpaper_secondary_color[32];
	int wallpaper_opacity;
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
bool orange_assets_set_wallpaper_settings(
	struct orange_assets *assets,
	const char *light_path,
	const char *dark_path,
	const char *options,
	const char *primary_color,
	const char *secondary_color,
	const char *shading_type,
	int opacity);
cairo_surface_t *orange_assets_icon(
	struct orange_assets *assets,
	enum orange_asset_icon_variant variant,
	const char *name);
void orange_assets_preload_icon(
	struct orange_assets *assets,
	const char *name);

#endif
