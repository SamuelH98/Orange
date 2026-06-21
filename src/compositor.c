#include "orange/buffer.h"
#include "orange/compositor.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/menubar.h"
#include "orange/shell.h"
#include "orange/util.h"

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

#define DESKTOP_ENTRY_RELOAD_TICKS 5
#define VOLUME_RELOAD_TICKS 3

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
	struct wlr_scene_buffer *overlay_scene_buffer;
	struct orange_buffer *overlay_buffer;
	struct wl_listener frame;
	struct wl_listener present;
	struct wl_listener destroy;
	int width;
	int height;
	bool shell_dirty;
	bool overlay_dirty;
	bool overlay_bounds_valid;
	struct orange_rect overlay_bounds;
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
	struct wl_listener request_minimize;
	int x;
	int y;
	int width;
	int height;
	bool mapped;
	bool minimized;
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
	struct wlr_scene_tree *overlay_tree;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_output_layout *output_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct orange_assets assets;
	struct orange_config config;
	bool interface_settings_applied;
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
	bool notification_center_open;
	bool launcher_open;
	enum orange_launcher_mode launcher_display_mode;
	bool launcher_position_set;
	int launcher_x;
	int launcher_y;
	char launcher_query[ORANGE_LAUNCHER_QUERY_MAX];
	int launcher_hot_app;
	int launcher_app_indices[ORANGE_LAUNCHER_APP_MAX];
	int launcher_app_count;
	int launcher_current_mode;
	int launcher_scroll_row;
	bool launcher_search_focus;
	char launcher_filter_cache_key[256];
	bool launcher_drag_active;
	int launcher_drag_offset_x;
	int launcher_drag_offset_y;
	bool launcher_scroll_drag_active;
	int launcher_scroll_drag_offset_y;
	bool launcher_app_drag_active;
	bool launcher_app_drag_moved;
	int launcher_app_drag_flat_index;
	int launcher_app_drag_entry_index;
	int launcher_app_drag_start_x;
	int launcher_app_drag_start_y;
	int launcher_app_drag_insert_before;
	char launcher_categories[ORANGE_LAUNCHER_CATEGORY_MAX][ORANGE_LAUNCHER_CATEGORY_NAME_MAX];
	int launcher_category_count;
	int launcher_category_active;
	struct orange_status_state status;
	struct orange_app_menu_model app_menu;
	char app_menu_cache_key[128];
	char app_menu_bus_name[128];
	char app_menu_action_path[128];
	int status_poll_ticks;
	int desktop_entry_poll_ticks;
	int volume_poll_ticks;
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

	struct orange_volume_info volumes[ORANGE_DESKTOP_VOLUME_MAX];
	int volume_count;
	int desktop_volume_count;
	struct wl_event_source *volume_timer;

	bool dock_drag_active;
	bool dock_drag_moved;
	int dock_drag_index;
	int dock_drag_start_x;
	int dock_drag_start_y;
	int dock_drag_insert_before;
	bool dock_drag_remove;
	bool once_committed;
	uint32_t seat_caps;
	bool dock_bounce_active;
	uint32_t dock_bounce_start;
	int dock_bounce_launcher_idx;
};

static void server_mark_shell_dirty(struct orange_server *server);
static void server_mark_overlay_dirty(struct orange_server *server);
static void server_apply_cursor_config(struct orange_server *server);
static void process_desktop_drag(struct orange_server *server);
static void start_dock_drag(struct orange_server *server,
	const struct orange_shell_layout *layout, int index);
static void process_dock_drag(struct orange_server *server);
static void finish_dock_drag(struct orange_server *server);
static void process_launcher_drag(struct orange_server *server);
static void process_launcher_scroll_drag(struct orange_server *server);
static void process_launcher_app_drag(struct orange_server *server);
static void finish_launcher_app_drag(struct orange_server *server);

static const char *orange_settings_command =
	"if [ -x build/orange-settings ]; then "
	"GSK_RENDERER=cairo build/orange-settings orange.conf; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings; "
	"elif command -v xfce4-settings-manager >/dev/null 2>&1; then "
	"xfce4-settings-manager; "
	"elif command -v mate-control-center >/dev/null 2>&1; then "
	"mate-control-center; "
	"fi; true";

static const char *network_settings_command =
	"if command -v nm-connection-editor >/dev/null 2>&1; then "
	"nm-connection-editor; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_networkmanagement; "
	"fi; true";

static const char *bluetooth_settings_command =
	"if command -v blueman-manager >/dev/null 2>&1; then "
	"blueman-manager; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_bluetooth; "
	"fi; true";

static const char *notification_settings_command =
	"if command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_notifications; "
	"elif command -v xfce4-notifyd-config >/dev/null 2>&1; then "
	"xfce4-notifyd-config; "
	"elif [ -x build/orange-settings ]; then "
	"GSK_RENDERER=cairo build/orange-settings orange.conf; "
	"fi; true";

static const char *sound_settings_command =
	"if command -v pavucontrol >/dev/null 2>&1; then "
	"pavucontrol; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_pulseaudio; "
	"fi; true";

static const char *display_settings_command =
	"if command -v wdisplays >/dev/null 2>&1; then "
	"wdisplays; "
	"elif command -v arandr >/dev/null 2>&1; then "
	"arandr; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_kscreen; "
	"elif command -v xfce4-display-settings >/dev/null 2>&1; then "
	"xfce4-display-settings; "
	"fi; true";

static const char *power_settings_command =
	"if command -v xfce4-power-manager-settings >/dev/null 2>&1; then "
	"xfce4-power-manager-settings; "
	"elif command -v mate-power-preferences >/dev/null 2>&1; then "
	"mate-power-preferences; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings powerdevilprofilesconfig; "
	"fi; true";

static const char *keyboard_settings_command =
	"if command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_keyboard; "
	"elif command -v xfce4-keyboard-settings >/dev/null 2>&1; then "
	"xfce4-keyboard-settings; "
	"fi; true";

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

/* g_bus_get_sync() returns a process-wide shared connection internally, but
 * still pays a lookup/lock on every call. We additionally cache it here per
 * bus type so callers (which may run several times per status poll) don't
 * repeatedly hit that path, and so a failed lookup doesn't get retried until
 * the next poll instead of every call within the same poll. */
static GDBusConnection *dbus_cached_connection(GBusType bus_type) {
	static GDBusConnection *system_conn = NULL;
	static GDBusConnection *session_conn = NULL;
	GDBusConnection **slot = bus_type == G_BUS_TYPE_SYSTEM ?
		&system_conn : &session_conn;
	if (*slot != NULL && g_dbus_connection_is_closed(*slot)) {
		g_object_unref(*slot);
		*slot = NULL;
	}
	if (*slot == NULL) {
		GError *error = NULL;
		*slot = g_bus_get_sync(bus_type, NULL, &error);
		if (error != NULL) {
			g_error_free(error);
		}
	}
	return *slot;
}

/* Fetch every property on an interface in a single round trip instead of
 * one "Get" call per property. Returns a floating-ref-free a{sv} GVariant
 * (caller must g_variant_unref it), or NULL on failure. */
static GVariant *dbus_get_all_properties(
		GBusType bus_type,
		const char *bus_name,
		const char *object_path,
		const char *interface) {
	GDBusConnection *connection = dbus_cached_connection(bus_type);
	if (connection == NULL) {
		return NULL;
	}
	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		"org.freedesktop.DBus.Properties",
		"GetAll",
		g_variant_new("(s)", interface),
		G_VARIANT_TYPE("(a{sv})"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		80,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return NULL;
	}
	GVariant *dict = NULL;
	g_variant_get(reply, "(@a{sv})", &dict);
	g_variant_unref(reply);
	return dict;
}

static void update_network_status(struct orange_status_state *status) {
	GVariant *props = dbus_get_all_properties(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.NetworkManager",
		"/org/freedesktop/NetworkManager",
		"org.freedesktop.NetworkManager");
	if (props == NULL) {
		return;
	}

	uint32_t state = 0;
	if (!g_variant_lookup(props, "Connectivity", "u", &state)) {
		g_variant_unref(props);
		return;
	}

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

	gboolean wireless_enabled = TRUE;
	if (g_variant_lookup(props, "WirelessEnabled", "b", &wireless_enabled) &&
			!wireless_enabled) {
		status->network_connected = false;
		snprintf(status->network_icon, sizeof(status->network_icon),
			"%s", "network-offline");
		snprintf(status->network_label, sizeof(status->network_label),
			"%s", "Wi-Fi Disabled");
	}
	g_variant_unref(props);
}

static void update_battery_status(struct orange_status_state *status) {
	GVariant *props = dbus_get_all_properties(G_BUS_TYPE_SYSTEM,
		"org.freedesktop.UPower",
		"/org/freedesktop/UPower/devices/DisplayDevice",
		"org.freedesktop.UPower.Device");
	if (props == NULL) {
		return;
	}

	gboolean present = FALSE;
	if (g_variant_lookup(props, "IsPresent", "b", &present)) {
		status->battery_present = present;
	}

	double percent = -1.0;
	if (g_variant_lookup(props, "Percentage", "d", &percent)) {
		if (percent < 0.0) {
			percent = 0.0;
		} else if (percent > 100.0) {
			percent = 100.0;
		}
		status->battery_percent = (int)lrint(percent);
		status->battery_present = true;
	}

	uint32_t battery_state = 0;
	if (g_variant_lookup(props, "State", "u", &battery_state)) {
		status->battery_charging = battery_state == 1 || battery_state == 4;
	}

	const char *icon_name = NULL;
	if (g_variant_lookup(props, "IconName", "&s", &icon_name) &&
			icon_name != NULL && icon_name[0] != '\0') {
		snprintf(status->battery_icon, sizeof(status->battery_icon),
			"%s", icon_name);
	}
	g_variant_unref(props);

	if (status->battery_percent >= 0) {
		snprintf(status->battery_label, sizeof(status->battery_label),
			status->battery_charging ? "Battery %d%% Charging" : "Battery %d%%",
			status->battery_percent);
	} else {
		snprintf(status->battery_label, sizeof(status->battery_label),
			"%s", "Battery");
	}
}

static bool has_mount_path(struct orange_volume_info *volumes, int count, const char *path) {
	for (int i = 0; i < count; i++) {
		if (strcmp(volumes[i].mount_path, path) == 0) {
			return true;
		}
	}
	return false;
}

static void add_volume(struct orange_volume_info *volumes, int *count, int *desktop_count,
		const char *label, const char *mount_path, const char *icon_name,
		bool is_removable, bool is_internal) {
	if (*count >= ORANGE_DESKTOP_VOLUME_MAX) {
		return;
	}
	if (has_mount_path(volumes, *count, mount_path)) {
		return;
	}
	struct orange_volume_info *info = &volumes[*count];
	memset(info, 0, sizeof(*info));
	snprintf(info->label, sizeof(info->label), "%s", label);
	snprintf(info->mount_path, sizeof(info->mount_path), "%s", mount_path);
	snprintf(info->icon_name, sizeof(info->icon_name), "%s", icon_name);
	info->is_removable = is_removable;
	info->is_internal = is_internal;
	(*count)++;
	(*desktop_count)++;
}

static void update_volumes(struct orange_server *server) {
	const struct orange_config *config = &server->config;
	struct orange_volume_info volumes[ORANGE_DESKTOP_VOLUME_MAX];
	int count = 0;
	int desktop_count = 0;

	/* Scan /proc/mounts for real disk-backed filesystems */
	if (config->desktop_show_hard_disks) {
		FILE *mounts_file = fopen("/proc/mounts", "r");
		if (mounts_file != NULL) {
			char buf[4096];
			while (fgets(buf, sizeof(buf), mounts_file) != NULL &&
					count < ORANGE_DESKTOP_VOLUME_MAX) {
				char device[256], mount_point[256], fstype[64];
				if (sscanf(buf, "%255s %255s %63s", device, mount_point, fstype) != 3) {
					continue;
				}
				if (strncmp(device, "/dev/", 5) != 0) {
					continue;
				}
				if (strcmp(mount_point, "/") == 0) {
					add_volume(volumes, &count, &desktop_count,
						"Macintosh HD", "/", "drive-harddisk",
						false, true);
				} else if (strcmp(mount_point, "/home") == 0) {
					add_volume(volumes, &count, &desktop_count,
						"Home", "/home", "drive-harddisk",
						false, true);
				}
			}
			fclose(mounts_file);
		}
	}

	/* Check GVolumeMonitor for removable media and external drives */
	GVolumeMonitor *monitor = g_volume_monitor_get();
	if (monitor != NULL) {
		GList *mounts = g_volume_monitor_get_mounts(monitor);
		for (GList *l = mounts; l != NULL && count < ORANGE_DESKTOP_VOLUME_MAX; l = l->next) {
			GMount *mount = G_MOUNT(l->data);

			GVolume *volume = g_mount_get_volume(mount);
			bool has_volume = volume != NULL;
			bool can_eject = has_volume ? g_volume_can_eject(volume) : false;

			bool show = false;
			if (!can_eject && config->desktop_show_hard_disks) {
				show = true;
			} else if (can_eject && config->desktop_show_external_disks) {
				show = true;
			} else if (can_eject && config->desktop_show_removable_media) {
				show = true;
			}

			if (has_volume) {
				g_object_unref(volume);
			}

			if (!show) {
				g_object_unref(mount);
				continue;
			}

			char mount_path[ORANGE_VOLUME_PATH_MAX] = "";
			GFile *root_file = g_mount_get_root(mount);
			if (root_file != NULL) {
				char *path = g_file_get_path(root_file);
				if (path != NULL) {
					snprintf(mount_path, sizeof(mount_path), "%s", path);
					g_free(path);
				}
				g_object_unref(root_file);
			}

			const char *name = g_mount_get_name(mount);
			add_volume(volumes, &count, &desktop_count,
				name != NULL ? name : "Volume",
				mount_path[0] != '\0' ? mount_path : name != NULL ? name : "volume",
				can_eject ? "media-removable" : "drive-harddisk",
				can_eject, !can_eject);

			g_object_unref(mount);
		}
		g_list_free(mounts);
		g_object_unref(monitor);
	}

	server->desktop_volume_count = desktop_count;
	server->volume_count = count;
	memcpy(server->volumes, volumes, sizeof(volumes[0]) * count);
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

static const char *config_resolved_cursor_theme(
		const struct orange_config *config) {
	if (config->cursor_theme[0] != '\0') {
		return config->cursor_theme;
	}
	if (config->icon_theme[0] != '\0') {
		return config->icon_theme;
	}
	return NULL;
}

static bool nullable_strings_equal(const char *a, const char *b) {
	if (a == NULL || b == NULL) {
		return a == b;
	}
	return strcmp(a, b) == 0;
}

static int config_resolved_cursor_size(const struct orange_config *config) {
	return config->cursor_size > 0 ? config->cursor_size : 28;
}

static void set_env_or_unset(const char *name, const char *value) {
	if (value != NULL && value[0] != '\0') {
		setenv(name, value, true);
	} else {
		unsetenv(name);
	}
}

static void set_gsettings_string_if_writable(GSettings *settings,
		GSettingsSchema *schema,
		const char *key,
		const char *value) {
	if (value == NULL || value[0] == '\0' ||
			!g_settings_schema_has_key(schema, key) ||
			!g_settings_is_writable(settings, key)) {
		return;
	}
	g_settings_set_string(settings, key, value);
}

static void set_gsettings_int_if_writable(GSettings *settings,
		GSettingsSchema *schema,
		const char *key,
		int value) {
	if (!g_settings_schema_has_key(schema, key) ||
			!g_settings_is_writable(settings, key)) {
		return;
	}
	g_settings_set_int(settings, key, value);
}

static void server_apply_interface_settings(struct orange_server *server) {
	server->interface_settings_applied = true;
	const char *session_bus = getenv("DBUS_SESSION_BUS_ADDRESS");
	if (session_bus == NULL || session_bus[0] == '\0') {
		return;
	}

	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL) {
		return;
	}

	GSettingsSchema *schema = g_settings_schema_source_lookup(source,
		"org.gnome.desktop.interface", true);
	if (schema == NULL) {
		return;
	}

	GSettings *settings = g_settings_new_full(schema, NULL, NULL);
	if (settings != NULL) {
		const char *gtk_theme =
			server->config.appearance == ORANGE_APPEARANCE_DARK ?
			server->config.gtk_theme_dark : server->config.gtk_theme_light;
		set_gsettings_string_if_writable(settings, schema, "gtk-theme",
			gtk_theme);
		set_gsettings_string_if_writable(settings, schema, "icon-theme",
			server->config.icon_theme);
		set_gsettings_string_if_writable(settings, schema, "cursor-theme",
			config_resolved_cursor_theme(&server->config));
		set_gsettings_int_if_writable(settings, schema, "cursor-size",
			config_resolved_cursor_size(&server->config));
		g_settings_sync();
		g_object_unref(settings);
	}
	g_settings_schema_unref(schema);
}

static void server_ensure_xcursor_path(void) {
	const char *current = getenv("XCURSOR_PATH");
	if (current != NULL && current[0] != '\0') {
		return;
	}

	char path[4096] = "";
	const char *home = getenv("HOME");
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home != NULL && xdg_data_home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/icons", xdg_data_home);
	} else if (home != NULL && home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/.local/share/icons", home);
	}
	if (home != NULL && home[0] != '\0') {
		size_t len = strlen(path);
		snprintf(path + len, sizeof(path) - len, "%s%s/.icons",
			len > 0 ? ":" : "", home);
	}

	size_t len = strlen(path);
	snprintf(path + len, sizeof(path) - len,
		"%s/usr/local/share/icons:/usr/share/icons",
		len > 0 ? ":" : "");
	setenv("XCURSOR_PATH", path, true);
}

static void server_apply_theme_env(struct orange_server *server) {
	const char *theme = server->config.appearance == ORANGE_APPEARANCE_DARK ?
		server->config.gtk_theme_dark : server->config.gtk_theme_light;
	set_env_or_unset("GTK_THEME", theme);
	set_env_or_unset("GTK_ICON_THEME", server->config.icon_theme);
	setenv("ORANGE_APPEARANCE",
		orange_config_appearance_name(server->config.appearance), true);
	setenv("GTK_CSD", "1", true);
}

static void server_apply_cursor_config(struct orange_server *server) {
	const char *theme = config_resolved_cursor_theme(&server->config);
	int size = config_resolved_cursor_size(&server->config);
	char size_text[16];
	snprintf(size_text, sizeof(size_text), "%d", size);
	set_env_or_unset("XCURSOR_THEME", theme);
	setenv("XCURSOR_SIZE", size_text, true);
	server_ensure_xcursor_path();

	if (server->cursor == NULL) {
		return;
	}

	if (server->xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	server->xcursor_manager = wlr_xcursor_manager_create(theme, (uint32_t)size);
	if (server->xcursor_manager == NULL) {
		wlr_log(WLR_ERROR, "failed to create xcursor manager");
		return;
	}
	if (!wlr_xcursor_manager_load(server->xcursor_manager, 1.0)) {
		wlr_log(WLR_ERROR, "failed to load cursor theme %s at size %d",
			theme != NULL ? theme : "(default)", size);
	}
	wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "default");
}

static void server_reload_assets_if_ready(struct orange_server *server) {
	if (server->assets.asset_root[0] == '\0') {
		return;
	}
	char asset_root[4096];
	snprintf(asset_root, sizeof(asset_root), "%s", server->assets.asset_root);
	orange_assets_finish(&server->assets);
	orange_assets_init(&server->assets);
	orange_assets_load(&server->assets, asset_root, server->config.icon_theme);
}

static void server_load_config(struct orange_server *server, bool force_dirty) {
	struct orange_config next;
	orange_config_load(&next, server->options->config_path);
	bool changed = memcmp(&server->config, &next, sizeof(next)) != 0;
	bool appearance_changed = server->config.appearance != next.appearance ||
		strcmp(server->config.gtk_theme_light, next.gtk_theme_light) != 0 ||
		strcmp(server->config.gtk_theme_dark, next.gtk_theme_dark) != 0;
	bool icon_theme_changed =
		strcmp(server->config.icon_theme, next.icon_theme) != 0;
	bool cursor_changed =
		!nullable_strings_equal(
			config_resolved_cursor_theme(&server->config),
			config_resolved_cursor_theme(&next)) ||
		server->config.cursor_size != next.cursor_size;
	server->config = next;
	server_apply_theme_env(server);
	if (!server->interface_settings_applied || appearance_changed ||
			icon_theme_changed || cursor_changed || force_dirty) {
		server_apply_interface_settings(server);
	}
	if (cursor_changed || icon_theme_changed || force_dirty) {
		server_apply_cursor_config(server);
	}
	if (icon_theme_changed || force_dirty) {
		server_reload_assets_if_ready(server);
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

static const char *launcher_active_category_filter(
		const struct orange_server *server) {
	if (server == NULL ||
			server->launcher_current_mode != ORANGE_LAUNCHER_MODE_APPS ||
			server->launcher_category_active <= 0 ||
			server->launcher_category_active >= server->launcher_category_count) {
		return NULL;
	}
	const char *filter =
		server->launcher_categories[server->launcher_category_active];
	return filter[0] != '\0' ? filter : NULL;
}

/* Pre-resolve icon file paths for every desktop entry so the icon-theme
 * directory scan is done once at startup instead of on every launcher open.
 * We deliberately only cache the file path here — the actual surface is
 * loaded lazily on first draw so that expensive SVG rendering doesn't block
 * compositor startup. */
static void server_preload_app_icons(struct orange_server *server) {
	for (size_t i = 0; i < server->desktop_entry_count; i++) {
		const char *name = server->desktop_entries[i].icon;
		if (name[0] == '\0') {
			name = "application-x-executable";
		}
		orange_assets_preload_icon(&server->assets, name);
	}
}

static void launcher_preload_icons(struct orange_server *server) {
	if (server->launcher_display_mode != ORANGE_LAUNCHER_DISPLAY_FULL ||
			server->launcher_app_count <= 0) {
		return;
	}
	bool dark = is_dark_config(&server->config);
	int variant = dark ?
		ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	int n = server->launcher_app_count;
	if (n > ORANGE_LAUNCHER_APP_MAX) {
		n = ORANGE_LAUNCHER_APP_MAX;
	}
	for (int i = 0; i < n; i++) {
		const char *icon_name = orange_launcher_app_icon(
			server->desktop_entries, (int)server->desktop_entry_count,
			server->launcher_app_indices, server->launcher_app_count, i);
		if (icon_name != NULL) {
			orange_assets_icon(&server->assets, variant, icon_name);
		}
	}
}

static void launcher_refresh_filter(struct orange_server *server) {
	if (server->launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
		server->launcher_app_count = 0;
		server->launcher_hot_app = -1;
		return;
	}

	char key[256];
	snprintf(key, sizeof(key), "%s|%s|%zu",
		server->launcher_query,
		launcher_active_category_filter(server) != NULL ?
			launcher_active_category_filter(server) : "",
		server->desktop_entry_count);
	bool cache_hit = strcmp(server->launcher_filter_cache_key, key) == 0;
	if (!cache_hit) {
		snprintf(server->launcher_filter_cache_key,
			sizeof(server->launcher_filter_cache_key), "%s", key);
		server->launcher_app_count = orange_launcher_filter(
			server->desktop_entries, (int)server->desktop_entry_count,
			server->launcher_query,
			launcher_active_category_filter(server),
			server->launcher_app_indices, ORANGE_LAUNCHER_APP_MAX);
	}

	/* Always re-validate hot_app and keep icons preloaded, even on a
	 * cache hit — these are cheap and other code paths (dock hover, key
	 * navigation) can move hot_app independently of the filtered set. */
	if (server->launcher_app_count <= 0) {
		server->launcher_hot_app = -1;
		return;
	}
	if (server->launcher_hot_app >= server->launcher_app_count) {
		server->launcher_hot_app = server->launcher_app_count - 1;
	}
	if (server->launcher_hot_app < 0) {
		server->launcher_hot_app = 0;
	}
	launcher_preload_icons(server);
}

static void launcher_open_overlay(struct orange_server *server, bool focus_search,
		enum orange_launcher_mode display_mode) {
	server->launcher_open = true;
	server->launcher_display_mode = display_mode;
	server->launcher_query[0] = '\0';
	server->launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	server->launcher_scroll_row = 0;
	server->launcher_hot_app = -1;
	server->launcher_search_focus = focus_search;
	server->system_menu_open = false;
	server->notification_center_open = false;
	/* Default category tabs for Apps mode */
	const char *cats[] = {
		"All", "Utilities", "Productivity", "Social", "Media", "Info",
	};
	int nc = (int)(sizeof(cats) / sizeof(cats[0]));
	if (nc > ORANGE_LAUNCHER_CATEGORY_MAX) {
		nc = ORANGE_LAUNCHER_CATEGORY_MAX;
	}
	server->launcher_category_count = nc;
	server->launcher_category_active = 0;
	for (int i = 0; i < nc; i++) {
		snprintf(server->launcher_categories[i],
			sizeof(server->launcher_categories[i]),
			"%s", cats[i]);
	}
	launcher_refresh_filter(server);
	server_mark_overlay_dirty(server);
}

static void launcher_close_overlay(struct orange_server *server) {
	if (!server->launcher_open) {
		return;
	}
	server->launcher_open = false;
	server->launcher_query[0] = '\0';
	server->launcher_hot_app = -1;
	server->launcher_search_focus = false;
	server->launcher_position_set = false;
	server->launcher_drag_active = false;
	server->launcher_scroll_drag_active = false;
	server->launcher_app_drag_active = false;
	server->launcher_app_drag_moved = false;
	server->launcher_app_drag_flat_index = -1;
	server->launcher_app_drag_entry_index = -1;
	server->launcher_app_drag_insert_before = -1;
	server_mark_overlay_dirty(server);
}

static void launcher_toggle_overlay(struct orange_server *server, bool focus_search,
		enum orange_launcher_mode display_mode) {
	if (server->launcher_open) {
		launcher_close_overlay(server);
	} else {
		launcher_open_overlay(server, focus_search, display_mode);
	}
}

/* Forward declaration — defined later in this file. */
static void launch_desktop_entry(const struct orange_desktop_entry *entry);

/* Launch the app at a flat (filtered) index and close the overlay. */
static void launcher_launch_index(struct orange_server *server, int flat_index) {
	if (flat_index < 0 || flat_index >= server->launcher_app_count) {
		return;
	}
	int entry_index = server->launcher_app_indices[flat_index];
	if (entry_index >= 0 &&
			entry_index < (int)server->desktop_entry_count) {
		launch_desktop_entry(&server->desktop_entries[entry_index]);
	}
	launcher_close_overlay(server);
}

static uint32_t monotonic_time_msec(void) {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static bool contains_case(const char *haystack, const char *needle) {
	return haystack != NULL && needle != NULL && strcasestr(haystack, needle) != NULL;
}

static void app_menu_model_reset(struct orange_app_menu_model *menu) {
	if (menu != NULL) {
		memset(menu, 0, sizeof(*menu));
	}
}

static bool gmenu_item_string(
		GMenuModel *model,
		int item,
		const char *attribute,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return false;
	}
	buffer[0] = '\0';
	GVariant *value = g_menu_model_get_item_attribute_value(
		model, item, attribute, G_VARIANT_TYPE_STRING);
	if (value == NULL) {
		return false;
	}
	const char *text = g_variant_get_string(value, NULL);
	if (text != NULL && text[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", text);
	}
	g_variant_unref(value);
	return buffer[0] != '\0';
}

static void parse_gmenu_items(
		GMenuModel *model,
		struct orange_app_menu_model *menu,
		int tab,
		int depth) {
	if (model == NULL || menu == NULL ||
			tab < 0 || tab >= ORANGE_APP_MENU_TAB_COUNT ||
			depth > 4) {
		return;
	}
	int n_items = g_menu_model_get_n_items(model);
	for (int i = 0; i < n_items; i++) {
		GMenuModel *section = g_menu_model_get_item_link(
			model, i, G_MENU_LINK_SECTION);
		if (section != NULL) {
			if (menu->item_counts[tab] > 0 &&
					menu->item_counts[tab] < ORANGE_APP_MENU_ITEM_MAX) {
				menu->items[tab][menu->item_counts[tab]].separator = true;
			}
			parse_gmenu_items(section, menu, tab, depth + 1);
			g_object_unref(section);
			continue;
		}

		if (menu->item_counts[tab] >= ORANGE_APP_MENU_ITEM_MAX) {
			break;
		}
		struct orange_app_menu_item *out =
			&menu->items[tab][menu->item_counts[tab]];
		if (!gmenu_item_string(model, i, G_MENU_ATTRIBUTE_LABEL,
				out->label, sizeof(out->label))) {
			GMenuModel *submenu = g_menu_model_get_item_link(
				model, i, G_MENU_LINK_SUBMENU);
			if (submenu != NULL) {
				parse_gmenu_items(submenu, menu, tab, depth + 1);
				g_object_unref(submenu);
			}
			continue;
		}
		gmenu_item_string(model, i, G_MENU_ATTRIBUTE_ACTION,
			out->action, sizeof(out->action));
		out->enabled = true;
		menu->item_counts[tab]++;
	}
}

static bool parse_gmenu_root(
		GMenuModel *root,
		struct orange_app_menu_model *menu) {
	if (root == NULL || menu == NULL) {
		return false;
	}
	int n_items = g_menu_model_get_n_items(root);
	for (int i = 0; i < n_items && menu->tab_count < ORANGE_APP_MENU_TAB_COUNT; i++) {
		char label[ORANGE_APP_MENU_LABEL_MAX];
		GMenuModel *submenu = g_menu_model_get_item_link(
			root, i, G_MENU_LINK_SUBMENU);
		GMenuModel *section = NULL;
		if (submenu == NULL) {
			section = g_menu_model_get_item_link(root, i, G_MENU_LINK_SECTION);
		}
		GMenuModel *child = submenu != NULL ? submenu : section;
		if (child == NULL) {
			continue;
		}
		if (!gmenu_item_string(root, i, G_MENU_ATTRIBUTE_LABEL,
				label, sizeof(label))) {
			if (section != NULL) {
				parse_gmenu_root(section, menu);
			}
			g_object_unref(child);
			continue;
		}
		int tab = menu->tab_count++;
		snprintf(menu->tab_labels[tab],
			sizeof(menu->tab_labels[tab]), "%s", label);
		parse_gmenu_items(child, menu, tab, 0);
		g_object_unref(child);
	}
	menu->available = menu->tab_count > 0;
	menu->native = menu->available;
	return menu->available;
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

static void server_reload_desktop_entries(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	server->desktop_entry_count = orange_desktop_entry_load_all_xdg(
		server->desktop_entries, 1024);
	/* Entry contents (not just count) may have changed; force
	 * launcher_refresh_filter to recompute next time it's called instead
	 * of trusting the count-based cache key. */
	server->launcher_filter_cache_key[0] = '\0';
}

static void launch_desktop_entry(
		const struct orange_desktop_entry *entry) {
	if (entry == NULL) {
		return;
	}
	char command[1024];
	bool expanded = orange_desktop_entry_expand_exec(entry,
		command, sizeof(command));
	const char *exec = expanded ? command : entry->exec;
	if (orange_desktop_entry_id_matches(entry->id, "org.gnome.Settings") ||
			strstr(exec, "gnome-control-center") != NULL) {
		char wrapped[1200];
		snprintf(wrapped, sizeof(wrapped),
			"XDG_CURRENT_DESKTOP=GNOME %s", exec);
		launch_command(wrapped);
	} else {
		launch_command(exec);
	}
}

static int dock_launcher_for_view(
	struct orange_server *server,
	struct orange_view *view);
static void focus_view(struct orange_view *view, struct wlr_surface *surface);
static bool focus_view_for_dock_launcher(
	struct orange_server *server,
	int launcher_idx);

static void launch_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0 ||
			launcher_idx >= ORANGE_DOCK_MAX ||
			server->config.dock_apps[launcher_idx][0] == '\0') {
		return;
	}

	const char *app_id = server->config.dock_apps[launcher_idx];
	/* The dock launcher grid icon opens the native Launchpad/Spotlight overlay
	 * rather than an external picker. An external picker can still be forced
	 * via the ORANGE_APP_PICKER environment variable. */
	if (strcmp(app_id, "__launcher__") == 0) {
		if (getenv("ORANGE_APP_PICKER") != NULL &&
				getenv("ORANGE_APP_PICKER")[0] != '\0') {
			launch_app_picker();
		} else {
			launcher_open_overlay(server, false,
				ORANGE_LAUNCHER_DISPLAY_FULL);
		}
		return;
	}
	if (focus_view_for_dock_launcher(server, launcher_idx)) {
		return;
	}
	if (orange_dock_builtin_command(app_id) != NULL) {
		launch_command(orange_dock_builtin_command(app_id));
		return;
	}
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_app_id(server, app_id);
	if (entry != NULL) {
		launch_desktop_entry(entry);
	} else {
		const char *command = orange_dock_command(launcher_idx,
			server->desktop_entries, (int)server->desktop_entry_count,
			&server->config);
		if (command != NULL) {
			launch_command(command);
		}
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
	int dock_count = orange_dock_count(&server->config);
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

static bool focus_view_for_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return false;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || view->minimized) {
			continue;
		}
		if (dock_launcher_for_view(server, view) == launcher_idx) {
			focus_view(view, view->xdg_surface->surface);
			return true;
		}
	}
	return false;
}

static void focus_view(struct orange_view *view, struct wlr_surface *surface);

static bool focus_view_for_app_id(struct orange_server *server,
		const char *needle) {
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) continue;
		const char *app_id = view->xdg_surface->toplevel->app_id;
		if (app_id != NULL && strcasestr(app_id, needle) != NULL) {
			struct wlr_surface *surface = view->xdg_surface->surface;
			if (surface != NULL) {
				focus_view(view, surface);
				return true;
			}
		}
	}
	return false;
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

static void format_app_id_label(
		char *buffer,
		size_t buffer_size,
		const char *app_id) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	buffer[0] = '\0';
	if (app_id == NULL || app_id[0] == '\0') {
		return;
	}

	const char *start = app_id;
	size_t len = strlen(app_id);
	const char *suffixes[] = {".desktop", ".exe", NULL};
	for (int i = 0; suffixes[i] != NULL; i++) {
		size_t suffix_len = strlen(suffixes[i]);
		if (len > suffix_len &&
				strcasecmp(app_id + len - suffix_len, suffixes[i]) == 0) {
			len -= suffix_len;
			break;
		}
	}
	for (size_t i = len; i > 0; i--) {
		if (app_id[i - 1] == '.') {
			start = app_id + i;
			len -= i;
			break;
		}
	}
	if (len >= buffer_size) {
		len = buffer_size - 1;
	}
	for (size_t i = 0; i < len; i++) {
		char c = start[i];
		buffer[i] = c == '-' || c == '_' ? ' ' : c;
	}
	buffer[len] = '\0';
	if (buffer[0] >= 'a' && buffer[0] <= 'z') {
		buffer[0] = (char)(buffer[0] - 'a' + 'A');
	}
	for (size_t i = 1; buffer[i] != '\0'; i++) {
		if (buffer[i - 1] == ' ' && buffer[i] >= 'a' && buffer[i] <= 'z') {
			buffer[i] = (char)(buffer[i] - 'a' + 'A');
		}
	}
}

static void server_active_app_label(
		struct orange_server *server,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	snprintf(buffer, buffer_size, "%s", "Files");
	if (server == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped ||
			server->focused_view->xdg_surface == NULL ||
			server->focused_view->xdg_surface->toplevel == NULL) {
		return;
	}

	int launcher_idx = dock_launcher_for_view(server, server->focused_view);
	const char *label = launcher_idx >= 0 ?
		orange_dock_label(launcher_idx,
			server->desktop_entries,
			(int)server->desktop_entry_count,
			&server->config) : NULL;
	if (label != NULL && label[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", label);
		return;
	}

	const char *app_id = server->focused_view->xdg_surface->toplevel->app_id;
	format_app_id_label(buffer, buffer_size, app_id);
	if (buffer[0] != '\0') {
		return;
	}

	const char *title = server->focused_view->xdg_surface->toplevel->title;
	if (title != NULL && title[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", title);
	}
}

static const char *focused_app_id(struct orange_server *server) {
	if (server == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped ||
			server->focused_view->xdg_surface == NULL ||
			server->focused_view->xdg_surface->toplevel == NULL) {
		return NULL;
	}
	return server->focused_view->xdg_surface->toplevel->app_id;
}

static void strip_desktop_suffix(
		const char *source,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	buffer[0] = '\0';
	if (source == NULL || source[0] == '\0') {
		return;
	}
	snprintf(buffer, buffer_size, "%s", source);
	size_t len = strlen(buffer);
	const char *suffix = ".desktop";
	size_t suffix_len = strlen(suffix);
	if (len > suffix_len &&
			strcasecmp(buffer + len - suffix_len, suffix) == 0) {
		buffer[len - suffix_len] = '\0';
	}
}

static bool import_gmenu_for_bus_name(
		struct orange_server *server,
		GDBusConnection *connection,
		const char *bus_name,
		struct orange_app_menu_model *menu) {
	static const char *menu_paths[] = {
		"/org/gtk/menus",
		"/org/gtk/Menus",
		NULL,
	};
	if (connection == NULL || bus_name == NULL ||
			!g_dbus_is_name(bus_name)) {
		return false;
	}
	for (int i = 0; menu_paths[i] != NULL; i++) {
		struct orange_app_menu_model candidate;
		app_menu_model_reset(&candidate);
		GDBusMenuModel *dbus_model = g_dbus_menu_model_get(
			connection, bus_name, menu_paths[i]);
		if (dbus_model == NULL) {
			continue;
		}
		bool ok = parse_gmenu_root(G_MENU_MODEL(dbus_model), &candidate);
		g_object_unref(dbus_model);
		if (ok) {
			*menu = candidate;
			snprintf(server->app_menu_bus_name,
				sizeof(server->app_menu_bus_name), "%s", bus_name);
			snprintf(server->app_menu_action_path,
				sizeof(server->app_menu_action_path), "%s", "/org/gtk/Actions");
			return true;
		}
	}
	return false;
}

static void server_update_app_menu_model(struct orange_server *server) {
	char active_label[ORANGE_APP_MENU_LABEL_MAX];
	server_active_app_label(server, active_label, sizeof(active_label));
	const char *app_id = focused_app_id(server);
	char key[128];
	snprintf(key, sizeof(key), "%s|%s",
		app_id != NULL ? app_id : "",
		active_label);
	if (strcmp(server->app_menu_cache_key, key) == 0) {
		return;
	}
	snprintf(server->app_menu_cache_key,
		sizeof(server->app_menu_cache_key), "%s", key);
	app_menu_model_reset(&server->app_menu);
	server->app_menu_bus_name[0] = '\0';
	server->app_menu_action_path[0] = '\0';

	GError *error = NULL;
	GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (connection == NULL) {
		return;
	}

	char stripped_id[128];
	strip_desktop_suffix(app_id, stripped_id, sizeof(stripped_id));
	const char *candidates[5] = {0};
	int count = 0;
	if (stripped_id[0] != '\0') {
		candidates[count++] = stripped_id;
	}
	if (contains_case(app_id, "firefox") ||
			contains_case(active_label, "firefox")) {
		candidates[count++] = "org.mozilla.firefox";
	}
	if (contains_case(app_id, "nautilus") ||
			contains_case(active_label, "files")) {
		candidates[count++] = "org.gnome.Nautilus";
	}
	for (int i = 0; i < count; i++) {
		if (import_gmenu_for_bus_name(server, connection,
				candidates[i], &server->app_menu)) {
			break;
		}
	}
	g_object_unref(connection);
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
	server_update_app_menu_model(server);
	orange_shell_layout_compute(output->width, output->height,
		server->system_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		server->desktop_volume_count,
		layout);
	char active_app_label[ORANGE_APP_MENU_LABEL_MAX];
	server_active_app_label(server, active_app_label, sizeof(active_app_label));
	orange_shell_layout_set_app_menu_tabs(layout, active_app_label,
		&server->app_menu);
	orange_shell_layout_set_context_menu(layout,
		server->context_menu_kind,
		server->context_menu_index,
		server->context_menu_cursor_x,
		server->context_menu_cursor_y,
		&server->app_menu);
	if (server->notification_center_open) {
		orange_shell_layout_set_notification_center(layout);
	}
	if (server->launcher_open) {
		launcher_refresh_filter(server);
		bool searching = server->launcher_query[0] != '\0';
		layout->launcher_position_set = server->launcher_position_set;
		layout->launcher_x = server->launcher_x;
		layout->launcher_y = server->launcher_y;
		layout->launcher_scroll_row = server->launcher_scroll_row;
		layout->launcher_current_mode = server->launcher_current_mode;
		layout->launcher_category_count = server->launcher_category_count;
		layout->launcher_category_active = server->launcher_category_active;
		orange_shell_layout_set_launcher(layout, searching,
			server->launcher_display_mode, server->launcher_app_count);
	}
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
	bool vertical =
		layout->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		layout->dock_position == ORANGE_DOCK_POSITION_RIGHT;
	double max_scale = config->dock_magnification_scale;
	if (max_scale < 1.0) {
		max_scale = 1.0;
	} else if (max_scale > 2.20) {
		max_scale = 2.20;
	}
	int icon = first.height;
	int hover_top = vertical ?
		first.y - icon / 2 :
		first.y - (int)(icon * (max_scale - 1.0) + 0.5);
	int hover_bottom = vertical ?
		last.y + last.height + icon / 2 :
		layout->dock.y + layout->dock.height;
	int hover_left = vertical ?
		layout->dock.x - icon / 3 :
		first.x - icon / 2;
	int hover_right = vertical ?
		layout->dock.x + layout->dock.width + icon / 3 :
		last.x + last.width + icon / 2;
	if (x < hover_left || x > hover_right ||
			y < hover_top || y > hover_bottom) {
		return -1;
	}

	int best = -1;
	int best_distance = INT_MAX;
	for (int i = 0; i < layout->dock_item_count; i++) {
		int center = vertical ?
			layout->dock_items[i].y + layout->dock_items[i].height / 2 :
			layout->dock_items[i].x + layout->dock_items[i].width / 2;
		int pointer = vertical ? y : x;
		int distance = abs(pointer - center);
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
		output->overlay_dirty = true;
		if (!output->commit_pending && !output->wlr_output->frame_pending) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void server_mark_overlay_dirty(struct orange_server *server) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output->overlay_dirty = true;
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

	if (output->overlay_scene_buffer != NULL) {
		wlr_scene_node_destroy(&output->overlay_scene_buffer->node);
		output->overlay_scene_buffer = NULL;
	}
	if (output->overlay_buffer != NULL) {
		wlr_buffer_drop(&output->overlay_buffer->base);
		output->overlay_buffer = NULL;
	}
	output->overlay_buffer = orange_buffer_create(width, height);
	if (output->overlay_buffer == NULL) {
		wlr_log(WLR_ERROR, "failed to allocate overlay buffer");
	} else {
		output->overlay_scene_buffer = wlr_scene_buffer_create(
			output->server->overlay_tree,
			&output->overlay_buffer->base);
		if (output->overlay_scene_buffer == NULL) {
			wlr_log(WLR_ERROR, "failed to create overlay scene buffer");
			wlr_buffer_drop(&output->overlay_buffer->base);
			output->overlay_buffer = NULL;
		} else {
			output->overlay_scene_buffer->point_accepts_input = shell_buffer_accepts_input;
			wlr_scene_buffer_set_dest_size(output->overlay_scene_buffer, width, height);
			if (output->layout_output != NULL) {
				wlr_scene_node_set_position(&output->overlay_scene_buffer->node,
					output->layout_output->x,
					output->layout_output->y);
			}
		}
	}

	output->width = width;
	output->height = height;
	output->shell_dirty = true;
	output->overlay_dirty = true;
	output->overlay_bounds_valid = false;
	output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
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

	server_update_app_menu_model(output->server);
	server_update_dock_open(output->server);
	struct orange_shell_state state = {
		.system_menu_open = output->server->system_menu_open,
		.notification_center_open = output->server->notification_center_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_pointer_x = (int)cursor_x,
		.dock_pointer_y = (int)cursor_y,
		.dock_drag_index = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_index : -1,
		.dock_drag_insert_before = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_insert_before :
			(output->server->launcher_app_drag_active &&
				output->server->launcher_app_drag_moved ?
				output->server->launcher_app_drag_insert_before : -1),
		.dock_drag_x = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			(int)cursor_x : 0,
		.dock_drag_y = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			(int)cursor_y : 0,
		.dock_drag_remove = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_remove : false,
		.now = time(NULL),
		.assets = &output->server->assets,
		.config = &output->server->config,
		.status = output->server->status,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.volumes = output->server->volumes,
		.volume_count = output->server->volume_count,
		.desktop_volume_count = output->server->desktop_volume_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
		.launcher_open = output->server->launcher_open,
		.launcher_display_mode = output->server->launcher_display_mode,
		.launcher_position_set = output->server->launcher_position_set,
		.launcher_x = output->server->launcher_x,
		.launcher_y = output->server->launcher_y,
		.launcher_hot_app = output->server->launcher_hot_app,
		.launcher_app_count = output->server->launcher_app_count,
		.launcher_scroll = 0,
		.launcher_scroll_row = output->server->launcher_scroll_row,
		.launcher_current_mode = output->server->launcher_current_mode,
		.launcher_search_focus = output->server->launcher_search_focus,
		.launcher_app_drag_active = false,
		.launcher_app_drag_x = 0,
		.launcher_app_drag_y = 0,
		.launcher_app_drag_entry_index = -1,
		.launcher_category_count = output->server->launcher_category_count,
		.launcher_category_active = output->server->launcher_category_active,
		.dock_bounce_active = output->server->dock_bounce_active,
		.dock_bounce_launcher_idx = output->server->dock_bounce_launcher_idx,
		.dock_bounce_start_time = output->server->dock_bounce_start,
	};
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	memcpy(state.launcher_query, output->server->launcher_query,
		sizeof(state.launcher_query));
	memcpy(state.launcher_app_indices, output->server->launcher_app_indices,
		sizeof(state.launcher_app_indices));
	memcpy(state.launcher_categories, output->server->launcher_categories,
		sizeof(state.launcher_categories));
	server_active_app_label(output->server,
		state.active_app_label, sizeof(state.active_app_label));
	state.app_menu = output->server->app_menu;
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
	};
	orange_shell_draw_with_options(output->shell_buffer->pixels,
		output->width,
		output->height,
		output->shell_buffer->stride,
		&state,
		&options);

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, 0, 0, output->width, output->height);
	wlr_scene_buffer_set_buffer_with_damage(output->shell_scene_buffer,
		&output->shell_buffer->base,
		&damage);
	pixman_region32_fini(&damage);
	output->shell_dirty = false;
}

static void output_clear_overlay_buffer(struct orange_output *output) {
	if (output == NULL || output->overlay_buffer == NULL ||
			output->overlay_scene_buffer == NULL) {
		return;
	}
	memset(output->overlay_buffer->pixels, 0,
		(size_t)output->overlay_buffer->stride * (size_t)output->height);
	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, 0, 0,
		output->width, output->height);
	wlr_scene_buffer_set_buffer_with_damage(
		output->overlay_scene_buffer,
		&output->overlay_buffer->base, &damage);
	pixman_region32_fini(&damage);
	output->overlay_bounds_valid = false;
	output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
}

static bool output_rect_has_area(const struct orange_rect *rect) {
	return rect != NULL && rect->width > 0 && rect->height > 0;
}

static struct orange_rect output_union_rect(struct orange_rect a,
		struct orange_rect b) {
	if (!output_rect_has_area(&a)) {
		return b;
	}
	if (!output_rect_has_area(&b)) {
		return a;
	}
	int x1 = a.x < b.x ? a.x : b.x;
	int y1 = a.y < b.y ? a.y : b.y;
	int x2 = a.x + a.width;
	int y2 = a.y + a.height;
	int bx2 = b.x + b.width;
	int by2 = b.y + b.height;
	if (bx2 > x2) {
		x2 = bx2;
	}
	if (by2 > y2) {
		y2 = by2;
	}
	return (struct orange_rect){x1, y1, x2 - x1, y2 - y1};
}

static void output_clear_overlay_region(struct orange_output *output,
		const struct orange_rect *rect) {
	if (output == NULL || output->overlay_buffer == NULL ||
			!output_rect_has_area(rect)) {
		return;
	}
	int x = rect->x;
	int y = rect->y;
	int w = rect->width;
	int h = rect->height;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > output->width) {
		w = output->width - x;
	}
	if (y + h > output->height) {
		h = output->height - y;
	}
	if (w <= 0 || h <= 0) {
		return;
	}
	for (int row = y; row < y + h; row++) {
		memset((unsigned char *)output->overlay_buffer->pixels +
				(size_t)row * (size_t)output->overlay_buffer->stride +
				(size_t)x * sizeof(uint32_t),
			0, (size_t)w * sizeof(uint32_t));
	}
}

static void output_set_overlay_damage(struct orange_output *output,
		const struct orange_rect *rect) {
	if (output == NULL || output->overlay_scene_buffer == NULL ||
			!output_rect_has_area(rect)) {
		return;
	}
	pixman_region32_t damage;
	pixman_region32_init_rect(&damage,
		rect->x, rect->y, rect->width, rect->height);
	wlr_scene_buffer_set_buffer_with_damage(output->overlay_scene_buffer,
		&output->overlay_buffer->base,
		&damage);
	pixman_region32_fini(&damage);
}

static void output_redraw_overlay(struct orange_output *output) {
	if (!output_ensure_shell_buffer(output)) {
		return;
	}
	if (output->overlay_buffer == NULL) {
		return;
	}
	if (!output->overlay_dirty) {
		return;
	}

	struct orange_server *server = output->server;
	if (server->context_menu_kind == ORANGE_CONTEXT_MENU_NONE &&
			!server->launcher_open &&
			!server->system_menu_open &&
			!server->notification_center_open &&
			!server->dock_drag_active &&
			!server->launcher_app_drag_active) {
		if (output->overlay_bounds_valid) {
			output_clear_overlay_region(output, &output->overlay_bounds);
			output_set_overlay_damage(output, &output->overlay_bounds);
			output->overlay_bounds_valid = false;
			output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
		}
		output->overlay_dirty = false;
		return;
	}

	double cursor_x = server->cursor->x;
	double cursor_y = server->cursor->y;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);

	server_update_app_menu_model(output->server);
	server_update_dock_open(output->server);
	struct orange_shell_state state = {
		.system_menu_open = output->server->system_menu_open,
		.notification_center_open = output->server->notification_center_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_pointer_x = (int)cursor_x,
		.dock_pointer_y = (int)cursor_y,
		.dock_drag_index = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_index : -1,
		.dock_drag_insert_before = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_insert_before :
			(output->server->launcher_app_drag_active &&
				output->server->launcher_app_drag_moved ?
				output->server->launcher_app_drag_insert_before : -1),
		.dock_drag_x = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			(int)cursor_x : 0,
		.dock_drag_y = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			(int)cursor_y : 0,
		.dock_drag_remove = output->server->dock_drag_active &&
			output->server->dock_drag_moved ?
			output->server->dock_drag_remove : false,
		.now = time(NULL),
		.assets = &output->server->assets,
		.config = &output->server->config,
		.status = output->server->status,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.volumes = output->server->volumes,
		.volume_count = output->server->volume_count,
		.desktop_volume_count = output->server->desktop_volume_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
		.launcher_open = output->server->launcher_open,
		.launcher_display_mode = output->server->launcher_display_mode,
		.launcher_position_set = output->server->launcher_position_set,
		.launcher_x = output->server->launcher_x,
		.launcher_y = output->server->launcher_y,
		.launcher_hot_app = output->server->launcher_hot_app,
		.launcher_app_count = output->server->launcher_app_count,
		.launcher_scroll = 0,
		.launcher_scroll_row = output->server->launcher_scroll_row,
		.launcher_current_mode = output->server->launcher_current_mode,
		.launcher_search_focus = output->server->launcher_search_focus,
		.launcher_app_drag_active = output->server->launcher_app_drag_active &&
			output->server->launcher_app_drag_moved,
		.launcher_app_drag_x = output->server->launcher_app_drag_active ?
			(int)cursor_x : 0,
		.launcher_app_drag_y = output->server->launcher_app_drag_active ?
			(int)cursor_y : 0,
		.launcher_app_drag_entry_index =
			output->server->launcher_app_drag_entry_index,
		.launcher_category_count = output->server->launcher_category_count,
		.launcher_category_active = output->server->launcher_category_active,
	};
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	memcpy(state.launcher_query, output->server->launcher_query,
		sizeof(state.launcher_query));
	memcpy(state.launcher_app_indices, output->server->launcher_app_indices,
		sizeof(state.launcher_app_indices));
	memcpy(state.launcher_categories, output->server->launcher_categories,
		sizeof(state.launcher_categories));
	server_active_app_label(output->server,
		state.active_app_label, sizeof(state.active_app_label));
	state.app_menu = output->server->app_menu;

	struct orange_rect current_bounds;
	if (!orange_shell_overlay_bounds(output->width, output->height,
			&state, &current_bounds)) {
		if (output->overlay_bounds_valid) {
			output_clear_overlay_region(output, &output->overlay_bounds);
			output_set_overlay_damage(output, &output->overlay_bounds);
			output->overlay_bounds_valid = false;
			output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
		}
		output->overlay_dirty = false;
		return;
	}
	struct orange_rect damage_bounds = current_bounds;
	if (output->overlay_bounds_valid) {
		damage_bounds = output_union_rect(damage_bounds,
			output->overlay_bounds);
	}
	output_clear_overlay_region(output, &damage_bounds);
	orange_shell_draw_overlay_with_backdrop(output->overlay_buffer->pixels,
		output->width,
		output->height,
		output->overlay_buffer->stride,
		output->shell_buffer->pixels,
		output->shell_buffer->stride,
		&state);

	output_set_overlay_damage(output, &damage_bounds);
	output->overlay_bounds = current_bounds;
	output->overlay_bounds_valid = true;
	output->overlay_dirty = false;
}

static void focus_view(struct orange_view *view, struct wlr_surface *surface) {
	if (view == NULL || surface == NULL || !view->mapped) {
		return;
	}

	struct orange_server *server = view->server;
	if (view->minimized) {
		view->minimized = false;
		wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	}
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
	server_mark_shell_dirty(server);
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

static void constrain_view_position(
		struct orange_server *server,
		int *x, int *y, int width, int height) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		double oxd = 0, oyd = 0;
		wlr_output_layout_output_coords(server->output_layout,
			output->wlr_output, &oxd, &oyd);
		int ox = (int)oxd;
		int oy = (int)oyd;
		double s = output->wlr_output->scale;
		int out_w = (int)(output->width / s);
		int out_h = (int)(output->height / s);
		if (*x + width > ox &&
				*x < ox + out_w &&
				*y + height > oy &&
				*y < oy + out_h) {
			struct orange_shell_layout layout;
			compute_shell_layout_for_output(server, output, &layout);
			struct orange_rect work =
				orange_shell_layout_work_area(&layout);
			int work_left = ox + (int)(work.x / s);
			int work_top = oy + (int)(work.y / s);
			int work_right = ox + (int)((work.x + work.width) / s);
			int work_bottom = oy + (int)((work.y + work.height) / s);
			if (work_right < work_left) {
				work_right = work_left;
			}
			if (work_bottom < work_top) {
				work_bottom = work_top;
			}
			if (height >= work_bottom - work_top) {
				*y = work_top;
			} else if (*y < work_top) {
				*y = work_top;
			} else if (*y + height > work_bottom) {
				*y = work_bottom - height;
			}
			if (width >= work_right - work_left) {
				*x = work_left;
			} else if (*x < work_left) {
				*x = work_left;
			} else if (*x + width > work_right) {
				*x = work_right - width;
			}
			return;
		}
	}
}

static void process_cursor_motion(struct orange_server *server, uint32_t time_msec) {
	if (server->cursor_mode == ORANGE_CURSOR_MOVE && server->grabbed_view != NULL) {
		struct orange_view *view = server->grabbed_view;
		int x = (int)(server->cursor->x - server->grab_cursor_x);
		int y = (int)(server->cursor->y - server->grab_cursor_y);
		constrain_view_position(server, &x, &y,
			view->width, view->height);
		view->x = x;
		view->y = y;
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

		constrain_view_position(server, &x, &y, width, height);

		view->x = x;
		view->y = y;
		view->width = width;
		view->height = height;
		wlr_scene_node_set_position(&view->scene_tree->node, x, y);
		wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, width, height);
		return;
	}

	if (server->launcher_drag_active) {
		process_launcher_drag(server);
		return;
	}

	if (server->launcher_scroll_drag_active) {
		process_launcher_scroll_drag(server);
		return;
	}

	if (server->launcher_app_drag_active) {
		process_launcher_app_drag(server);
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

		if (view != NULL && server->xcursor_manager != NULL) {
			double cx = server->cursor->x;
			double cy = server->cursor->y;
			int l = view->x, r = view->x + view->width;
			int t = view->y, b = view->y + view->height;
			if (cx >= l && cx <= r && cy >= t && cy <= b) {
				uint32_t edges = 0;
				if (cx - l < 5) edges |= WLR_EDGE_LEFT;
				if (r - cx < 5) edges |= WLR_EDGE_RIGHT;
				if (cy - t < 5) edges |= WLR_EDGE_TOP;
				if (b - cy < 5) edges |= WLR_EDGE_BOTTOM;
				if (edges) {
					const char *name = "default";
					if (edges == (WLR_EDGE_LEFT)) name = "w-resize";
					else if (edges == (WLR_EDGE_RIGHT)) name = "e-resize";
					else if (edges == (WLR_EDGE_TOP)) name = "n-resize";
					else if (edges == (WLR_EDGE_BOTTOM)) name = "s-resize";
					else if (edges == (WLR_EDGE_LEFT | WLR_EDGE_TOP)) name = "nw-resize";
					else if (edges == (WLR_EDGE_RIGHT | WLR_EDGE_TOP)) name = "ne-resize";
					else if (edges == (WLR_EDGE_LEFT | WLR_EDGE_BOTTOM)) name = "sw-resize";
					else if (edges == (WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM)) name = "se-resize";
					wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, name);
				}
			}
		}
	} else {
		wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "default");
		wlr_seat_pointer_notify_clear_focus(server->seat);
	}

	/* Skip dock-hover layout computation when the cursor is over a client
	 * surface — the dock is behind the window and magnification is invisible. */
	if (surface == NULL) {
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
				bool vertical =
					layout.dock_position == ORANGE_DOCK_POSITION_LEFT ||
					layout.dock_position == ORANGE_DOCK_POSITION_RIGHT;
				int item_axis = vertical ?
					layout.dock_items[new_hot].height :
					layout.dock_items[new_hot].width;
				dock_redraw_threshold = item_axis / 4;
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

static bool add_named_modifier(
		struct wlr_keyboard *keyboard,
		struct wlr_keyboard_modifiers *modifiers,
		const char *name) {
	if (keyboard == NULL || keyboard->keymap == NULL ||
			modifiers == NULL || name == NULL) {
		return false;
	}
	xkb_mod_index_t index = xkb_keymap_mod_get_index(keyboard->keymap, name);
	if (index == XKB_MOD_INVALID || index >= sizeof(xkb_mod_mask_t) * 8) {
		return false;
	}
	modifiers->depressed |= ((xkb_mod_mask_t)1 << index);
	return true;
}

static bool send_focused_shortcut(
		struct orange_server *server,
		uint32_t keycode,
		bool ctrl,
		bool shift,
		bool alt) {
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped) {
		return false;
	}

	struct wlr_keyboard_modifiers saved = keyboard->modifiers;
	struct wlr_keyboard_modifiers next = saved;
	if (ctrl) {
		add_named_modifier(keyboard, &next, XKB_MOD_NAME_CTRL);
	}
	if (shift) {
		add_named_modifier(keyboard, &next, XKB_MOD_NAME_SHIFT);
	}
	if (alt) {
		add_named_modifier(keyboard, &next, XKB_MOD_NAME_ALT);
	}

	uint32_t now = monotonic_time_msec();
	wlr_seat_set_keyboard(server->seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(server->seat, &next);
	wlr_seat_keyboard_notify_key(server->seat, now, keycode,
		WL_KEYBOARD_KEY_STATE_PRESSED);
	wlr_seat_keyboard_notify_key(server->seat, now, keycode,
		WL_KEYBOARD_KEY_STATE_RELEASED);
	wlr_seat_keyboard_notify_modifiers(server->seat, &saved);
	return true;
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

static struct orange_output *find_output_for_point(
		struct orange_server *server, int x, int y) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		double oxd = 0, oyd = 0;
		wlr_output_layout_output_coords(server->output_layout,
			output->wlr_output, &oxd, &oyd);
		int ox = (int)oxd, oy = (int)oyd;
		if (x >= ox && x < ox + output->width &&
				y >= oy && y < oy + output->height) {
			return output;
		}
	}
	return NULL;
}

static void apply_maximize(struct orange_view *view, bool maximized) {
	struct orange_server *server = view->server;
	view->maximized = maximized;
	wlr_xdg_toplevel_set_maximized(view->xdg_surface->toplevel, maximized);
	if (!maximized) {
		return;
	}

	struct orange_output *output = find_output_for_point(server,
		view->x + view->width / 2, view->y + view->height / 2);
	if (output == NULL) {
		output = wl_container_of(server->outputs.next, output, link);
	}
	if (output == NULL) {
		return;
	}

	double s = output->wlr_output->scale;
	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);

	double oxd = 0, oyd = 0;
	wlr_output_layout_output_coords(server->output_layout,
		output->wlr_output, &oxd, &oyd);
	int ox = (int)oxd, oy = (int)oyd;

	struct orange_rect work = orange_shell_layout_work_area(&layout);
	int work_x = (int)(work.x / s);
	int work_y = (int)(work.y / s);
	int work_w = (int)(work.width / s);
	int work_h = (int)(work.height / s);

	int margin = scaled_i(8, s);
	view->x = ox + work_x + margin;
	view->y = oy + work_y + margin;
	view->width = work_w - margin * 2;
	view->height = work_h - margin * 2;
	if (view->width < 1) {
		view->width = 1;
	}
	if (view->height < 1) {
		view->height = 1;
	}
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
		view->width, view->height);
}

static void apply_fullscreen(struct orange_view *view, bool fullscreen) {
	struct orange_server *server = view->server;
	view->fullscreen = fullscreen;
	wlr_xdg_toplevel_set_fullscreen(view->xdg_surface->toplevel, fullscreen);
	if (!fullscreen) {
		return;
	}

	struct orange_output *output = find_output_for_point(server,
		view->x + view->width / 2, view->y + view->height / 2);
	if (output == NULL) {
		output = wl_container_of(server->outputs.next, output, link);
	}
	if (output == NULL) {
		return;
	}

	double s = output->wlr_output->scale;
	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);

	double oxd = 0, oyd = 0;
	wlr_output_layout_output_coords(server->output_layout,
		output->wlr_output, &oxd, &oyd);
	int ox = (int)oxd, oy = (int)oyd;

	struct orange_rect work = orange_shell_layout_work_area(&layout);

	view->x = ox + (int)(work.x / s);
	view->y = oy + (int)(work.y / s);
	view->width = (int)(work.width / s);
	view->height = (int)(work.height / s);
	if (view->width < 1) {
		view->width = 1;
	}
	if (view->height < 1) {
		view->height = 1;
	}
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
		view->width, view->height);
}

static void handle_shell_menu_action(struct orange_server *server, int index) {
	switch (index) {
	case 0:
		focus_view_for_app_id(server, ".About");
		break;
	case 1:
		if (!focus_view_for_app_id(server, "orange-settings") &&
				!focus_view_for_app_id(server, "settings")) {
			launch_command(orange_settings_command);
		}
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
	server_mark_overlay_dirty(server);
}

static void clear_context_menu(struct orange_server *server) {
	if (server->context_menu_kind != ORANGE_CONTEXT_MENU_NONE) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
		server->context_menu_cursor_x = 0;
		server->context_menu_cursor_y = 0;
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

static void cycle_dock_size(struct orange_server *server) {
	if (server->config.dock_scale < 0.93) {
		server->config.dock_scale = 1.00;
	} else if (server->config.dock_scale < 1.09) {
		server->config.dock_scale = 1.18;
	} else {
		server->config.dock_scale = 0.84;
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void cycle_dock_position(struct orange_server *server) {
	switch (server->config.dock_position) {
	case ORANGE_DOCK_POSITION_BOTTOM:
		server->config.dock_position = ORANGE_DOCK_POSITION_LEFT;
		break;
	case ORANGE_DOCK_POSITION_LEFT:
		server->config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
		break;
	case ORANGE_DOCK_POSITION_RIGHT:
	default:
		server->config.dock_position = ORANGE_DOCK_POSITION_BOTTOM;
		break;
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void cycle_minimize_effect(struct orange_server *server) {
	server->config.minimize_effect =
		server->config.minimize_effect == ORANGE_MINIMIZE_EFFECT_SCALE ?
		ORANGE_MINIMIZE_EFFECT_GENIE : ORANGE_MINIMIZE_EFFECT_SCALE;
	orange_config_save(&server->config, server->options->config_path);
}

static bool try_focused_app_dbus_action(
		struct orange_server *server,
		const char *action_name) {
	if (server == NULL || action_name == NULL || action_name[0] == '\0') {
		return false;
	}

	char active_label[ORANGE_APP_MENU_LABEL_MAX];
	server_active_app_label(server, active_label, sizeof(active_label));

	char stripped_id[128] = "";
	const char *app_id = focused_app_id(server);
	if (app_id != NULL) {
		strip_desktop_suffix(app_id, stripped_id, sizeof(stripped_id));
	}

	GError *error = NULL;
	GDBusConnection *connection = g_bus_get_sync(
		G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (connection == NULL) {
		return false;
	}

	const char *candidates[5] = {NULL};
	int count = 0;
	if (stripped_id[0] != '\0') {
		candidates[count++] = stripped_id;
	}
	if ((app_id != NULL && contains_case(app_id, "firefox")) ||
			contains_case(active_label, "firefox")) {
		candidates[count++] = "org.mozilla.firefox";
	}
	if ((app_id != NULL && contains_case(app_id, "nautilus")) ||
			contains_case(active_label, "files")) {
		candidates[count++] = "org.gnome.Nautilus";
	}
	if ((app_id != NULL && contains_case(app_id, "code")) ||
			contains_case(active_label, "code") ||
			(app_id != NULL && contains_case(app_id, "vscode")) ||
			contains_case(active_label, "vscode")) {
		candidates[count++] = "com.visualstudio.code";
	}

	static const char *paths[] = {
		"/org/gtk/Actions",
		"/org/gtk/actions",
		NULL,
	};
	for (int i = 0; i < count; i++) {
		for (int p = 0; paths[p] != NULL; p++) {
			GDBusActionGroup *group = g_dbus_action_group_get(
				connection, candidates[i], paths[p]);
			if (group == NULL) {
				continue;
			}
			GActionGroup *actions = G_ACTION_GROUP(group);
			if (g_action_group_has_action(actions, action_name)) {
				g_action_group_activate_action(
					actions, action_name, NULL);
				g_object_unref(group);
				g_object_unref(connection);
				return true;
			}
			g_object_unref(group);
		}
	}
	g_object_unref(connection);
	return false;
}

static bool activate_imported_app_menu_action(
		struct orange_server *server,
		enum orange_context_menu_kind kind,
		int item_index) {
	if (server == NULL || !server->app_menu.available ||
			!server->app_menu.native ||
			server->app_menu_bus_name[0] == '\0') {
		return false;
	}
	int tab = -1;
	switch (kind) {
	case ORANGE_CONTEXT_MENU_APP:
		tab = ORANGE_APP_MENU_TAB_APP;
		break;
	case ORANGE_CONTEXT_MENU_APP_FILE:
		tab = ORANGE_APP_MENU_TAB_FILE;
		break;
	case ORANGE_CONTEXT_MENU_APP_EDIT:
		tab = ORANGE_APP_MENU_TAB_EDIT;
		break;
	case ORANGE_CONTEXT_MENU_APP_VIEW:
		tab = ORANGE_APP_MENU_TAB_VIEW;
		break;
	case ORANGE_CONTEXT_MENU_APP_GO:
	case ORANGE_CONTEXT_MENU_APP_HISTORY:
		tab = ORANGE_APP_MENU_TAB_GO;
		break;
	case ORANGE_CONTEXT_MENU_APP_WINDOW:
	case ORANGE_CONTEXT_MENU_APP_BOOKMARKS:
		tab = ORANGE_APP_MENU_TAB_WINDOW;
		break;
	case ORANGE_CONTEXT_MENU_APP_TOOLS:
		tab = ORANGE_APP_MENU_TAB_TOOLS;
		break;
	case ORANGE_CONTEXT_MENU_APP_HELP:
		tab = ORANGE_APP_MENU_TAB_HELP;
		break;
	default:
		break;
	}
	if (tab < 0 || tab >= server->app_menu.tab_count ||
			item_index < 0 ||
			item_index >= server->app_menu.item_counts[tab]) {
		return false;
	}
	const char *action = server->app_menu.items[tab][item_index].action;
	if (action == NULL || action[0] == '\0') {
		return false;
	}

	GError *error = NULL;
	GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (connection == NULL) {
		return false;
	}
	GDBusActionGroup *group = g_dbus_action_group_get(connection,
		server->app_menu_bus_name,
		server->app_menu_action_path[0] != '\0' ?
			server->app_menu_action_path : "/org/gtk/Actions");
	g_object_unref(connection);
	if (group == NULL) {
		return false;
	}
	const char *name = action;
	const char *dot = strchr(action, '.');
	if (dot != NULL && dot[1] != '\0') {
		name = dot + 1;
	}
	GActionGroup *actions = G_ACTION_GROUP(group);
	bool activated = false;
	if (g_action_group_has_action(actions, name)) {
		g_action_group_activate_action(actions, name, NULL);
		activated = true;
	} else if (g_action_group_has_action(actions, action)) {
		g_action_group_activate_action(actions, action, NULL);
		activated = true;
	}
	g_object_unref(group);
	return activated;
}

static void handle_app_file_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_N, true, false, false);
		break;
	case 1:
		send_focused_shortcut(server, KEY_O, true, false, false);
		break;
	case 3:
		send_focused_shortcut(server, KEY_W, true, false, false);
		break;
	case 4:
		send_focused_shortcut(server, KEY_S, true, false, false);
		break;
	case 5:
		send_focused_shortcut(server, KEY_S, true, true, false);
		break;
	case 6:
		send_focused_shortcut(server, KEY_P, true, false, false);
		break;
	default:
		break;
	}
}

static void handle_app_edit_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_Z, true, false, false);
		break;
	case 1:
		send_focused_shortcut(server, KEY_Z, true, true, false);
		break;
	case 2:
		send_focused_shortcut(server, KEY_X, true, false, false);
		break;
	case 3:
		send_focused_shortcut(server, KEY_C, true, false, false);
		break;
	case 4:
		send_focused_shortcut(server, KEY_V, true, false, false);
		break;
	case 5:
		send_focused_shortcut(server, KEY_A, true, false, false);
		break;
	case 6:
		send_focused_shortcut(server, KEY_F, true, false, false);
		break;
	default:
		break;
	}
}

static void handle_app_view_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_EQUAL, true, false, false);
		break;
	case 1:
		send_focused_shortcut(server, KEY_MINUS, true, false, false);
		break;
	case 2:
		send_focused_shortcut(server, KEY_0, true, false, false);
		break;
	case 4:
		send_focused_shortcut(server, KEY_F11, false, false, false);
		break;
	default:
		break;
	}
}

static void handle_app_go_menu_action(
		struct orange_server *server,
		int item_index) {
	(void)server;
	switch (item_index) {
	case 0:
		launch_command("xdg-open \"$HOME\" || true");
		break;
	case 1:
		launch_command("xdg-open \"$HOME/Desktop\" || true");
		break;
	case 2:
		launch_command("xdg-open \"$HOME/Documents\" || true");
		break;
	case 3:
		launch_command("xdg-open \"$HOME/Downloads\" || true");
		break;
	case 4:
		launcher_open_overlay(server, true,
			ORANGE_LAUNCHER_DISPLAY_FULL);
		break;
	default:
		break;
	}
}

static void handle_app_window_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_M, true, false, false);
		break;
	case 1:
		if (server->focused_view != NULL) {
			apply_maximize(server->focused_view,
				!server->focused_view->maximized);
		}
		break;
	case 2:
		cycle_focus(server);
		break;
	case 3:
		if (server->focused_view != NULL) {
			wlr_scene_node_raise_to_top(
				&server->focused_view->scene_tree->node);
		}
		break;
	default:
		break;
	}
}

static void handle_app_history_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_LEFT, false, false, true);
		break;
	case 1:
		send_focused_shortcut(server, KEY_RIGHT, false, false, true);
		break;
	case 2:
		send_focused_shortcut(server, KEY_HOME, false, false, true);
		break;
	case 3:
		send_focused_shortcut(server, KEY_H, true, false, false);
		break;
	case 4:
		send_focused_shortcut(server, KEY_DELETE, true, true, false);
		break;
	default:
		break;
	}
}

static void handle_app_bookmarks_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_D, true, false, false);
		break;
	case 1:
	case 2:
		send_focused_shortcut(server, KEY_B, true, false, false);
		break;
	case 3:
		send_focused_shortcut(server, KEY_O, true, true, false);
		break;
	default:
		break;
	}
}

static void handle_app_tools_menu_action(
		struct orange_server *server,
		int item_index) {
	switch (item_index) {
	case 0:
		send_focused_shortcut(server, KEY_J, true, false, false);
		break;
	case 1:
		send_focused_shortcut(server, KEY_A, true, true, false);
		break;
	case 3:
		send_focused_shortcut(server, KEY_COMMA, true, false, false);
		break;
	default:
		break;
	}
}

static void handle_context_menu_action(struct orange_server *server, int item_index) {
	enum orange_context_menu_kind kind = server->context_menu_kind;
	int target = server->context_menu_index;
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;

	if (activate_imported_app_menu_action(server, kind, item_index)) {
		server_mark_shell_dirty(server);
		return;
	}

	if (kind == ORANGE_CONTEXT_MENU_APP) {
		switch (item_index) {
		case 0:
			if (!try_focused_app_dbus_action(server, "about") &&
					!try_focused_app_dbus_action(server, "help.about") &&
					!try_focused_app_dbus_action(server, "show-about") &&
					!try_focused_app_dbus_action(server, "help-about")) {
				char active_label[ORANGE_APP_MENU_LABEL_MAX];
				server_active_app_label(server, active_label,
					sizeof(active_label));
				char launch_cmd[512];
				if (strcasestr(active_label, "files") != NULL ||
						strcasestr(active_label, "nautilus") != NULL) {
					snprintf(launch_cmd, sizeof(launch_cmd),
						"dbus-send --session --dest=org.gnome.Nautilus "
						"--type=method_call --print-reply "
						"/org/gtk/Actions org.gtk.Actions.Activate "
						"string:about array:string: 2>/dev/null || "
						"dbus-send --session --dest=org.gnome.Nautilus "
						"--type=method_call --print-reply "
						"/org/gtk/actions org.gtk.Actions.Activate "
						"string:about array:string: 2>/dev/null || true");
				} else if (strcasestr(active_label, "firefox") != NULL) {
					snprintf(launch_cmd, sizeof(launch_cmd),
						"dbus-send --session --dest=org.mozilla.firefox "
						"--type=method_call --print-reply "
						"/org/gtk/Actions org.gtk.Actions.Activate "
						"string:about array:string: 2>/dev/null || "
						"dbus-send --session --dest=org.mozilla.firefox "
						"--type=method_call --print-reply "
						"/org/gtk/actions org.gtk.Actions.Activate "
						"string:about array:string: 2>/dev/null || true");
				} else {
					launch_cmd[0] = '\0';
				}
				if (launch_cmd[0] != '\0') {
					launch_command(launch_cmd);
				}
			}
			break;
		case 1:
			if (!try_focused_app_dbus_action(server, "preferences") &&
					!try_focused_app_dbus_action(server, "settings") &&
					!focus_view_for_app_id(server, "orange-settings") &&
					!focus_view_for_app_id(server, "settings")) {
				launch_command(orange_settings_command);
			}
			break;
		case 6:
			if (!try_focused_app_dbus_action(server, "quit") &&
					!try_focused_app_dbus_action(server, "app.quit")) {
				send_focused_shortcut(server, KEY_Q, true, false, false);
			}
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_APP_FILE) {
		handle_app_file_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_EDIT) {
		handle_app_edit_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_VIEW) {
		handle_app_view_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_GO) {
		handle_app_go_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_WINDOW) {
		handle_app_window_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_HISTORY) {
		handle_app_history_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_BOOKMARKS) {
		handle_app_bookmarks_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_TOOLS) {
		handle_app_tools_menu_action(server, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_APP_HELP) {
		if (item_index == 0) {
			launcher_open_overlay(server, true,
				ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
		} else if (item_index == 1) {
			send_focused_shortcut(server, KEY_F1, false, false, false);
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK) {
		int launcher_idx = dock_launcher_for_visible_index(server, target);
		switch (item_index) {
		case 0:
			launch_dock_launcher(server, launcher_idx);
			break;
		case 1:
			launch_command("xdg-open \"$HOME\" || true");
			break;
		case 2:
			if (orange_dock_config_remove_visible(&server->config, target)) {
				orange_config_save(&server->config,
					server->options->config_path);
				server_mark_shell_dirty(server);
			}
			break;
		case 4:
			launch_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR) {
		switch (item_index) {
		case 0:
			server->config.dock_magnification =
				!server->config.dock_magnification;
			orange_config_save(&server->config,
				server->options->config_path);
			break;
		case 1:
			cycle_dock_size(server);
			break;
		case 2:
			cycle_dock_position(server);
			break;
		case 3:
			cycle_minimize_effect(server);
			break;
		case 4:
			launch_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		switch (item_index) {
		case 0:
			launch_command(orange_settings_command);
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
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_VOLUME) {
		if (target >= 0 && target < server->desktop_volume_count) {
			const struct orange_volume_info *vol = &server->volumes[target];
			switch (item_index) {
			case 0: /* Open */
				if (vol->mount_path[0] != '\0') {
					char cmd[1024];
					snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" || true", vol->mount_path);
					launch_command(cmd);
				}
				break;
			case 1: /* Get Info */
				launch_command("nautilus --properties \"$HOME\" || true");
				break;
			case 2: /* Eject */
				if (vol->mount_path[0] != '\0') {
					char cmd[1024];
					snprintf(cmd, sizeof(cmd), "gio mount -u \"%s\" || true", vol->mount_path);
					launch_command(cmd);
				}
				break;
			default:
				break;
			}
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP) {
		switch (item_index) {
		case 0:
			launch_command("xdg-open \"$HOME/Desktop\" || true");
			break;
		case 6:
			launch_command(orange_settings_command);
			break;
		case 7:
			launch_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_WIFI) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_command(network_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_SOUND) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_command(sound_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_command(power_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS) {
		switch (item_index) {
		case 0:
			launch_command(network_settings_command);
			break;
		case 1:
			launch_command(bluetooth_settings_command);
			break;
		case 2:
			launch_command(orange_settings_command);
			break;
		case 3:
			launch_command(notification_settings_command);
			break;
		case 4:
			launch_command(sound_settings_command);
			break;
		case 5:
			launch_command(display_settings_command);
			break;
		case 6:
			launch_command(display_settings_command);
			break;
		case 7:
			launch_command(power_settings_command);
			break;
		case 8:
			launch_command(keyboard_settings_command);
			break;
		case 9:
			launch_command(orange_settings_command);
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
	struct orange_rect work = orange_shell_layout_work_area(&layout);
	int next_x = local_x - server->desktop_drag_offset_x;
	int next_y = local_y - server->desktop_drag_offset_y;
	int min_x = work.x;
	int min_y = work.y;
	int max_x = work.x + work.width - item.width;
	int max_y = work.y + work.height - item.height;
	if (max_x < min_x) {
		max_x = min_x;
	}
	if (max_y < min_y) {
		max_y = min_y;
	}
	if (next_x < min_x) {
		next_x = min_x;
	}
	if (next_x > max_x) {
		next_x = max_x;
	}
	if (next_y < min_y) {
		next_y = min_y;
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
	} else if (index >= 0 && index < server->desktop_volume_count) {
		const struct orange_volume_info *vol = &server->volumes[index];
		if (vol->mount_path[0] != '\0') {
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" || true", vol->mount_path);
			launch_command(cmd);
		}
	}
}

static void start_dock_drag(struct orange_server *server,
		const struct orange_shell_layout *layout, int index) {
	server->dock_drag_active = true;
	server->dock_drag_moved = false;
	server->dock_drag_index = index;
	server->dock_drag_start_x = (int)server->cursor->x;
	server->dock_drag_start_y = (int)server->cursor->y;
	server->dock_drag_insert_before = -1;
	server->dock_drag_remove = false;
	(void)layout;
}

static int dock_visible_trash_index(
		struct orange_server *server,
		const struct orange_shell_layout *layout) {
	if (server == NULL || layout == NULL) {
		return -1;
	}
	for (int i = 0; i < layout->dock_item_count; i++) {
		int launcher_idx = dock_launcher_for_visible_index(server, i);
		if (launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				strcmp(server->config.dock_apps[launcher_idx], "__trash__") == 0) {
			return i;
		}
	}
	return -1;
}

static bool dock_visible_index_is_removable(
		struct orange_server *server,
		int visible_index) {
	int launcher_idx = dock_launcher_for_visible_index(server, visible_index);
	return launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
		server->config.dock_apps[launcher_idx][0] != '\0' &&
		!orange_dock_app_is_permanent(server->config.dock_apps[launcher_idx]);
}

static bool shell_layout_dock_is_vertical(
		const struct orange_shell_layout *layout) {
	return layout != NULL &&
		(layout->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		 layout->dock_position == ORANGE_DOCK_POSITION_RIGHT);
}

static int dock_insert_position_for_point(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		int local_x,
		int local_y,
		int dragged_visible_index) {
	if (layout == NULL || layout->dock_item_count <= 0) {
		return 0;
	}
	int count = layout->dock_item_count;
	int trash = dock_visible_trash_index(server, layout);
	int app_limit = trash >= 0 ? trash : count;
	int insert = app_limit;
	for (int i = 0; i < app_limit; i++) {
		if (i == dragged_visible_index) {
			continue;
		}
		struct orange_rect r = layout->dock_items[i];
		int pointer_axis = shell_layout_dock_is_vertical(layout) ?
			local_y : local_x;
		int item_mid = shell_layout_dock_is_vertical(layout) ?
			r.y + r.height / 2 : r.x + r.width / 2;
		if (pointer_axis < item_mid) {
			insert = i;
			break;
		}
	}
	if (dragged_visible_index >= 0 &&
			dragged_visible_index < app_limit &&
			insert > dragged_visible_index) {
		insert--;
	}
	int remaining_limit = app_limit;
	if (dragged_visible_index >= 0 &&
			dragged_visible_index < app_limit) {
		remaining_limit--;
	}
	if (insert < 0) {
		insert = 0;
	}
	if (insert > remaining_limit) {
		insert = remaining_limit;
	}
	return insert;
}

static bool dock_remove_zone_contains(
		const struct orange_shell_layout *layout,
		int dragged_visible_index,
		int local_x,
		int local_y) {
	if (layout == NULL || dragged_visible_index < 0 ||
			dragged_visible_index >= layout->dock_item_count) {
		return false;
	}
	struct orange_rect item = layout->dock_items[dragged_visible_index];
	int margin_y = item.height / 3;
	int margin_x = item.width / 2;
	return local_y < layout->dock.y - margin_y ||
		local_y > layout->dock.y + layout->dock.height + margin_y ||
		local_x < layout->dock.x - margin_x ||
		local_x > layout->dock.x + layout->dock.width + margin_x;
}

static bool dock_drop_zone_contains(
		const struct orange_shell_layout *layout,
		int local_x,
		int local_y) {
	if (layout == NULL || layout->dock_item_count <= 0 ||
			layout->dock.width <= 0 || layout->dock.height <= 0) {
		return false;
	}
	struct orange_rect first = layout->dock_items[0];
	int pad_x = shell_layout_dock_is_vertical(layout) ?
		first.width / 3 : first.width / 2;
	int pad_top = first.height / 2;
	int pad_bottom = shell_layout_dock_is_vertical(layout) ?
		first.height / 2 : first.height / 3;
	return local_x >= layout->dock.x - pad_x &&
		local_x <= layout->dock.x + layout->dock.width + pad_x &&
		local_y >= layout->dock.y - pad_top &&
		local_y <= layout->dock.y + layout->dock.height + pad_bottom;
}

static void process_dock_drag(struct orange_server *server) {
	if (!server->dock_drag_active) {
		return;
	}

	int dx = (int)server->cursor->x - server->dock_drag_start_x;
	int dy = (int)server->cursor->y - server->dock_drag_start_y;
	bool was_moved = server->dock_drag_moved;
	if (abs(dx) > 3 || abs(dy) > 3) {
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
	bool remove = dock_visible_index_is_removable(server, dragged) &&
		dock_remove_zone_contains(&layout, dragged, local_x, local_y);
	int insert = remove ? -1 :
		dock_insert_position_for_point(server, &layout, local_x, local_y,
			dragged);
	bool shell_changed = !was_moved ||
		remove != server->dock_drag_remove ||
		insert != server->dock_drag_insert_before;
	server->dock_drag_remove = remove;
	server->dock_drag_insert_before = insert;
	if (shell_changed) {
		server_mark_shell_dirty(server);
	}
	server_mark_overlay_dirty(server);
}

static void finish_dock_drag(struct orange_server *server) {
	if (!server->dock_drag_active) {
		return;
	}
	int index = server->dock_drag_index;
	bool moved = server->dock_drag_moved;
	int insert = server->dock_drag_insert_before;
	bool remove = server->dock_drag_remove;
	server->dock_drag_active = false;
	server->dock_drag_moved = false;
	server->dock_drag_index = -1;
	server->dock_drag_insert_before = -1;
	server->dock_drag_remove = false;
	server_mark_overlay_dirty(server);
	if (moved) {
		server_mark_shell_dirty(server);
	}

	int dock_count = orange_dock_count(&server->config);
	if (moved && remove && index >= 0 && index < dock_count &&
			orange_dock_config_remove_visible(&server->config, index)) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (moved && index >= 0 && index < dock_count &&
			insert >= 0 &&
			orange_dock_config_reorder_visible(&server->config, index, insert)) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (!moved && index >= 0 && index < dock_count) {
		int launcher_idx = dock_launcher_for_visible_index(server, index);
		if (launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
				server->config.dock_apps[launcher_idx][0] != '\0' &&
				!orange_dock_app_is_permanent(
					server->config.dock_apps[launcher_idx]) &&
				server->config.animate_opening_applications) {
			server->dock_bounce_active = true;
			server->dock_bounce_start = monotonic_time_msec();
			server->dock_bounce_launcher_idx = launcher_idx;
		}
		launch_dock_launcher(server, launcher_idx);
		server_mark_shell_dirty(server);
	}
}

static void clamp_launcher_position(
		const struct orange_shell_layout *layout,
		int *x,
		int *y) {
	if (layout == NULL || x == NULL || y == NULL) {
		return;
	}
	int margin = 16;
	int max_x = layout->width - margin - layout->launcher_search_field.width;
	int max_y = layout->height - margin - layout->launcher_search_field.height;
	int min_y = layout->menu_bar.height + margin;
	if (max_x < margin) {
		max_x = margin;
	}
	if (max_y < min_y) {
		max_y = min_y;
	}
	if (*x < margin) {
		*x = margin;
	}
	if (*x > max_x) {
		*x = max_x;
	}
	if (*y < min_y) {
		*y = min_y;
	}
	if (*y > max_y) {
		*y = max_y;
	}
}

static void start_launcher_drag(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		int local_x,
		int local_y) {
	if (server == NULL || layout == NULL ||
			layout->launcher_search_field.width <= 0 ||
			layout->launcher_search_field.height <= 0) {
		return;
	}
	server->launcher_drag_active = true;
	server->launcher_drag_offset_x = local_x - layout->launcher_search_field.x;
	server->launcher_drag_offset_y = local_y - layout->launcher_search_field.y;
	server->launcher_position_set = true;
	server->launcher_x = layout->launcher_search_field.x;
	server->launcher_y = layout->launcher_search_field.y;
}

static void process_launcher_drag(struct orange_server *server) {
	if (server == NULL || !server->launcher_drag_active) {
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
	int next_x = local_x - server->launcher_drag_offset_x;
	int next_y = local_y - server->launcher_drag_offset_y;
	clamp_launcher_position(&layout, &next_x, &next_y);
	if (!server->launcher_position_set ||
			next_x != server->launcher_x ||
			next_y != server->launcher_y) {
		server->launcher_position_set = true;
		server->launcher_x = next_x;
		server->launcher_y = next_y;
		server_mark_overlay_dirty(server);
	}
}

static int launcher_scroll_row_for_thumb_y(
		const struct orange_shell_layout *layout,
		int thumb_y) {
	if (layout == NULL || layout->launcher_max_scroll <= 0 ||
			layout->launcher_scroll_track.height <= 0 ||
			layout->launcher_scroll_thumb.height <= 0) {
		return 0;
	}
	int travel = layout->launcher_scroll_track.height -
		layout->launcher_scroll_thumb.height;
	if (travel <= 0) {
		return 0;
	}
	int offset = thumb_y - layout->launcher_scroll_track.y;
	if (offset < 0) {
		offset = 0;
	}
	if (offset > travel) {
		offset = travel;
	}
	int row = (int)lrint((double)offset *
		(double)layout->launcher_max_scroll / (double)travel);
	if (row < 0) {
		row = 0;
	}
	if (row > layout->launcher_max_scroll) {
		row = layout->launcher_max_scroll;
	}
	return row;
}

static void set_launcher_scroll_row(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		int row) {
	if (server == NULL || layout == NULL) {
		return;
	}
	if (row < 0) {
		row = 0;
	}
	if (row > layout->launcher_max_scroll) {
		row = layout->launcher_max_scroll;
	}
	if (row != server->launcher_scroll_row) {
		server->launcher_scroll_row = row;
		server->launcher_hot_app = -1;
		server_mark_overlay_dirty(server);
	}
}

static void start_launcher_scroll_drag(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		int local_y) {
	if (server == NULL || layout == NULL ||
			layout->launcher_max_scroll <= 0 ||
			layout->launcher_scroll_track.width <= 0 ||
			layout->launcher_scroll_thumb.height <= 0) {
		return;
	}
	int offset_y = local_y - layout->launcher_scroll_thumb.y;
	if (offset_y < 0 || offset_y > layout->launcher_scroll_thumb.height) {
		offset_y = layout->launcher_scroll_thumb.height / 2;
		int row = launcher_scroll_row_for_thumb_y(layout, local_y - offset_y);
		set_launcher_scroll_row(server, layout, row);
	}
	server->launcher_scroll_drag_active = true;
	server->launcher_scroll_drag_offset_y = offset_y;
}

static void process_launcher_scroll_drag(struct orange_server *server) {
	if (server == NULL || !server->launcher_scroll_drag_active) {
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
	int thumb_y = local_y - server->launcher_scroll_drag_offset_y;
	int row = launcher_scroll_row_for_thumb_y(&layout, thumb_y);
	set_launcher_scroll_row(server, &layout, row);
}

static void start_launcher_app_drag(
		struct orange_server *server,
		int flat_index) {
	if (server == NULL || flat_index < 0 ||
			flat_index >= server->launcher_app_count) {
		return;
	}
	int entry_index = server->launcher_app_indices[flat_index];
	if (entry_index < 0 || entry_index >= (int)server->desktop_entry_count) {
		return;
	}
	server->launcher_app_drag_active = true;
	server->launcher_app_drag_moved = false;
	server->launcher_app_drag_flat_index = flat_index;
	server->launcher_app_drag_entry_index = entry_index;
	server->launcher_app_drag_start_x = (int)server->cursor->x;
	server->launcher_app_drag_start_y = (int)server->cursor->y;
	server->launcher_app_drag_insert_before = -1;
	server->launcher_search_focus = false;
}

static void process_launcher_app_drag(struct orange_server *server) {
	if (server == NULL || !server->launcher_app_drag_active) {
		return;
	}
	int dx = (int)server->cursor->x - server->launcher_app_drag_start_x;
	int dy = (int)server->cursor->y - server->launcher_app_drag_start_y;
	if (abs(dx) > 3 || abs(dy) > 3) {
		server->launcher_app_drag_moved = true;
	}
	if (!server->launcher_app_drag_moved) {
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	int insert = -1;
	if (output != NULL &&
			server->launcher_app_drag_entry_index >= 0 &&
			server->launcher_app_drag_entry_index < (int)server->desktop_entry_count) {
		struct orange_shell_layout layout;
		compute_shell_layout_for_output(server, output, &layout);
		const char *app_id =
			server->desktop_entries[server->launcher_app_drag_entry_index].id;
		if (!orange_dock_config_contains_app(&server->config, app_id) &&
				dock_drop_zone_contains(&layout, local_x, local_y)) {
			insert = dock_insert_position_for_point(server, &layout,
				local_x, local_y, -1);
		}
	}
	bool shell_changed =
		insert != server->launcher_app_drag_insert_before;
	server->launcher_app_drag_insert_before = insert;
	if (shell_changed) {
		server_mark_shell_dirty(server);
	}
	server_mark_overlay_dirty(server);
}

static void finish_launcher_app_drag(struct orange_server *server) {
	if (server == NULL || !server->launcher_app_drag_active) {
		return;
	}
	bool moved = server->launcher_app_drag_moved;
	int flat_index = server->launcher_app_drag_flat_index;
	int entry_index = server->launcher_app_drag_entry_index;
	int insert = server->launcher_app_drag_insert_before;
	server->launcher_app_drag_active = false;
	server->launcher_app_drag_moved = false;
	server->launcher_app_drag_flat_index = -1;
	server->launcher_app_drag_entry_index = -1;
	server->launcher_app_drag_insert_before = -1;
	server_mark_overlay_dirty(server);
	if (moved && insert >= 0) {
		server_mark_shell_dirty(server);
	}

	if (!moved) {
		launcher_launch_index(server, flat_index);
		return;
	}
	if (entry_index < 0 || entry_index >= (int)server->desktop_entry_count ||
			insert < 0) {
		return;
	}
	const char *app_id = server->desktop_entries[entry_index].id;
	if (orange_dock_config_insert_app(&server->config, app_id, insert)) {
		orange_config_save(&server->config, server->options->config_path);
		launcher_close_overlay(server);
		server_mark_shell_dirty(server);
	}
}

static void open_status_menu(
		struct orange_server *server,
		enum orange_context_menu_kind kind,
		int index,
		int local_x,
		int local_y) {
	server->context_menu_kind = kind;
	server->context_menu_index = index;
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	server->system_menu_open = false;
	server->notification_center_open = false;
}

static bool server_uses_firefox_menu_profile(struct orange_server *server) {
	char active_label[ORANGE_APP_MENU_LABEL_MAX];
	server_active_app_label(server, active_label, sizeof(active_label));
	const char *app_id = focused_app_id(server);
	return !server->app_menu.available &&
		(contains_case(app_id, "firefox") ||
		 contains_case(active_label, "firefox") ||
		 contains_case(app_id, "browser") ||
		 contains_case(active_label, "browser"));
}

static enum orange_context_menu_kind app_menu_kind_for_tab(
		struct orange_server *server,
		int tab) {
	bool firefox = server_uses_firefox_menu_profile(server);
	switch (tab) {
	case ORANGE_APP_MENU_TAB_APP:
		return ORANGE_CONTEXT_MENU_APP;
	case ORANGE_APP_MENU_TAB_FILE:
		return ORANGE_CONTEXT_MENU_APP_FILE;
	case ORANGE_APP_MENU_TAB_EDIT:
		return ORANGE_CONTEXT_MENU_APP_EDIT;
	case ORANGE_APP_MENU_TAB_VIEW:
		return ORANGE_CONTEXT_MENU_APP_VIEW;
	case ORANGE_APP_MENU_TAB_GO:
		return firefox ? ORANGE_CONTEXT_MENU_APP_HISTORY :
			ORANGE_CONTEXT_MENU_APP_GO;
	case ORANGE_APP_MENU_TAB_WINDOW:
		return firefox ? ORANGE_CONTEXT_MENU_APP_BOOKMARKS :
			ORANGE_CONTEXT_MENU_APP_WINDOW;
	case ORANGE_APP_MENU_TAB_TOOLS:
		return ORANGE_CONTEXT_MENU_APP_TOOLS;
	case ORANGE_APP_MENU_TAB_HELP:
		return ORANGE_CONTEXT_MENU_APP_HELP;
	default:
		return ORANGE_CONTEXT_MENU_APP;
	}
}

static void open_app_menu(
		struct orange_server *server,
		int tab,
		int local_x,
		int local_y) {
	server->context_menu_kind = app_menu_kind_for_tab(server, tab);
	server->context_menu_index =
		dock_launcher_for_view(server, server->focused_view);
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	server->system_menu_open = false;
	server->notification_center_open = false;
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
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = !server->system_menu_open;
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_SYSTEM_MENU_ITEM:
		server->notification_center_open = false;
		clear_context_menu(server);
		handle_shell_menu_action(server, hit.index);
		break;
	case ORANGE_HIT_APP_MENU:
		server->notification_center_open = false;
		server->system_menu_open = false;
		if (server->context_menu_kind == app_menu_kind_for_tab(server, hit.index)) {
			clear_context_menu(server);
		} else {
			open_app_menu(server, hit.index, local_x, local_y);
		}
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_STATUS_ITEM:
		clear_context_menu(server);
		server->system_menu_open = false;
		if (hit.index == ORANGE_STATUS_ITEM_CLOCK) {
			server->notification_center_open =
				!server->notification_center_open;
		} else {
			server->notification_center_open = false;
			switch (hit.index) {
			case ORANGE_STATUS_ITEM_WIFI:
				open_status_menu(server,
					ORANGE_CONTEXT_MENU_STATUS_WIFI,
					hit.index, local_x, local_y);
				break;
			case ORANGE_STATUS_ITEM_SOUND:
				open_status_menu(server,
					ORANGE_CONTEXT_MENU_STATUS_SOUND,
					hit.index, local_x, local_y);
				break;
			case ORANGE_STATUS_ITEM_BATTERY:
				open_status_menu(server,
					ORANGE_CONTEXT_MENU_STATUS_BATTERY,
					hit.index, local_x, local_y);
				break;
			case ORANGE_STATUS_ITEM_SEARCH:
				launcher_open_overlay(server, true,
					ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
				break;
			case ORANGE_STATUS_ITEM_CONTROL_CENTER:
				open_status_menu(server,
					ORANGE_CONTEXT_MENU_STATUS,
					-1, local_x, local_y);
				break;
			default:
				break;
			}
		}
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_STATUS_AREA:
		break;
	case ORANGE_HIT_NOTIFICATION_CENTER:
		clear_context_menu(server);
		server->system_menu_open = false;
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_NOTIFICATION_CENTER_EDIT:
		clear_context_menu(server);
		server->system_menu_open = false;
		server->notification_center_open = false;
		launch_command(orange_settings_command);
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_DOCK_ITEM:
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		start_dock_drag(server, &layout, hit.index);
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_DOCK_SEPARATOR:
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_WIDGET:
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_DESKTOP_ITEM:
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		start_desktop_drag(server, output, &layout, hit.index, local_x, local_y);
		server_mark_shell_dirty(server);
		break;
	case ORANGE_HIT_LAUNCHER_SEARCH:
		server->launcher_search_focus = true;
		start_launcher_drag(server, &layout, local_x, local_y);
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_LAUNCHER_MODE:
		if (hit.index >= 0 && hit.index < ORANGE_LAUNCHER_MODE_COUNT) {
			if (server->launcher_display_mode ==
					ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
				if (hit.index != ORANGE_LAUNCHER_MODE_APPS) {
					server_mark_overlay_dirty(server);
					break;
				}
				server->launcher_display_mode = ORANGE_LAUNCHER_DISPLAY_FULL;
				server->launcher_position_set = true;
				server->launcher_x = layout.launcher_search_field.x;
				server->launcher_y = layout.launcher_search_field.y;
			}
			server->launcher_current_mode = hit.index;
			server->launcher_scroll_row = 0;
			server->launcher_hot_app = -1;
			server->launcher_search_focus = false;
			launcher_refresh_filter(server);
			server_mark_overlay_dirty(server);
		}
		break;
	case ORANGE_HIT_LAUNCHER_CATEGORY:
		if (hit.index >= 0 && hit.index < server->launcher_category_count) {
			server->launcher_category_active = hit.index;
			server->launcher_scroll_row = 0;
			server->launcher_hot_app = -1;
			server_mark_overlay_dirty(server);
		}
		break;
	case ORANGE_HIT_LAUNCHER_SCROLLBAR:
		start_launcher_scroll_drag(server, &layout, local_y);
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_LAUNCHER_APP:
		start_launcher_app_drag(server, hit.index);
		break;
	case ORANGE_HIT_LAUNCHER_BACKGROUND:
		/* Clicking the dimmed backdrop closes the overlay, like macOS. */
		launcher_close_overlay(server);
		break;
	case ORANGE_HIT_DESKTOP:
	case ORANGE_HIT_NONE:
		clear_context_menu(server);
		if (server->notification_center_open) {
			server->notification_center_open = false;
		}
		if (server->system_menu_open) {
			server->system_menu_open = false;
		}
		server_mark_overlay_dirty(server);
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
	server->notification_center_open = false;
	if (server->launcher_open) {
		launcher_close_overlay(server);
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
		return;
	}
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	if (hit.kind == ORANGE_HIT_DOCK_SEPARATOR) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_SEPARATOR;
		server->context_menu_index = -1;
	} else if (hit.kind == ORANGE_HIT_DOCK_ITEM) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_STATUS_ITEM) {
		switch (hit.index) {
		case ORANGE_STATUS_ITEM_WIFI:
			open_status_menu(server, ORANGE_CONTEXT_MENU_STATUS_WIFI,
				hit.index, local_x, local_y);
			break;
		case ORANGE_STATUS_ITEM_SOUND:
			open_status_menu(server, ORANGE_CONTEXT_MENU_STATUS_SOUND,
				hit.index, local_x, local_y);
			break;
		case ORANGE_STATUS_ITEM_BATTERY:
			open_status_menu(server, ORANGE_CONTEXT_MENU_STATUS_BATTERY,
				hit.index, local_x, local_y);
			break;
		case ORANGE_STATUS_ITEM_SEARCH:
			launcher_open_overlay(server, true,
				ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
			server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
			server->context_menu_index = -1;
			break;
		case ORANGE_STATUS_ITEM_CONTROL_CENTER:
			open_status_menu(server, ORANGE_CONTEXT_MENU_STATUS,
				-1, local_x, local_y);
			break;
		case ORANGE_STATUS_ITEM_CLOCK:
			server->notification_center_open = true;
			server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
			server->context_menu_index = -1;
			break;
		default:
			server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
			server->context_menu_index = -1;
			break;
		}
	} else if (hit.kind == ORANGE_HIT_APP_MENU) {
		open_app_menu(server, hit.index, local_x, local_y);
	} else if (hit.kind == ORANGE_HIT_WIDGET) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_WIDGET;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP_ITEM) {
		/* Desktop items are volumes (macOS behavior) */
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_VOLUME;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP;
		server->context_menu_index = -1;
	} else {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
	}
	server_mark_overlay_dirty(server);
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

static void clear_overlay_buffers(struct orange_server *server) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output_clear_overlay_buffer(output);
	}
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
	if (event->state == WLR_BUTTON_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_app_drag_active) {
		finish_launcher_app_drag(server);
		return;
	}
	if (event->state == WLR_BUTTON_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_drag_active) {
		server->launcher_drag_active = false;
		return;
	}
	if (event->state == WLR_BUTTON_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_scroll_drag_active) {
		server->launcher_scroll_drag_active = false;
		return;
	}

	if (event->state == WLR_BUTTON_PRESSED && event->button == BTN_LEFT) {
		bool overlay_active = server->context_menu_kind != ORANGE_CONTEXT_MENU_NONE ||
			server->launcher_open || server->system_menu_open ||
			server->notification_center_open;
		if (overlay_active) {
			int local_x = 0, local_y = 0;
			struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
			if (output != NULL) {
				struct orange_shell_layout layout;
				compute_shell_layout_for_output(server, output, &layout);
				struct orange_shell_hit hit = orange_shell_hit_test(&layout, local_x, local_y);
				switch (hit.kind) {
				case ORANGE_HIT_CONTEXT_MENU_ITEM:
				case ORANGE_HIT_SYSTEM_MENU_ITEM:
				case ORANGE_HIT_LAUNCHER_SEARCH:
				case ORANGE_HIT_LAUNCHER_MODE:
				case ORANGE_HIT_LAUNCHER_APP:
				case ORANGE_HIT_LAUNCHER_CATEGORY:
				case ORANGE_HIT_LAUNCHER_SCROLLBAR:
				case ORANGE_HIT_LAUNCHER_BACKGROUND:
				case ORANGE_HIT_NOTIFICATION_CENTER:
				case ORANGE_HIT_NOTIFICATION_CENTER_EDIT:
					handle_shell_click(server, (int)server->cursor->x, (int)server->cursor->y);
					return;
				default:
					clear_context_menu(server);
					server->system_menu_open = false;
					server->notification_center_open = false;
					if (server->launcher_open) {
						launcher_close_overlay(server);
					}
					clear_overlay_buffers(server);
					server_mark_overlay_dirty(server);
					break;
				}
			}
		}
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
	if (server->launcher_open &&
			event->orientation == WLR_AXIS_ORIENTATION_VERTICAL) {
		int local_x = 0;
		int local_y = 0;
		struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
		if (output != NULL) {
			struct orange_shell_layout layout;
			compute_shell_layout_for_output(server, output, &layout);
			if (event->delta_discrete == 0 && event->delta == 0.0) {
				return;
			}
			int step = event->delta_discrete != 0 ?
				(event->delta_discrete > 0 ? 1 : -1) :
				(event->delta > 0.0 ? 1 : -1);
			int next = server->launcher_scroll_row + step;
			if (next < 0) {
				next = 0;
			}
			if (next > layout.launcher_max_scroll) {
				next = layout.launcher_max_scroll;
			}
			set_launcher_scroll_row(server, &layout, next);
		}
		return;
	}
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
		launcher_toggle_overlay(server, true,
			ORANGE_LAUNCHER_DISPLAY_FULL);
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
		if (server->launcher_open) {
			launcher_close_overlay(server);
			return true;
		}
		if (server->system_menu_open) {
			server->system_menu_open = false;
			server_mark_overlay_dirty(server);
			return true;
		}
		if (server->notification_center_open) {
			server->notification_center_open = false;
			server_mark_overlay_dirty(server);
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

	/* The launcher overlay is modal: it consumes all key presses so the user
	 * can type a search query and navigate results without the focused client
	 * receiving the keystrokes. Super+Space toggles it closed, Escape closes,
	 * arrows/enter navigate, and printable keys feed the search field. */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && server->launcher_open) {
		bool consumed = false;
		bool mode_switch = false;
		for (int i = 0; i < nsyms; i++) {
			xkb_keysym_t sym = syms[i];
			if (sym == XKB_KEY_space && logo) {
				launcher_close_overlay(server);
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Escape) {
				launcher_close_overlay(server);
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
				int target = server->launcher_hot_app >= 0 ?
					server->launcher_hot_app : 0;
				if (server->launcher_app_count > 0) {
					launcher_launch_index(server, target);
				}
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_BackSpace) {
				int qlen = (int)strlen(server->launcher_query);
				if (qlen > 0) {
					server->launcher_query[qlen - 1] = '\0';
					server->launcher_hot_app = -1;
					launcher_refresh_filter(server);
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				break;
			}
			/* Cmd+1/2/3/4 for browse mode switching (Tahoe Spotlight) */
			if (logo && sym >= XKB_KEY_1 && sym <= XKB_KEY_4) {
				int new_mode = sym - XKB_KEY_1;
				if (new_mode >= 0 && new_mode < ORANGE_LAUNCHER_MODE_COUNT) {
					if (server->launcher_display_mode ==
							ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY &&
							new_mode != ORANGE_LAUNCHER_MODE_APPS) {
						consumed = true;
						mode_switch = true;
						break;
					}
					if (server->launcher_display_mode ==
							ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
						server->launcher_display_mode =
							ORANGE_LAUNCHER_DISPLAY_FULL;
					}
					server->launcher_current_mode = new_mode;
					server->launcher_scroll_row = 0;
					server->launcher_hot_app = -1;
					launcher_refresh_filter(server);
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				mode_switch = true;
				break;
			}
			if (sym == XKB_KEY_Left && !logo) {
				if (server->launcher_hot_app > 0) {
					server->launcher_hot_app--;
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Right && !logo) {
				if (server->launcher_hot_app + 1 <
						server->launcher_app_count) {
					server->launcher_hot_app++;
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Up && !logo) {
				if (server->launcher_hot_app < 0 && server->launcher_app_count > 0) {
					server->launcher_hot_app = 0;
				} else if (server->launcher_hot_app >= ORANGE_LAUNCHER_COLS) {
					server->launcher_hot_app -= ORANGE_LAUNCHER_COLS;
					int hot_row = server->launcher_hot_app / ORANGE_LAUNCHER_COLS;
					if (hot_row < server->launcher_scroll_row) {
						server->launcher_scroll_row = hot_row;
					}
					server_mark_overlay_dirty(server);
				} else if (server->launcher_scroll_row > 0) {
					server->launcher_scroll_row--;
					int offset = server->launcher_hot_app >= 0 ?
						server->launcher_hot_app % ORANGE_LAUNCHER_COLS : 0;
					server->launcher_hot_app = server->launcher_scroll_row *
						ORANGE_LAUNCHER_COLS + offset;
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Down && !logo) {
				if (server->launcher_hot_app < 0 && server->launcher_app_count > 0) {
					server->launcher_hot_app = 0;
				} else if (server->launcher_hot_app + ORANGE_LAUNCHER_COLS <
						server->launcher_app_count) {
					server->launcher_hot_app += ORANGE_LAUNCHER_COLS;
					int hot_row = server->launcher_hot_app / ORANGE_LAUNCHER_COLS;
					int last_visible_row = server->launcher_scroll_row +
						ORANGE_LAUNCHER_ROWS - 1;
					if (hot_row > last_visible_row) {
						server->launcher_scroll_row++;
					}
					server_mark_overlay_dirty(server);
				} else if (server->launcher_hot_app >= 0) {
					int max_row = (server->launcher_app_count +
						ORANGE_LAUNCHER_COLS - 1) / ORANGE_LAUNCHER_COLS - 1;
					int last_visible_row = server->launcher_scroll_row +
						ORANGE_LAUNCHER_ROWS - 1;
					if (last_visible_row < max_row) {
						server->launcher_scroll_row++;
					}
					server_mark_overlay_dirty(server);
				}
				consumed = true;
				break;
			}
		}
		if (!consumed && !logo && !mode_switch) {
			uint32_t cp = xkb_state_key_get_utf32(
				keyboard->keyboard->xkb_state, keycode);
			if (cp >= 0x20 && cp != 0x7f) {
				int qlen = (int)strlen(server->launcher_query);
				if (qlen + 1 < ORANGE_LAUNCHER_QUERY_MAX) {
					char buf[5];
					int n = 0;
					if (cp < 0x80) {
						buf[n++] = (char)cp;
					} else if (cp < 0x800) {
						buf[n++] = (char)(0xC0 | (cp >> 6));
						buf[n++] = (char)(0x80 | (cp & 0x3F));
					} else if (cp < 0x10000) {
						buf[n++] = (char)(0xE0 | (cp >> 12));
						buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
						buf[n++] = (char)(0x80 | (cp & 0x3F));
					} else {
						buf[n++] = (char)(0xF0 | (cp >> 18));
						buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
						buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
						buf[n++] = (char)(0x80 | (cp & 0x3F));
					}
					buf[n] = '\0';
					if (qlen + n < ORANGE_LAUNCHER_QUERY_MAX) {
						memcpy(server->launcher_query + qlen, buf, n + 1);
						server->launcher_hot_app = -1;
						launcher_refresh_filter(server);
						server_mark_overlay_dirty(server);
					}
				}
				consumed = true;
			}
		}
		if (consumed) {
			return;
		}
	}

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

static void set_view_minimized(struct orange_view *view, bool minimized) {
	if (view == NULL || !view->mapped) {
		return;
	}
	view->minimized = minimized;
	wlr_scene_node_set_enabled(&view->scene_tree->node, !minimized);
	if (minimized) {
		wlr_xdg_toplevel_set_activated(view->xdg_surface->toplevel, false);
		if (view->server->focused_view == view) {
			view->server->focused_view = NULL;
			wlr_seat_keyboard_notify_clear_focus(view->server->seat);
		}
		if (view->server->grabbed_view == view) {
			view->server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
			view->server->grabbed_view = NULL;
		}
	}
	wlr_xdg_surface_schedule_configure(view->xdg_surface);
	server_mark_shell_dirty(view->server);
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

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(
		view->server, &local_x, &local_y);
	(void)local_x;
	(void)local_y;
	if (output == NULL && !wl_list_empty(&view->server->outputs)) {
		output = wl_container_of(view->server->outputs.next, output, link);
	}
	if (output != NULL) {
		struct orange_shell_layout layout;
		compute_shell_layout_for_output(view->server, output, &layout);
		struct orange_rect work = orange_shell_layout_work_area(&layout);
		double s = output->wlr_output->scale;
		double oxd = 0, oyd = 0;
		wlr_output_layout_output_coords(view->server->output_layout,
			output->wlr_output, &oxd, &oyd);
		int work_left = (int)oxd + (int)(work.x / s);
		int work_top = (int)oyd + (int)(work.y / s);
		int work_right = (int)oxd + (int)((work.x + work.width) / s);
		int work_bottom = (int)oyd + (int)((work.y + work.height) / s);
		view->x = work_left + (work_right - work_left - view->width) / 2;
		view->y = work_top + 30;
		constrain_view_position(view->server, &view->x, &view->y,
			view->width, view->height);
		if (view->y + view->height > work_bottom) {
			view->y = work_top;
		}
	} else {
		struct wlr_box layout_box;
		wlr_output_layout_get_box(view->server->output_layout, NULL,
			&layout_box);
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
	}

	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	view->mapped = true;
	view->minimized = false;
	/* A newly mapped app dismisses the launcher overlay: the launch action is
	 * complete, so the grid/Spotlight view should give way to the app. */
	if (view->server->launcher_open) {
		launcher_close_overlay(view->server);
	}
	focus_view(view, view->xdg_surface->surface);
	server_mark_shell_dirty(view->server);
}

static void handle_view_unmap(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, unmap);
	(void)data;
	view->mapped = false;
	view->minimized = false;
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
	wl_list_remove(&view->request_minimize.link);
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

static void handle_view_request_minimize(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, request_minimize);
	(void)data;
	set_view_minimized(view, view->xdg_surface->toplevel->requested.minimized);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_popup *popup = xdg_surface->popup;

		struct wlr_scene_tree *parent_tree = NULL;
		if (popup->parent != NULL && popup->parent->data != NULL) {
			struct orange_view *pv = popup->parent->data;
			if (pv->scene_tree != NULL) {
				parent_tree = pv->scene_tree;
			}
		}

		struct wlr_scene_tree *scene_tree = wlr_scene_xdg_surface_create(
			parent_tree ? parent_tree : server->window_tree, xdg_surface);
		if (scene_tree == NULL) {
			return;
		}

		xdg_surface->surface->data = scene_tree;
		return;
	}

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
	view->request_minimize.notify = handle_view_request_minimize;
	wl_signal_add(&xdg_surface->toplevel->events.request_minimize,
		&view->request_minimize);

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
	output_redraw_overlay(output);
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
	struct orange_server *server = output->server;
	if (server->dock_bounce_active) {
		uint32_t now = monotonic_time_msec();
		uint32_t elapsed = now - server->dock_bounce_start;
		if (elapsed >= ORANGE_DOCK_BOUNCE_DURATION_MS) {
			server->dock_bounce_active = false;
		} else {
			output->shell_dirty = true;
		}
	}
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

static bool volumes_changed_since(struct orange_server *server,
		int prev_count, int prev_desktop_count,
		const struct orange_volume_info prev[ORANGE_DESKTOP_VOLUME_MAX]) {
	if (prev_count != server->volume_count ||
			prev_desktop_count != server->desktop_volume_count) {
		return true;
	}
	return memcmp(prev, server->volumes,
		sizeof(server->volumes[0]) * prev_count) != 0;
}

static int handle_clock_timer(void *data) {
	struct orange_server *server = data;
	if (!server->desktop_drag_active) {
		if (server->desktop_entry_poll_ticks <= 0) {
			server_load_config(server, false);
			server_reload_desktop_entries(server);
			server->desktop_entry_poll_ticks = DESKTOP_ENTRY_RELOAD_TICKS;
		} else {
			server->desktop_entry_poll_ticks--;
		}
		bool need_redraw = false;
		if (server->volume_poll_ticks <= 0) {
			int prev_vc = server->volume_count;
			int prev_dc = server->desktop_volume_count;
			struct orange_volume_info prev[ORANGE_DESKTOP_VOLUME_MAX];
			if (prev_vc > 0) {
				memcpy(prev, server->volumes, sizeof(prev[0]) * prev_vc);
			}
			update_volumes(server);
			need_redraw = volumes_changed_since(
				server, prev_vc, prev_dc, prev);
			server->volume_poll_ticks = VOLUME_RELOAD_TICKS;
		} else {
			server->volume_poll_ticks--;
		}
		bool status_changed = false;
		if (server->status_poll_ticks <= 0) {
			status_changed = server_update_status(server);
			server->status_poll_ticks = 15;
		} else {
			server->status_poll_ticks--;
		}
		if (need_redraw || status_changed) {
			server_mark_shell_dirty(server);
		}
	} else {
		bool status_changed = false;
		if (server->status_poll_ticks <= 0) {
			status_changed = server_update_status(server);
			server->status_poll_ticks = 15;
		} else {
			server->status_poll_ticks--;
		}
		if (status_changed) {
			server_mark_shell_dirty(server);
		}
	}
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
	server->desktop_entry_poll_ticks = 0;
	server->volume_poll_ticks = 0;
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;
	server->desktop_drag_index = -1;
	server->volume_count = 0;
	server->desktop_volume_count = 0;
	server->volume_timer = NULL;
	server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
	wl_list_init(&server->outputs);
	wl_list_init(&server->views);
	wl_list_init(&server->keyboards);
	orange_config_set_defaults(&server->config);
	server_load_config(server, false);
	status_set_defaults(&server->status);
	server_update_status(server);
	server_reload_desktop_entries(server);
	update_volumes(server);
	orange_assets_init(&server->assets);
	orange_assets_load(&server->assets, options->asset_root,
		server->config.icon_theme);
	orange_menubar_warm_assets(&server->assets);
	server_preload_app_icons(server);

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
	server->overlay_tree = wlr_scene_tree_create(&server->scene->tree);
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
