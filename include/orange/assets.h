#ifndef ORANGE_ASSETS_H
#define ORANGE_ASSETS_H

#include <cairo/cairo.h>
#include <stdbool.h>

#define ORANGE_ASSET_DOCK_ICON_MAX 32
#define ORANGE_ASSET_CONTEXT_MENU_ICON_MAX 32

enum orange_asset_icon_variant {
	ORANGE_ASSET_ICON_LIGHT,
	ORANGE_ASSET_ICON_DARK,
	ORANGE_ASSET_ICON_VARIANTS,
};

enum orange_status_icon {
	ORANGE_STATUS_ICON_BATTERY,
	ORANGE_STATUS_ICON_WIFI,
	ORANGE_STATUS_ICON_SEARCH,
	ORANGE_STATUS_ICON_CONTROL_CENTER,
	ORANGE_STATUS_ICON_WEATHER,
	ORANGE_STATUS_ICON_COUNT,
};

struct orange_assets {
	cairo_surface_t *wallpaper;
	cairo_surface_t *wallpaper_dark;
	cairo_surface_t *wallpaper_beach_day;
	cairo_surface_t *system_menu;
	cairo_surface_t *dock_icons[ORANGE_ASSET_ICON_VARIANTS][ORANGE_ASSET_DOCK_ICON_MAX];
	cairo_surface_t *desktop_icons[ORANGE_ASSET_ICON_VARIANTS][ORANGE_ASSET_DOCK_ICON_MAX];
	cairo_surface_t *status_icons[ORANGE_STATUS_ICON_COUNT];
	cairo_surface_t *context_menu_icons[ORANGE_ASSET_CONTEXT_MENU_ICON_MAX];
	int dock_icon_count;
	int desktop_icon_count;
	int context_menu_icon_count;
};

void orange_assets_init(struct orange_assets *assets);
bool orange_assets_load(struct orange_assets *assets, const char *root);
void orange_assets_finish(struct orange_assets *assets);
cairo_surface_t *orange_assets_desktop_icon(
	const struct orange_assets *assets,
	enum orange_asset_icon_variant variant,
	const char *name);
cairo_surface_t *orange_assets_context_menu_icon(
	const struct orange_assets *assets,
	const char *name);

#endif
