#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/shell.h"

#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(const char *argv0) {
	fprintf(stderr,
		"usage: %s [--width N] [--height N] [--assets PATH] [--config PATH] [--timestamp N] [--active-app NAME] [--foreground-only] [--launcher] [--launcher-search] [--launcher-full] [--launcher-mode N] [--launcher-query TEXT] [--notification-center] [--context-menu KIND] [--context-index N] [--context-x N] [--context-y N] output.png\n",
		argv0);
}

static bool parse_context_menu_kind(
		const char *value,
		enum orange_context_menu_kind *kind) {
	if (strcmp(value, "none") == 0) {
		*kind = ORANGE_CONTEXT_MENU_NONE;
	} else if (strcmp(value, "app") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP;
	} else if (strcmp(value, "app-file") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_FILE;
	} else if (strcmp(value, "app-edit") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_EDIT;
	} else if (strcmp(value, "app-view") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_VIEW;
	} else if (strcmp(value, "app-go") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_GO;
	} else if (strcmp(value, "app-window") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_WINDOW;
	} else if (strcmp(value, "app-history") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_HISTORY;
	} else if (strcmp(value, "app-bookmarks") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_BOOKMARKS;
	} else if (strcmp(value, "app-help") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_HELP;
	} else if (strcmp(value, "app-tools") == 0) {
		*kind = ORANGE_CONTEXT_MENU_APP_TOOLS;
	} else if (strcmp(value, "dock") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK;
	} else if (strcmp(value, "dock-running") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK_RUNNING;
	} else if (strcmp(value, "dock-separator") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK_SEPARATOR;
	} else if (strcmp(value, "dock-launcher") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK_LAUNCHER;
	} else if (strcmp(value, "dock-trash") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DOCK_TRASH;
	} else if (strcmp(value, "widget") == 0) {
		*kind = ORANGE_CONTEXT_MENU_WIDGET;
	} else if (strcmp(value, "desktop") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP;
	} else if (strcmp(value, "desktop-icon") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP_ICON;
	} else if (strcmp(value, "desktop-file") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP_FILE;
	} else if (strcmp(value, "desktop-volume") == 0) {
		*kind = ORANGE_CONTEXT_MENU_DESKTOP_VOLUME;
	} else if (strcmp(value, "status") == 0) {
		*kind = ORANGE_CONTEXT_MENU_STATUS;
	} else if (strcmp(value, "status-wifi") == 0) {
		*kind = ORANGE_CONTEXT_MENU_STATUS_WIFI;
	} else if (strcmp(value, "status-sound") == 0) {
		*kind = ORANGE_CONTEXT_MENU_STATUS_SOUND;
	} else if (strcmp(value, "status-battery") == 0) {
		*kind = ORANGE_CONTEXT_MENU_STATUS_BATTERY;
	} else {
		return false;
	}
	return true;
}

int main(int argc, char **argv) {
	int width = 2880;
	int height = 1800;
	const char *asset_root = "assets";
	const char *config_path = "orange.conf";
	time_t now = 1757638380;
	bool foreground_only = false;
	bool notification_center = false;
	bool launcher = false;
	bool launcher_search = false;
	int launcher_mode = 0;
	const char *launcher_query = NULL;
	const char *active_app_label = NULL;
	enum orange_context_menu_kind context_kind = ORANGE_CONTEXT_MENU_NONE;
	int context_index = -1;
	int context_x = -1;
	int context_y = -1;
	const char *output = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
			width = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
			height = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
			asset_root = argv[++i];
		} else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
			config_path = argv[++i];
		} else if (strcmp(argv[i], "--timestamp") == 0 && i + 1 < argc) {
			now = (time_t)strtoll(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "--active-app") == 0 && i + 1 < argc) {
			active_app_label = argv[++i];
		} else if (strcmp(argv[i], "--foreground-only") == 0) {
			foreground_only = true;
		} else if (strcmp(argv[i], "--launcher") == 0) {
			launcher = true;
		} else if (strcmp(argv[i], "--launcher-search") == 0) {
			launcher = true;
			launcher_search = true;
		} else if (strcmp(argv[i], "--launcher-full") == 0) {
			launcher = true;
		} else if (strcmp(argv[i], "--launcher-mode") == 0 && i + 1 < argc) {
			launcher_mode = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--launcher-query") == 0 && i + 1 < argc) {
			launcher_query = argv[++i];
		} else if (strcmp(argv[i], "--notification-center") == 0) {
			notification_center = true;
		} else if (strcmp(argv[i], "--context-menu") == 0 && i + 1 < argc) {
			if (!parse_context_menu_kind(argv[++i], &context_kind)) {
				usage(argv[0]);
				return 2;
			}
		} else if (strcmp(argv[i], "--context-index") == 0 && i + 1 < argc) {
			context_index = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--context-x") == 0 && i + 1 < argc) {
			context_x = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--context-y") == 0 && i + 1 < argc) {
			context_y = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else if (output == NULL) {
			output = argv[i];
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	if (output == NULL || width <= 0 || height <= 0) {
		usage(argv[0]);
		return 2;
	}
	if (context_kind != ORANGE_CONTEXT_MENU_NONE) {
		if (context_x < 0) {
			context_x = width / 2;
		}
		if (context_y < 0) {
			context_y = height / 2;
		}
		if (context_index < 0 && context_kind != ORANGE_CONTEXT_MENU_DESKTOP) {
			context_index = 0;
		}
	}

	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)height, (size_t)stride);
	if (pixels == NULL) {
		return 1;
	}

	struct orange_assets assets;
	orange_assets_init(&assets);
	struct orange_config config;
	orange_config_load(&config, config_path);
	orange_assets_load(&assets, asset_root, config.icon_theme);
	struct orange_desktop_entry desktop_entries[ORANGE_DESKTOP_MAX];
	size_t desktop_entry_count = orange_desktop_entry_load_all_xdg(
		desktop_entries, ORANGE_DESKTOP_MAX);
	struct orange_file_info render_files[1] = {{
		.name = "Project Notes.txt",
		.path = "/tmp/Project Notes.txt",
		.icon_name = "text-x-generic",
		.is_directory = false,
	}};
	struct orange_volume_info render_volumes[1] = {{
		.label = "Orange Disk",
		.mount_path = "/media/orange/Orange Disk",
		.icon_name = "drive-removable-media",
		.is_removable = true,
		.is_internal = false,
	}};
	int render_file_count =
		context_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE ? 1 : 0;
	int render_volume_count =
		context_kind == ORANGE_CONTEXT_MENU_DESKTOP_VOLUME ? 1 : 0;

	struct orange_shell_state state = {
		.system_menu_open = false,
		.notification_center_open = notification_center,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = now,
		.assets = &assets,
		.config = &config,
		.desktop_entries = desktop_entries,
		.desktop_entry_count = (int)desktop_entry_count,
		.volumes = render_volumes,
		.volume_count = render_volume_count,
		.desktop_volume_count = render_volume_count,
		.desktop_files = render_files,
		.desktop_file_count = render_file_count,
		.launcher_open = launcher,
		.launcher_display_mode = launcher_search ?
			ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY :
			ORANGE_LAUNCHER_DISPLAY_FULL,
		.launcher_position_set = false,
		.launcher_current_mode = launcher_mode,
		.context_menu_kind = context_kind,
		.context_menu_index = context_index,
		.context_menu_cursor_x = context_x,
		.context_menu_cursor_y = context_y,
	};
	if (launcher_query != NULL && launcher_query[0] != '\0') {
		snprintf(state.launcher_query, sizeof(state.launcher_query),
			"%s", launcher_query);
	}
	if (launcher && !launcher_search) {
		state.launcher_app_count = orange_launcher_filter(
			desktop_entries, (int)desktop_entry_count,
			state.launcher_query,
			NULL,
			state.launcher_app_indices,
			ORANGE_LAUNCHER_APP_MAX);
	}
	if (launcher && !launcher_search &&
			launcher_mode == ORANGE_LAUNCHER_MODE_APPS) {
		const char *cats[] = {
			"All", "Utilities", "Productivity", "Social", "Media", "Info",
		};
		int nc = (int)(sizeof(cats) / sizeof(cats[0]));
		if (nc > ORANGE_LAUNCHER_CATEGORY_MAX) {
			nc = ORANGE_LAUNCHER_CATEGORY_MAX;
		}
		state.launcher_category_count = nc;
		state.launcher_category_active = 0;
		for (int i = 0; i < nc; i++) {
			snprintf(state.launcher_categories[i],
				sizeof(state.launcher_categories[i]),
				"%s", cats[i]);
		}
	}
	if (active_app_label != NULL && active_app_label[0] != '\0') {
		snprintf(state.active_app_label, sizeof(state.active_app_label),
			"%s", active_app_label);
	}
	if (foreground_only) {
		const struct orange_shell_draw_options options = {
			.draw_wallpaper = false,
		};
		orange_shell_draw_with_options(pixels, width, height, stride,
			&state, &options);
	} else {
		orange_shell_draw(pixels, width, height, stride, &state);
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)pixels,
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		stride);
	cairo_status_t status = cairo_surface_write_to_png(surface, output);
	cairo_surface_destroy(surface);
	orange_assets_finish(&assets);
	free(pixels);

	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to write %s: %s\n",
			output, cairo_status_to_string(status));
		return 1;
	}

	return 0;
}
