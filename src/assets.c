#include "orange/assets.h"

#include <stdio.h>
#include <string.h>

static const char *dock_icon_names[] = {
	"files",
	"launcher",
	"browser",
	"messages",
	"mail",
	"maps",
	"photos",
	"video",
	"phone",
	"calendar",
	"contacts",
	"reminders",
	"notes",
	"tv",
	"music",
	"software",
	"settings",
	"calculator",
	"trash",
};

static const char *desktop_icon_names[] = {
	"desktop-preview",
	"notes",
	"files",
	"trash",
};

static const char *context_menu_icon_names[] = {
	"info.circle",
	"gear",
	"bag",
	"clock",
	"xmark.octagon",
	"moon.zzz",
	"arrow.counterclockwise",
	"power",
	"lock",
	"arrow.right.to.line.alt",
	"arrow.up.doc",
	"magnifyingglass",
	"minus.circle",
	"person.crop.circle.badge.plus",
	"square.and.pencil",
	"rectangle.3.offgrid",
	"square.grid.2x2",
	"rectangle.grid.3x2",
	"folder.badge.plus",
	"rectangle.stack",
	"arrow.up.arrow.down",
	"wand.and.stars",
	"sidebar.right",
	"photo",
	"doc.on.doc",
	"plus.square.on.square",
	"eye",
	"square.and.arrow.up",
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
	snprintf(path, sizeof(path), "%s/private/%s", root, relative);
	return load_png(path);
}

void orange_assets_init(struct orange_assets *assets) {
	assets->wallpaper = NULL;
	assets->wallpaper_dark = NULL;
	assets->wallpaper_beach_day = NULL;
	assets->system_menu = NULL;
	assets->dock_icon_count = 0;
	assets->desktop_icon_count = 0;
	for (int variant = 0; variant < ORANGE_ASSET_ICON_VARIANTS; variant++) {
		for (int i = 0; i < ORANGE_ASSET_DOCK_ICON_MAX; i++) {
			assets->dock_icons[variant][i] = NULL;
			assets->desktop_icons[variant][i] = NULL;
		}
	}
	for (int i = 0; i < ORANGE_STATUS_ICON_COUNT; i++) {
		assets->status_icons[i] = NULL;
	}
	assets->context_menu_icon_count = 0;
	for (int i = 0; i < ORANGE_ASSET_CONTEXT_MENU_ICON_MAX; i++) {
		assets->context_menu_icons[i] = NULL;
	}
}

bool orange_assets_load(struct orange_assets *assets, const char *root) {
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

	assets->system_menu = load_root_png(root, "orange-menu.png");
	if (assets->system_menu == NULL) {
		assets->system_menu = load_root_png(root, "system-menu.png");
	}
	loaded = loaded || assets->system_menu != NULL;

	int icon_count = (int)(sizeof(dock_icon_names) / sizeof(dock_icon_names[0]));
	assets->dock_icon_count = icon_count;
	for (int i = 0; i < icon_count && i < ORANGE_ASSET_DOCK_ICON_MAX; i++) {
		char relative[512];
		snprintf(relative, sizeof(relative), "icons/%s.png", dock_icon_names[i]);
		assets->dock_icons[ORANGE_ASSET_ICON_LIGHT][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->dock_icons[ORANGE_ASSET_ICON_LIGHT][i] != NULL;

		snprintf(relative, sizeof(relative), "icons/%s-dark.png", dock_icon_names[i]);
		assets->dock_icons[ORANGE_ASSET_ICON_DARK][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->dock_icons[ORANGE_ASSET_ICON_DARK][i] != NULL;
	}

	int desktop_icon_count =
		(int)(sizeof(desktop_icon_names) / sizeof(desktop_icon_names[0]));
	assets->desktop_icon_count = desktop_icon_count;
	for (int i = 0; i < desktop_icon_count && i < ORANGE_ASSET_DOCK_ICON_MAX; i++) {
		char relative[512];
		snprintf(relative, sizeof(relative), "icons/%s.png", desktop_icon_names[i]);
		assets->desktop_icons[ORANGE_ASSET_ICON_LIGHT][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->desktop_icons[ORANGE_ASSET_ICON_LIGHT][i] != NULL;

		snprintf(relative, sizeof(relative), "icons/%s-dark.png", desktop_icon_names[i]);
		assets->desktop_icons[ORANGE_ASSET_ICON_DARK][i] =
			load_root_png(root, relative);
		loaded = loaded || assets->desktop_icons[ORANGE_ASSET_ICON_DARK][i] != NULL;
	}

	assets->status_icons[ORANGE_STATUS_ICON_BATTERY] =
		load_root_png(root, "status/battery.100.png");
	loaded = loaded || assets->status_icons[ORANGE_STATUS_ICON_BATTERY] != NULL;

	assets->status_icons[ORANGE_STATUS_ICON_WIFI] =
		load_root_png(root, "status/wifi.png");
	loaded = loaded || assets->status_icons[ORANGE_STATUS_ICON_WIFI] != NULL;

	assets->status_icons[ORANGE_STATUS_ICON_SEARCH] =
		load_root_png(root, "status/magnifyingglass.png");
	loaded = loaded || assets->status_icons[ORANGE_STATUS_ICON_SEARCH] != NULL;

	assets->status_icons[ORANGE_STATUS_ICON_CONTROL_CENTER] =
		load_root_png(root, "status/control-center.png");
	loaded = loaded || assets->status_icons[ORANGE_STATUS_ICON_CONTROL_CENTER] != NULL;

	assets->status_icons[ORANGE_STATUS_ICON_WEATHER] =
		load_root_png(root, "status/weather.png");
	loaded = loaded || assets->status_icons[ORANGE_STATUS_ICON_WEATHER] != NULL;

	int ctx_count = (int)(sizeof(context_menu_icon_names) /
		sizeof(context_menu_icon_names[0]));
	assets->context_menu_icon_count = ctx_count;
	for (int i = 0; i < ctx_count && i < ORANGE_ASSET_CONTEXT_MENU_ICON_MAX; i++) {
		char relative[512];
		snprintf(relative, sizeof(relative), "context-menu/%s.png",
			context_menu_icon_names[i]);
		assets->context_menu_icons[i] = load_root_png(root, relative);
		loaded = loaded || assets->context_menu_icons[i] != NULL;
	}

	return loaded;
}

void orange_assets_finish(struct orange_assets *assets) {
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
	if (assets->system_menu != NULL) {
		cairo_surface_destroy(assets->system_menu);
		assets->system_menu = NULL;
	}
	for (int variant = 0; variant < ORANGE_ASSET_ICON_VARIANTS; variant++) {
		for (int i = 0; i < ORANGE_ASSET_DOCK_ICON_MAX; i++) {
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
	for (int i = 0; i < ORANGE_STATUS_ICON_COUNT; i++) {
		if (assets->status_icons[i] != NULL) {
			cairo_surface_destroy(assets->status_icons[i]);
			assets->status_icons[i] = NULL;
		}
	}
	for (int i = 0; i < ORANGE_ASSET_CONTEXT_MENU_ICON_MAX; i++) {
		if (assets->context_menu_icons[i] != NULL) {
			cairo_surface_destroy(assets->context_menu_icons[i]);
			assets->context_menu_icons[i] = NULL;
		}
	}
	assets->dock_icon_count = 0;
	assets->desktop_icon_count = 0;
	assets->context_menu_icon_count = 0;
}

cairo_surface_t *orange_assets_desktop_icon(
		const struct orange_assets *assets,
		enum orange_asset_icon_variant variant,
		const char *name) {
	if (assets == NULL || name == NULL ||
			variant < 0 || variant >= ORANGE_ASSET_ICON_VARIANTS) {
		return NULL;
	}
	for (int i = 0; i < assets->desktop_icon_count &&
			i < (int)(sizeof(desktop_icon_names) / sizeof(desktop_icon_names[0])); i++) {
		if (strcmp(name, desktop_icon_names[i]) == 0) {
			cairo_surface_t *surface = assets->desktop_icons[variant][i];
			if (surface == NULL && variant == ORANGE_ASSET_ICON_DARK) {
				surface = assets->desktop_icons[ORANGE_ASSET_ICON_LIGHT][i];
			}
			return surface;
		}
	}
	return NULL;
}

cairo_surface_t *orange_assets_context_menu_icon(
		const struct orange_assets *assets,
		const char *name) {
	if (assets == NULL || name == NULL) {
		return NULL;
	}
	for (int i = 0; i < assets->context_menu_icon_count &&
			i < (int)(sizeof(context_menu_icon_names) /
				sizeof(context_menu_icon_names[0])); i++) {
		if (strcmp(name, context_menu_icon_names[i]) == 0) {
			return assets->context_menu_icons[i];
		}
	}
	return NULL;
}
