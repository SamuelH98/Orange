#include "tahoe/assets.h"

#include <stdio.h>
#include <string.h>

static const char *dock_icon_names[] = {
	"finder",
	"launchpad",
	"safari",
	"messages",
	"mail",
	"maps",
	"photos",
	"facetime",
	"phone",
	"calendar",
	"contacts",
	"reminders",
	"notes",
	"tv",
	"music",
	"rocket",
	"app-store",
	"calculator",
	"settings",
	"desktop-preview",
	"trash",
};

static const char *desktop_icon_names[] = {
	"desktop-preview",
	"notes",
	"finder",
	"trash",
};

static cairo_surface_t *load_png(const char *path) {
	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

static cairo_surface_t *load_root_png(const char *root, const char *relative) {
	char path[4096];
	snprintf(path, sizeof(path), "%s/%s", root, relative);
	cairo_surface_t *surface = load_png(path);
	if (surface != NULL) {
		return surface;
	}
	snprintf(path, sizeof(path), "%s/apple/%s", root, relative);
	return load_png(path);
}

void tahoe_assets_init(struct tahoe_assets *assets) {
	assets->wallpaper = NULL;
	assets->wallpaper_dark = NULL;
	assets->wallpaper_beach_day = NULL;
	assets->apple_menu = NULL;
	assets->dock_icon_count = 0;
	assets->desktop_icon_count = 0;
	for (int variant = 0; variant < TAHOE_ASSET_ICON_VARIANTS; variant++) {
		for (int i = 0; i < TAHOE_ASSET_DOCK_ICON_MAX; i++) {
			assets->dock_icons[variant][i] = NULL;
			assets->desktop_icons[variant][i] = NULL;
		}
	}
	for (int i = 0; i < TAHOE_STATUS_ICON_COUNT; i++) {
		assets->status_icons[i] = NULL;
	}
}

bool tahoe_assets_load(struct tahoe_assets *assets, const char *root) {
	if (root == NULL || root[0] == '\0') {
		return false;
	}

	bool loaded = false;

	assets->wallpaper = load_root_png(root, "wallpaper.png");
	loaded = loaded || assets->wallpaper != NULL;

	assets->wallpaper_dark = load_root_png(root, "wallpaper-dark.png");
	loaded = loaded || assets->wallpaper_dark != NULL;

	assets->wallpaper_beach_day = load_root_png(root, "wallpaper-beach-day.png");
	loaded = loaded || assets->wallpaper_beach_day != NULL;

	assets->apple_menu = load_root_png(root, "tahoe-menu.png");
	if (assets->apple_menu == NULL) {
		assets->apple_menu = load_root_png(root, "apple-menu.png");
	}
	loaded = loaded || assets->apple_menu != NULL;

	int icon_count = (int)(sizeof(dock_icon_names) / sizeof(dock_icon_names[0]));
	assets->dock_icon_count = icon_count;
	for (int i = 0; i < icon_count && i < TAHOE_ASSET_DOCK_ICON_MAX; i++) {
		char relative[512];
		snprintf(relative, sizeof(relative), "icons/%s.png", dock_icon_names[i]);
		assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->dock_icons[TAHOE_ASSET_ICON_LIGHT][i] != NULL;

		snprintf(relative, sizeof(relative), "icons/%s-dark.png", dock_icon_names[i]);
		assets->dock_icons[TAHOE_ASSET_ICON_DARK][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->dock_icons[TAHOE_ASSET_ICON_DARK][i] != NULL;
	}

	int desktop_icon_count =
		(int)(sizeof(desktop_icon_names) / sizeof(desktop_icon_names[0]));
	assets->desktop_icon_count = desktop_icon_count;
	for (int i = 0; i < desktop_icon_count && i < TAHOE_ASSET_DOCK_ICON_MAX; i++) {
		char relative[512];
		snprintf(relative, sizeof(relative), "icons/%s.png", desktop_icon_names[i]);
		assets->desktop_icons[TAHOE_ASSET_ICON_LIGHT][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->desktop_icons[TAHOE_ASSET_ICON_LIGHT][i] != NULL;

		snprintf(relative, sizeof(relative), "icons/%s-dark.png", desktop_icon_names[i]);
		assets->desktop_icons[TAHOE_ASSET_ICON_DARK][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->desktop_icons[TAHOE_ASSET_ICON_DARK][i] != NULL;
	}

	assets->status_icons[TAHOE_STATUS_ICON_BATTERY] =
		load_root_png(root, "status/battery.100.png");
	loaded = loaded || assets->status_icons[TAHOE_STATUS_ICON_BATTERY] != NULL;

	assets->status_icons[TAHOE_STATUS_ICON_WIFI] =
		load_root_png(root, "status/wifi.png");
	loaded = loaded || assets->status_icons[TAHOE_STATUS_ICON_WIFI] != NULL;

	assets->status_icons[TAHOE_STATUS_ICON_SEARCH] =
		load_root_png(root, "status/magnifyingglass.png");
	loaded = loaded || assets->status_icons[TAHOE_STATUS_ICON_SEARCH] != NULL;

	assets->status_icons[TAHOE_STATUS_ICON_CONTROL_CENTER] =
		load_root_png(root, "status/control-center.png");
	loaded = loaded || assets->status_icons[TAHOE_STATUS_ICON_CONTROL_CENTER] != NULL;

	assets->status_icons[TAHOE_STATUS_ICON_WEATHER] =
		load_root_png(root, "status/weather.png");
	loaded = loaded || assets->status_icons[TAHOE_STATUS_ICON_WEATHER] != NULL;

	return loaded;
}

void tahoe_assets_finish(struct tahoe_assets *assets) {
	if (assets->wallpaper != NULL) {
		cairo_surface_destroy(assets->wallpaper);
		assets->wallpaper = NULL;
	}
	if (assets->wallpaper_dark != NULL) {
		cairo_surface_destroy(assets->wallpaper_dark);
		assets->wallpaper_dark = NULL;
	}
	if (assets->wallpaper_beach_day != NULL) {
		cairo_surface_destroy(assets->wallpaper_beach_day);
		assets->wallpaper_beach_day = NULL;
	}
	if (assets->apple_menu != NULL) {
		cairo_surface_destroy(assets->apple_menu);
		assets->apple_menu = NULL;
	}
	for (int variant = 0; variant < TAHOE_ASSET_ICON_VARIANTS; variant++) {
		for (int i = 0; i < TAHOE_ASSET_DOCK_ICON_MAX; i++) {
			if (assets->dock_icons[variant][i] != NULL) {
				cairo_surface_destroy(assets->dock_icons[variant][i]);
				assets->dock_icons[variant][i] = NULL;
			}
			if (assets->desktop_icons[variant][i] != NULL) {
				cairo_surface_destroy(assets->desktop_icons[variant][i]);
				assets->desktop_icons[variant][i] = NULL;
			}
		}
	}
	for (int i = 0; i < TAHOE_STATUS_ICON_COUNT; i++) {
		if (assets->status_icons[i] != NULL) {
			cairo_surface_destroy(assets->status_icons[i]);
			assets->status_icons[i] = NULL;
		}
	}
	assets->dock_icon_count = 0;
	assets->desktop_icon_count = 0;
}

cairo_surface_t *tahoe_assets_desktop_icon(
		const struct tahoe_assets *assets,
		enum tahoe_asset_icon_variant variant,
		const char *name) {
	if (assets == NULL || name == NULL ||
			variant < 0 || variant >= TAHOE_ASSET_ICON_VARIANTS) {
		return NULL;
	}
	for (int i = 0; i < assets->desktop_icon_count &&
			i < (int)(sizeof(desktop_icon_names) / sizeof(desktop_icon_names[0])); i++) {
		if (strcmp(name, desktop_icon_names[i]) == 0) {
			cairo_surface_t *surface = assets->desktop_icons[variant][i];
			if (surface == NULL && variant == TAHOE_ASSET_ICON_DARK) {
				surface = assets->desktop_icons[TAHOE_ASSET_ICON_LIGHT][i];
			}
			return surface;
		}
	}
	return NULL;
}
