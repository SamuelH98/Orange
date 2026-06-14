#include "orange/buffer.h"
#include "orange/compositor.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/shell.h"

#include <gio/gio.h>
#include <assert.h>
#include <linux/input-event-codes.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/config.h>
#include <wlr/render/allocator.h>
#ifdef ORANGE_HAVE_VULKAN
#include <wlr/render/vulkan.h>
#endif
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

enum orange_cursor_mode {
	ORANGE_CURSOR_PASSTHROUGH,
	ORANGE_CURSOR_MOVE,
	ORANGE_CURSOR_RESIZE,
};

struct orange_server;

struct orange_output {
	struct wl_list link;
	struct orange_server *server;
	struct wlr_output *wlr_output;
	struct wlr_output_layout_output *layout_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_buffer *shell_scene_buffer;
	struct orange_buffer *shell_buffer;
	struct wl_listener frame;
	struct wl_listener present;
	struct wl_listener destroy;
	int width;
	int height;
	bool shell_dirty;
	bool commit_pending;
};

struct orange_view {
	struct wl_list link;
	struct orange_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	int x;
	int y;
	int width;
	int height;
	bool mapped;
	bool maximized;
	bool fullscreen;
};

struct orange_keyboard {
	struct wl_list link;
	struct orange_server *server;
	struct wlr_keyboard *keyboard;
	struct wl_listener key;
	struct wl_listener modifiers;
	struct wl_listener destroy;
};

struct orange_decoration {
	struct wl_listener request_mode;
	struct wl_listener destroy;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
};

struct orange_server {
	const struct orange_options *options;
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_tree *shell_tree;
	struct wlr_scene_tree *window_tree;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_output_layout *output_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct orange_assets assets;
	struct orange_config config;
	struct orange_desktop_entry desktop_entries[1024];
	size_t desktop_entry_count;

	struct wl_list outputs;
	struct wl_list views;
	struct wl_list keyboards;

	struct wl_listener new_output;
	struct wl_listener new_input;
	struct wl_listener new_xdg_surface;
	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
	struct wl_listener new_xdg_decoration;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_event_source *clock_timer;

	struct orange_view *focused_view;
	enum orange_cursor_mode cursor_mode;
	struct orange_view *grabbed_view;
	double grab_cursor_x;
	double grab_cursor_y;
	int grab_view_x;
	int grab_view_y;
	int grab_view_width;
	int grab_view_height;
	uint32_t resize_edges;

	bool system_menu_open;
	struct orange_status_state status;
	int status_poll_ticks;
	int hot_dock_index;
	int last_dock_pointer_x;
	int last_dock_pointer_y;
	bool dock_open[ORANGE_DOCK_MAX];
	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
	bool desktop_drag_active;
	bool desktop_drag_moved;
	int desktop_drag_index;
	int desktop_drag_offset_x;
	int desktop_drag_offset_y;
	int desktop_drag_start_x;
	int desktop_drag_start_y;
	bool dock_drag_active;
	bool dock_drag_moved;
	int dock_drag_index;
	int dock_drag_start_x;
	int dock_drag_insert_before;
	bool once_committed;
	uint32_t seat_caps;
};

static void server_mark_shell_dirty(struct orange_server *server);
static void server_apply_cursor_config(struct orange_server *server);
static void process_desktop_drag(struct orange_server *server);
static void start_dock_drag(struct orange_server *server,
	const struct orange_shell_layout *layout, int index);
static void process_dock_drag(struct orange_server *server);
static void finish_dock_drag(struct orange_server *server);

static void status_set_defaults(struct orange_status_state *status) {
	memset(status, 0, sizeof(*status));
	snprintf(status->network_icon, sizeof(status->network_icon),
		"%s", "network-wireless");
	snprintf(status->network_label, sizeof(status->network_label),
		"%s", "Wi-Fi");
	status->network_available = true;
	snprintf(status->sound_icon, sizeof(status->sound_icon),
		"%s", "audio-volume-high");
	snprintf(status->sound_label, sizeof(status->sound_label),
		"%s", "Sound");
	status->sound_percent = -1;
	snprintf(status->battery_icon, sizeof(status->battery_icon),
		"%s", "battery");
	snprintf(status->battery_label, sizeof(status->battery_label),
		"%s", "Battery");
	status->battery_percent = -1;
}

static GVariant *dbus_get_property(
		GBusType bus_type,
		const char *bus_name,
		const char *object_path,
		const char *interface,
		const char *property) {
	GError *error = NULL;
	GDBusConnection *connection = g_bus_get_sync(bus_type, NULL, &error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (connection == NULL) {
		return NULL;
	}
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		"org.freedesktop.DBus.Properties",
		"Get",
		g_variant_new("(ss)", interface, property),
		G_VARIANT_TYPE("(v)"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		80,
		NULL,
		&error);
	g_object_unref(connection);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return NULL;
	}
	GVariant *value = NULL;
	g_variant_get(reply, "(v)", &value);
	g_variant_unref(reply);
	return value;
}

static void update_network_status(struct orange_status_state *status) {
	GVariant *connectivity = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.NetworkManager",
		"/org/freedesktop/NetworkManager",
		"org.freedesktop.NetworkManager",
		"Connectivity");
	if (connectivity == NULL) {
		return;
	}
	uint32_t state = g_variant_get_uint32(connectivity);
	g_variant_unref(connectivity);

	status->network_available = true;
	status->network_connected = state == 4;
	if (state == 4) {
		snprintf(status->network_icon, sizeof(status->network_icon),
			"%s", "network-wireless");
		snprintf(status->network_label, sizeof(status->network_label),
			"%s", "Wi-Fi Connected");
	} else if (state == 3 || state == 2) {
		snprintf(status->network_icon, sizeof(status->network_icon),
			"%s", "network-error");
		snprintf(status->network_label, sizeof(status->network_label),
			"%s", "Wi-Fi Limited");
	} else {
		snprintf(status->network_icon, sizeof(status->network_icon),
			"%s", "network-offline");
		snprintf(status->network_label, sizeof(status->network_label),
			"%s", "Wi-Fi Off");
	}

	GVariant *wireless = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.NetworkManager",
		"/org/freedesktop/NetworkManager",
		"org.freedesktop.NetworkManager",
		"WirelessEnabled");
	if (wireless != NULL) {
		if (!g_variant_get_boolean(wireless)) {
			status->network_connected = false;
			snprintf(status->network_icon, sizeof(status->network_icon),
				"%s", "network-offline");
			snprintf(status->network_label, sizeof(status->network_label),
				"%s", "Wi-Fi Disabled");
		}
		g_variant_unref(wireless);
	}
}

static void update_battery_status(struct orange_status_state *status) {
	const char *path = "/org/freedesktop/UPower/devices/DisplayDevice";
	GVariant *present = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.UPower", path,
		"org.freedesktop.UPower.Device", "IsPresent");
	if (present != NULL) {
		status->battery_present = g_variant_get_boolean(present);
		g_variant_unref(present);
	}

	GVariant *percentage = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.UPower", path,
		"org.freedesktop.UPower.Device", "Percentage");
	if (percentage != NULL) {
		double percent = g_variant_get_double(percentage);
		if (percent < 0.0) {
			percent = 0.0;
		} else if (percent > 100.0) {
			percent = 100.0;
		}
		status->battery_percent = (int)lrint(percent);
		status->battery_present = true;
		g_variant_unref(percentage);
	}

	GVariant *state = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.UPower", path,
		"org.freedesktop.UPower.Device", "State");
	if (state != NULL) {
		uint32_t battery_state = g_variant_get_uint32(state);
		status->battery_charging = battery_state == 1 || battery_state == 4;
		g_variant_unref(state);
	}

	GVariant *icon = dbus_get_property(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.UPower", path,
		"org.freedesktop.UPower.Device", "IconName");
	if (icon != NULL) {
		const char *icon_name = g_variant_get_string(icon, NULL);
		if (icon_name != NULL && icon_name[0] != '\0') {
			snprintf(status->battery_icon, sizeof(status->battery_icon),
				"%s", icon_name);
		}
		g_variant_unref(icon);
	}

	if (status->battery_percent >= 0) {
		snprintf(status->battery_label, sizeof(status->battery_label),
			status->battery_charging ? "Battery %d%% Charging" : "Battery %d%%",
			status->battery_percent);
	} else {
		snprintf(status->battery_label, sizeof(status->battery_label),
			"%s", "Battery");
	}
}

static bool server_update_status(struct orange_server *server) {
	struct orange_status_state next;
	status_set_defaults(&next);
	update_network_status(&next);
	update_battery_status(&next);

	bool changed = memcmp(&server->status, &next, sizeof(next)) != 0;
	server->status = next;
	return changed;
}

static void server_apply_theme_env(struct orange_server *server) {
	const char *theme = server->config.appearance == ORANGE_APPEARANCE_DARK ?
		server->config.gtk_theme_dark : server->config.gtk_theme_light;
	if (theme[0] != '\0') {
		setenv("GTK_THEME", theme, true);
	} else {
		unsetenv("GTK_THEME");
	}
	if (server->config.icon_theme[0] != '\0') {
		setenv("GTK_ICON_THEME", server->config.icon_theme, true);
	} else {
		unsetenv("GTK_ICON_THEME");
	}
	setenv("ORANGE_APPEARANCE",
		orange_config_appearance_name(server->config.appearance), true);
	setenv("GTK_CSD", "1", true);
}

static void server_apply_cursor_config(struct orange_server *server) {
	if (server->cursor == NULL) {
		return;
	}
	const char *theme = server->config.cursor_theme[0] != '\0' ?
		server->config.cursor_theme : NULL;
	int size = server->config.cursor_size > 0 ? server->config.cursor_size : 28;
	char size_text[16];
	snprintf(size_text, sizeof(size_text), "%d", size);
	if (theme != NULL) {
		setenv("XCURSOR_THEME", theme, true);
	} else {
		unsetenv("XCURSOR_THEME");
	}
	setenv("XCURSOR_SIZE", size_text, true);

	if (server->xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	server->xcursor_manager = wlr_xcursor_manager_create(theme, (uint32_t)size);
	if (server->xcursor_manager == NULL) {
		wlr_log(WLR_ERROR, "failed to create xcursor manager");
		return;
	}
	wlr_xcursor_manager_load(server->xcursor_manager, 1.0);
	wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "default");
}

static void server_load_config(struct orange_server *server, bool force_dirty) {
	struct orange_config next;
	orange_config_load(&next, server->options->config_path);
	bool changed = memcmp(&server->config, &next, sizeof(next)) != 0;
	bool cursor_changed =
		strcmp(server->config.cursor_theme, next.cursor_theme) != 0 ||
		server->config.cursor_size != next.cursor_size;
	server->config = next;
	server_apply_theme_env(server);
	if (cursor_changed || force_dirty) {
		server_apply_cursor_config(server);
	}
	if (changed || force_dirty) {
		server_mark_shell_dirty(server);
	}
}

static bool shell_buffer_accepts_input(
		struct wlr_scene_buffer *buffer,
		double *sx,
		double *sy) {
	(void)buffer;
	(void)sx;
	(void)sy;
	return false;
}

static void launch_command(const char *command) {
	if (command == NULL || command[0] == '\0') {
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "fork failed while launching: %s", command);
		return;
	}
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
}

static void launch_terminal(void) {
	const char *terminal = getenv("ORANGE_TERMINAL");
	if (terminal != NULL && terminal[0] != '\0') {
		launch_command(terminal);
		return;
	}
	launch_command("foot || alacritty || kitty || weston-terminal || xterm");
}

static void launch_app_picker(void) {
	const char *picker = getenv("ORANGE_APP_PICKER");
	if (picker != NULL && picker[0] != '\0') {
		launch_command(picker);
		return;
	}
	launch_command("wofi --show drun || rofi -show drun || true");
}

static bool contains_case(const char *haystack, const char *needle) {
	return haystack != NULL && needle != NULL && strcasestr(haystack, needle) != NULL;
}

static const struct orange_desktop_entry *server_desktop_entry_for_app_id(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	for (size_t i = 0; i < server->desktop_entry_count; i++) {
		if (orange_desktop_entry_id_matches(
				server->desktop_entries[i].id, app_id)) {
			return &server->desktop_entries[i];
		}
	}
	return NULL;
}

static void launch_desktop_entry(
		const struct orange_desktop_entry *entry) {
	if (entry == NULL) {
		return;
	}
	char command[1024];
	if (orange_desktop_entry_expand_exec(entry, command, sizeof(command))) {
		launch_command(command);
	} else {
		launch_command(entry->exec);
	}
}

static void launch_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0 ||
			launcher_idx >= ORANGE_DOCK_MAX ||
			server->config.dock_apps[launcher_idx][0] == '\0') {
		return;
	}

	const char *command = orange_shell_dock_command(launcher_idx,
		server->desktop_entries, (int)server->desktop_entry_count,
		&server->config);
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_app_id(server,
			server->config.dock_apps[launcher_idx]);
	if (entry != NULL) {
		launch_desktop_entry(entry);
	} else {
		launch_command(command);
	}
}

static int dock_launcher_for_visible_index(
		struct orange_server *server,
		int visible_index) {
	if (server == NULL || visible_index < 0) {
		return -1;
	}

	int launchers[ORANGE_DOCK_MAX] = {0};
	bool used[ORANGE_DOCK_MAX] = {0};
	int visible = 0;
	int dock_count = orange_shell_dock_count(&server->config);
	for (int i = 0; i < dock_count && visible < ORANGE_DOCK_MAX; i++) {
		int idx = server->config.dock_order[i];
		if (idx < 0 || idx >= ORANGE_DOCK_MAX ||
				server->config.dock_apps[idx][0] == '\0' ||
				used[idx]) {
			continue;
		}
		launchers[visible++] = idx;
		used[idx] = true;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX && visible < dock_count; i++) {
		if (server->config.dock_apps[i][0] != '\0' && !used[i]) {
			launchers[visible++] = i;
			used[i] = true;
		}
	}
	return visible_index < visible ? launchers[visible_index] : -1;
}

static int dock_visible_index_for_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return -1;
	}
	int dock_count = orange_shell_dock_count(&server->config);
	for (int i = 0; i < dock_count; i++) {
		if (dock_launcher_for_visible_index(server, i) == launcher_idx) {
			return i;
		}
	}
	return -1;
}

static int dock_launcher_for_view(struct orange_server *server, struct orange_view *view) {
	if (view == NULL || view->xdg_surface == NULL ||
			view->xdg_surface->toplevel == NULL) {
		return -1;
	}
	const char *app_id = view->xdg_surface->toplevel->app_id;
	const char *title = view->xdg_surface->toplevel->title;
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		const char *dock_id = server->config.dock_apps[i];
		if (dock_id[0] == '\0' || dock_id[0] == '_') {
			continue;
		}
		if (orange_desktop_entry_id_matches(app_id, dock_id)) {
			return i;
		}
		const struct orange_desktop_entry *entry =
			server_desktop_entry_for_app_id(server, dock_id);
		if (entry != NULL &&
				(orange_desktop_entry_id_matches(entry->id, app_id) ||
				contains_case(app_id, entry->name) ||
				contains_case(title, entry->name) ||
				contains_case(app_id, entry->icon))) {
			return i;
		}
	}
	if (contains_case(app_id, "files") ||
			contains_case(app_id, "nautilus") || contains_case(title, "files")) {
		return server_desktop_entry_for_app_id(server,
			"org.gnome.Nautilus.desktop") != NULL ? 1 : -1;
	}
	if (contains_case(app_id, "browser") ||
			contains_case(app_id, "firefox") || contains_case(app_id, "chrom") ||
			contains_case(title, "browser")) {
		return 2;
	}
	if (contains_case(app_id, "mail") || contains_case(title, "mail")) {
		return -1;
	}
	if (contains_case(app_id, "map") || contains_case(title, "map")) {
		return -1;
	}
	if (contains_case(app_id, "photo") || contains_case(title, "photo")) {
		return -1;
	}
	if (contains_case(app_id, "calendar") || contains_case(title, "calendar")) {
		return -1;
	}
	if (contains_case(app_id, "text") || contains_case(app_id, "gedit") ||
			contains_case(app_id, "mousepad") || contains_case(title, "notes")) {
		return server_desktop_entry_for_app_id(server,
			"org.gnome.TextEditor.desktop") != NULL ? 4 : -1;
	}
	if (contains_case(app_id, "software") || contains_case(app_id, "discover") ||
			contains_case(title, "software")) {
		return server_desktop_entry_for_app_id(server,
			"org.gnome.Software.desktop") != NULL ? 6 : -1;
	}
	if (contains_case(app_id, "calculator") || contains_case(app_id, "kcalc") ||
			contains_case(title, "calculator")) {
		return server_desktop_entry_for_app_id(server,
			"org.gnome.Calculator.desktop") != NULL ? 3 : -1;
	}
	if (contains_case(app_id, "settings") || contains_case(app_id, "control-center") ||
			contains_case(title, "settings")) {
		return server_desktop_entry_for_app_id(server,
			"org.gnome.Settings.desktop") != NULL ? 5 : -1;
	}
	return -1;
}

static void server_update_dock_open(struct orange_server *server) {
	memset(server->dock_open, 0, sizeof(server->dock_open));
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		int index = dock_launcher_for_view(server, view);
		if (index >= 0 && index < ORANGE_DOCK_MAX) {
			server->dock_open[index] = true;
		}
	}
}

static struct orange_output *output_from_wlr_output(
		struct orange_server *server,
		struct wlr_output *wlr_output) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			return output;
		}
	}
	return NULL;
}

static struct orange_output *output_at_cursor(
		struct orange_server *server,
		int *local_x,
		int *local_y) {
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout,
		server->cursor->x,
		server->cursor->y);
	if (wlr_output == NULL) {
		return NULL;
	}

	double lx = server->cursor->x;
	double ly = server->cursor->y;
	wlr_output_layout_output_coords(server->output_layout, wlr_output, &lx, &ly);
	*local_x = (int)lx;
	*local_y = (int)ly;
	return output_from_wlr_output(server, wlr_output);
}

static void compute_shell_layout_for_output(
		struct orange_server *server,
		struct orange_output *output,
		struct orange_shell_layout *layout) {
	orange_shell_layout_compute(output->width, output->height,
		server->system_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		layout);
	orange_shell_layout_set_context_menu(layout,
		server->context_menu_kind,
		server->context_menu_index,
		server->context_menu_cursor_x,
		server->context_menu_cursor_y);
}

static int dock_hover_index_for_pointer(
		const struct orange_shell_layout *layout,
		const struct orange_config *config,
		int x,
		int y) {
	if (layout->dock_item_count <= 0 || config == NULL ||
			!config->dock_magnification) {
		return -1;
	}

	struct orange_rect first = layout->dock_items[0];
	struct orange_rect last = layout->dock_items[layout->dock_item_count - 1];
	double max_scale = config->dock_magnification_scale;
	if (max_scale < 1.0) {
		max_scale = 1.0;
	} else if (max_scale > 2.20) {
		max_scale = 2.20;
	}
	int icon = first.height;
	int hover_top = first.y - (int)(icon * (max_scale - 1.0) + 0.5);
	int hover_bottom = layout->dock.y + layout->dock.height;
	int hover_left = first.x - icon / 2;
	int hover_right = last.x + last.width + icon / 2;
	if (x < hover_left || x > hover_right ||
			y < hover_top || y > hover_bottom) {
		return -1;
	}

	int best = -1;
	int best_distance = INT_MAX;
	for (int i = 0; i < layout->dock_item_count; i++) {
		int center = layout->dock_items[i].x + layout->dock_items[i].width / 2;
		int distance = abs(x - center);
		if (distance < best_distance) {
			best_distance = distance;
			best = i;
		}
	}
	return best;
}

static void server_mark_shell_dirty(struct orange_server *server) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output->shell_dirty = true;
		if (!output->commit_pending && !output->wlr_output->frame_pending) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static bool output_ensure_shell_buffer(struct orange_output *output) {
	int width = 0;
	int height = 0;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	if (width <= 0 || height <= 0) {
		width = output->server->options->width;
		height = output->server->options->height;
	}

	if (output->shell_buffer != NULL &&
			output->width == width &&
			output->height == height) {
		return true;
	}

	if (output->shell_scene_buffer != NULL) {
		wlr_scene_node_destroy(&output->shell_scene_buffer->node);
		output->shell_scene_buffer = NULL;
	}
	if (output->shell_buffer != NULL) {
		wlr_buffer_drop(&output->shell_buffer->base);
		output->shell_buffer = NULL;
	}

	output->shell_buffer = orange_buffer_create(width, height);
	if (output->shell_buffer == NULL) {
		wlr_log(WLR_ERROR, "failed to allocate shell buffer");
		return false;
	}

	output->shell_scene_buffer = wlr_scene_buffer_create(
		output->server->shell_tree,
		&output->shell_buffer->base);
	if (output->shell_scene_buffer == NULL) {
		wlr_log(WLR_ERROR, "failed to create shell scene buffer");
		wlr_buffer_drop(&output->shell_buffer->base);
		output->shell_buffer = NULL;
		return false;
	}
	output->shell_scene_buffer->point_accepts_input = shell_buffer_accepts_input;
	wlr_scene_buffer_set_dest_size(output->shell_scene_buffer, width, height);
	if (output->layout_output != NULL) {
		wlr_scene_node_set_position(&output->shell_scene_buffer->node,
			output->layout_output->x,
			output->layout_output->y);
	}

	output->width = width;
	output->height = height;
	output->shell_dirty = true;
	return true;
}

static void output_redraw_shell(struct orange_output *output) {
	if (!output_ensure_shell_buffer(output)) {
		return;
	}
	if (!output->shell_dirty) {
		return;
	}

	double cursor_x = output->server->cursor->x;
	double cursor_y = output->server->cursor->y;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);

	server_update_dock_open(output->server);
	struct orange_shell_state state = {
		.system_menu_open = output->server->system_menu_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_pointer_x = (int)cursor_x,
		.dock_pointer_y = (int)cursor_y,
		.dock_drag_index = output->server->dock_drag_active ?
			output->server->dock_drag_index : -1,
		.dock_drag_insert_before = output->server->dock_drag_active ?
			output->server->dock_drag_insert_before : -1,
		.dock_drag_x = output->server->dock_drag_active ?
			(int)cursor_x : 0,
		.dock_drag_y = output->server->dock_drag_active ?
			(int)cursor_y : 0,
		.now = time(NULL),
		.assets = &output->server->assets,
		.config = &output->server->config,
		.status = output->server->status,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
	};
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	orange_shell_draw(output->shell_buffer->pixels,
		output->width,
		output->height,
		output->shell_buffer->stride,
		&state);

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, 0, 0, output->width, output->height);
	wlr_scene_buffer_set_buffer_with_damage(output->shell_scene_buffer,
		&output->shell_buffer->base,
		&damage);
	pixman_region32_fini(&damage);
	output->shell_dirty = false;
}

static void focus_view(struct orange_view *view, struct wlr_surface *surface) {
	if (view == NULL || surface == NULL || !view->mapped) {
		return;
	}

	struct orange_server *server = view->server;
	if (server->focused_view != NULL && server->focused_view != view) {
		wlr_xdg_toplevel_set_activated(
			server->focused_view->xdg_surface->toplevel,
			false);
	}

	server->focused_view = view;
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	wlr_xdg_toplevel_set_activated(view->xdg_surface->toplevel, true);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(server->seat,
			surface,
			keyboard->keycodes,
			keyboard->num_keycodes,
			&keyboard->modifiers);
	}
}

static struct wlr_surface *surface_at(
		struct orange_server *server,
		double lx,
		double ly,
		double *sx,
		double *sy,
		struct orange_view **view) {
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->window_tree->node,
		lx,
		ly,
		sx,
		sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		*view = NULL;
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (scene_surface == NULL) {
		*view = NULL;
		return NULL;
	}

	struct wlr_surface *surface = scene_surface->surface;
	struct wlr_surface *root = wlr_surface_get_root_surface(surface);
	*view = root != NULL ? root->data : NULL;
	return surface;
}

static void process_cursor_motion(struct orange_server *server, uint32_t time_msec) {
	if (server->cursor_mode == ORANGE_CURSOR_MOVE && server->grabbed_view != NULL) {
		struct orange_view *view = server->grabbed_view;
		view->x = (int)(server->cursor->x - server->grab_cursor_x);
		view->y = (int)(server->cursor->y - server->grab_cursor_y);
		wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
		return;
	}

	if (server->cursor_mode == ORANGE_CURSOR_RESIZE && server->grabbed_view != NULL) {
		struct orange_view *view = server->grabbed_view;
		double dx = server->cursor->x - server->grab_cursor_x;
		double dy = server->cursor->y - server->grab_cursor_y;
		int x = server->grab_view_x;
		int y = server->grab_view_y;
		int width = server->grab_view_width;
		int height = server->grab_view_height;

		if (server->resize_edges & WLR_EDGE_LEFT) {
			x = server->grab_view_x + (int)dx;
			width = server->grab_view_width - (int)dx;
		} else if (server->resize_edges & WLR_EDGE_RIGHT) {
			width = server->grab_view_width + (int)dx;
		}

		if (server->resize_edges & WLR_EDGE_TOP) {
			y = server->grab_view_y + (int)dy;
			height = server->grab_view_height - (int)dy;
		} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
			height = server->grab_view_height + (int)dy;
		}

		if (width < 180) {
			width = 180;
		}
		if (height < 120) {
			height = 120;
		}

		view->x = x;
		view->y = y;
		view->width = width;
		view->height = height;
		wlr_scene_node_set_position(&view->scene_tree->node, x, y);
		wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, width, height);
		return;
	}

	if (server->dock_drag_active) {
		process_dock_drag(server);
		return;
	}

	if (server->desktop_drag_active) {
		process_desktop_drag(server);
		return;
	}

	double sx = 0.0;
	double sy = 0.0;
	struct orange_view *view = NULL;
	struct wlr_surface *surface = surface_at(server,
		server->cursor->x,
		server->cursor->y,
		&sx,
		&sy,
		&view);

	if (surface != NULL) {
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);
	} else {
		wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "default");
		wlr_seat_pointer_notify_clear_focus(server->seat);
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	int new_hot = -1;
	int dock_redraw_threshold = 16;
	if (output != NULL) {
		struct orange_shell_layout layout;
		compute_shell_layout_for_output(server, output, &layout);
		struct orange_shell_hit hit =
			orange_shell_hit_test(&layout, local_x, local_y);
		if (hit.kind == ORANGE_HIT_DOCK_ITEM) {
			new_hot = hit.index;
		} else {
			new_hot = dock_hover_index_for_pointer(&layout,
				&server->config, local_x, local_y);
		}
		if (new_hot >= 0 && new_hot < layout.dock_item_count) {
			int item_w = layout.dock_items[new_hot].width;
			dock_redraw_threshold = item_w / 4;
			if (dock_redraw_threshold < 12) {
				dock_redraw_threshold = 12;
			}
			if (dock_redraw_threshold > 28) {
				dock_redraw_threshold = 28;
			}
		}
	}
	if (new_hot != server->hot_dock_index) {
		server->hot_dock_index = new_hot;
		server->last_dock_pointer_x = local_x;
		server->last_dock_pointer_y = local_y;
		server_mark_shell_dirty(server);
	} else if (new_hot >= 0 && server->config.dock_magnification) {
		int dx = abs(local_x - server->last_dock_pointer_x);
		int dy = abs(local_y - server->last_dock_pointer_y);
		if (dx >= dock_redraw_threshold || dy >= dock_redraw_threshold) {
			server->last_dock_pointer_x = local_x;
			server->last_dock_pointer_y = local_y;
			server_mark_shell_dirty(server);
		}
	} else if (new_hot < 0) {
		server->last_dock_pointer_x = INT_MIN;
		server->last_dock_pointer_y = INT_MIN;
	}
}

static void begin_interactive(
		struct orange_view *view,
		enum orange_cursor_mode mode,
		uint32_t edges) {
	struct orange_server *server = view->server;
	server->cursor_mode = mode;
	server->grabbed_view = view;
	server->grab_cursor_x = server->cursor->x;
	server->grab_cursor_y = server->cursor->y;
	if (mode == ORANGE_CURSOR_MOVE) {
		server->grab_cursor_x = server->cursor->x - view->x;
		server->grab_cursor_y = server->cursor->y - view->y;
	}
	server->grab_view_x = view->x;
	server->grab_view_y = view->y;
	server->grab_view_width = view->width;
	server->grab_view_height = view->height;
	server->resize_edges = edges;
}

static void close_focused_view(struct orange_server *server) {
	if (server->focused_view != NULL && server->focused_view->mapped) {
		wlr_xdg_toplevel_send_close(server->focused_view->xdg_surface->toplevel);
	}
}

static void cycle_focus(struct orange_server *server) {
	if (wl_list_empty(&server->views)) {
		return;
	}

	struct orange_view *first_mapped = NULL;
	struct orange_view *candidate = NULL;
	bool seen_focused = server->focused_view == NULL;

	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		if (first_mapped == NULL) {
			first_mapped = view;
		}
		if (seen_focused) {
			candidate = view;
			break;
		}
		if (view == server->focused_view) {
			seen_focused = true;
		}
	}

	if (candidate == NULL) {
		candidate = first_mapped;
	}
	if (candidate != NULL) {
		focus_view(candidate, candidate->xdg_surface->surface);
	}
}

static void apply_maximize(struct orange_view *view, bool maximized) {
	struct orange_server *server = view->server;
	view->maximized = maximized;
	wlr_xdg_toplevel_set_maximized(view->xdg_surface->toplevel, maximized);
	if (!maximized) {
		return;
	}

	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0) {
		return;
	}
	view->x = box.x + 42;
	view->y = box.y + 48;
	view->width = box.width - 84;
	view->height = box.height - 150;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
		view->width,
		view->height);
}

static void apply_fullscreen(struct orange_view *view, bool fullscreen) {
	struct orange_server *server = view->server;
	view->fullscreen = fullscreen;
	wlr_xdg_toplevel_set_fullscreen(view->xdg_surface->toplevel, fullscreen);
	if (!fullscreen) {
		return;
	}

	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0) {
		return;
	}
	view->x = box.x;
	view->y = box.y;
	view->width = box.width;
	view->height = box.height;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
		view->width,
		view->height);
}

static void handle_shell_menu_action(struct orange_server *server, int index) {
	switch (index) {
	case 0:
		launch_command("GSK_RENDERER=cairo build/orange-about || true");
		break;
	case 1:
		launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || true");
		break;
	case 2:
		launch_command("gnome-software || plasma-discover || true");
		break;
	case 4:
		close_focused_view(server);
		break;
	case 5:
		launch_command("systemctl suspend || true");
		break;
	case 6:
		launch_command("systemctl reboot || true");
		break;
	case 7:
		launch_command("systemctl poweroff || true");
		break;
	case 8:
		launch_command("xdg-screensaver lock || gnome-screensaver-command -l || true");
		break;
	case 9:
		wl_display_terminate(server->display);
		break;
	default:
		break;
	}
	server->system_menu_open = false;
	server_mark_shell_dirty(server);
}

static void clear_context_menu(struct orange_server *server) {
	if (server->context_menu_kind != ORANGE_CONTEXT_MENU_NONE) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
		server->context_menu_cursor_x = 0;
		server->context_menu_cursor_y = 0;
		server_mark_shell_dirty(server);
	}
}

static void set_widget_size(
		struct orange_server *server,
		int target,
		enum orange_widget_size size) {
	if (target == 0) {
		server->config.calendar_widget_size = size;
	} else if (target == 1) {
		server->config.weather_widget_size = size;
	} else {
		return;
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void remove_widget(struct orange_server *server, int target) {
	if (target == 0) {
		server->config.calendar_widget_visible = false;
	} else if (target == 1) {
		server->config.weather_widget_visible = false;
	} else {
		return;
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void handle_context_menu_action(struct orange_server *server, int item_index) {
	enum orange_context_menu_kind kind = server->context_menu_kind;
	int target = server->context_menu_index;
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;

	if (kind == ORANGE_CONTEXT_MENU_DOCK) {
		int launcher_idx = dock_launcher_for_visible_index(server, target);
		switch (item_index) {
		case 0:
			launch_dock_launcher(server, launcher_idx);
			break;
		case 1:
			launch_command("xdg-open \"$HOME\" || true");
			break;
		case 4:
			launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || true");
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		switch (item_index) {
		case 0:
			launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || true");
			break;
		case 1:
			set_widget_size(server, target, ORANGE_WIDGET_SIZE_SMALL);
			break;
		case 2:
			set_widget_size(server, target, ORANGE_WIDGET_SIZE_MEDIUM);
			break;
		case 3:
			set_widget_size(server, target, ORANGE_WIDGET_SIZE_LARGE);
			break;
		case 4:
			remove_widget(server, target);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_ICON) {
		switch (item_index) {
		case 0:
			if (target >= 0 && target < (int)server->desktop_entry_count) {
				launch_desktop_entry(&server->desktop_entries[target]);
			}
			break;
		case 6:
			if (target >= 0 && target < (int)server->desktop_entry_count) {
				launch_command("xdg-open \"$HOME\" || true");
			}
			break;
		case 8:
			launch_command("gio trash \"$HOME/Desktop\" || true");
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		switch (item_index) {
		case 0:
			launch_command("xdg-open \"$HOME/Desktop\" || true");
			break;
		case 6:
			launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || true");
			break;
		case 7:
			launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || true");
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		switch (item_index) {
		case 0:
			launch_command("nm-connection-editor || gnome-control-center wifi || true");
			break;
		case 1:
			launch_command("blueman-manager || gnome-control-center bluetooth || systemsettings kcm_bluetooth || true");
			break;
		case 2:
			launch_command("gnome-control-center sharing || true");
			break;
		case 3:
			launch_command("gnome-control-center notifications || systemsettings kcm_notifications || true");
			break;
		case 4:
			launch_command("pavucontrol || gnome-control-center sound || true");
			break;
		case 5:
			launch_command("gnome-control-center display || systemsettings kcm_kscreen || true");
			break;
		case 6:
			launch_command("gnome-control-center display || systemsettings kcm_kscreen || true");
			break;
		case 7:
			launch_command("gnome-control-center power || true");
			break;
		case 8:
			launch_command("gnome-control-center keyboard || systemsettings kcm_keyboard || true");
			break;
		case 9:
			launch_command("GSK_RENDERER=cairo build/orange-settings orange.conf || gnome-control-center || systemsettings || true");
			break;
		default:
			break;
		}
	}
	server_mark_shell_dirty(server);
}

static void start_desktop_drag(struct orange_server *server,
		struct orange_output *output,
		const struct orange_shell_layout *layout,
		int index,
		int local_x,
		int local_y) {
	if (index < 0 || index >= layout->desktop_item_count ||
			index >= ORANGE_DESKTOP_POSITION_MAX) {
		return;
	}
	struct orange_rect item = layout->desktop_items[index];
	(void)output;
	server->desktop_drag_active = true;
	server->desktop_drag_moved = false;
	server->desktop_drag_index = index;
	server->desktop_drag_offset_x = local_x - item.x;
	server->desktop_drag_offset_y = local_y - item.y;
	server->desktop_drag_start_x = local_x;
	server->desktop_drag_start_y = local_y;
	clear_context_menu(server);
}

static void process_desktop_drag(struct orange_server *server) {
	if (!server->desktop_drag_active) {
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		return;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	int index = server->desktop_drag_index;
	if (index < 0 || index >= layout.desktop_item_count ||
			index >= ORANGE_DESKTOP_POSITION_MAX) {
		return;
	}

	int dx = local_x - server->desktop_drag_start_x;
	int dy = local_y - server->desktop_drag_start_y;
	if (abs(dx) > 4 || abs(dy) > 4) {
		server->desktop_drag_moved = true;
	}
	if (!server->desktop_drag_moved) {
		return;
	}

	struct orange_rect item = layout.desktop_items[index];
	int next_x = local_x - server->desktop_drag_offset_x;
	int next_y = local_y - server->desktop_drag_offset_y;
	if (next_x < 0) {
		next_x = 0;
	}
	if (next_y < layout.menu_bar.height) {
		next_y = layout.menu_bar.height;
	}
	if (next_x + item.width > output->width) {
		next_x = output->width - item.width;
	}
	int max_y = output->height - item.height;
	if (layout.dock.width > 0 && layout.dock.y > layout.menu_bar.height) {
		int dock_margin = layout.menu_bar.height / 6;
		if (dock_margin < 6) {
			dock_margin = 6;
		}
		int dock_max_y = layout.dock.y - dock_margin - item.height;
		if (dock_max_y < max_y) {
			max_y = dock_max_y;
		}
	}
	if (max_y < layout.menu_bar.height) {
		max_y = layout.menu_bar.height;
	}
	if (next_y > max_y) {
		next_y = max_y;
	}
	server->config.desktop_positions[index].valid = true;
	server->config.desktop_positions[index].x = next_x;
	server->config.desktop_positions[index].y = next_y;
	server_mark_shell_dirty(server);
}

static void finish_desktop_drag(struct orange_server *server) {
	if (!server->desktop_drag_active) {
		return;
	}
	int index = server->desktop_drag_index;
	bool moved = server->desktop_drag_moved;
	server->desktop_drag_active = false;
	server->desktop_drag_moved = false;
	server->desktop_drag_index = -1;

	if (moved) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (index >= 0 && index < (int)server->desktop_entry_count) {
		launch_desktop_entry(&server->desktop_entries[index]);
	}
}

static void start_dock_drag(struct orange_server *server,
		const struct orange_shell_layout *layout, int index) {
	server->dock_drag_active = true;
	server->dock_drag_moved = false;
	server->dock_drag_index = index;
	server->dock_drag_start_x = (int)server->cursor->x;
	server->dock_drag_insert_before = -1;
	(void)layout;
}

static void process_dock_drag(struct orange_server *server) {
	if (!server->dock_drag_active) {
		return;
	}

	int dx = (int)server->cursor->x - server->dock_drag_start_x;
	if (abs(dx) > 4) {
		server->dock_drag_moved = true;
	}
	if (!server->dock_drag_moved) {
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		return;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	int dragged = server->dock_drag_index;
	int insert = layout.dock_item_count;
	for (int i = 0; i < layout.dock_item_count; i++) {
		if (i == dragged) {
			continue;
		}
		struct orange_rect r = layout.dock_items[i];
		if (local_x < r.x + r.width / 2) {
			insert = i;
			break;
		}
	}
	if (insert > dragged && insert < layout.dock_item_count) {
		insert--;
	}
	if (insert >= layout.dock_item_count) {
		insert = layout.dock_item_count - 1;
	}
	if (insert != server->dock_drag_insert_before) {
		server->dock_drag_insert_before = insert;
		server_mark_shell_dirty(server);
	}
}

static void finish_dock_drag(struct orange_server *server) {
	if (!server->dock_drag_active) {
		return;
	}
	int index = server->dock_drag_index;
	bool moved = server->dock_drag_moved;
	int insert = server->dock_drag_insert_before;
	server->dock_drag_active = false;
	server->dock_drag_moved = false;
	server->dock_drag_index = -1;
	server->dock_drag_insert_before = -1;

	int dock_count = orange_shell_dock_count(&server->config);
	if (moved && index >= 0 && index < dock_count &&
			insert >= 0 && insert < dock_count) {
		int order[ORANGE_DOCK_MAX];
		for (int i = 0; i < dock_count; i++) {
			order[i] = dock_launcher_for_visible_index(server, i);
			if (order[i] < 0) {
				order[i] = i;
			}
		}
		int launcher = order[index];
		if (insert > index) {
			for (int i = index; i < insert; i++) {
				order[i] = order[i + 1];
			}
			order[insert] = launcher;
		} else if (insert < index) {
			for (int i = index; i > insert; i--) {
				order[i] = order[i - 1];
			}
			order[insert] = launcher;
		}
		for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
			server->config.dock_order[i] = i < dock_count ? order[i] : i;
		}
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (!moved && index >= 0 && index < dock_count) {
		int launcher_idx = dock_launcher_for_visible_index(server, index);
		launch_dock_launcher(server, launcher_idx);
		server_mark_shell_dirty(server);
	}
}

static void handle_shell_click(struct orange_server *server, int x, int y) {
	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		(void)x;
		(void)y;
		return;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, local_x, local_y);

	switch (hit.kind) {
	case ORANGE_HIT_CONTEXT_MENU_ITEM:
		handle_context_menu_action(server, hit.index);
		break;
	case ORANGE_HIT_SYSTEM_MENU:
		clear_context_menu(server);
		server->system_menu_open = !server->system_menu_open;
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_SYSTEM_MENU_ITEM:
		clear_context_menu(server);
		handle_shell_menu_action(server, hit.index);
		break;
	case ORANGE_HIT_APP_MENU:
		clear_context_menu(server);
		server->system_menu_open = false;
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_STATUS_AREA:
		clear_context_menu(server);
		server->system_menu_open = false;
		server->context_menu_kind = ORANGE_CONTEXT_MENU_STATUS;
		server->context_menu_index = -1;
		server->context_menu_cursor_x = local_x;
		server->context_menu_cursor_y = local_y;
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_DOCK_ITEM:
		clear_context_menu(server);
		server->system_menu_open = false;
		start_dock_drag(server, &layout, hit.index);
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_WIDGET:
		clear_context_menu(server);
		server->system_menu_open = false;
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_DESKTOP_ITEM:
		clear_context_menu(server);
		server->system_menu_open = false;
		start_desktop_drag(server, output, &layout, hit.index, local_x, local_y);
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_DESKTOP:
	case ORANGE_HIT_NONE:
		clear_context_menu(server);
		if (server->system_menu_open) {
			server->system_menu_open = false;
			server_mark_shell_dirty(server);
		}
		break;
	}
}

static void handle_shell_right_click(struct orange_server *server, int x, int y) {
	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		(void)x;
		(void)y;
		return;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, local_x, local_y);

	server->system_menu_open = false;
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	if (hit.kind == ORANGE_HIT_DOCK_ITEM) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_STATUS_AREA) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_STATUS;
		server->context_menu_index = -1;
	} else if (hit.kind == ORANGE_HIT_APP_MENU) {
		int launcher_idx = dock_launcher_for_view(server, server->focused_view);
		int visible_idx = dock_visible_index_for_launcher(server, launcher_idx);
		if (visible_idx < 0) {
			for (int i = 0; i < orange_shell_dock_count(&server->config); i++) {
				launcher_idx = dock_launcher_for_visible_index(server, i);
				if (launcher_idx >= 0 &&
						server->config.dock_apps[launcher_idx][0] != '_' &&
						strcmp(server->config.dock_apps[launcher_idx], "__trash__") != 0) {
					visible_idx = i;
					break;
				}
			}
		}
		server->context_menu_kind = visible_idx >= 0 ?
			ORANGE_CONTEXT_MENU_DOCK : ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = visible_idx;
	} else if (hit.kind == ORANGE_HIT_WIDGET) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_WIDGET;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP_ITEM) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_ICON;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP;
		server->context_menu_index = -1;
	} else {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
	}
	server_mark_shell_dirty(server);
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor,
		&event->pointer->base,
		event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	if (event->state == WLR_BUTTON_RELEASED &&
			server->cursor_mode != ORANGE_CURSOR_PASSTHROUGH) {
		server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
		server->grabbed_view = NULL;
	}
	if (event->state == WLR_BUTTON_RELEASED &&
			event->button == BTN_LEFT &&
			server->dock_drag_active) {
		finish_dock_drag(server);
		return;
	}

	if (event->state == WLR_BUTTON_RELEASED &&
			event->button == BTN_LEFT &&
			server->desktop_drag_active) {
		finish_desktop_drag(server);
		return;
	}

	double sx = 0.0;
	double sy = 0.0;
	struct orange_view *view = NULL;
	struct wlr_surface *surface = surface_at(server,
		server->cursor->x,
		server->cursor->y,
		&sx,
		&sy,
		&view);

	if (event->state == WLR_BUTTON_PRESSED && event->button == BTN_LEFT) {
		if (surface != NULL && view != NULL) {
			clear_context_menu(server);
			focus_view(view, surface);
			wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		} else {
			wlr_seat_pointer_notify_clear_focus(server->seat);
			wlr_seat_keyboard_notify_clear_focus(server->seat);
			handle_shell_click(server, (int)server->cursor->x, (int)server->cursor->y);
			return;
		}
	} else if (event->state == WLR_BUTTON_PRESSED && event->button == BTN_RIGHT) {
		if (surface == NULL) {
			wlr_seat_pointer_notify_clear_focus(server->seat);
			handle_shell_right_click(server,
				(int)server->cursor->x,
				(int)server->cursor->y);
			return;
		}
	}

	if (surface != NULL) {
		wlr_seat_pointer_notify_button(server->seat,
			event->time_msec,
			event->button,
			event->state);
	}
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server->seat,
		event->time_msec,
		event->orientation,
		event->delta,
		event->delta_discrete,
		event->source);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_frame);
	(void)data;
	wlr_seat_pointer_notify_frame(server->seat);
}

static bool handle_keybinding(
		struct orange_server *server,
		xkb_keysym_t sym) {
	switch (sym) {
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		launch_terminal();
		return true;
	case XKB_KEY_space:
		launch_app_picker();
		return true;
	case XKB_KEY_q:
	case XKB_KEY_Q:
		close_focused_view(server);
		return true;
	case XKB_KEY_Tab:
		cycle_focus(server);
		return true;
	case XKB_KEY_m:
	case XKB_KEY_M:
		if (server->focused_view != NULL) {
			apply_maximize(server->focused_view,
				!server->focused_view->maximized);
		}
		return true;
	case XKB_KEY_f:
	case XKB_KEY_F:
		if (server->focused_view != NULL) {
			apply_fullscreen(server->focused_view,
				!server->focused_view->fullscreen);
		}
		return true;
	case XKB_KEY_Escape:
		if (server->system_menu_open) {
			server->system_menu_open = false;
			server_mark_shell_dirty(server);
			return true;
		}
		return false;
	default:
		return false;
	}
}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
	struct orange_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct orange_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;

	bool handled = false;
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms = NULL;
	int nsyms = xkb_state_key_get_syms(
		keyboard->keyboard->xkb_state,
		keycode,
		&syms);
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);
	bool logo = modifiers & WLR_MODIFIER_LOGO;

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && logo) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
			if (handled) {
				break;
			}
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(server->seat, keyboard->keyboard);
		wlr_seat_keyboard_notify_key(server->seat,
			event->time_msec,
			event->keycode,
			event->state);
	}
}

static void handle_keyboard_modifiers(struct wl_listener *listener, void *data) {
	struct orange_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	(void)data;
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->keyboard);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->keyboard->modifiers);
}

static void handle_keyboard_destroy(struct wl_listener *listener, void *data) {
	struct orange_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	(void)data;
	wl_list_remove(&keyboard->link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->destroy.link);
	free(keyboard);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD: {
		struct wlr_keyboard *wlr_keyboard =
			wlr_keyboard_from_input_device(device);
		struct xkb_context *context =
			xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		struct xkb_keymap *keymap =
			xkb_keymap_new_from_names(context, NULL,
				XKB_KEYMAP_COMPILE_NO_FLAGS);
		wlr_keyboard_set_keymap(wlr_keyboard, keymap);
		xkb_keymap_unref(keymap);
		xkb_context_unref(context);
		wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

		struct orange_keyboard *keyboard = calloc(1, sizeof(*keyboard));
		if (keyboard == NULL) {
			return;
		}
		keyboard->server = server;
		keyboard->keyboard = wlr_keyboard;
		keyboard->key.notify = handle_keyboard_key;
		wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
		keyboard->modifiers.notify = handle_keyboard_modifiers;
		wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
		keyboard->destroy.notify = handle_keyboard_destroy;
		wl_signal_add(&device->events.destroy, &keyboard->destroy);
		wl_list_insert(&server->keyboards, &keyboard->link);

		server->seat_caps |= WL_SEAT_CAPABILITY_KEYBOARD;
		wlr_seat_set_keyboard(server->seat, wlr_keyboard);
		break;
	}
	case WLR_INPUT_DEVICE_POINTER:
	case WLR_INPUT_DEVICE_TOUCH:
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		wlr_cursor_attach_input_device(server->cursor, device);
		server->seat_caps |= WL_SEAT_CAPABILITY_POINTER;
		break;
	default:
		break;
	}

	wlr_seat_set_capabilities(server->seat, server->seat_caps);
}

static void handle_request_set_cursor(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	if (server->seat->pointer_state.focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor,
			event->surface,
			event->hotspot_x,
			event->hotspot_y);
	}
}

static void handle_request_set_selection(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void handle_request_set_primary_selection(
		struct wl_listener *listener,
		void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

static void handle_view_commit(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, commit);
	(void)data;
	if (!view->mapped) {
		return;
	}
	struct wlr_box geom;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geom);
	if (geom.width > 0 && geom.height > 0 &&
			view->server->cursor_mode != ORANGE_CURSOR_RESIZE) {
		view->width = geom.width;
		view->height = geom.height;
	}
}

static void handle_view_map(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, map);
	(void)data;

	struct wlr_box geom;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geom);
	view->width = geom.width > 0 ? geom.width : 900;
	view->height = geom.height > 0 ? geom.height : 620;

	struct wlr_box layout_box;
	wlr_output_layout_get_box(view->server->output_layout, NULL, &layout_box);
	if (layout_box.width <= 0 || layout_box.height <= 0) {
		layout_box = (struct wlr_box){0, 0,
			view->server->options->width,
			view->server->options->height};
	}

	view->x = layout_box.x + (layout_box.width - view->width) / 2;
	view->y = layout_box.y + 78;
	if (view->x < layout_box.x + 24) {
		view->x = layout_box.x + 24;
	}

	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	view->mapped = true;
	focus_view(view, view->xdg_surface->surface);
	server_mark_shell_dirty(view->server);
}

static void handle_view_unmap(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, unmap);
	(void)data;
	view->mapped = false;
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	if (view->server->focused_view == view) {
		view->server->focused_view = NULL;
		wlr_seat_keyboard_notify_clear_focus(view->server->seat);
	}
	if (view->server->grabbed_view == view) {
		view->server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}
	server_mark_shell_dirty(view->server);
}

static void handle_view_destroy(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, destroy);
	(void)data;
	/* wlroots already destroyed the scene tree (via
	 * wlr_scene_xdg_surface_create's internal listener). The unmap listener
	 * cleaned up focused_view / grabbed_view state. We just unlink and free. */
	wl_list_remove(&view->link);
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->commit.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	free(view);
}

static void handle_view_request_move(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, request_move);
	(void)data;
	begin_interactive(view, ORANGE_CURSOR_MOVE, 0);
}

static void handle_view_request_resize(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, request_resize);
	struct wlr_xdg_toplevel_resize_event *event = data;
	begin_interactive(view, ORANGE_CURSOR_RESIZE, event->edges);
}

static void handle_view_request_maximize(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, request_maximize);
	(void)data;
	apply_maximize(view, view->xdg_surface->toplevel->requested.maximized);
}

static void handle_view_request_fullscreen(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, request_fullscreen);
	(void)data;
	apply_fullscreen(view, view->xdg_surface->toplevel->requested.fullscreen);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	struct orange_view *view = calloc(1, sizeof(*view));
	if (view == NULL) {
		return;
	}

	view->server = server;
	view->xdg_surface = xdg_surface;
	view->scene_tree = wlr_scene_xdg_surface_create(
		server->window_tree,
		xdg_surface);
	if (view->scene_tree == NULL) {
		free(view);
		return;
	}
	view->scene_tree->node.data = view;
	xdg_surface->surface->data = view;
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	view->map.notify = handle_view_map;
	wl_signal_add(&xdg_surface->surface->events.map, &view->map);
	view->unmap.notify = handle_view_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_view_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
	view->commit.notify = handle_view_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->commit);

	view->request_move.notify = handle_view_request_move;
	wl_signal_add(&xdg_surface->toplevel->events.request_move,
		&view->request_move);
	view->request_resize.notify = handle_view_request_resize;
	wl_signal_add(&xdg_surface->toplevel->events.request_resize,
		&view->request_resize);
	view->request_maximize.notify = handle_view_request_maximize;
	wl_signal_add(&xdg_surface->toplevel->events.request_maximize,
		&view->request_maximize);
	view->request_fullscreen.notify = handle_view_request_fullscreen;
	wl_signal_add(&xdg_surface->toplevel->events.request_fullscreen,
		&view->request_fullscreen);

	if (wl_resource_get_version(xdg_surface->toplevel->resource) >=
			XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
		wlr_xdg_toplevel_set_wm_capabilities(xdg_surface->toplevel,
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
	}
	wlr_xdg_toplevel_set_size(xdg_surface->toplevel, 900, 620);
	wl_list_insert(&server->views, &view->link);
}

static void handle_decoration_request_mode(
		struct wl_listener *listener,
		void *data) {
	struct orange_decoration *decoration =
		wl_container_of(listener, decoration, request_mode);
	(void)data;
	wlr_xdg_toplevel_decoration_v1_set_mode(decoration->decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void handle_decoration_destroy(struct wl_listener *listener, void *data) {
	struct orange_decoration *decoration =
		wl_container_of(listener, decoration, destroy);
	(void)data;
	wl_list_remove(&decoration->request_mode.link);
	wl_list_remove(&decoration->destroy.link);
	free(decoration);
}

static void handle_new_xdg_decoration(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xdg_decoration);
	(void)server;
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration = data;

	struct orange_decoration *decoration = calloc(1, sizeof(*decoration));
	if (decoration == NULL) {
		return;
	}
	decoration->decoration = wlr_decoration;
	decoration->request_mode.notify = handle_decoration_request_mode;
	wl_signal_add(&wlr_decoration->events.request_mode,
		&decoration->request_mode);
	decoration->destroy.notify = handle_decoration_destroy;
	wl_signal_add(&wlr_decoration->events.destroy, &decoration->destroy);

	wlr_xdg_toplevel_decoration_v1_set_mode(wlr_decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void handle_output_frame(struct wl_listener *listener, void *data) {
	struct orange_output *output = wl_container_of(listener, output, frame);
	(void)data;

	if (output->commit_pending || output->wlr_output->frame_pending) {
		return;
	}
	output_redraw_shell(output);
	if (!output->wlr_output->needs_frame && !output->server->options->once) {
		return;
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!wlr_scene_output_commit(output->scene_output, NULL)) {
		wlr_log(WLR_ERROR, "failed to commit scene output");
		return;
	}
	output->commit_pending = true;
	wlr_scene_output_send_frame_done(output->scene_output, &now);

	if (output->server->options->once && !output->server->once_committed) {
		output->server->once_committed = true;
		wl_display_terminate(output->server->display);
	}
}

static void handle_output_present(struct wl_listener *listener, void *data) {
	struct orange_output *output = wl_container_of(listener, output, present);
	(void)data;
	output->commit_pending = false;
	if (output->shell_dirty || output->wlr_output->needs_frame) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct orange_output *output =
		wl_container_of(listener, output, destroy);
	(void)data;
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->present.link);
	wl_list_remove(&output->destroy.link);
	if (output->shell_scene_buffer != NULL) {
		wlr_scene_node_destroy(&output->shell_scene_buffer->node);
	}
	if (output->shell_buffer != NULL) {
		wlr_buffer_drop(&output->shell_buffer->base);
	}
	if (output->scene_output != NULL) {
		wlr_scene_output_destroy(output->scene_output);
	}
	free(output);
}

static struct wlr_output_mode *find_closest_output_mode(
		struct wlr_output *output,
		int target_width,
		int target_height) {
	struct wlr_output_mode *best_mode = NULL;
	long long best_score = LLONG_MAX;
	long long target_area = (long long)target_width * target_height;
	struct wlr_output_mode *mode;

	wl_list_for_each(mode, &output->modes, link) {
		long long area = (long long)mode->width * mode->height;
		long long area_diff = llabs(area - target_area);
		long long width_diff = llabs((long long)mode->width - target_width);
		long long height_diff = llabs((long long)mode->height - target_height);
		long long score = area_diff + width_diff + height_diff;

		if (score < best_score) {
			best_score = score;
			best_mode = mode;
		}
	}

	return best_mode;
}

static bool commit_initial_output_state(
		struct orange_server *server,
		struct wlr_output *wlr_output) {
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	wlr_output_state_set_custom_mode(&state,
		server->options->width, server->options->height, 0);

	if (wlr_output_commit_state(wlr_output, &state)) {
		wlr_output_state_finish(&state);
		return true;
	}
	wlr_output_state_finish(&state);

	wlr_log(WLR_INFO, "custom mode %dx%d not supported, trying fixed modes",
		server->options->width, server->options->height);

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode == NULL && !wl_list_empty(&wlr_output->modes)) {
		mode = find_closest_output_mode(wlr_output,
			server->options->width,
			server->options->height);
	}

	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	if (mode != NULL) {
		wlr_log(WLR_INFO, "using output mode %dx%d",
			mode->width, mode->height);
		wlr_output_state_set_mode(&state, mode);
	} else {
		wlr_log(WLR_INFO, "enabling output at current size %dx%d",
			wlr_output->width, wlr_output->height);
	}

	bool ok = wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
	if (!ok) {
		wlr_log(WLR_ERROR, "failed to commit initial output state");
	}
	return ok;
}

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
		wlr_log(WLR_ERROR, "failed to initialize output renderer");
		return;
	}

	if (!commit_initial_output_state(server, wlr_output)) {
		return;
	}

	struct orange_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		return;
	}
	output->server = server;
	output->wlr_output = wlr_output;
	output->layout_output = wlr_output_layout_add_auto(
		server->output_layout,
		wlr_output);
	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout,
		output->layout_output,
		output->scene_output);

	output->frame.notify = handle_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	output->present.notify = handle_output_present;
	wl_signal_add(&wlr_output->events.present, &output->present);
	output->destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	wl_list_insert(&server->outputs, &output->link);

	if (output_ensure_shell_buffer(output)) {
		wlr_output_schedule_frame(wlr_output);
	}

	if (server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor,
			server->xcursor_manager, "default");
	}

	wlr_log(WLR_INFO, "enabled output %s (%dx%d)",
		wlr_output->name,
		output->width,
		output->height);
}

static int handle_clock_timer(void *data) {
	struct orange_server *server = data;
	if (!server->desktop_drag_active) {
		server_load_config(server, false);
	}
	bool status_changed = false;
	if (server->status_poll_ticks <= 0) {
		status_changed = server_update_status(server);
		server->status_poll_ticks = 15;
	} else {
		server->status_poll_ticks--;
	}
	(void)status_changed;
	server_mark_shell_dirty(server);
	wl_event_source_timer_update(server->clock_timer, 1000);
	return 0;
}

static int handle_terminate_signal(int signo, void *data) {
	struct wl_display *display = data;
	(void)signo;
	wl_display_terminate(display);
	return 0;
}

static bool ensure_runtime_dir(void) {
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir != NULL && runtime_dir[0] != '\0' &&
			access(runtime_dir, W_OK | X_OK) == 0) {
		return true;
	}

	char fallback[256];
	snprintf(fallback, sizeof(fallback), "/tmp/orange-runtime-%ld",
		(long)getuid());
	if (mkdir(fallback, 0700) < 0 && access(fallback, W_OK | X_OK) != 0) {
		wlr_log(WLR_ERROR, "failed to create runtime dir %s", fallback);
		return false;
	}
	chmod(fallback, 0700);
	setenv("XDG_RUNTIME_DIR", fallback, true);
	wlr_log(WLR_INFO, "using fallback XDG_RUNTIME_DIR=%s", fallback);
	return true;
}

static bool server_init(struct orange_server *server,
		const struct orange_options *options) {
	memset(server, 0, sizeof(*server));
	server->options = options;
	server->hot_dock_index = -1;
	server->last_dock_pointer_x = INT_MIN;
	server->last_dock_pointer_y = INT_MIN;
	server->status_poll_ticks = 0;
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;
	server->desktop_drag_index = -1;
	server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
	wl_list_init(&server->outputs);
	wl_list_init(&server->views);
	wl_list_init(&server->keyboards);
	orange_config_set_defaults(&server->config);
	server_load_config(server, false);
	status_set_defaults(&server->status);
	server_update_status(server);
	server->desktop_entry_count = orange_desktop_entry_load_all_xdg(
		server->desktop_entries, 1024);
	orange_assets_init(&server->assets);
	orange_assets_load(&server->assets, options->asset_root,
		server->config.icon_theme);

	server->display = wl_display_create();
	if (server->display == NULL) {
		wlr_log(WLR_ERROR, "failed to create Wayland display");
		return false;
	}

	if (options->headless) {
		server->backend = wlr_headless_backend_create(server->display);
	} else {
		server->backend = wlr_backend_autocreate(server->display, NULL);
	}
	if (server->backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create backend");
		return false;
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (server->renderer == NULL) {
		const char *renderer_env = getenv("WLR_RENDERER");
		if (renderer_env != NULL && renderer_env[0] != '\0') {
			wlr_log(WLR_ERROR,
				"WLR_RENDERER=%s failed, falling back to auto-detected renderer",
				renderer_env);
			unsetenv("WLR_RENDERER");
			server->renderer = wlr_renderer_autocreate(server->backend);
		}
	}
	if (server->renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create renderer — no renderer is available");
		return false;
	}
	if (!wlr_renderer_init_wl_display(server->renderer, server->display)) {
		wlr_log(WLR_ERROR, "failed to initialize renderer protocols");
		return false;
	}

#ifdef ORANGE_HAVE_VULKAN
	if (wlr_renderer_is_vk(server->renderer)) {
		wlr_log(WLR_INFO, "wlroots selected the Vulkan renderer");
	} else {
		wlr_log(WLR_INFO, "wlroots selected a non-Vulkan renderer");
	}
#else
	wlr_log(WLR_INFO, "wlroots selected a non-Vulkan renderer (Vulkan not available at build time)");
#endif

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (server->allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create allocator");
		return false;
	}

	wlr_compositor_create(server->display, 5, server->renderer);
	wlr_subcompositor_create(server->display);
	wlr_data_device_manager_create(server->display);
	wlr_primary_selection_v1_device_manager_create(server->display);

	struct wlr_server_decoration_manager *server_decoration_manager =
		wlr_server_decoration_manager_create(server->display);
	wlr_server_decoration_manager_set_default_mode(server_decoration_manager,
		WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager =
		wlr_xdg_decoration_manager_v1_create(server->display);
	server->new_xdg_decoration.notify = handle_new_xdg_decoration;
	wl_signal_add(&xdg_decoration_manager->events.new_toplevel_decoration,
		&server->new_xdg_decoration);

	server->xdg_shell = wlr_xdg_shell_create(server->display, 5);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
		&server->new_xdg_surface);

	server->scene = wlr_scene_create();
	server->shell_tree = wlr_scene_tree_create(&server->scene->tree);
	server->window_tree = wlr_scene_tree_create(&server->scene->tree);
	server->output_layout = wlr_output_layout_create();
	server->scene_layout = wlr_scene_attach_output_layout(server->scene,
		server->output_layout);

	server->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
	server_apply_cursor_config(server);

	server->cursor_motion.notify = handle_cursor_motion;
	wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);
	server->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute,
		&server->cursor_motion_absolute);
	server->cursor_button.notify = handle_cursor_button;
	wl_signal_add(&server->cursor->events.button, &server->cursor_button);
	server->cursor_axis.notify = handle_cursor_axis;
	wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);
	server->cursor_frame.notify = handle_cursor_frame;
	wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);

	server->seat = wlr_seat_create(server->display, "seat0");
	server->request_set_cursor.notify = handle_request_set_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor,
		&server->request_set_cursor);
	server->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&server->seat->events.request_set_selection,
		&server->request_set_selection);
	server->request_set_primary_selection.notify =
		handle_request_set_primary_selection;
	wl_signal_add(&server->seat->events.request_set_primary_selection,
		&server->request_set_primary_selection);

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);
	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);

	struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
	wl_event_loop_add_signal(loop, SIGINT, handle_terminate_signal, server->display);
	wl_event_loop_add_signal(loop, SIGTERM, handle_terminate_signal, server->display);
	server->clock_timer = wl_event_loop_add_timer(loop, handle_clock_timer, server);
	wl_event_source_timer_update(server->clock_timer, 1000);

	const char *socket = NULL;
	if (!options->once) {
		if (!ensure_runtime_dir()) {
			return false;
		}

		socket = wl_display_add_socket_auto(server->display);
		if (socket == NULL) {
			wlr_log(WLR_ERROR, "failed to create Wayland socket");
			return false;
		}
		setenv("WAYLAND_DISPLAY", socket, true);
	} else {
		wlr_log(WLR_INFO, "one-shot mode skips Wayland client socket creation");
	}

	if (options->headless) {
		wlr_headless_add_output(server->backend,
			(unsigned int)options->width,
			(unsigned int)options->height);
	}

	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "failed to start backend");
		return false;
	}

	if (server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor,
			server->xcursor_manager, "default");
	}

	if (socket != NULL) {
		wlr_log(WLR_INFO, "running on Wayland display %s", socket);
	}
	return true;
}

static void server_finish(struct orange_server *server) {
	orange_assets_finish(&server->assets);
	if (server->xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	if (server->display != NULL) {
		wl_display_destroy_clients(server->display);
		wl_display_destroy(server->display);
	}
}

int orange_compositor_run(const struct orange_options *options) {
	struct sigaction sigchld = {
		.sa_handler = SIG_IGN,
		.sa_flags = SA_NOCLDWAIT,
	};
	sigemptyset(&sigchld.sa_mask);
	sigaction(SIGCHLD, &sigchld, NULL);

	wlr_log_init(WLR_INFO, NULL);

	const char *xdg_config = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	const char *config_dir = (xdg_config != NULL && xdg_config[0] != '\0')
		? xdg_config : NULL;
	char fallback[4096];
	if (config_dir == NULL && home != NULL) {
		int n = snprintf(fallback, sizeof(fallback), "%s/.config", home);
		if (n > 0 && n < (int)sizeof(fallback)) {
			config_dir = fallback;
		}
	}
	static const char *versions[] = {"gtk-4.0", "gtk-3.0"};
	for (size_t vi = 0; vi < sizeof(versions) / sizeof(versions[0]); vi++) {
		if (config_dir == NULL) {
			break;
		}
		char path[4096];
		int n = snprintf(path, sizeof(path), "%s/%s", config_dir, versions[vi]);
		if (n <= 0 || n >= (int)sizeof(path)) {
			continue;
		}
		mkdir(path, 0755);
		n = snprintf(path, sizeof(path), "%s/%s/settings.ini",
			config_dir, versions[vi]);
		if (n <= 0 || n >= (int)sizeof(path)) {
			continue;
		}
		FILE *f = fopen(path, "r");
		if (f != NULL) {
			fclose(f);
			continue;
		}
		f = fopen(path, "w");
		if (f == NULL) {
			continue;
		}
		fprintf(f, "[Settings]\ngtk-decoration-layout=close,minimize,maximize:\n");
		fclose(f);
	}

	struct orange_server server;
	if (!server_init(&server, options)) {
		server_finish(&server);
		return 1;
	}

	wl_display_run(server.display);
	server_finish(&server);
	return 0;
}
