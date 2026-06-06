#ifndef TAHOE_ASSETS_H
#define TAHOE_ASSETS_H

#include <cairo/cairo.h>
#include <stdbool.h>

struct tahoe_assets {
	cairo_surface_t *wallpaper;
	cairo_surface_t *apple_menu;
};

void tahoe_assets_init(struct tahoe_assets *assets);
bool tahoe_assets_load(struct tahoe_assets *assets, const char *root);
void tahoe_assets_finish(struct tahoe_assets *assets);

#endif
