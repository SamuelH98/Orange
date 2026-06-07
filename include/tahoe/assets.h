#ifndef TAHOE_ASSETS_H
#define TAHOE_ASSETS_H

#include <cairo/cairo.h>
#include <stdbool.h>

#define TAHOE_ASSET_DOCK_ICON_MAX 32

enum tahoe_asset_icon_variant {
	TAHOE_ASSET_ICON_LIGHT,
	TAHOE_ASSET_ICON_DARK,
	TAHOE_ASSET_ICON_VARIANTS,
};

enum tahoe_status_icon {
	TAHOE_STATUS_ICON_BATTERY,
	TAHOE_STATUS_ICON_WIFI,
	TAHOE_STATUS_ICON_SEARCH,
	TAHOE_STATUS_ICON_CONTROL_CENTER,
	TAHOE_STATUS_ICON_WEATHER,
	TAHOE_STATUS_ICON_COUNT,
};

struct tahoe_assets {
	cairo_surface_t *wallpaper;
	cairo_surface_t *wallpaper_dark;
	cairo_surface_t *wallpaper_beach_day;
	cairo_surface_t *apple_menu;
	cairo_surface_t *dock_icons[TAHOE_ASSET_ICON_VARIANTS][TAHOE_ASSET_DOCK_ICON_MAX];
	cairo_surface_t *desktop_icons[TAHOE_ASSET_ICON_VARIANTS][TAHOE_ASSET_DOCK_ICON_MAX];
	cairo_surface_t *status_icons[TAHOE_STATUS_ICON_COUNT];
	int dock_icon_count;
	int desktop_icon_count;
};

void tahoe_assets_init(struct tahoe_assets *assets);
bool tahoe_assets_load(struct tahoe_assets *assets, const char *root);
void tahoe_assets_finish(struct tahoe_assets *assets);
cairo_surface_t *tahoe_assets_desktop_icon(
	const struct tahoe_assets *assets,
	enum tahoe_asset_icon_variant variant,
	const char *name);

#endif
