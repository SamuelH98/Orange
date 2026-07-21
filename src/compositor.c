#include "orange/buffer.h"
#include "orange/compositor.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/desktop.h"
#include "orange/menubar.h"
#include "orange/session_services.h"
#include "orange/shell.h"
#include "orange/util.h"

#include <cairo/cairo.h>
#include <gio/gio.h>
#include <assert.h>
#include <dirent.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <linux/input-event-codes.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#include <wlr/render/wlr_texture.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
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
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/xwayland/server.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "xdg-shell-protocol.h"

#define DESKTOP_ENTRY_RELOAD_TICKS 5
#define VOLUME_RELOAD_TICKS 3
#define DESKTOP_FILE_RELOAD_TICKS 3
#define TRASH_RELOAD_TICKS 1
#define DESKTOP_DOUBLE_CLICK_MS 500
#define CURSOR_LOADING_TIMEOUT_MS 15000
#define MINIMIZE_ANIMATION_DURATION_MS 280
#define WORKSPACE_ANIMATION_DURATION_MS 170
#define GENIE_ANIMATION_STRIPS 64
#define GENIE_ANIMATION_BOUND_SAMPLES 18
#define ORANGE_ATSPI_SCAN_NODE_MAX 20
#define ORANGE_APP_MENU_BUS_NAME_MAX 128
#define ORANGE_APP_MENU_OBJECT_PATH_MAX 256
#define ORANGE_WORKSPACE_MAX 36
#define ORANGE_EDGE_TILE_MARGIN 24
#define ORANGE_HOT_CORNER_SIZE 10

enum orange_cursor_mode {
	ORANGE_CURSOR_PASSTHROUGH,
	ORANGE_CURSOR_MOVE,
	ORANGE_CURSOR_RESIZE,
};

enum orange_edge_tile_mode {
	ORANGE_EDGE_TILE_NONE,
	ORANGE_EDGE_TILE_LEFT,
	ORANGE_EDGE_TILE_RIGHT,
	ORANGE_EDGE_TILE_TOP,
};

struct orange_server;

struct orange_atspi_action_ref {
	char bus_name[ORANGE_APP_MENU_BUS_NAME_MAX];
	char object_path[ORANGE_APP_MENU_OBJECT_PATH_MAX];
	int action_index;
};

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
	struct orange_buffer *backdrop_buffer;
	struct wl_listener frame;
	struct wl_listener present;
	struct wl_listener destroy;
	int width;
	int height;
	bool shell_dirty;
	bool dock_dirty;
	struct orange_rect dock_dirty_bounds;
	bool overlay_dirty;
	bool overlay_bounds_valid;
	struct orange_rect overlay_bounds;
	bool commit_pending;
	bool commit_failed;
	int current_workspace;
};

struct orange_view {
	struct wl_list link;
	struct orange_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener toplevel_destroy;
	struct wl_listener commit;
	struct wl_listener set_title;
	struct wl_listener set_app_id;
	struct wl_event_source *initial_configure_idle;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener xwayland_associate;
	struct wl_listener xwayland_dissociate;
	int dock_launcher_index;
	int x;
	int y;
	int width;
	int height;
	int workspace;
	bool mapped;
	bool minimized;
	struct orange_buffer *minimized_snapshot;
	bool maximized;
	bool fullscreen;
	bool fullscreen_restore_valid;
	int fullscreen_restore_x;
	int fullscreen_restore_y;
	int fullscreen_restore_width;
	int fullscreen_restore_height;
};

struct orange_minimize_animation {
	bool active;
	bool restoring;
	bool focus_on_complete;
	enum orange_minimize_effect effect;
	enum orange_dock_position dock_position;
	uint32_t start_ms;
	uint32_t duration_ms;
	struct orange_output *output;
	struct orange_view *view;
	struct orange_buffer *snapshot;
	struct orange_rect from_rect;
	struct orange_rect to_rect;
};

struct orange_workspace_animation {
	bool active;
	uint32_t start_ms;
	uint32_t duration_ms;
	int from_workspace;
	int to_workspace;
	int direction;
	int span;
	struct orange_output *output;
};

struct orange_popup {
	struct orange_server *server;
	struct wlr_xdg_popup *popup;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_scene_tree *scene_tree;
	struct orange_view *owner_view;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_event_source *initial_configure_idle;
};

struct orange_keyboard {
	struct wl_list link;
	struct orange_server *server;
	struct wlr_keyboard *keyboard;
	struct wl_listener key;
	struct wl_listener modifiers;
	struct wl_listener destroy;
	xkb_mod_index_t mod_ctrl;
	xkb_mod_index_t mod_shift;
	xkb_mod_index_t mod_alt;
};

struct orange_decoration {
	struct wl_listener request_mode;
	struct wl_listener destroy;
	struct wl_listener map;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
};

struct orange_server {
	const struct orange_options *options;
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_compositor *wlr_compositor;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_tree *shell_tree;
	struct wlr_scene_tree *window_tree;
	struct wlr_scene_tree *overlay_tree;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_output_layout *output_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_viewporter *viewporter;
	struct wlr_fractional_scale_manager_v1 *fractional_scale_manager;
	struct wlr_xwayland *xwayland;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct orange_assets assets;
	struct orange_config config;
	bool interface_settings_applied;
	struct orange_desktop_entry desktop_entries[512];
	size_t desktop_entry_count;

	struct wl_list outputs;
	struct wl_list views;
	struct wl_list keyboards;

	struct wl_listener new_output;
	struct wl_listener new_input;
	struct wl_listener new_xdg_surface;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
	struct wl_listener new_xdg_decoration;
	struct wl_listener new_xwayland_surface;
	struct wl_listener xwayland_ready;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_event_source *clock_timer;
	struct wl_event_source *glib_timer;

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
	struct orange_launcher_result launcher_results[ORANGE_LAUNCHER_RESULT_MAX];
	int launcher_result_count;
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
	struct orange_atspi_action_ref atspi_actions[ORANGE_APP_MENU_ITEM_MAX];
	int atspi_action_count;
	const char *app_menu_cached_app_id;
	int64_t app_menu_cached_pid;
	uint32_t app_menu_next_probe_ms;
	uint32_t app_menu_retry_until_ms;
	bool app_menu_tried_atspi;
	char app_menu_bus_name[ORANGE_APP_MENU_BUS_NAME_MAX];
	char app_menu_action_path[ORANGE_APP_MENU_OBJECT_PATH_MAX];
	int status_poll_ticks;
	int desktop_entry_poll_ticks;
	int volume_poll_ticks;
	int desktop_file_poll_ticks;
	int trash_poll_ticks;
	bool trash_full;
	int hot_dock_index;
	int last_dock_pointer_x;
	int last_dock_pointer_y;
	bool dock_auto_hide_revealed;
	bool dock_auto_hide_animating;
	bool dock_auto_hide_target_revealed;
	uint32_t dock_auto_hide_anim_start;
	double dock_auto_hide_anim_start_progress;
	double dock_auto_hide_progress;
	bool dock_open[ORANGE_DOCK_MAX];
	int dock_temporary_count;
	char dock_temporary_app_ids[ORANGE_DOCK_MAX][128];
	bool dock_temporary_open[ORANGE_DOCK_MAX];
	struct orange_minimize_animation minimize_animation;
	struct orange_workspace_animation workspace_animation;
	bool dock_state_dirty;
	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
	int open_with_file_index;
	int open_with_app_count;
	bool open_with_submenu_open;
	bool dock_options_submenu_open;
	bool dock_minimize_submenu_open;
	bool dock_position_submenu_open;
	bool context_menu_alt_pressed;
	bool context_menu_dock_temporary;
	char context_menu_app_id[256];
	char open_with_app_ids[ORANGE_OPEN_WITH_APP_MAX][128];
	char open_with_app_labels[ORANGE_OPEN_WITH_APP_MAX][ORANGE_APP_MENU_LABEL_MAX];
	char open_with_app_icons[ORANGE_OPEN_WITH_APP_MAX][ORANGE_ASSET_ICON_NAME_MAX];
	bool desktop_drag_active;
	bool desktop_drag_moved;
	int desktop_drag_index;
	int desktop_drag_offset_x;
	int desktop_drag_offset_y;
	int desktop_drag_start_x;
	int desktop_drag_start_y;
	int desktop_drag_initial_x[ORANGE_DESKTOP_MAX];
	int desktop_drag_initial_y[ORANGE_DESKTOP_MAX];
	int desktop_last_click_index;
	uint32_t desktop_last_click_time_ms;
	struct orange_desktop_rename_state desktop_rename;
	bool desktop_select_active;
	bool desktop_select_moved;
	int desktop_select_start_x;
	int desktop_select_start_y;
	int desktop_select_x;
	int desktop_select_y;
	bool desktop_selected[ORANGE_DESKTOP_MAX];

	struct orange_volume_info volumes[ORANGE_DESKTOP_VOLUME_MAX];
	int volume_count;
	int desktop_volume_count;
	struct wl_event_source *volume_timer;

	struct orange_file_info desktop_files[ORANGE_DESKTOP_FILE_MAX];
	int desktop_file_count;
	struct wl_event_source *desktop_file_timer;

	bool dock_drag_active;
	bool dock_drag_moved;
	int dock_drag_index;
	bool dock_drag_temporary;
	char dock_drag_app_id[128];
	int dock_drag_start_x;
	int dock_drag_start_y;
	int dock_drag_insert_before;
	bool dock_drag_remove;
	bool dock_resize_active;
	bool dock_resize_moved;
	enum orange_dock_position dock_resize_position;
	int dock_resize_start_x;
	int dock_resize_start_y;
	double dock_resize_start_scale;
	bool once_committed;
	uint32_t seat_caps;
	bool dock_bounce_active;
	uint32_t dock_bounce_start;
	int dock_bounce_launcher_idx;
	bool cursor_loading_active;
	int cursor_loading_launcher_idx;
	uint32_t cursor_loading_start_ms;
	GSettings *gnome_interface_settings;
	GSettings *gnome_mutter_settings;
	GSettings *gnome_wm_settings;
	GSettings *gnome_app_switcher_settings;
	GSettings *gnome_window_switcher_settings;
	int interface_poll_ticks;
	int multitasking_poll_ticks;
	GSettings *background_settings;
	int background_poll_ticks;
	bool hot_corners_enabled;
	bool hot_corner_armed;
	bool edge_tiling_enabled;
	bool animations_enabled;
	bool dynamic_workspaces_enabled;
	bool workspaces_only_on_primary;
	bool app_switcher_current_workspace_only;
	bool window_switcher_current_workspace_only;
	int workspace_count;
	int current_workspace;

	bool app_switcher_active;
	int app_switcher_index;
	int app_switcher_count;
	struct orange_view *app_switcher_views[128];

	struct orange_session_services session_services;
};

static void orange_listener_init(struct wl_listener *listener) {
	wl_list_init(&listener->link);
}

static void orange_listener_remove(struct wl_listener *listener) {
	if (listener->link.next == NULL || listener->link.prev == NULL ||
			wl_list_empty(&listener->link)) {
		return;
	}
	wl_list_remove(&listener->link);
	wl_list_init(&listener->link);
}

static bool view_is_xwayland(struct orange_view *view) {
	return view != NULL && view->xwayland_surface != NULL;
}

static struct wlr_surface *view_get_wlr_surface(struct orange_view *view) {
	if (view == NULL) {
		return NULL;
	}
	if (view_is_xwayland(view)) {
		return view->xwayland_surface->surface;
	}
	if (view->xdg_surface != NULL) {
		return view->xdg_surface->surface;
	}
	return NULL;
}

static const char *view_get_title(struct orange_view *view) {
	if (view == NULL) {
		return NULL;
	}
	if (view_is_xwayland(view)) {
		return view->xwayland_surface->title;
	}
	if (view->xdg_surface != NULL && view->xdg_surface->toplevel != NULL) {
		return view->xdg_surface->toplevel->title;
	}
	return NULL;
}

static const char *view_get_app_id(struct orange_view *view) {
	if (view == NULL) {
		return NULL;
	}
	if (view_is_xwayland(view)) {
		return view->xwayland_surface->class;
	}
	if (view->xdg_surface != NULL && view->xdg_surface->toplevel != NULL) {
		return view->xdg_surface->toplevel->app_id;
	}
	return NULL;
}

static void server_init_listener_links(struct orange_server *server) {
	orange_listener_init(&server->new_output);
	orange_listener_init(&server->new_input);
	orange_listener_init(&server->new_xdg_surface);
	orange_listener_init(&server->new_xdg_toplevel);
	orange_listener_init(&server->new_xdg_popup);
	orange_listener_init(&server->request_set_cursor);
	orange_listener_init(&server->request_set_selection);
	orange_listener_init(&server->request_set_primary_selection);
	orange_listener_init(&server->new_xdg_decoration);
	orange_listener_init(&server->cursor_motion);
	orange_listener_init(&server->cursor_motion_absolute);
	orange_listener_init(&server->cursor_button);
	orange_listener_init(&server->cursor_axis);
	orange_listener_init(&server->cursor_frame);
}

static void server_remove_wlroots_listeners(struct orange_server *server) {
	orange_listener_remove(&server->new_xdg_decoration);
	orange_listener_remove(&server->new_xdg_surface);
	orange_listener_remove(&server->new_xdg_toplevel);
	orange_listener_remove(&server->new_xdg_popup);
	orange_listener_remove(&server->request_set_cursor);
	orange_listener_remove(&server->request_set_selection);
	orange_listener_remove(&server->request_set_primary_selection);
	orange_listener_remove(&server->cursor_motion);
	orange_listener_remove(&server->cursor_motion_absolute);
	orange_listener_remove(&server->cursor_button);
	orange_listener_remove(&server->cursor_axis);
	orange_listener_remove(&server->cursor_frame);
	orange_listener_remove(&server->new_output);
	orange_listener_remove(&server->new_input);
}

static void server_mark_shell_dirty(struct orange_server *server);
static void server_mark_overlay_dirty(struct orange_server *server);
static void server_mark_dock_visual_dirty(struct orange_server *server);
static void server_mark_dock_dirty(struct orange_server *server);
static void server_refresh_dock_state(struct orange_server *server);
static void server_apply_cursor_config(struct orange_server *server);
static void server_reload_cursor_scales(struct orange_server *server);
static void process_desktop_drag(struct orange_server *server);
static void process_desktop_selection(struct orange_server *server);
static struct orange_rect normalized_rect_from_points(
	int x1, int y1, int x2, int y2);
static void start_dock_drag(struct orange_server *server,
	const struct orange_shell_layout *layout, int index);
static void process_dock_drag(struct orange_server *server);
static void finish_dock_drag(struct orange_server *server);
static const char *dock_resize_cursor_name(enum orange_dock_position position);
static void start_dock_resize(struct orange_server *server,
	const struct orange_shell_layout *layout);
static void process_dock_resize(struct orange_server *server);
static void finish_dock_resize(struct orange_server *server);
static void handle_new_xwayland_surface(struct wl_listener *listener, void *data);
static void handle_xwayland_ready(struct wl_listener *listener, void *data);
static void process_launcher_drag(struct orange_server *server);
static void process_launcher_scroll_drag(struct orange_server *server);
static void process_launcher_app_drag(struct orange_server *server);
static void finish_launcher_app_drag(struct orange_server *server);
static void set_view_minimized(struct orange_view *view, bool minimized);
static void set_view_minimized_with_focus(
	struct orange_view *view,
	bool minimized,
	bool focus_on_restore);
static bool view_configure_activated(
	struct orange_view *view,
	bool activated);
static void focus_view(struct orange_view *view, struct wlr_surface *surface);
static void focus_desktop_files(struct orange_server *server);
static bool view_is_visible_on_current_workspace(
	const struct orange_view *view);
static void server_apply_workspace_visibility(struct orange_server *server);
static void server_switch_workspace(struct orange_server *server, int workspace);
static void server_finish_workspace_animation(struct orange_server *server);
static bool server_update_workspace_animation(struct orange_server *server);
static bool output_area_in_layout(
	struct orange_server *server,
	struct orange_output *output,
	struct orange_rect *rect);
static bool has_active_xdg_popup_grab(struct orange_server *server);
static int server_minimized_dock_count(
	const struct orange_server *server,
	const struct orange_view *pending_view);
static void draw_minimized_dock_snapshots(
	struct orange_output *output,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	struct orange_buffer *buffer,
	const struct orange_rect *clip);

static char orange_client_wayland_display[128];
static char orange_server_xwayland_display[128];
static bool orange_client_launch_private_dbus;

#define ORANGE_APP_MENU_DISCOVERY_RETRY_MS 1000
#define ORANGE_APP_MENU_DISCOVERY_WINDOW_MS 5000

#define ORANGE_GNOME_SETTINGS_DESKTOP "GNOME:Unity:ubuntu"
#define ORANGE_GNOME_APP_ENV \
	"env -u GTK_THEME -u GTK_ICON_THEME " \
	"NO_AT_BRIDGE=1 GSK_RENDERER=cairo " \
	"XDG_CURRENT_DESKTOP=" ORANGE_GNOME_SETTINGS_DESKTOP " " \
	"XDG_SESSION_DESKTOP=gnome " \
	"DESKTOP_SESSION=gnome " \
	"GNOME_DESKTOP_SESSION_ID=this-is-deprecated "
#define ORANGE_GNOME_SETTINGS_ENV \
	ORANGE_GNOME_APP_ENV
#define ORANGE_GNOME_SETTINGS_FAST_ENV \
	ORANGE_GNOME_SETTINGS_ENV

static const char *orange_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center || "
	ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center applications || "
	ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center multitasking || "
	ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center display || "
	ORANGE_GNOME_SETTINGS_FAST_ENV "gnome-control-center system; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings; "
	"elif command -v xfce4-settings-manager >/dev/null 2>&1; then "
	"xfce4-settings-manager; "
	"elif command -v mate-control-center >/dev/null 2>&1; then "
	"mate-control-center; "
	"fi; true";

static const char *applications_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center applications || "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_componentchooser || systemsettings; "
	"fi; true";

static const char *orange_trash_command =
	"if command -v nautilus >/dev/null 2>&1; then "
	ORANGE_GNOME_APP_ENV "nautilus trash:///; "
	"elif command -v gio >/dev/null 2>&1; then "
	"gio open trash:///; "
	"else "
	"xdg-open trash:///; "
	"fi; true";

static const char *orange_about_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center system about || "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center system || "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center -s About; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_about-distro || systemsettings kcm_info; "
	"else "
	"printf 'Orange\\n'; "
	"fi; true";

static const char *background_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center background; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_wallpaper; "
	"fi; true";

static const char *network_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center wifi || "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center network; "
	"elif command -v nm-connection-editor >/dev/null 2>&1; then "
	"nm-connection-editor; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_networkmanagement; "
	"fi; true";

static const char *sound_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center sound; "
	"elif command -v pavucontrol >/dev/null 2>&1; then "
	"pavucontrol; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings kcm_pulseaudio; "
	"fi; true";

static const char *power_settings_command =
	"if command -v gnome-control-center >/dev/null 2>&1; then "
	ORANGE_GNOME_SETTINGS_ENV "gnome-control-center power; "
	"elif command -v xfce4-power-manager-settings >/dev/null 2>&1; then "
	"xfce4-power-manager-settings; "
	"elif command -v mate-power-preferences >/dev/null 2>&1; then "
	"mate-power-preferences; "
	"elif command -v systemsettings >/dev/null 2>&1; then "
	"systemsettings powerdevilprofilesconfig; "
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

static void unescape_mount_token(
		const char *src,
		char *dst,
		size_t dst_size) {
	size_t out = 0;
	for (size_t i = 0; src != NULL && src[i] != '\0' && out + 1 < dst_size; i++) {
		if (src[i] == '\\' &&
				src[i + 1] >= '0' && src[i + 1] <= '7' &&
				src[i + 2] >= '0' && src[i + 2] <= '7' &&
				src[i + 3] >= '0' && src[i + 3] <= '7') {
			int value = (src[i + 1] - '0') * 64 +
				(src[i + 2] - '0') * 8 +
				(src[i + 3] - '0');
			dst[out++] = (char)value;
			i += 3;
		} else {
			dst[out++] = src[i];
		}
	}
	dst[out] = '\0';
}

static void volume_label_for_mount(
		const char *device,
		const char *mount_point,
		char *label,
		size_t label_size) {
	if (label_size == 0) {
		return;
	}
	label[0] = '\0';
	if (strcmp(mount_point, "/") == 0) {
		snprintf(label, label_size, "%s", "System");
		return;
	}
	if (strcmp(mount_point, "/home") == 0) {
		snprintf(label, label_size, "%s", "Home");
		return;
	}
	const char *base = strrchr(device, '/');
	base = base != NULL ? base + 1 : device;
	if (base != NULL && base[0] != '\0') {
		snprintf(label, label_size, "%s", base);
	} else if (mount_point != NULL && mount_point[0] != '\0') {
		snprintf(label, label_size, "%s", mount_point);
	} else {
		snprintf(label, label_size, "%s", "Volume");
	}
}

static void update_volumes(struct orange_server *server, bool include_gio) {
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
				char device_token[256], mount_token[256], fstype[64];
				if (sscanf(buf, "%255s %255s %63s",
						device_token, mount_token, fstype) != 3) {
					continue;
				}
				char device[256];
				char mount_point[256];
				unescape_mount_token(device_token, device, sizeof(device));
				unescape_mount_token(mount_token, mount_point,
					sizeof(mount_point));
				if (strncmp(device, "/dev/", 5) != 0) {
					continue;
				}
				if (strcmp(mount_point, "/") == 0) {
					char label[256];
					volume_label_for_mount(device, mount_point,
						label, sizeof(label));
					add_volume(volumes, &count, &desktop_count,
						label, "/", "drive-harddisk",
						false, true);
				} else if (strcmp(mount_point, "/home") == 0) {
					char label[256];
					volume_label_for_mount(device, mount_point,
						label, sizeof(label));
					add_volume(volumes, &count, &desktop_count,
						label, "/home", "drive-harddisk",
						false, true);
				}
			}
			fclose(mounts_file);
		}
	}

	if (!include_gio) {
		server->desktop_volume_count = desktop_count;
		server->volume_count = count;
		memcpy(server->volumes, volumes, sizeof(volumes[0]) * count);
		return;
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

static bool file_is_image_by_extension(const char *name) {
	if (name == NULL) {
		return false;
	}
	const char *dot = strrchr(name, '.');
	if (dot == NULL || dot == name) {
		return false;
	}
	dot++;
	if (strcasecmp(dot, "png") == 0 || strcasecmp(dot, "jpg") == 0 ||
			strcasecmp(dot, "jpeg") == 0 || strcasecmp(dot, "gif") == 0 ||
			strcasecmp(dot, "bmp") == 0 || strcasecmp(dot, "svg") == 0 ||
			strcasecmp(dot, "webp") == 0) {
		return true;
	}
	return false;
}

static const char *file_icon_by_extension(const char *name) {
	if (name == NULL) return "text-x-generic";
	const char *dot = strrchr(name, '.');
	if (dot == NULL || dot == name) {
		return "text-x-generic";
	}
	dot++;
	if (file_is_image_by_extension(name)) {
		return "image-x-generic";
	}
	if (strcasecmp(dot, "pdf") == 0) {
		return "application-pdf";
	}
	if (strcasecmp(dot, "txt") == 0 || strcasecmp(dot, "md") == 0 ||
			strcasecmp(dot, "rst") == 0) {
		return "text-x-generic";
	}
	if (strcasecmp(dot, "mp3") == 0 || strcasecmp(dot, "wav") == 0 ||
			strcasecmp(dot, "flac") == 0 || strcasecmp(dot, "ogg") == 0 ||
			strcasecmp(dot, "m4a") == 0) {
		return "audio-x-generic";
	}
	if (strcasecmp(dot, "mp4") == 0 || strcasecmp(dot, "avi") == 0 ||
			strcasecmp(dot, "mkv") == 0 || strcasecmp(dot, "mov") == 0 ||
			strcasecmp(dot, "webm") == 0) {
		return "video-x-generic";
	}
	if (strcasecmp(dot, "zip") == 0 || strcasecmp(dot, "tar") == 0 ||
			strcasecmp(dot, "gz") == 0 || strcasecmp(dot, "rar") == 0 ||
			strcasecmp(dot, "7z") == 0) {
		return "package-x-generic";
	}
	if (strcasecmp(dot, "doc") == 0 || strcasecmp(dot, "docx") == 0) {
		return "application-msword";
	}
	if (strcasecmp(dot, "xls") == 0 || strcasecmp(dot, "xlsx") == 0) {
		return "application-vnd.ms-excel";
	}
	if (strcasecmp(dot, "html") == 0 || strcasecmp(dot, "htm") == 0) {
		return "text-html";
	}
	return "text-x-generic";
}

static void update_desktop_files(struct orange_server *server) {
	server->desktop_file_count = 0;
	server->launcher_filter_cache_key[0] = '\0';
	const char *home = getenv("HOME");
	if (home == NULL || home[0] == '\0') {
		return;
	}
	char desktop_path[1024];
	if (strlen(home) + strlen("/Desktop") >= sizeof(desktop_path)) {
		return;
	}
	snprintf(desktop_path, sizeof(desktop_path), "%s/Desktop", home);

	DIR *dir = opendir(desktop_path);
	if (dir == NULL) {
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL &&
			server->desktop_file_count < ORANGE_DESKTOP_FILE_MAX) {
		if (entry->d_name[0] == '.') {
			continue;
		}
		size_t len = strlen(entry->d_name);
		if (len == 0) {
			continue;
		}
		/* Skip .desktop files — they belong in the launcher */
		if (len > 8 && strcmp(entry->d_name + len - 8, ".desktop") == 0) {
			continue;
		}
		int idx = server->desktop_file_count;
		snprintf(server->desktop_files[idx].name,
			sizeof(server->desktop_files[idx].name), "%s", entry->d_name);
		size_t desktop_len = strlen(desktop_path);
		if (desktop_len + 1 + len >=
				sizeof(server->desktop_files[idx].path)) {
			continue;
		}
		memcpy(server->desktop_files[idx].path, desktop_path, desktop_len);
		server->desktop_files[idx].path[desktop_len] = '/';
		memcpy(server->desktop_files[idx].path + desktop_len + 1,
			entry->d_name, len + 1);

		struct stat st;
		if (stat(server->desktop_files[idx].path, &st) == 0) {
			server->desktop_files[idx].is_directory = S_ISDIR(st.st_mode);
		} else {
			server->desktop_files[idx].is_directory = false;
		}

		if (server->desktop_files[idx].is_directory) {
			server->desktop_files[idx].is_image = false;
			snprintf(server->desktop_files[idx].icon_name,
				sizeof(server->desktop_files[idx].icon_name), "%s", "folder");
		} else {
			server->desktop_files[idx].is_image =
				file_is_image_by_extension(entry->d_name);
			const char *icon = file_icon_by_extension(entry->d_name);
			snprintf(server->desktop_files[idx].icon_name,
				sizeof(server->desktop_files[idx].icon_name), "%s", icon);
		}
		server->desktop_file_count++;
	}
	closedir(dir);
}

static bool trash_files_path(char *out, size_t out_size) {
	if (out == NULL || out_size == 0) {
		return false;
	}
	const char *data_home = getenv("XDG_DATA_HOME");
	int n;
	if (data_home != NULL && data_home[0] != '\0') {
		n = snprintf(out, out_size, "%s/Trash/files", data_home);
	} else {
		const char *home = getenv("HOME");
		if (home == NULL || home[0] == '\0') {
			out[0] = '\0';
			return false;
		}
		n = snprintf(out, out_size, "%s/.local/share/Trash/files", home);
	}
	return n > 0 && n < (int)out_size;
}

static bool trash_directory_has_entries(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return false;
	}
	DIR *dir = opendir(path);
	if (dir == NULL) {
		return false;
	}
	bool has_entries = false;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		has_entries = true;
		break;
	}
	closedir(dir);
	return has_entries;
}

static bool server_update_trash_state(struct orange_server *server) {
	if (server == NULL) {
		return false;
	}
	char path[4096];
	bool full = trash_files_path(path, sizeof(path)) &&
		trash_directory_has_entries(path);
	bool changed = server->trash_full != full;
	server->trash_full = full;
	return changed;
}

static bool server_poll_trash_state(struct orange_server *server) {
	if (server == NULL) {
		return false;
	}
	if (server->trash_poll_ticks > 0) {
		server->trash_poll_ticks--;
		return false;
	}
	bool changed = server_update_trash_state(server);
	server->trash_poll_ticks = TRASH_RELOAD_TICKS;
	return changed;
}

static bool server_update_status(struct orange_server *server) {
	struct orange_status_state next;
	status_set_defaults(&next);
	update_network_status(&next);
	update_battery_status(&next);

	bool changed = memcmp(&server->status, &next, sizeof(next)) != 0;
	bool indicator_changed =
		orange_session_services_refresh_status_notifier_items(
			&server->session_services);
	server->status = next;
	return changed || indicator_changed;
}

static int server_copy_status_notifier_items(
		struct orange_server *server,
		struct orange_status_notifier_item *items,
		int capacity) {
	if (server == NULL) {
		return 0;
	}
	return orange_session_services_copy_status_notifier_items(
		&server->session_services, items, capacity);
}

static void server_populate_status_notifier_items(
		struct orange_server *server,
		struct orange_shell_state *state) {
	if (state == NULL) {
		return;
	}
	state->status_notifier_item_count =
		server_copy_status_notifier_items(server,
			state->status_notifier_items,
			ORANGE_STATUS_NOTIFIER_ITEM_MAX);
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

static void set_env_default(const char *name, const char *value) {
	const char *current = getenv(name);
	if ((current == NULL || current[0] == '\0') &&
			value != NULL && value[0] != '\0') {
		setenv(name, value, true);
	}
}

static void clear_legacy_appmenu_environment(void) {
	unsetenv("GTK_MODULES");
}

static void apply_client_toolkit_environment(bool force_wayland_backends) {
	clear_legacy_appmenu_environment();
	if (force_wayland_backends) {
		setenv("GDK_BACKEND", "wayland,x11", true);   // fallback list, not exclusive
		setenv("QT_QPA_PLATFORM", "wayland;xcb", true);
		setenv("SDL_VIDEODRIVER", "wayland,x11", true);
		setenv("CLUTTER_BACKEND", "wayland", true);
		setenv("MOZ_ENABLE_WAYLAND", "1", true);
	} else {
		unsetenv("GDK_BACKEND");
		unsetenv("QT_QPA_PLATFORM");
		unsetenv("SDL_VIDEODRIVER");
		unsetenv("CLUTTER_BACKEND");
		unsetenv("MOZ_ENABLE_WAYLAND");
	}
}

static bool command_is_discord(const char *command) {
	return command != NULL && strcasestr(command, "discord") != NULL;
}

static bool command_is_flatpak(const char *command) {
	return command != NULL && strstr(command, "flatpak") != NULL;
}

static bool command_is_chromium_browser(const char *command) {
	if (command == NULL) {
		return false;
	}
	static const char *const needles[] = {
		"brave-browser",
		"chromium",
		"google-chrome",
		"chrome",
		"vivaldi",
		"microsoft-edge",
		NULL,
	};
	for (int i = 0; needles[i] != NULL; i++) {
		if (strcasestr(command, needles[i]) != NULL) {
			return true;
		}
	}
	return false;
}

static void set_chromium_ozone_flags(const char *name) {
	static const char *const ozone_flags =
		"--enable-features=UseOzonePlatform --ozone-platform=wayland";
	const char *current = getenv(name);
	if (current == NULL || current[0] == '\0') {
		setenv(name, ozone_flags, true);
		return;
	}
	char flags[4096];
	snprintf(flags, sizeof(flags), "%s %s", current, ozone_flags);
	setenv(name, flags, true);
}

static void apply_client_ozone_environment(
		const char *command,
		bool force_wayland_backends) {
	if (!force_wayland_backends) {
		unsetenv("ELECTRON_OZONE_PLATFORM_HINT");
		unsetenv("NIXOS_OZONE_WL");
		unsetenv("CHROME_FLAGS");
		unsetenv("CHROMIUM_FLAGS");
		return;
	}
	if (command_is_discord(command)) {
		setenv("ELECTRON_OZONE_PLATFORM_HINT", "wayland", true);
		unsetenv("NIXOS_OZONE_WL");
		unsetenv("CHROME_FLAGS");
		unsetenv("CHROMIUM_FLAGS");
		return;
	}
	setenv("ELECTRON_OZONE_PLATFORM_HINT", "wayland", true);
	setenv("NIXOS_OZONE_WL", "1", true);
	if (command_is_chromium_browser(command)) {
		set_chromium_ozone_flags("CHROME_FLAGS");
		set_chromium_ozone_flags("CHROMIUM_FLAGS");
	} else {
		unsetenv("CHROME_FLAGS");
		unsetenv("CHROMIUM_FLAGS");
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

static void server_update_settings_ini(
	const char *config_dir,
	const struct orange_config *config);
static void server_apply_theme_env(struct orange_server *server);

static enum orange_appearance appearance_from_gnome_color_scheme(
		const char *scheme,
		enum orange_appearance fallback) {
	if (scheme == NULL || scheme[0] == '\0') {
		return fallback;
	}
	if (strcmp(scheme, "prefer-dark") == 0) {
		return ORANGE_APPEARANCE_DARK;
	}
	if (strcmp(scheme, "default") == 0 ||
			strcmp(scheme, "prefer-light") == 0) {
		return ORANGE_APPEARANCE_LIGHT;
	}
	return fallback;
}

static void server_apply_interface_settings(struct orange_server *server) {
	server->interface_settings_applied = true;
	const char *session_bus = getenv("DBUS_SESSION_BUS_ADDRESS");
	if (session_bus != NULL && session_bus[0] != '\0') {
		GSettingsSchemaSource *source = g_settings_schema_source_get_default();
		if (source != NULL) {
			GSettingsSchema *schema = g_settings_schema_source_lookup(source,
				"org.gnome.desktop.interface", true);
			if (schema != NULL) {
				GSettings *settings = g_settings_new_full(schema, NULL, NULL);
				if (settings != NULL) {
					const char *gtk_theme =
						server->config.appearance == ORANGE_APPEARANCE_DARK ?
						server->config.gtk_theme_dark : server->config.gtk_theme_light;
					set_gsettings_string_if_writable(settings, schema, "gtk-theme",
						gtk_theme);
					set_gsettings_string_if_writable(settings, schema, "icon-theme",
						server->config.icon_theme);
					set_gsettings_string_if_writable(settings, schema, "color-scheme",
						server->config.appearance == ORANGE_APPEARANCE_DARK ?
							"prefer-dark" : "default");
					set_gsettings_string_if_writable(settings, schema, "cursor-theme",
						config_resolved_cursor_theme(&server->config));
					set_gsettings_int_if_writable(settings, schema, "cursor-size",
						config_resolved_cursor_size(&server->config));
					g_settings_sync();
					g_object_unref(settings);
				}
				g_settings_schema_unref(schema);
			}
		}
	}

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
	if (config_dir != NULL) {
		server_update_settings_ini(config_dir, &server->config);
	}
}

static bool server_sync_appearance_from_gnome_interface(
		struct orange_server *server) {
	if (server == NULL || server->gnome_interface_settings == NULL) {
		return false;
	}

	GSettings *settings = g_settings_new("org.gnome.desktop.interface");
	if (settings == NULL) {
		return false;
	}
	char *scheme = g_settings_get_string(settings, "color-scheme");
	g_object_unref(settings);
	enum orange_appearance next = appearance_from_gnome_color_scheme(
		scheme, server->config.appearance);
	g_free(scheme);
	if (next == server->config.appearance) {
		return false;
	}

	server->config.appearance = next;
	orange_config_save(&server->config, server->options->config_path);
	server_apply_theme_env(server);
	server_apply_interface_settings(server);
	orange_session_services_set_appearance(&server->session_services,
		server->config.appearance);
	orange_session_services_emit_setting_changed(&server->session_services);
	server_mark_shell_dirty(server);
	server_mark_overlay_dirty(server);
	return true;
}

static void server_setup_gnome_interface_settings(struct orange_server *server) {
	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL) {
		return;
	}
	GSettingsSchema *schema = g_settings_schema_source_lookup(source,
		"org.gnome.desktop.interface", true);
	if (schema == NULL) {
		return;
	}
	server->gnome_interface_settings = g_settings_new_full(schema, NULL, NULL);
	g_settings_schema_unref(schema);
	if (server->gnome_interface_settings == NULL) {
		return;
	}
	server->interface_poll_ticks = 0;
	server_sync_appearance_from_gnome_interface(server);
}

static GSettings *server_new_gsettings(const char *schema_id) {
	if (schema_id == NULL || schema_id[0] == '\0') {
		return NULL;
	}
	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL) {
		return NULL;
	}
	GSettingsSchema *schema = g_settings_schema_source_lookup(source,
		schema_id, true);
	if (schema == NULL) {
		return NULL;
	}
	GSettings *settings = g_settings_new_full(schema, NULL, NULL);
	g_settings_schema_unref(schema);
	return settings;
}

static int clamp_workspace_count(int count) {
	if (count < 1) {
		return 1;
	}
	if (count > ORANGE_WORKSPACE_MAX) {
		return ORANGE_WORKSPACE_MAX;
	}
	return count;
}

static int clamp_workspace_index(int workspace, int count) {
	count = clamp_workspace_count(count);
	if (workspace < 0) {
		return 0;
	}
	if (workspace >= count) {
		return count - 1;
	}
	return workspace;
}

static int server_highest_occupied_workspace(
		const struct orange_server *server) {
	int highest = -1;
	if (server == NULL) {
		return highest;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || view->workspace < 0) {
			continue;
		}
		if (view->workspace > highest) {
			highest = view->workspace;
		}
	}
	return highest;
}

static bool server_normalize_workspaces(struct orange_server *server) {
	if (server == NULL) {
		return false;
	}

	int next_count = clamp_workspace_count(server->workspace_count);
	if (server->dynamic_workspaces_enabled) {
		int highest = server_highest_occupied_workspace(server);
		next_count = highest >= 0 ? highest + 2 : 1;
		if (next_count < server->current_workspace + 1) {
			next_count = server->current_workspace + 1;
		}
		next_count = clamp_workspace_count(next_count);
	}

	int next_current = clamp_workspace_index(server->current_workspace,
		next_count);
	bool changed = next_count != server->workspace_count ||
		next_current != server->current_workspace;
	server->workspace_count = next_count;
	server->current_workspace = next_current;

	if (!server->workspaces_only_on_primary) {
		struct orange_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			int oc = clamp_workspace_index(
				output->current_workspace, next_count);
			if (oc != output->current_workspace) {
				output->current_workspace = oc;
				changed = true;
			}
		}
	}

	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		int next = clamp_workspace_index(view->workspace, next_count);
		if (next != view->workspace) {
			view->workspace = next;
			changed = true;
		}
	}
	return changed;
}

static bool server_sync_multitasking_settings(struct orange_server *server) {
	if (server == NULL) {
		return false;
	}

	bool hot_corners = true;
	bool animations_enabled = true;
	bool edge_tiling = true;
	bool dynamic_workspaces = true;
	bool workspaces_only_on_primary = false;
	bool app_switch_current_workspace_only = false;
	bool window_switch_current_workspace_only = false;
	int workspace_count = 4;

	if (server->gnome_interface_settings != NULL) {
		hot_corners = g_settings_get_boolean(
			server->gnome_interface_settings, "enable-hot-corners");
		animations_enabled = g_settings_get_boolean(
			server->gnome_interface_settings, "enable-animations");
	}
	if (server->gnome_mutter_settings != NULL) {
		edge_tiling = g_settings_get_boolean(
			server->gnome_mutter_settings, "edge-tiling");
		dynamic_workspaces = g_settings_get_boolean(
			server->gnome_mutter_settings, "dynamic-workspaces");
		workspaces_only_on_primary = g_settings_get_boolean(
			server->gnome_mutter_settings,
			"workspaces-only-on-primary");
	}
	if (server->gnome_wm_settings != NULL) {
		workspace_count = g_settings_get_int(
			server->gnome_wm_settings, "num-workspaces");
	}
	if (server->gnome_app_switcher_settings != NULL) {
		app_switch_current_workspace_only = g_settings_get_boolean(
			server->gnome_app_switcher_settings,
			"current-workspace-only");
	}
	if (server->gnome_window_switcher_settings != NULL) {
		window_switch_current_workspace_only = g_settings_get_boolean(
			server->gnome_window_switcher_settings,
			"current-workspace-only");
	}
	workspace_count = clamp_workspace_count(workspace_count);

	bool changed =
		server->hot_corners_enabled != hot_corners ||
		server->animations_enabled != animations_enabled ||
		server->edge_tiling_enabled != edge_tiling ||
		server->dynamic_workspaces_enabled != dynamic_workspaces ||
		server->workspaces_only_on_primary != workspaces_only_on_primary ||
		server->app_switcher_current_workspace_only !=
			app_switch_current_workspace_only ||
		server->window_switcher_current_workspace_only !=
			window_switch_current_workspace_only ||
		(!dynamic_workspaces &&
		 server->workspace_count != workspace_count);

	server->hot_corners_enabled = hot_corners;
	server->animations_enabled = animations_enabled;
	server->edge_tiling_enabled = edge_tiling;
	server->dynamic_workspaces_enabled = dynamic_workspaces;
	server->workspaces_only_on_primary = workspaces_only_on_primary;
	server->app_switcher_current_workspace_only =
		app_switch_current_workspace_only;
	server->window_switcher_current_workspace_only =
		window_switch_current_workspace_only;
	if (!dynamic_workspaces) {
		server->workspace_count = workspace_count;
	}
	if (!server->animations_enabled && server->workspace_animation.active) {
		server_finish_workspace_animation(server);
	}
	changed = server_normalize_workspaces(server) || changed;
	if (changed) {
		server_apply_workspace_visibility(server);
	}
	return changed;
}

static void server_setup_multitasking_settings(struct orange_server *server) {
	server->gnome_mutter_settings =
		server_new_gsettings("org.gnome.mutter");
	server->gnome_wm_settings =
		server_new_gsettings("org.gnome.desktop.wm.preferences");
	server->gnome_app_switcher_settings =
		server_new_gsettings("org.gnome.shell.app-switcher");
	server->gnome_window_switcher_settings =
		server_new_gsettings("org.gnome.shell.window-switcher");
	server->multitasking_poll_ticks = 0;
	server_sync_multitasking_settings(server);
}

static void key_file_set_string_or_remove(GKeyFile *keyfile,
		const char *group,
		const char *key,
		const char *value) {
	if (value != NULL && value[0] != '\0') {
		g_key_file_set_string(keyfile, group, key, value);
	} else {
		g_key_file_remove_key(keyfile, group, key, NULL);
	}
}

static void server_update_settings_ini(
		const char *config_dir,
		const struct orange_config *config) {
	if (config == NULL) {
		return;
	}

	bool dark = config->appearance == ORANGE_APPEARANCE_DARK;
	const char *gtk_theme = dark ?
		config->gtk_theme_dark : config->gtk_theme_light;
	const char *cursor_theme = config_resolved_cursor_theme(config);
	int cursor_size = config_resolved_cursor_size(config);
	static const char *versions[] = {"gtk-4.0", "gtk-3.0"};
	for (size_t vi = 0; vi < sizeof(versions) / sizeof(versions[0]); vi++) {
		char path[4096];
		int n = snprintf(path, sizeof(path), "%s/%s/settings.ini",
			config_dir, versions[vi]);
		if (n <= 0 || n >= (int)sizeof(path)) {
			continue;
		}
		char *slash = strrchr(path, '/');
		if (slash != NULL) {
			*slash = '\0';
			mkdir(path, 0755);
			*slash = '/';
		}
		GKeyFile *keyfile = g_key_file_new();
		g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL);
		g_key_file_set_string(keyfile, "Settings", "color-scheme",
			dark ? "prefer-dark" : "default");
		key_file_set_string_or_remove(keyfile, "Settings",
			"gtk-theme-name", gtk_theme);
		key_file_set_string_or_remove(keyfile, "Settings",
			"gtk-icon-theme-name", config->icon_theme);
		key_file_set_string_or_remove(keyfile, "Settings",
			"gtk-cursor-theme-name", cursor_theme);
		if (cursor_size > 0) {
			g_key_file_set_integer(keyfile, "Settings",
				"gtk-cursor-theme-size", cursor_size);
		} else {
			g_key_file_remove_key(keyfile, "Settings",
				"gtk-cursor-theme-size", NULL);
		}
		if (vi == 1) {
			g_key_file_set_boolean(keyfile, "Settings",
				"gtk-application-prefer-dark-theme", dark);
		}
		gsize length = 0;
		char *data = g_key_file_to_data(keyfile, &length, NULL);
		if (data != NULL) {
			FILE *f = fopen(path, "w");
			if (f != NULL) {
				fwrite(data, 1, length, f);
				fclose(f);
			}
			g_free(data);
		}
		g_key_file_unref(keyfile);
	}
}

static void background_uri_to_path(const char *uri,
		char *out,
		size_t out_size) {
	if (out == NULL || out_size == 0) {
		return;
	}
	out[0] = '\0';
	if (uri == NULL || uri[0] == '\0') {
		return;
	}
	if (strncmp(uri, "file://", 7) == 0) {
		GError *error = NULL;
		char *path = g_filename_from_uri(uri, NULL, &error);
		if (path != NULL) {
			snprintf(out, out_size, "%s", path);
			g_free(path);
		}
		if (error != NULL) {
			g_error_free(error);
		}
	} else if (uri[0] == '/') {
		snprintf(out, out_size, "%s", uri);
	}
}

static bool server_refresh_background_settings(struct orange_server *server) {
	if (server == NULL || server->background_settings == NULL) {
		return false;
	}

	char *light_uri = g_settings_get_string(
		server->background_settings, "picture-uri");
	char *dark_uri = g_settings_get_string(
		server->background_settings, "picture-uri-dark");
	char *options = g_settings_get_string(
		server->background_settings, "picture-options");
	char *primary = g_settings_get_string(
		server->background_settings, "primary-color");
	char *secondary = g_settings_get_string(
		server->background_settings, "secondary-color");
	char *shading = g_settings_get_string(
		server->background_settings, "color-shading-type");
	int opacity = g_settings_get_int(
		server->background_settings, "picture-opacity");

	char light_path[4096];
	char dark_path[4096];
	background_uri_to_path(light_uri, light_path, sizeof(light_path));
	background_uri_to_path(dark_uri, dark_path, sizeof(dark_path));

	bool changed = orange_assets_set_wallpaper_settings(&server->assets,
		light_path,
		dark_path,
		options,
		primary,
		secondary,
		shading,
		opacity);

	g_free(light_uri);
	g_free(dark_uri);
	g_free(options);
	g_free(primary);
	g_free(secondary);
	g_free(shading);

	if (changed) {
		server_mark_shell_dirty(server);
		server_mark_overlay_dirty(server);
	}
	return changed;
}

static void server_setup_background_settings(struct orange_server *server) {
	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL) {
		return;
	}
	GSettingsSchema *schema = g_settings_schema_source_lookup(source,
		"org.gnome.desktop.background", true);
	if (schema == NULL) {
		return;
	}
	server->background_settings = g_settings_new_full(schema, NULL, NULL);
	g_settings_schema_unref(schema);
	if (server->background_settings == NULL) {
		return;
	}
	server->background_poll_ticks = 0;
	server_refresh_background_settings(server);
}

static const char *server_display_config_output_string(
		const char *value,
		const char *fallback) {
	return value != NULL && value[0] != '\0' ? value : fallback;
}

static double server_display_config_output_scale(
		struct wlr_output *wlr_output) {
	if (wlr_output == NULL || wlr_output->scale <= 0.0f) {
		return 1.0;
	}
	return (double)wlr_output->scale;
}

static int server_display_config_output_width(struct orange_output *output) {
	if (output == NULL) {
		return 1;
	}
	if (output->wlr_output != NULL && output->wlr_output->width > 0) {
		return output->wlr_output->width;
	}
	return output->width > 0 ? output->width : output->server->options->width;
}

static int server_display_config_output_height(struct orange_output *output) {
	if (output == NULL) {
		return 1;
	}
	if (output->wlr_output != NULL && output->wlr_output->height > 0) {
		return output->wlr_output->height;
	}
	return output->height > 0 ? output->height : output->server->options->height;
}

static double server_display_config_output_refresh(
		struct wlr_output *wlr_output) {
	if (wlr_output == NULL || wlr_output->refresh <= 0) {
		return 60.0;
	}
	return (double)wlr_output->refresh / 1000.0;
}

static void server_display_config_mode_id_from_mode(
		const struct wlr_output_mode *mode,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0 || mode == NULL) {
		return;
	}
	double refresh = (double)mode->refresh / 1000.0;
	snprintf(buffer, buffer_size, "%dx%d@%.3f", mode->width, mode->height, refresh);
}

static void server_display_config_mode_id(
		struct orange_output *output,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	int width = server_display_config_output_width(output);
	int height = server_display_config_output_height(output);
	double refresh = server_display_config_output_refresh(output->wlr_output);
	snprintf(buffer, buffer_size, "%dx%d@%.3f", width, height, refresh);
}

static struct wlr_output_mode *server_display_config_find_mode(
		struct wlr_output *wlr_output,
		const char *mode_id) {
	if (wlr_output == NULL || mode_id == NULL || mode_id[0] == '\0') {
		return NULL;
	}
	struct wlr_output_mode *mode;
	wl_list_for_each(mode, &wlr_output->modes, link) {
		char id[64];
		server_display_config_mode_id_from_mode(mode, id, sizeof(id));
		if (strcmp(id, mode_id) == 0) {
			return mode;
		}
	}
	return NULL;
}

static GVariant *server_display_config_supported_scales(double current_scale) {
	static const double common_scales[] = {1.0, 1.25, 1.5, 1.75, 2.0};
	GVariantBuilder scales;
	g_variant_builder_init(&scales, G_VARIANT_TYPE("ad"));
	bool added_current = false;
	for (size_t i = 0; i < sizeof(common_scales) / sizeof(common_scales[0]); i++) {
		g_variant_builder_add(&scales, "d", common_scales[i]);
		if (fabs(common_scales[i] - current_scale) < 0.001) {
			added_current = true;
		}
	}
	if (!added_current) {
		g_variant_builder_add(&scales, "d", current_scale);
	}
	return g_variant_builder_end(&scales);
}

static GVariant *server_display_config_empty_properties(void) {
	GVariantBuilder props;
	g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
	return g_variant_builder_end(&props);
}

static void server_display_config_add_output(
		struct orange_output *output,
		bool primary,
		GVariantBuilder *monitors,
		GVariantBuilder *logical_monitors) {
	if (output == NULL || output->wlr_output == NULL ||
			monitors == NULL || logical_monitors == NULL) {
		return;
	}

	const char *connector = server_display_config_output_string(
		output->wlr_output->name, "Unknown");
	const char *vendor = server_display_config_output_string(
		output->wlr_output->make, "Orange");
	const char *product = server_display_config_output_string(
		output->wlr_output->model, "Display");
	const char *serial = server_display_config_output_string(
		output->wlr_output->serial, "");
	double scale = server_display_config_output_scale(output->wlr_output);

	GVariantBuilder modes;
	g_variant_builder_init(&modes, G_VARIANT_TYPE("a(siiddada{sv})"));
	if (!wl_list_empty(&output->wlr_output->modes)) {
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &output->wlr_output->modes, link) {
			char mode_id[64];
			server_display_config_mode_id_from_mode(mode, mode_id, sizeof(mode_id));
			double mode_refresh = (double)mode->refresh / 1000.0;
			int current_width = 0, current_height = 0, current_refresh = 0;
			if (output->wlr_output->current_mode) {
				current_width = output->wlr_output->current_mode->width;
				current_height = output->wlr_output->current_mode->height;
				current_refresh = output->wlr_output->current_mode->refresh;
			}
			bool is_current = mode->width == current_width
				&& mode->height == current_height
				&& mode->refresh == current_refresh;
			double mode_scale = is_current ? scale : 1.0;
			int mode_properties = 0;
			if (is_current)
				mode_properties |= (1 << 0);
			if (mode->preferred)
				mode_properties |= (1 << 1);

			GVariantBuilder mode_props;
			g_variant_builder_init(&mode_props, G_VARIANT_TYPE("a{sv}"));
			g_variant_builder_add(&mode_props, "{sv}", "is-current",
				g_variant_new_boolean(is_current));
			g_variant_builder_add(&mode_props, "{sv}", "is-preferred",
				g_variant_new_boolean(mode->preferred));

			g_variant_builder_add_value(&modes,
				g_variant_new("(siidd@ad@a{sv})",
					mode_id,
					mode->width,
					mode->height,
					mode_refresh,
					mode_scale,
					server_display_config_supported_scales(mode_scale),
					g_variant_builder_end(&mode_props)));
		}
	} else {
		char mode_id[64];
		server_display_config_mode_id(output, mode_id, sizeof(mode_id));
		g_variant_builder_add_value(&modes,
			g_variant_new("(siidd@ad@a{sv})",
				mode_id,
				server_display_config_output_width(output),
				server_display_config_output_height(output),
				server_display_config_output_refresh(output->wlr_output),
				scale,
				server_display_config_supported_scales(scale),
				orange_session_services_display_config_mode_properties()));
	}

	g_variant_builder_add(monitors,
		"((ssss)@a(siiddada{sv})@a{sv})",
		connector,
		vendor,
		product,
		serial,
		g_variant_builder_end(&modes),
		orange_session_services_display_config_monitor_properties(
			connector, vendor, product));

	GVariantBuilder specs;
	g_variant_builder_init(&specs, G_VARIANT_TYPE("a(ssss)"));
	g_variant_builder_add(&specs, "(ssss)", connector, vendor, product, serial);

	int x = output->layout_output != NULL ? output->layout_output->x : 0;
	int y = output->layout_output != NULL ? output->layout_output->y : 0;
	g_variant_builder_add(logical_monitors,
		"(iidub@a(ssss)@a{sv})",
		x,
		y,
		scale,
		(guint32)output->wlr_output->transform,
		primary,
		g_variant_builder_end(&specs),
		server_display_config_empty_properties());
}

static GVariant *server_display_config_current_state(
		void *data,
		uint32_t serial) {
	struct orange_server *server = data;
	if (server == NULL) {
		return NULL;
	}

	GVariantBuilder monitors;
	GVariantBuilder logical_monitors;
	g_variant_builder_init(&monitors,
		G_VARIANT_TYPE("a((ssss)a(siiddada{sv})a{sv})"));
	g_variant_builder_init(&logical_monitors,
		G_VARIANT_TYPE("a(iiduba(ssss)a{sv})"));

	bool primary = true;
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		server_display_config_add_output(output, primary,
			&monitors, &logical_monitors);
		primary = false;
	}

	return g_variant_new("(u@a((ssss)a(siiddada{sv})a{sv})@a(iiduba(ssss)a{sv})@a{sv})",
		serial != 0 ? serial : 1,
		g_variant_builder_end(&monitors),
		g_variant_builder_end(&logical_monitors),
		orange_session_services_display_config_state_properties());
}

static bool display_config_transform_is_valid(guint32 transform) {
	return transform <= WL_OUTPUT_TRANSFORM_FLIPPED_270;
}

static bool server_apply_display_config_ui_scale(
		struct orange_server *server,
		double scale,
		GError **error) {
	if (server == NULL || server->options == NULL) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Orange DisplayConfig has no config provider");
		return false;
	}

	struct orange_config previous = server->config;
	orange_config_apply_ui_scale(&server->config, scale);
	if (!orange_config_save(&server->config,
			server->options->config_path)) {
		server->config = previous;
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Failed to save Orange UI scale to %s",
			server->options->config_path != NULL ?
				server->options->config_path : "config");
		return false;
	}
	return true;
}

static struct orange_output *server_output_for_display_config_connector(
		struct orange_server *server,
		const char *connector) {
	if (server == NULL || connector == NULL) {
		return NULL;
	}
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output != NULL &&
				output->wlr_output->name != NULL &&
				strcmp(output->wlr_output->name, connector) == 0) {
			return output;
		}
	}
	return NULL;
}

static bool server_apply_display_config_to_output(
		struct orange_server *server,
		struct orange_output *output,
		uint32_t method,
		int32_t x,
		int32_t y,
		double scale,
		guint32 transform,
		const char *mode_id,
		GError **error) {
	if (output == NULL || output->wlr_output == NULL) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"DisplayConfig output is unavailable");
		return false;
	}
	if (!isfinite(scale) || scale <= 0.0 || scale > 8.0) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid DisplayConfig scale %.3f", scale);
		return false;
	}
	if (!display_config_transform_is_valid(transform)) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid DisplayConfig transform %u", transform);
		return false;
	}
	if (method == 0) {
		return true;
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);

	wlr_output_state_set_scale(&state, (float)scale);
	wlr_output_state_set_transform(&state,
		(enum wl_output_transform)transform);

	if (mode_id != NULL && mode_id[0] != '\0') {
		struct wlr_output_mode *mode = server_display_config_find_mode(
			output->wlr_output, mode_id);
		if (mode != NULL) {
			wlr_output_state_set_mode(&state, mode);
		} else {
			// Try custom mode: parse "WxH@R"
			int w = 0, h = 0;
			double r = 0.0;
			if (sscanf(mode_id, "%dx%d@%lf", &w, &h, &r) >= 2) {
				int refresh_mhz = (int)(r * 1000.0 + 0.5);
				wlr_output_state_set_custom_mode(&state, w, h, refresh_mhz);
			}
		}
	}

	wlr_output_state_set_enabled(&state, true);

	if (!wlr_output_commit_state(output->wlr_output, &state)) {
		wlr_output_state_finish(&state);
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Failed to commit DisplayConfig for %s",
			output->wlr_output->name != NULL ?
				output->wlr_output->name : "output");
		return false;
	}
	wlr_output_state_finish(&state);

	// Apply layout position
	if (output->layout_output != NULL) {
		wlr_output_layout_remove(server->output_layout, output->wlr_output);
	}
	output->layout_output = wlr_output_layout_add(server->output_layout,
		output->wlr_output, x, y);

	output->shell_dirty = true;
	output->overlay_dirty = true;
	wlr_output_schedule_frame(output->wlr_output);
	return true;
}

static bool server_process_display_config_apply(
		struct orange_server *server,
		GVariant *logical_monitors,
		uint32_t method,
		double *ui_scale,
		bool *ui_scale_seen,
		bool *touched_output,
		GError **error) {
	if (server == NULL || logical_monitors == NULL) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Orange DisplayConfig has no output provider");
		return false;
	}

	GVariantIter logical_iter;
	g_variant_iter_init(&logical_iter, logical_monitors);
	gint32 x = 0;
	gint32 y = 0;
	double scale = 1.0;
	guint32 transform = 0;
	gboolean primary = false;
	GVariant *monitor_configs = NULL;
	while (g_variant_iter_next(&logical_iter, "(iidub@a(ssa{sv}))",
			&x, &y, &scale, &transform, &primary,
			&monitor_configs)) {
		if (ui_scale != NULL && ui_scale_seen != NULL &&
				(primary || !*ui_scale_seen)) {
			*ui_scale = scale;
			*ui_scale_seen = true;
		}

		GVariantIter monitor_iter;
		g_variant_iter_init(&monitor_iter, monitor_configs);
		const char *connector = NULL;
		const char *mode_id = NULL;
		GVariant *monitor_props = NULL;
		while (g_variant_iter_next(&monitor_iter, "(&s&sa{sv})",
				&connector, &mode_id, &monitor_props)) {
			struct orange_output *output =
				server_output_for_display_config_connector(
					server, connector);
			if (output == NULL) {
				g_set_error(error, G_DBUS_ERROR,
					G_DBUS_ERROR_INVALID_ARGS,
					"Unknown DisplayConfig output %s",
					connector != NULL ? connector : "");
				g_variant_unref(monitor_props);
				g_variant_unref(monitor_configs);
				return false;
			}
			bool ok = server_apply_display_config_to_output(
				server, output, method, x, y, scale, transform, mode_id, error);
			g_variant_unref(monitor_props);
			if (!ok) {
				g_variant_unref(monitor_configs);
				return false;
			}
			if (touched_output != NULL) {
				*touched_output = true;
			}
		}
		g_variant_unref(monitor_configs);
	}

	return true;
}

static bool server_display_config_apply(
		void *data,
		uint32_t method,
		GVariant *logical_monitors,
		GVariant *properties,
		GError **error) {
	struct orange_server *server = data;
	(void)properties;

	double ui_scale = 1.0;
	bool ui_scale_seen = false;
	bool touched_output = false;
	if (!server_process_display_config_apply(server, logical_monitors, 0,
			&ui_scale, &ui_scale_seen, &touched_output, error)) {
		return false;
	}
	if (!touched_output) {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"DisplayConfig apply contained no outputs");
		return false;
	}
	if (method == 0) {
		return true;
	}
	if (!ui_scale_seen ||
			!server_apply_display_config_ui_scale(
				server, ui_scale, error)) {
		return false;
	}

	touched_output = false;
	if (!server_process_display_config_apply(server, logical_monitors, method,
			NULL, NULL, &touched_output, error)) {
		return false;
	}
	server_mark_shell_dirty(server);
	server_mark_overlay_dirty(server);
	return true;
}

static int64_t dbus_connection_pid_for_name(
		GDBusConnection *connection,
		const char *bus_name) {
	if (connection == NULL || bus_name == NULL || bus_name[0] == '\0') {
		return 0;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"org.freedesktop.DBus",
		"GetConnectionUnixProcessID",
		g_variant_new("(s)", bus_name),
		G_VARIANT_TYPE("(u)"),
		G_DBUS_CALL_FLAGS_NONE,
		200,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return 0;
	}

	guint32 pid = 0;
	g_variant_get(reply, "(u)", &pid);
	g_variant_unref(reply);
	return (int64_t)pid;
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
		wlr_log(WLR_ERROR, "%s", "failed to create xcursor manager");
		return;
	}
	server_reload_cursor_scales(server);
	wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "default");
}

static void server_reload_cursor_scales(struct orange_server *server) {
	if (server->xcursor_manager == NULL) {
		return;
	}
	double max_scale = 1.0;
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		double s = ui_scale_for_size(output->width, output->height);
		if (s > max_scale) {
			max_scale = s;
		}
	}
	if (!wlr_xcursor_manager_load(server->xcursor_manager, max_scale)) {
		wlr_log(WLR_ERROR, "failed to load cursor theme at scale %.2f",
			max_scale);
	}
}

static void server_reload_assets_if_ready(struct orange_server *server) {
	orange_assets_finish(&server->assets);
	orange_assets_init(&server->assets);
	orange_assets_load(&server->assets, server->config.icon_theme);
	server_refresh_background_settings(server);
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
	orange_font_family = server->config.font_family[0] != '\0' ?
		server->config.font_family : "Cantarell";
	server_apply_theme_env(server);
	if (!server->interface_settings_applied || appearance_changed ||
			icon_theme_changed || cursor_changed || force_dirty) {
		server_apply_interface_settings(server);
	}
	if (appearance_changed) {
		orange_session_services_set_appearance(&server->session_services,
			server->config.appearance);
		orange_session_services_emit_setting_changed(
			&server->session_services);
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

static void apply_client_launch_environment(
		const char *command,
		bool force_wayland_backends) {
	if (orange_client_wayland_display[0] != '\0') {
		setenv("WAYLAND_DISPLAY", orange_client_wayland_display, true);
	}
	if (orange_server_xwayland_display[0] != '\0') {
		setenv("DISPLAY", orange_server_xwayland_display, true);
	}
	apply_client_toolkit_environment(force_wayland_backends);
	apply_client_ozone_environment(command, force_wayland_backends);
}

static void reset_child_process_signal_state(void) {
	struct sigaction action = {
		.sa_handler = SIG_DFL,
		.sa_flags = 0,
	};
	sigemptyset(&action.sa_mask);
	sigaction(SIGCHLD, &action, NULL);
	sigset_t empty;
	sigemptyset(&empty);
	sigprocmask(SIG_SETMASK, &empty, NULL);
}

static void launch_command_with_environment(
		const char *command,
		bool client_environment,
		bool allow_private_dbus,
		bool force_wayland_backends) {
	if (command == NULL || command[0] == '\0') {
		return;
	}

	bool use_private_dbus = client_environment &&
		allow_private_dbus &&
		orange_client_launch_private_dbus &&
		!command_is_flatpak(command);

	if (client_environment) {
		wlr_log(WLR_INFO, "launching client command%s: %s",
			use_private_dbus ? " with private DBus" : "",
			command);
	} else {
		wlr_log(WLR_INFO, "launching shell command: %s", command);
	}

	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "fork failed while launching: %s", command);
		return;
	}
	if (pid == 0) {
		setsid();
		reset_child_process_signal_state();
		if (client_environment) {
			apply_client_launch_environment(command,
				force_wayland_backends);
			if (use_private_dbus) {
				execlp("dbus-run-session", "dbus-run-session",
					"--", "sh", "-c", command, (char *)NULL);
			}
		}
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
}

static void launch_command(const char *command) {
	launch_command_with_environment(command, true, true, true);
}

static void launch_game_command(const char *command) {
	launch_command_with_environment(command, true, false, false);
}

static void launch_session_command(const char *command) {
	launch_command_with_environment(command, true, false, true);
}

static void launch_shell_command(const char *command) {
	launch_command_with_environment(command, false, false, true);
}

static void server_export_client_environment(void) {
	set_env_default("XDG_SESSION_TYPE", "wayland");
	set_env_default("XDG_CURRENT_DESKTOP", "Orange");
	set_env_default("XDG_SESSION_DESKTOP", "orange");
	set_env_default("DESKTOP_SESSION", "orange");
	apply_client_toolkit_environment(true);
	apply_client_ozone_environment(NULL, true);
	launch_shell_command(
		"if command -v dbus-update-activation-environment >/dev/null 2>&1; then "
		"dbus-update-activation-environment --systemd "
		"WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP "
		"DESKTOP_SESSION XDG_SESSION_TYPE GTK_THEME GTK_ICON_THEME "
		"ORANGE_APPEARANCE XCURSOR_THEME XCURSOR_SIZE "
		"GDK_BACKEND QT_QPA_PLATFORM SDL_VIDEODRIVER CLUTTER_BACKEND "
		"MOZ_ENABLE_WAYLAND ELECTRON_OZONE_PLATFORM_HINT NIXOS_OZONE_WL "
		">/dev/null 2>&1 || "
		"dbus-update-activation-environment "
		"WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP "
		"DESKTOP_SESSION XDG_SESSION_TYPE GTK_THEME GTK_ICON_THEME "
		"ORANGE_APPEARANCE XCURSOR_THEME XCURSOR_SIZE "
		"GDK_BACKEND QT_QPA_PLATFORM SDL_VIDEODRIVER CLUTTER_BACKEND "
		"MOZ_ENABLE_WAYLAND ELECTRON_OZONE_PLATFORM_HINT NIXOS_OZONE_WL "
		">/dev/null 2>&1 || true; "
		"fi");
}

static bool append_shell_quoted_arg(
		char *out,
		size_t out_size,
		size_t *len,
		const char *text) {
	if (out == NULL || len == NULL || *len + 1 >= out_size) {
		return false;
	}
	out[(*len)++] = '\'';
	for (const char *p = text != NULL ? text : ""; *p != '\0'; p++) {
		if (*p == '\'') {
			const char *quote = "'\\''";
			for (const char *q = quote; *q != '\0'; q++) {
				if (*len + 1 >= out_size) {
					return false;
				}
				out[(*len)++] = *q;
			}
		} else {
			if (*len + 1 >= out_size) {
				return false;
			}
			out[(*len)++] = *p;
		}
	}
	if (*len + 1 >= out_size) {
		return false;
	}
	out[(*len)++] = '\'';
	out[*len] = '\0';
	return true;
}

static bool shell_quote_arg(const char *text, char *out, size_t out_size) {
	if (out == NULL || out_size == 0) {
		return false;
	}
	size_t len = 0;
	out[0] = '\0';
	return append_shell_quoted_arg(out, out_size, &len, text);
}

static void launch_terminal(void) {
	const char *terminal = getenv("ORANGE_TERMINAL");
	if (terminal != NULL && terminal[0] != '\0') {
		launch_command(terminal);
		return;
	}
	launch_command("ghostty || foot || alacritty || kitty || weston-terminal || xterm");
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

static uint32_t launcher_dock_filter_hash(
		const struct orange_server *server) {
	uint32_t hash = 2166136261u;
	if (server == NULL) {
		return hash;
	}
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		const char *id = server->config.dock_apps[i];
		for (const char *p = id; p != NULL && *p != '\0'; p++) {
			hash ^= (uint32_t)(unsigned char)*p;
			hash *= 16777619u;
		}
		hash ^= 0xffu;
		hash *= 16777619u;
	}
	int temp_count = server->dock_temporary_count;
	if (temp_count < 0) {
		temp_count = 0;
	}
	if (temp_count > ORANGE_DOCK_MAX) {
		temp_count = ORANGE_DOCK_MAX;
	}
	for (int i = 0; i < temp_count; i++) {
		const char *id = server->dock_temporary_app_ids[i];
		for (const char *p = id; p != NULL && *p != '\0'; p++) {
			hash ^= (uint32_t)(unsigned char)*p;
			hash *= 16777619u;
		}
		hash ^= 0xfeu;
		hash *= 16777619u;
	}
	return hash;
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

static bool launcher_uses_list_results(const struct orange_server *server);

static void launcher_preload_icons(struct orange_server *server) {
	if (server->launcher_display_mode != ORANGE_LAUNCHER_DISPLAY_FULL ||
			server->launcher_app_count <= 0) {
		return;
	}
	bool dark = is_dark_config(&server->config);
	int variant = dark ?
		ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	if (launcher_uses_list_results(server)) {
		int n = server->launcher_result_count;
		if (n > ORANGE_LAUNCHER_RESULT_MAX) {
			n = ORANGE_LAUNCHER_RESULT_MAX;
		}
		for (int i = 0; i < n; i++) {
			const char *icon_name = server->launcher_results[i].icon_name;
			if (icon_name != NULL && icon_name[0] != '\0') {
				orange_assets_icon(&server->assets, variant, icon_name);
			}
		}
		return;
	}
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
	server_refresh_dock_state(server);
	if (server->launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
		server->launcher_app_count = 0;
		server->launcher_result_count = 0;
		server->launcher_hot_app = -1;
		return;
	}

	char key[256];
	snprintf(key, sizeof(key), "%s|%d|%s|%zu|%d|%08x",
		server->launcher_query,
		server->launcher_current_mode,
		launcher_active_category_filter(server) != NULL ?
			launcher_active_category_filter(server) : "",
		server->desktop_entry_count,
		server->desktop_file_count,
		launcher_dock_filter_hash(server));
	bool cache_hit = strcmp(server->launcher_filter_cache_key, key) == 0;
	if (!cache_hit) {
		snprintf(server->launcher_filter_cache_key,
			sizeof(server->launcher_filter_cache_key), "%s", key);
		server->launcher_result_count = 0;
		memset(server->launcher_results, 0, sizeof(server->launcher_results));
		if (launcher_uses_list_results(server)) {
			server->launcher_result_count = orange_launcher_build_results(
				server->desktop_entries, (int)server->desktop_entry_count,
				server->desktop_files, server->desktop_file_count,
				server->launcher_query,
				server->launcher_current_mode,
				NULL,
				&server->config,
				server->dock_temporary_app_ids,
				server->dock_temporary_count,
				server->launcher_results,
				ORANGE_LAUNCHER_RESULT_MAX);
			server->launcher_app_count = server->launcher_result_count;
			for (int i = 0; i < server->launcher_app_count &&
					i < ORANGE_LAUNCHER_APP_MAX; i++) {
				server->launcher_app_indices[i] = i;
			}
		} else {
			server->launcher_app_count = orange_launcher_filter_available(
				server->desktop_entries, (int)server->desktop_entry_count,
				server->launcher_query,
				launcher_active_category_filter(server),
				&server->config,
				server->dock_temporary_app_ids,
				server->dock_temporary_count,
				server->launcher_app_indices, ORANGE_LAUNCHER_APP_MAX);
		}
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
	server->launcher_result_count = 0;
	memset(server->launcher_results, 0, sizeof(server->launcher_results));
	server->launcher_search_focus = focus_search;
	server->system_menu_open = false;
	server->notification_center_open = false;
	/* Default category tabs for Apps mode */
	const char *cats[] = {
		"All", "Utilities", "Photo & Video",
		"Productivity & Finance", "Creativity", "Entertainment",
		"Social", "Information & Reading",
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
	server->launcher_result_count = 0;
	memset(server->launcher_results, 0, sizeof(server->launcher_results));
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
static void launch_desktop_file_open(const char *path);
static bool create_new_desktop_folder(struct orange_server *server);
static void launch_desktop_paste_to_desktop(struct orange_server *server);

static bool launcher_uses_list_results(const struct orange_server *server) {
	return server != NULL &&
		(server->launcher_query[0] != '\0' ||
		 server->launcher_current_mode != ORANGE_LAUNCHER_MODE_APPS);
}

static void launch_web_search(const char *query) {
	if (query == NULL || query[0] == '\0') {
		return;
	}
	char *escaped = g_uri_escape_string(query, NULL, false);
	if (escaped == NULL || escaped[0] == '\0') {
		g_free(escaped);
		return;
	}
	char url[1024];
	snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", escaped);
	g_free(escaped);
	char quoted[1200];
	if (!shell_quote_arg(url, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[2600];
	snprintf(cmd, sizeof(cmd), "xdg-open %s || gio open %s || true",
		quoted, quoted);
	launch_command(cmd);
}

static bool launcher_run_action(
		struct orange_server *server,
		const char *action) {
	if (server == NULL || action == NULL || action[0] == '\0') {
		return false;
	}
	if (strcmp(action, "open-terminal") == 0) {
		launch_terminal();
		return true;
	}
	if (strcmp(action, "open-files") == 0) {
		launch_command("xdg-open \"$HOME\" || true");
		return true;
	}
	if (strcmp(action, "open-documents") == 0) {
		launch_command("xdg-open \"$HOME/Documents\" || true");
		return true;
	}
	if (strcmp(action, "open-downloads") == 0) {
		launch_command("xdg-open \"$HOME/Downloads\" || true");
		return true;
	}
	if (strcmp(action, "open-desktop") == 0) {
		launch_command("xdg-open \"$HOME/Desktop\" || true");
		return true;
	}
	if (strcmp(action, "open-settings") == 0) {
		launch_command("gnome-control-center || true");
		return true;
	}
	if (strcmp(action, "open-software") == 0) {
		launch_command("gnome-software || plasma-discover || true");
		return true;
	}
	if (strcmp(action, "recent-files") == 0) {
		launch_command("gio open recent:/// || xdg-open \"$HOME/.local/share/recently-used.xbel\" || true");
		return true;
	}
	if (strcmp(action, "new-folder") == 0) {
		return create_new_desktop_folder(server);
	}
	if (strcmp(action, "paste-desktop") == 0) {
		launch_desktop_paste_to_desktop(server);
		return true;
	}
	if (strcmp(action, "lock-screen") == 0) {
		launch_command("xdg-screensaver lock || gnome-screensaver-command -l || true");
		return true;
	}
	return false;
}

/* Launch the app at a flat (filtered) index and close the overlay. */
static void launcher_launch_index(struct orange_server *server, int flat_index) {
	if (flat_index < 0 || flat_index >= server->launcher_app_count) {
		return;
	}
	if (launcher_uses_list_results(server)) {
		if (flat_index >= server->launcher_result_count ||
				flat_index >= ORANGE_LAUNCHER_RESULT_MAX) {
			return;
		}
		const struct orange_launcher_result *result =
			&server->launcher_results[flat_index];
		switch (result->kind) {
		case ORANGE_LAUNCHER_RESULT_APP:
			if (result->source_index >= 0 &&
					result->source_index < (int)server->desktop_entry_count) {
				launch_desktop_entry(
					&server->desktop_entries[result->source_index]);
			}
			break;
		case ORANGE_LAUNCHER_RESULT_FILE:
			if (result->source_index >= 0 &&
					result->source_index < server->desktop_file_count) {
				launch_desktop_file_open(
					server->desktop_files[result->source_index].path);
			}
			break;
		case ORANGE_LAUNCHER_RESULT_ACTION:
			launcher_run_action(server, result->action);
			break;
		case ORANGE_LAUNCHER_RESULT_WEB:
			launch_web_search(result->action);
			break;
		}
		launcher_close_overlay(server);
		return;
	}
	int entry_index = server->launcher_app_indices[flat_index];
	if (entry_index >= 0 &&
			entry_index < (int)server->desktop_entry_count) {
		const char *app_id = server->desktop_entries[entry_index].id;
		if (orange_dock_temporary_add(server->dock_temporary_app_ids,
				&server->dock_temporary_count, app_id,
				&server->config)) {
			server->launcher_filter_cache_key[0] = '\0';
			server_mark_dock_dirty(server);
		}
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

static bool monotonic_msec_reached(uint32_t now, uint32_t target) {
	return (int32_t)(now - target) >= 0;
}

static void start_cursor_loading(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL) {
		return;
	}
	server->cursor_loading_active = true;
	server->cursor_loading_launcher_idx = launcher_idx;
	server->cursor_loading_start_ms = monotonic_time_msec();
	if (server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor,
			server->xcursor_manager, "progress");
	}
}

static void stop_cursor_loading(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	server->cursor_loading_active = false;
	server->cursor_loading_launcher_idx = -1;
	server->cursor_loading_start_ms = 0;
	if (server->xcursor_manager != NULL &&
			(server->seat == NULL ||
			 server->seat->pointer_state.focused_surface == NULL)) {
		wlr_cursor_set_xcursor(server->cursor,
			server->xcursor_manager, "default");
	}
}

static bool contains_case(const char *haystack, const char *needle) {
	return haystack != NULL && needle != NULL && strcasestr(haystack, needle) != NULL;
}

static void app_menu_model_reset(struct orange_app_menu_model *menu) {
	if (menu != NULL) {
		memset(menu, 0, sizeof(*menu));
	}
}



static void clean_imported_menu_label(
		const char *source,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	buffer[0] = '\0';
	if (source == NULL) {
		return;
	}

	size_t out = 0;
	for (size_t in = 0; source[in] != '\0' && out + 1 < buffer_size; in++) {
		if (source[in] == '_') {
			if (source[in + 1] == '_') {
				buffer[out++] = '_';
				in++;
			}
			continue;
		}
		buffer[out++] = source[in];
	}
	buffer[out] = '\0';
}

static const struct orange_desktop_entry *server_desktop_entry_for_app_id(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	const struct orange_desktop_entry *best = NULL;
	int best_score = 0;
	for (size_t i = 0; i < server->desktop_entry_count; i++) {
		const struct orange_desktop_entry *entry =
			&server->desktop_entries[i];
		if (!orange_desktop_entry_is_available(entry)) {
			continue;
		}
		int score = orange_desktop_entry_match_score(entry, app_id);
		if (score > best_score) {
			best = entry;
			best_score = score;
		}
	}
	return best;
}

static bool desktop_entry_matches_view(
		const struct orange_desktop_entry *entry,
		const char *app_id,
		const char *title) {
	if (entry == NULL) {
		return false;
	}
	if (!orange_desktop_entry_is_available(entry)) {
		return false;
	}
	if (app_id != NULL && app_id[0] != '\0') {
		if (orange_desktop_entry_id_matches(entry->id, app_id) ||
				contains_case(app_id, entry->name) ||
				contains_case(app_id, entry->icon)) {
			return true;
		}
	}
	if (title != NULL && title[0] != '\0') {
		return contains_case(title, entry->name) ||
			contains_case(title, entry->id) ||
			contains_case(title, entry->icon);
	}
	return false;
}

static const struct orange_desktop_entry *server_desktop_entry_for_view(
		struct orange_server *server,
		struct orange_view *view) {
	if (server == NULL || view == NULL) {
		return NULL;
	}
	const char *app_id = view_get_app_id(view);
	const char *title = view_get_title(view);
	if (app_id == NULL) {
		return NULL;
	}
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_app_id(server, app_id);
	if (entry != NULL) {
		return entry;
	}
	for (size_t i = 0; i < server->desktop_entry_count; i++) {
		if (desktop_entry_matches_view(&server->desktop_entries[i],
				app_id, title)) {
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
		server->desktop_entries,
		sizeof(server->desktop_entries) / sizeof(server->desktop_entries[0]));
	/* Entry contents (not just count) may have changed; force
	 * launcher_refresh_filter to recompute next time it's called instead
	 * of trusting the count-based cache key. */
	server->launcher_filter_cache_key[0] = '\0';
	server_mark_dock_dirty(server);
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
	if (orange_desktop_entry_id_matches(entry->id, "org.gnome.Settings")) {
		launch_session_command(orange_settings_command);
	} else if (strstr(exec, "gnome-control-center") != NULL) {
		char wrapped[1600];
		snprintf(wrapped, sizeof(wrapped),
			ORANGE_GNOME_SETTINGS_ENV "%s", exec);
		launch_session_command(wrapped);
	} else if (orange_desktop_entry_is_game(entry)) {
		launch_game_command(exec);
	} else {
		launch_command(exec);
	}
}

static void launch_dock_app_id(
	struct orange_server *server,
	const char *app_id);
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
	launch_dock_app_id(server, app_id);
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
	int trash_pos = -1;
	for (int i = 0; i < visible; i++) {
		if (strcmp(server->config.dock_apps[launchers[i]], "__trash__") == 0) {
			trash_pos = i;
			break;
		}
	}
	int available_special_slots = ORANGE_DOCK_MAX - visible;
	if (available_special_slots < 0) {
		available_special_slots = 0;
	}
	int temp_count = server->dock_temporary_count;
	if (temp_count < 0) {
		temp_count = 0;
	}
	if (temp_count > available_special_slots) {
		temp_count = available_special_slots;
	}
	available_special_slots -= temp_count;
	int minimized_count = server_minimized_dock_count(server, NULL);
	if (minimized_count > available_special_slots) {
		minimized_count = available_special_slots;
	}
	int special_count = temp_count + minimized_count;
	int insert_pos = trash_pos >= 0 ? trash_pos : visible;
	if (special_count > 0) {
		if (visible_index >= insert_pos &&
				visible_index < insert_pos + special_count) {
			return -1;
		}
		if (visible_index >= insert_pos + special_count) {
			visible_index -= special_count;
		}
	}
	return visible_index < visible ? launchers[visible_index] : -1;
}

static int dock_launcher_for_view(struct orange_server *server, struct orange_view *view) {
	if (view == NULL) {
		return -1;
	}
	if (view->dock_launcher_index >= 0) {
		return view->dock_launcher_index;
	}
	const char *app_id = view_get_app_id(view);
	const char *title = view_get_title(view);
	if (app_id == NULL) {
		return -1;
	}
	int index = -1;
	for (int i = 0; i < ORANGE_DOCK_MAX; i++) {
		const char *dock_id = server->config.dock_apps[i];
		if (dock_id[0] == '\0' || dock_id[0] == '_') {
			continue;
		}
		if (orange_desktop_entry_id_matches(app_id, dock_id)) {
			index = i;
			goto done;
		}
		const struct orange_desktop_entry *entry =
			server_desktop_entry_for_app_id(server, dock_id);
		if (desktop_entry_matches_view(entry, app_id, title)) {
			index = i;
			goto done;
		}
	}
	if (contains_case(app_id, "files") ||
			contains_case(app_id, "nautilus") || contains_case(title, "files")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Nautilus.desktop");
		goto done;
	}
	if (contains_case(app_id, "browser") ||
			contains_case(app_id, "firefox") || contains_case(app_id, "chrom") ||
			contains_case(title, "browser")) {
		index = orange_dock_config_find_app(&server->config,
			"firefox.desktop");
		goto done;
	}
	if (contains_case(app_id, "mail") || contains_case(title, "mail")) {
		goto done;
	}
	if (contains_case(app_id, "map") || contains_case(title, "map")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Maps.desktop");
		goto done;
	}
	if (contains_case(app_id, "photo") || contains_case(title, "photo")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Loupe.desktop");
		goto done;
	}
	if (contains_case(app_id, "calendar") || contains_case(title, "calendar")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Calendar.desktop");
		goto done;
	}
	if (contains_case(app_id, "text") || contains_case(app_id, "gedit") ||
			contains_case(app_id, "mousepad") || contains_case(title, "notes")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.TextEditor.desktop");
		goto done;
	}
	if (contains_case(app_id, "software") || contains_case(app_id, "discover") ||
			contains_case(title, "software")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Software.desktop");
		goto done;
	}
	if (contains_case(app_id, "calculator") || contains_case(app_id, "kcalc") ||
			contains_case(title, "calculator")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Calculator.desktop");
		goto done;
	}
	if (contains_case(app_id, "settings") || contains_case(app_id, "control-center") ||
			contains_case(title, "settings")) {
		index = orange_dock_config_find_app(&server->config,
			"org.gnome.Settings.desktop");
	}
done:
	view->dock_launcher_index = index;
	return index;
}

static bool dock_identity_matches(const char *a, const char *b) {
	if (a == NULL || b == NULL || a[0] == '\0' || b[0] == '\0') {
		return false;
	}
	return strcasecmp(a, b) == 0 ||
		orange_desktop_entry_id_matches(a, b) ||
		orange_desktop_entry_id_matches(b, a);
}

static bool view_dock_identity(
		struct orange_server *server,
		struct orange_view *view,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return false;
	}
	buffer[0] = '\0';
	if (server == NULL || view == NULL) {
		return false;
	}

	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_view(server, view);
	if (entry != NULL && entry->id[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", entry->id);
		return true;
	}

	const char *app_id = view_get_app_id(view);
	if (app_id != NULL && app_id[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", app_id);
		return true;
	}

	const char *title = view_get_title(view);
	if (title != NULL && title[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", title);
		return true;
	}
	return false;
}

static const struct orange_desktop_entry *server_desktop_entry_for_dock_identity(
		struct orange_server *server,
		const char *app_id) {
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_app_id(server, app_id);
	if (entry != NULL) {
		return entry;
	}
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}
	for (size_t i = 0; i < server->desktop_entry_count; i++) {
		if (desktop_entry_matches_view(
				&server->desktop_entries[i], app_id, app_id)) {
			return &server->desktop_entries[i];
		}
	}
	return NULL;
}

static bool view_matches_dock_identity(
		struct orange_server *server,
		struct orange_view *view,
		const char *app_id) {
	if (server == NULL || view == NULL ||
			app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_view(server, view);
	if (entry != NULL && desktop_entry_matches_view(entry, app_id, app_id)) {
		return true;
	}
	char identity[128];
	if (view_dock_identity(server, view, identity, sizeof(identity)) &&
			dock_identity_matches(identity, app_id)) {
		return true;
	}
	const char *view_app_id = view_get_app_id(view);
	const char *title = view_get_title(view);
	return dock_identity_matches(view_app_id, app_id) ||
		(title != NULL && title[0] != '\0' &&
		 (contains_case(title, app_id) || contains_case(app_id, title)));
}

static bool focus_view_for_dock_identity(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				!view_matches_dock_identity(server, view, app_id)) {
			continue;
		}
		focus_view(view, view_get_wlr_surface(view));
		return true;
	}
	return false;
}

static bool show_windows_for_dock_identity(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	struct orange_view *view;
	struct orange_view *focus_target = NULL;
	bool found = false;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				!view_matches_dock_identity(server, view, app_id)) {
			continue;
		}
		found = true;
		if (view->minimized) {
			set_view_minimized(view, false);
		}
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
		focus_target = view;
	}
	if (focus_target != NULL) {
		focus_view(focus_target, view_get_wlr_surface(focus_target));
	}
	return found;
}

static bool hide_windows_for_dock_identity(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	bool found = false;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				!view_matches_dock_identity(server, view, app_id)) {
			continue;
		}
		found = true;
		set_view_minimized(view, true);
	}
	return found;
}

static bool close_windows_for_dock_identity(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	bool found = false;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				!view_matches_dock_identity(server, view, app_id)) {
			continue;
		}
		found = true;
		if (view_is_xwayland(view)) {
			wlr_xwayland_surface_close(view->xwayland_surface);
		} else {
			wlr_xdg_toplevel_send_close(view->xdg_surface->toplevel);
		}
	}
	return found;
}

static void launch_dock_app_id(
		struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return;
	}
	if (focus_view_for_dock_identity(server, app_id)) {
		return;
	}
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_dock_identity(server, app_id);
	if (entry != NULL) {
		launch_desktop_entry(entry);
		return;
	}
	const char *builtin = orange_dock_builtin_command(app_id);
	if (builtin != NULL) {
		if (orange_desktop_entry_id_matches(app_id, "org.gnome.Settings") ||
				orange_desktop_entry_id_matches(app_id,
					"org.gnome.Settings.desktop")) {
			launch_session_command(builtin);
		} else {
			launch_command(builtin);
		}
		return;
	}
	struct orange_config temp_config = server->config;
	snprintf(temp_config.dock_apps[0], sizeof(temp_config.dock_apps[0]),
		"%s", app_id);
	for (int i = 1; i < ORANGE_DOCK_MAX; i++) {
		temp_config.dock_apps[i][0] = '\0';
	}
	const char *command = orange_dock_command(0,
		server->desktop_entries, (int)server->desktop_entry_count,
		&temp_config);
	if (command != NULL) {
		launch_command(command);
	}
}

static bool focus_view_for_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return false;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		if (dock_launcher_for_view(server, view) == launcher_idx) {
			focus_view(view, view_get_wlr_surface(view));
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
		const char *app_id = view_get_app_id(view);
		if (app_id != NULL && strcasestr(app_id, needle) != NULL) {
			struct wlr_surface *surface = view_get_wlr_surface(view);
			if (surface != NULL) {
				focus_view(view, surface);
				return true;
			}
		}
	}
	return false;
}

static bool show_windows_for_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return false;
	}
	struct orange_view *view;
	struct orange_view *focus_target = NULL;
	bool found = false;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				dock_launcher_for_view(server, view) != launcher_idx) {
			continue;
		}
		found = true;
		if (view->minimized) {
			set_view_minimized(view, false);
		}
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
		focus_target = view;
	}
	if (focus_target != NULL) {
		focus_view(focus_target, view_get_wlr_surface(focus_target));
	}
	return found;
}

static bool hide_windows_for_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return false;
	}
	bool found = false;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				dock_launcher_for_view(server, view) != launcher_idx) {
			continue;
		}
		found = true;
		set_view_minimized(view, true);
	}
	return found;
}

static bool close_windows_for_dock_launcher(
		struct orange_server *server,
		int launcher_idx) {
	if (server == NULL || launcher_idx < 0) {
		return false;
	}
	bool found = false;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped ||
				dock_launcher_for_view(server, view) != launcher_idx) {
			continue;
		}
		found = true;
		if (view_is_xwayland(view)) {
			wlr_xwayland_surface_close(view->xwayland_surface);
		} else {
			wlr_xdg_toplevel_send_close(view->xdg_surface->toplevel);
		}
	}
	return found;
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

static int server_dock_temporary_index(
		const struct orange_server *server,
		const char *app_id) {
	if (server == NULL || app_id == NULL || app_id[0] == '\0') {
		return -1;
	}
	int count = server->dock_temporary_count;
	if (count < 0) {
		count = 0;
	}
	if (count > ORANGE_DOCK_MAX) {
		count = ORANGE_DOCK_MAX;
	}
	for (int i = 0; i < count; i++) {
		if (dock_identity_matches(server->dock_temporary_app_ids[i], app_id)) {
			return i;
		}
	}
	return -1;
}

static bool server_dock_temporary_open_for_id(
		const struct orange_server *server,
		const char *app_id) {
	int index = server_dock_temporary_index(server, app_id);
	return index >= 0 && index < ORANGE_DOCK_MAX &&
		server->dock_temporary_open[index];
}

static void server_update_dock_temporary(struct orange_server *server) {
	char before_ids[ORANGE_DOCK_MAX][128];
	bool before_open[ORANGE_DOCK_MAX];
	int before_count = server->dock_temporary_count;
	memcpy(before_ids, server->dock_temporary_app_ids, sizeof(before_ids));
	memcpy(before_open, server->dock_temporary_open, sizeof(before_open));

	server->dock_temporary_count = orange_dock_temporary_prune_config(
		server->dock_temporary_app_ids,
		server->dock_temporary_count,
		&server->config);
	memset(server->dock_temporary_open, 0,
		sizeof(server->dock_temporary_open));
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		int index = dock_launcher_for_view(server, view);
		if (index >= 0) {
			continue;
		}
		char identity[128];
		if (!view_dock_identity(server, view, identity, sizeof(identity))) {
			continue;
		}
		orange_dock_temporary_add(server->dock_temporary_app_ids,
			&server->dock_temporary_count, identity, &server->config);
		int temp_index = server_dock_temporary_index(server, identity);
		if (temp_index >= 0 && temp_index < ORANGE_DOCK_MAX) {
			server->dock_temporary_open[temp_index] = true;
		}
	}
	if (before_count != server->dock_temporary_count ||
			memcmp(before_ids, server->dock_temporary_app_ids,
				sizeof(before_ids)) != 0 ||
			memcmp(before_open, server->dock_temporary_open,
				sizeof(before_open)) != 0) {
		server->launcher_filter_cache_key[0] = '\0';
	}
}

static bool view_has_minimized_dock_slot(
		const struct orange_view *view,
		const struct orange_view *pending_view) {
	return view != NULL && view->mapped && view->minimized_snapshot != NULL &&
		(view->minimized || view == pending_view);
}

static int server_minimized_dock_count(
		const struct orange_server *server,
		const struct orange_view *pending_view) {
	if (server == NULL) {
		return 0;
	}
	int count = 0;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view_has_minimized_dock_slot(view, pending_view) &&
				count < ORANGE_DOCK_MAX) {
			count++;
		}
	}
	return count;
}

static const char *view_minimized_dock_title(const struct orange_view *view) {
	if (view == NULL) {
		return "Minimized Window";
	}
	const char *title = view_get_title((struct orange_view *)view);
	if (title == NULL || title[0] == '\0') {
		return "Minimized Window";
	}
	return title;
}

static void server_populate_minimized_dock_titles(
		const struct orange_server *server,
		struct orange_shell_state *state) {
	if (server == NULL || state == NULL) {
		return;
	}
	memset(state->dock_minimized_titles, 0,
		sizeof(state->dock_minimized_titles));
	int index = 0;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view_has_minimized_dock_slot(view, NULL)) {
			continue;
		}
		if (index >= ORANGE_DOCK_MAX) {
			break;
		}
		snprintf(state->dock_minimized_titles[index],
			sizeof(state->dock_minimized_titles[index]),
			"%s", view_minimized_dock_title(view));
		index++;
	}
}

static int server_minimized_dock_index(
		const struct orange_server *server,
		const struct orange_view *needle,
		const struct orange_view *pending_view) {
	if (server == NULL || needle == NULL) {
		return -1;
	}
	int index = 0;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view_has_minimized_dock_slot(view, pending_view)) {
			continue;
		}
		if (view == needle) {
			return index;
		}
		index++;
	}
	return -1;
}

static struct orange_view *server_minimized_view_for_dock_index(
		struct orange_server *server,
		int minimized_index) {
	if (server == NULL || minimized_index < 0) {
		return NULL;
	}
	int index = 0;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view_has_minimized_dock_slot(view, NULL)) {
			continue;
		}
		if (index == minimized_index) {
			return view;
		}
		index++;
	}
	return NULL;
}

static struct orange_view *server_minimized_view_for_visible_dock_index(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		int visible_index) {
	if (layout == NULL || visible_index < 0 ||
			visible_index >= layout->dock_item_count ||
			visible_index >= ORANGE_DOCK_MAX ||
			!layout->dock_minimized[visible_index]) {
		return NULL;
	}
	return server_minimized_view_for_dock_index(server,
		layout->dock_minimized_indices[visible_index]);
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
			!server->focused_view->mapped) {
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

	const char *app_id = view_get_app_id(server->focused_view);
	format_app_id_label(buffer, buffer_size, app_id);
	if (buffer[0] != '\0') {
		return;
	}

	const char *title = view_get_title(server->focused_view);
	if (title != NULL && title[0] != '\0') {
		snprintf(buffer, buffer_size, "%s", title);
	}
}

static const char *focused_app_id(struct orange_server *server) {
	if (server == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped) {
		return NULL;
	}
	return view_get_app_id(server->focused_view);
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

static int64_t focused_view_client_pid(struct orange_server *server) {
	if (server == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped) {
		return 0;
	}

	struct wlr_surface *surface =
		view_get_wlr_surface(server->focused_view);
	if (surface == NULL || surface->resource == NULL) {
		return 0;
	}

	struct wl_client *client = wl_resource_get_client(surface->resource);
	if (client == NULL) {
		return 0;
	}
	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	(void)uid;
	(void)gid;
	return pid > 0 ? (int64_t)pid : 0;
}

static GDBusConnection *atspi_bus_connection(void) {
	static GDBusConnection *connection = NULL;
	static char address[512];

	if (connection != NULL && !g_dbus_connection_is_closed(connection)) {
		return g_object_ref(connection);
	}
	if (connection != NULL) {
		g_object_unref(connection);
		connection = NULL;
	}

	GDBusConnection *session = dbus_cached_connection(G_BUS_TYPE_SESSION);
	if (session == NULL) {
		return NULL;
	}

	if (address[0] == '\0') {
		GError *error = NULL;
		GVariant *reply = g_dbus_connection_call_sync(
			session,
			"org.a11y.Bus",
			"/org/a11y/bus",
			"org.a11y.Bus",
			"GetAddress",
			NULL,
			G_VARIANT_TYPE("(s)"),
			G_DBUS_CALL_FLAGS_NONE,
			500,
			NULL,
			&error);
		if (error != NULL) {
			g_error_free(error);
		}
		if (reply == NULL) {
			return NULL;
		}
		const char *addr = NULL;
		g_variant_get(reply, "(&s)", &addr);
		if (addr != NULL && addr[0] != '\0') {
			snprintf(address, sizeof(address), "%s", addr);
		}
		g_variant_unref(reply);
		if (address[0] == '\0') {
			return NULL;
		}
	}

	GError *error = NULL;
	connection = g_dbus_connection_new_for_address_sync(
		address,
		G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
			G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
		NULL,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	return connection != NULL ? g_object_ref(connection) : NULL;
}

static bool atspi_get_accessible_property_string(
		GDBusConnection *connection,
		const char *bus_name,
		const char *object_path,
		const char *property,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return false;
	}
	buffer[0] = '\0';
	if (connection == NULL || bus_name == NULL || object_path == NULL ||
			property == NULL) {
		return false;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		"org.freedesktop.DBus.Properties",
		"Get",
		g_variant_new("(ss)", "org.a11y.atspi.Accessible", property),
		G_VARIANT_TYPE("(v)"),
		G_DBUS_CALL_FLAGS_NONE,
		80,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return false;
	}

	GVariant *value = NULL;
	g_variant_get(reply, "(v)", &value);
	if (value != NULL && g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
		const char *text = g_variant_get_string(value, NULL);
		clean_imported_menu_label(text, buffer, buffer_size);
	}
	if (value != NULL) {
		g_variant_unref(value);
	}
	g_variant_unref(reply);
	return buffer[0] != '\0';
}

static int atspi_get_action_count(
		GDBusConnection *connection,
		const char *bus_name,
		const char *object_path) {
	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		"org.freedesktop.DBus.Properties",
		"Get",
		g_variant_new("(ss)", "org.a11y.atspi.Action", "NActions"),
		G_VARIANT_TYPE("(v)"),
		G_DBUS_CALL_FLAGS_NONE,
		80,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return 0;
	}

	int count = 0;
	GVariant *value = NULL;
	g_variant_get(reply, "(v)", &value);
	if (value != NULL && g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
		count = g_variant_get_int32(value);
	}
	if (value != NULL) {
		g_variant_unref(value);
	}
	g_variant_unref(reply);
	return count > 0 ? count : 0;
}

static bool atspi_call_string_method(
		GDBusConnection *connection,
		const char *bus_name,
		const char *object_path,
		const char *interface_name,
		const char *method_name,
		GVariant *parameters,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return false;
	}
	buffer[0] = '\0';
	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		interface_name,
		method_name,
		parameters,
		G_VARIANT_TYPE("(s)"),
		G_DBUS_CALL_FLAGS_NONE,
		80,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return false;
	}

	const char *text = NULL;
	g_variant_get(reply, "(&s)", &text);
	clean_imported_menu_label(text, buffer, buffer_size);
	g_variant_unref(reply);
	return buffer[0] != '\0';
}

static bool atspi_role_is_actionable(const char *role_name) {
	if (role_name == NULL || role_name[0] == '\0') {
		return true;
	}
	return contains_case(role_name, "button") ||
		contains_case(role_name, "menu") ||
		contains_case(role_name, "tab") ||
		contains_case(role_name, "combo") ||
		contains_case(role_name, "link") ||
		contains_case(role_name, "switch") ||
		contains_case(role_name, "toggle");
}

static bool atspi_label_already_used(
		const struct orange_app_menu_model *menu,
		const char *label) {
	if (menu == NULL || label == NULL) {
		return false;
	}
	for (int i = 0; i < menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS]; i++) {
		if (strcmp(menu->items[ORANGE_APP_MENU_TAB_TOOLS][i].label,
				label) == 0) {
			return true;
		}
	}
	return false;
}

static void atspi_maybe_add_action(
		struct orange_server *server,
		struct orange_app_menu_model *menu,
		const char *bus_name,
		const char *object_path,
		const char *label,
		const char *role_name,
		int action_count) {
	if (server == NULL || menu == NULL ||
			bus_name == NULL || object_path == NULL ||
			label == NULL || label[0] == '\0' ||
			action_count <= 0 ||
			server->atspi_action_count >= ORANGE_APP_MENU_ITEM_MAX ||
			menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] >=
				ORANGE_APP_MENU_ITEM_MAX ||
			!atspi_role_is_actionable(role_name) ||
			atspi_label_already_used(menu, label)) {
		return;
	}

	int ref_index = server->atspi_action_count++;
	struct orange_atspi_action_ref *ref = &server->atspi_actions[ref_index];
	snprintf(ref->bus_name, sizeof(ref->bus_name), "%s", bus_name);
	snprintf(ref->object_path, sizeof(ref->object_path), "%s", object_path);
	ref->action_index = 0;

	int item_index = menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS]++;
	struct orange_app_menu_item *item =
		&menu->items[ORANGE_APP_MENU_TAB_TOOLS][item_index];
	snprintf(item->label, sizeof(item->label), "%s", label);
	snprintf(item->action, sizeof(item->action), "atspi:%d", ref_index);
	item->enabled = true;
	item->separator = false;
	snprintf(menu->tab_labels[ORANGE_APP_MENU_TAB_TOOLS],
		sizeof(menu->tab_labels[ORANGE_APP_MENU_TAB_TOOLS]), "%s",
		"Actions");
	if (menu->tab_count <= ORANGE_APP_MENU_TAB_TOOLS) {
		menu->tab_count = ORANGE_APP_MENU_TAB_TOOLS + 1;
	}
	menu->available = true;
	menu->native = true;
}

static void atspi_scan_actions(
		struct orange_server *server,
		GDBusConnection *connection,
		const char *bus_name,
		const char *object_path,
		struct orange_app_menu_model *menu,
		int depth,
		int *nodes_seen) {
	if (server == NULL || connection == NULL || bus_name == NULL ||
			object_path == NULL || object_path[0] != '/' ||
			menu == NULL || depth > 5 || nodes_seen == NULL ||
			*nodes_seen >= ORANGE_ATSPI_SCAN_NODE_MAX ||
			menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] >=
				ORANGE_APP_MENU_ITEM_MAX) {
		return;
	}
	if (strcmp(object_path, "/org/a11y/atspi/null") == 0) {
		return;
	}
	(*nodes_seen)++;

	char label[ORANGE_APP_MENU_LABEL_MAX];
	char role[64];
	atspi_get_accessible_property_string(connection, bus_name,
		object_path, "Name", label, sizeof(label));
	atspi_call_string_method(connection, bus_name, object_path,
		"org.a11y.atspi.Accessible", "GetRoleName", NULL,
		role, sizeof(role));
	int action_count = atspi_get_action_count(connection, bus_name,
		object_path);
	atspi_maybe_add_action(server, menu, bus_name, object_path,
		label, role, action_count);

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		bus_name,
		object_path,
		"org.a11y.atspi.Accessible",
		"GetChildren",
		NULL,
		G_VARIANT_TYPE("(a(so))"),
		G_DBUS_CALL_FLAGS_NONE,
		50,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return;
	}

	GVariant *children = NULL;
	g_variant_get(reply, "(@a(so))", &children);
	GVariantIter iter;
	const char *child_bus = NULL;
	const char *child_path = NULL;
	g_variant_iter_init(&iter, children);
	while (g_variant_iter_next(&iter, "(&s&o)", &child_bus, &child_path)) {
		atspi_scan_actions(server, connection,
			child_bus != NULL && child_bus[0] != '\0' ? child_bus : bus_name,
			child_path, menu, depth + 1, nodes_seen);
		if (*nodes_seen >= ORANGE_ATSPI_SCAN_NODE_MAX ||
				menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] >=
					ORANGE_APP_MENU_ITEM_MAX) {
			break;
		}
	}
	g_variant_unref(children);
	g_variant_unref(reply);
}

static bool import_atspi_actions_for_pid(
		struct orange_server *server,
		int64_t focused_pid,
		struct orange_app_menu_model *menu) {
	if (server == NULL || focused_pid <= 0 || menu == NULL) {
		return false;
	}

	GDBusConnection *connection = atspi_bus_connection();
	if (connection == NULL) {
		return false;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		"org.a11y.atspi.Registry",
		"/org/a11y/atspi/accessible/root",
		"org.a11y.atspi.Accessible",
		"GetChildren",
		NULL,
		G_VARIANT_TYPE("(a(so))"),
		G_DBUS_CALL_FLAGS_NONE,
		100,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		g_object_unref(connection);
		return false;
	}

	GVariant *children = NULL;
	g_variant_get(reply, "(@a(so))", &children);
	GVariantIter iter;
	const char *app_bus = NULL;
	const char *app_path = NULL;
	bool found = false;
	g_variant_iter_init(&iter, children);
	while (g_variant_iter_next(&iter, "(&s&o)", &app_bus, &app_path)) {
		if (dbus_connection_pid_for_name(connection, app_bus) !=
				focused_pid) {
			continue;
		}
		int nodes_seen = 0;
		atspi_scan_actions(server, connection, app_bus, app_path,
			menu, 0, &nodes_seen);
		found = menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] > 0;
		break;
	}
	g_variant_unref(children);
	g_variant_unref(reply);
	g_object_unref(connection);
	if (found) {
		snprintf(server->app_menu_bus_name,
			sizeof(server->app_menu_bus_name), "%s", "atspi");
		server->app_menu_action_path[0] = '\0';
	}
	return found;
}



static void format_action_label(
		const char *action,
		char *buffer,
		size_t buffer_size) {
	if (buffer == NULL || buffer_size == 0) {
		return;
	}
	buffer[0] = '\0';
	if (action == NULL || action[0] == '\0') {
		return;
	}

	const char *start = strchr(action, '.');
	start = start != NULL && start[1] != '\0' ? start + 1 : action;
	size_t out = 0;
	bool capitalize = true;
	for (size_t i = 0; start[i] != '\0' && out + 1 < buffer_size; i++) {
		char c = start[i];
		if (c == '-' || c == '_' || c == '.') {
			if (out > 0 && buffer[out - 1] != ' ') {
				buffer[out++] = ' ';
			}
			capitalize = true;
			continue;
		}
		if (capitalize && c >= 'a' && c <= 'z') {
			c = (char)(c - 'a' + 'A');
		}
		buffer[out++] = c;
		capitalize = false;
	}
	buffer[out] = '\0';
}

static bool action_name_is_useful_menu_item(const char *action) {
	if (action == NULL || action[0] == '\0') {
		return false;
	}
	static const char *const useful[] = {
		"about",
		"preferences",
		"settings",
		"new",
		"new-window",
		"open",
		"save",
		"save-as",
		"print",
		"find",
		"undo",
		"redo",
		"cut",
		"copy",
		"paste",
		"select-all",
		"zoom-in",
		"zoom-out",
		"zoom-normal",
		"fullscreen",
		"quit",
		NULL,
	};
	const char *name = strchr(action, '.');
	name = name != NULL && name[1] != '\0' ? name + 1 : action;
	for (int i = 0; useful[i] != NULL; i++) {
		if (strcasecmp(name, useful[i]) == 0) {
			return true;
		}
	}
	return false;
}

static bool import_gactions_for_bus_name(
		struct orange_server *server,
		GDBusConnection *connection,
		const char *bus_name,
		const char *object_path,
		struct orange_app_menu_model *menu) {
	if (server == NULL || connection == NULL || bus_name == NULL ||
			object_path == NULL || object_path[0] != '/' || menu == NULL) {
		return false;
	}

	GDBusActionGroup *group = g_dbus_action_group_get(
		connection, bus_name, object_path);
	if (group == NULL) {
		return false;
	}

	GActionGroup *actions = G_ACTION_GROUP(group);
	gchar **names = g_action_group_list_actions(actions);
	for (int i = 0; names != NULL && names[i] != NULL &&
			menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] <
				ORANGE_APP_MENU_ITEM_MAX; i++) {
		if (!action_name_is_useful_menu_item(names[i])) {
			continue;
		}
		int item_index = menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS]++;
		struct orange_app_menu_item *item =
			&menu->items[ORANGE_APP_MENU_TAB_TOOLS][item_index];
		format_action_label(names[i], item->label, sizeof(item->label));
		snprintf(item->action, sizeof(item->action), "%s", names[i]);
		item->enabled = g_action_group_get_action_enabled(actions, names[i]);
		item->separator = false;
	}
	g_strfreev(names);
	g_object_unref(group);

	if (menu->item_counts[ORANGE_APP_MENU_TAB_TOOLS] <= 0) {
		return false;
	}
	snprintf(menu->tab_labels[ORANGE_APP_MENU_TAB_TOOLS],
		sizeof(menu->tab_labels[ORANGE_APP_MENU_TAB_TOOLS]), "%s",
		"Actions");
	menu->tab_count = ORANGE_APP_MENU_TAB_TOOLS + 1;
	menu->available = true;
	menu->native = true;
	snprintf(server->app_menu_bus_name,
		sizeof(server->app_menu_bus_name), "%s", bus_name);
	snprintf(server->app_menu_action_path,
		sizeof(server->app_menu_action_path), "%s", object_path);
	wlr_log(WLR_INFO, "imported GActions app menu from %s at %s",
		bus_name, object_path);
	return true;
}

static bool import_gactions_for_focused_pid(
		struct orange_server *server,
		GDBusConnection *connection,
		int64_t focused_pid,
		struct orange_app_menu_model *menu) {
	if (server == NULL || connection == NULL ||
			focused_pid <= 0 || menu == NULL) {
		return false;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"org.freedesktop.DBus",
		"ListNames",
		NULL,
		G_VARIANT_TYPE("(as)"),
		G_DBUS_CALL_FLAGS_NONE,
		200,
		NULL,
		&error);
	if (error != NULL) {
		g_error_free(error);
	}
	if (reply == NULL) {
		return false;
	}

	static const char *const action_paths[] = {
		"/org/gtk/Actions",
		"/org/gtk/actions",
		NULL,
	};
	char **names = NULL;
	g_variant_get(reply, "(^as)", &names);
	bool imported = false;
	for (int i = 0; names != NULL && names[i] != NULL; i++) {
		const char *name = names[i];
		if (name[0] == ':' ||
				strcmp(name, "org.freedesktop.DBus") == 0 ||
				strcmp(name, "com.canonical.AppMenu.Registrar") == 0) {
			continue;
		}
		if (dbus_connection_pid_for_name(connection, name) != focused_pid) {
			continue;
		}
		for (int p = 0; action_paths[p] != NULL; p++) {
			if (import_gactions_for_bus_name(server, connection,
					name, action_paths[p], menu)) {
				imported = true;
				break;
			}
		}
		if (imported) {
			break;
		}
	}
	g_strfreev(names);
	g_variant_unref(reply);
	return imported;
}

static bool app_id_has_app_menu(const char *app_id) {
	if (app_id == NULL || app_id[0] == '\0') {
		return true;
	}
	static const char *const no_menu_apps[] = {
		"gnome-control-center",
		"org.gnome.ControlCenter",
		"org.gnome.Settings",
		"gnome-settings",
		"gnome-terminal",
		"org.gnome.Terminal",
		"org.gnome.Software",
		"gnome-software",
		"org.gnome.Nautilus",
		"nautilus",
		"org.gnome.Calendar",
		"gnome-calendar",
		"org.gnome.Contacts",
		"gnome-contacts",
		"org.gnome.Maps",
		"gnome-maps",
		"org.gnome.Weather",
		"gnome-weather",
		"org.gnome.eog",
		"eog",
		"org.gnome.Evince",
		"evince",
		"org.gnome.Loupe",
		"loupe",
		"org.gnome.Calculator",
		"gnome-calculator",
		"org.gnome.clocks",
		"gnome-clocks",
		"org.gnome.seahorse",
		"seahorse",
		"org.gnome.Totem",
		"totem",
		"org.gnome.baobab",
		"baobab",
		"org.gnome.Characters",
		"gnome-characters",
		"org.gnome.DiskUtility",
		"gnome-disk-utility",
		"org.gnome.Logs",
		"gnome-logs",
		"org.gnome.SystemMonitor",
		"gnome-system-monitor",
		"org.gnome.Extensions",
		"gnome-extensions",
		NULL,
	};
	if (strchr(app_id, '.') != NULL && strstr(app_id, "control-center") == NULL &&
			strstr(app_id, "settings") == NULL) {
		return true;
	}
	for (int i = 0; no_menu_apps[i] != NULL; i++) {
		if (strcasecmp(app_id, no_menu_apps[i]) == 0) {
			return false;
		}
	}
	return true;
}

static void server_update_app_menu_model(struct orange_server *server) {
	char active_label[ORANGE_APP_MENU_LABEL_MAX];
	server_active_app_label(server, active_label, sizeof(active_label));
	const char *app_id = focused_app_id(server);
	if (!app_id_has_app_menu(app_id)) {
		return;
	}
	int64_t focused_pid = focused_view_client_pid(server);
	uint32_t now = monotonic_time_msec();
	bool same_key = server->app_menu_cached_pid == focused_pid &&
		server->app_menu_cached_app_id == app_id;
	if (same_key) {
		if (server->app_menu.available ||
				server->app_menu_retry_until_ms == 0 ||
				monotonic_msec_reached(now,
					server->app_menu_retry_until_ms) ||
				!monotonic_msec_reached(now,
					server->app_menu_next_probe_ms)) {
			return;
		}
	} else {
		server->app_menu_cached_app_id = app_id;
		server->app_menu_cached_pid = focused_pid;
		server->app_menu_next_probe_ms = now;
		server->app_menu_retry_until_ms =
			now + ORANGE_APP_MENU_DISCOVERY_WINDOW_MS;
		server->app_menu_tried_atspi = false;
	}
	server->app_menu_next_probe_ms =
		now + ORANGE_APP_MENU_DISCOVERY_RETRY_MS;
	app_menu_model_reset(&server->app_menu);
	memset(server->atspi_actions, 0, sizeof(server->atspi_actions));
	server->atspi_action_count = 0;
	server->app_menu_bus_name[0] = '\0';
	server->app_menu_action_path[0] = '\0';

	GDBusConnection *connection = dbus_cached_connection(G_BUS_TYPE_SESSION);
	if (connection == NULL) {
		return;
	}

	if (!server->app_menu.available) {
		import_gactions_for_focused_pid(server, connection, focused_pid,
			&server->app_menu);
	}
	if (server->app_menu.available) {
		server->app_menu_retry_until_ms = 0;
		server->app_menu_next_probe_ms = 0;
		return;
	}
	if (!server->app_menu_tried_atspi) {
		server->app_menu_tried_atspi = true;
		import_atspi_actions_for_pid(server, focused_pid,
			&server->app_menu);
	}
	if (server->app_menu.available) {
		server->app_menu_retry_until_ms = 0;
		server->app_menu_next_probe_ms = 0;
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

static void output_effective_size(
		struct orange_output *output,
		int *width,
		int *height) {
	int effective_width = 0;
	int effective_height = 0;
	if (output != NULL && output->wlr_output != NULL) {
		wlr_output_effective_resolution(output->wlr_output,
			&effective_width, &effective_height);
	}
	if (effective_width <= 0) {
		effective_width = output != NULL && output->width > 0 ?
			output->width :
			(output != NULL && output->server != NULL &&
				output->server->options != NULL ?
				output->server->options->width : 1);
	}
	if (effective_height <= 0) {
		effective_height = output != NULL && output->height > 0 ?
			output->height :
			(output != NULL && output->server != NULL &&
				output->server->options != NULL ?
				output->server->options->height : 1);
	}
	if (width != NULL) {
		*width = effective_width;
	}
	if (height != NULL) {
		*height = effective_height;
	}
}

static bool dock_context_menu_active(const struct orange_server *server) {
	if (server == NULL) {
		return false;
	}
	switch (server->context_menu_kind) {
	case ORANGE_CONTEXT_MENU_DOCK:
	case ORANGE_CONTEXT_MENU_DOCK_RUNNING:
	case ORANGE_CONTEXT_MENU_DOCK_LAUNCHER:
	case ORANGE_CONTEXT_MENU_DOCK_TRASH:
	case ORANGE_CONTEXT_MENU_DOCK_SEPARATOR:
		return true;
	default:
		return false;
	}
}

static bool dock_auto_hide_runtime_revealed(
		const struct orange_server *server) {
	return server != NULL &&
		(server->dock_auto_hide_revealed ||
			server->dock_drag_active ||
			server->launcher_app_drag_active ||
			server->dock_bounce_active ||
			dock_context_menu_active(server));
}

static bool dock_auto_hide_update_animation(
		struct orange_server *server,
		uint32_t now_ms) {
	if (server == NULL) {
		return false;
	}

	double old_progress = server->dock_auto_hide_progress;
	bool old_animating = server->dock_auto_hide_animating;
	if (!server->config.dock_auto_hide) {
		server->dock_auto_hide_progress = 1.0;
		server->dock_auto_hide_animating = false;
		server->dock_auto_hide_target_revealed = false;
		server->dock_auto_hide_anim_start_progress = 1.0;
		server->dock_auto_hide_anim_start = now_ms;
		return old_animating || fabs(old_progress - 1.0) > 0.001;
	}

	bool target_revealed = dock_auto_hide_runtime_revealed(server);
	double current = server->dock_auto_hide_progress;
	if (server->dock_auto_hide_animating) {
		current = orange_dock_auto_hide_progress(
			server->dock_auto_hide_anim_start_progress,
			server->dock_auto_hide_target_revealed,
			now_ms - server->dock_auto_hide_anim_start);
	}

	double target = target_revealed ? 1.0 : 0.0;
	if (target_revealed != server->dock_auto_hide_target_revealed ||
			(!server->dock_auto_hide_animating &&
			 fabs(current - target) > 0.001)) {
		server->dock_auto_hide_target_revealed = target_revealed;
		server->dock_auto_hide_anim_start = now_ms;
		server->dock_auto_hide_anim_start_progress =
			clamp(current, 0.0, 1.0);
		server->dock_auto_hide_animating =
			fabs(server->dock_auto_hide_anim_start_progress - target) > 0.001;
	}

	if (server->dock_auto_hide_animating) {
		uint32_t elapsed = now_ms - server->dock_auto_hide_anim_start;
		current = orange_dock_auto_hide_progress(
			server->dock_auto_hide_anim_start_progress,
			server->dock_auto_hide_target_revealed,
			elapsed);
		if (elapsed >= ORANGE_DOCK_AUTO_HIDE_DURATION_MS) {
			current = target;
			server->dock_auto_hide_animating = false;
		}
	} else {
		current = target;
	}
	server->dock_auto_hide_progress = clamp(current, 0.0, 1.0);
	return old_animating != server->dock_auto_hide_animating ||
		fabs(old_progress - server->dock_auto_hide_progress) > 0.001;
}

static bool dock_auto_hide_overlay_active(const struct orange_server *server) {
	return server != NULL && server->config.dock_auto_hide &&
		(dock_auto_hide_runtime_revealed(server) ||
		 server->dock_auto_hide_animating ||
		 server->dock_auto_hide_progress > 0.0);
}

static bool dock_auto_hide_blocked_for_layout(
		struct orange_server *server,
		struct orange_output *output,
		const struct orange_shell_layout *layout) {
	if (server == NULL || output == NULL || layout == NULL ||
			!server->config.dock_auto_hide ||
			layout->dock.width <= 0 || layout->dock.height <= 0) {
		return false;
	}
	(void)output;
	return true;
}

static bool dock_auto_hide_blocked_for_output(
		struct orange_server *server,
		struct orange_output *output) {
	if (server == NULL || output == NULL || !server->config.dock_auto_hide) {
		return false;
	}
	server_refresh_dock_state(server);

	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	struct orange_shell_layout layout;
	orange_shell_layout_compute_with_dock_state(width, height,
		server->system_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		server->desktop_volume_count,
		server->desktop_files,
		server->desktop_file_count,
		server->dock_temporary_count,
		server->dock_temporary_app_ids,
		server_minimized_dock_count(server, NULL),
		&layout);
	return dock_auto_hide_blocked_for_layout(server, output, &layout);
}

static void compute_shell_layout_for_output(
		struct orange_server *server,
		struct orange_output *output,
		struct orange_shell_layout *layout) {
	server_update_app_menu_model(server);
	server_refresh_dock_state(server);
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	orange_shell_layout_compute_with_dock_state(width, height,
		server->system_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		server->desktop_volume_count,
		server->desktop_files,
		server->desktop_file_count,
		server->dock_temporary_count,
		server->dock_temporary_app_ids,
		server_minimized_dock_count(server, NULL),
		layout);
	bool dock_auto_hide_blocked =
		dock_auto_hide_blocked_for_layout(server, output, layout);
	dock_auto_hide_update_animation(server, monotonic_time_msec());
	orange_shell_layout_apply_dock_auto_hide_progress(layout, &server->config,
		dock_auto_hide_blocked,
		dock_auto_hide_runtime_revealed(server),
		server->dock_auto_hide_progress);
	struct orange_status_notifier_item status_notifier_items[
		ORANGE_STATUS_NOTIFIER_ITEM_MAX];
	int status_notifier_item_count = server_copy_status_notifier_items(server,
		status_notifier_items, ORANGE_STATUS_NOTIFIER_ITEM_MAX);
	orange_shell_layout_set_status_notifier_items(layout,
		status_notifier_items, status_notifier_item_count);
	layout->open_with_app_count = server->open_with_app_count;
	layout->open_with_submenu_open = server->open_with_submenu_open;
	layout->dock_options_submenu_open = server->dock_options_submenu_open;
	layout->dock_minimize_submenu_open = server->dock_minimize_submenu_open;
	layout->dock_position_submenu_open = server->dock_position_submenu_open;
	layout->context_menu_alt_pressed = server->context_menu_alt_pressed;
	layout->context_menu_dock_temporary = server->context_menu_dock_temporary;
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
		orange_shell_layout_set_notification_center(layout, 0);
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

static bool desktop_item_info_for_context_target(
		struct orange_server *server,
		int target,
		struct orange_desktop_item_info *info) {
	if (server == NULL || info == NULL || target < 0) {
		return false;
	}
	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	(void)local_x;
	(void)local_y;
	if (output == NULL) {
		return false;
	}
	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	if (target >= layout.desktop_item_count) {
		return false;
	}
	*info = layout.desktop_item_info[target];
	return true;
}

static int desktop_file_index_for_context_target(
		struct orange_server *server,
		int target) {
	struct orange_desktop_item_info info;
	if (desktop_item_info_for_context_target(server, target, &info) &&
			info.kind == ORANGE_DESKTOP_ITEM_FILE) {
		return info.index;
	}
	return target >= 0 && target < server->desktop_file_count ? target : -1;
}

static int desktop_volume_index_for_context_target(
		struct orange_server *server,
		int target) {
	struct orange_desktop_item_info info;
	if (desktop_item_info_for_context_target(server, target, &info) &&
			info.kind == ORANGE_DESKTOP_ITEM_VOLUME) {
		return info.index;
	}
	int fallback = target - server->desktop_file_count;
	return fallback >= 0 && fallback < server->desktop_volume_count ?
		fallback : -1;
}

struct desktop_selection_summary {
	int count;
	int file_count;
	int volume_count;
	bool includes_target;
};

static struct desktop_selection_summary desktop_selection_summary_for_layout(
		const struct orange_server *server,
		const struct orange_shell_layout *layout,
		int target) {
	struct desktop_selection_summary summary = {0};
	if (server == NULL || layout == NULL) {
		return summary;
	}
	for (int i = 0; i < layout->desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (!server->desktop_selected[i]) {
			continue;
		}
		summary.count++;
		if (i == target) {
			summary.includes_target = true;
		}
		switch (layout->desktop_item_info[i].kind) {
		case ORANGE_DESKTOP_ITEM_FILE:
			summary.file_count++;
			break;
		case ORANGE_DESKTOP_ITEM_VOLUME:
			summary.volume_count++;
			break;
		default:
			break;
		}
	}
	return summary;
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
		if (!output->commit_pending && !output->wlr_output->frame_pending
				&& !output->commit_failed) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void server_status_notifier_changed(void *data) {
	struct orange_server *server = data;
	if (server != NULL) {
		server_mark_shell_dirty(server);
	}
}

static void server_mark_overlay_dirty(struct orange_server *server) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output->overlay_dirty = true;
		if (!output->commit_pending && !output->wlr_output->frame_pending
				&& !output->commit_failed) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void server_invalidate_dock_launchers(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		view->dock_launcher_index = -1;
	}
}

static void server_mark_dock_dirty(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	server_invalidate_dock_launchers(server);
	server->dock_state_dirty = true;
	server->launcher_filter_cache_key[0] = '\0';
	server_mark_shell_dirty(server);
}

static void server_refresh_dock_state(struct orange_server *server) {
	if (server == NULL || !server->dock_state_dirty) {
		return;
	}
	server_update_dock_open(server);
	server_update_dock_temporary(server);
	server->dock_state_dirty = false;
}

static bool rect_contains_point_with_margin(
		struct orange_rect rect,
		int x,
		int y,
		int margin) {
	return rect.width > 0 && rect.height > 0 &&
		x >= rect.x - margin &&
		y >= rect.y - margin &&
		x < rect.x + rect.width + margin &&
		y < rect.y + rect.height + margin;
}

static void set_dock_auto_hide_revealed(
		struct orange_server *server,
		bool revealed) {
	if (server == NULL || server->dock_auto_hide_revealed == revealed) {
		return;
	}
	server->dock_auto_hide_revealed = revealed;
	if (!revealed) {
		server->hot_dock_index = -1;
		server->last_dock_pointer_x = INT_MIN;
		server->last_dock_pointer_y = INT_MIN;
	}
	dock_auto_hide_update_animation(server, monotonic_time_msec());
	server_mark_overlay_dirty(server);
}

static void update_dock_auto_hide_for_pointer(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	if (!server->config.dock_auto_hide) {
		set_dock_auto_hide_revealed(server, false);
		return;
	}
	if (server->dock_drag_active || server->dock_resize_active ||
			server->launcher_app_drag_active || dock_context_menu_active(server)) {
		set_dock_auto_hide_revealed(server, true);
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		set_dock_auto_hide_revealed(server, false);
		return;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	if (!layout.dock_auto_hide_blocked) {
		set_dock_auto_hide_revealed(server, false);
		return;
	}
	if (!server->dock_auto_hide_revealed) {
		if (layout.dock_auto_hidden &&
				rect_contains_point_with_margin(
					layout.dock_reveal_zone, local_x, local_y, 0)) {
			set_dock_auto_hide_revealed(server, true);
		}
		return;
	}

	if (!rect_contains_point_with_margin(layout.dock, local_x, local_y, 32)) {
		set_dock_auto_hide_revealed(server, false);
	}
}

static void update_hot_corner_for_pointer(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	if (!server->hot_corners_enabled) {
		server->hot_corner_armed = true;
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		server->hot_corner_armed = true;
		return;
	}

	int size = ORANGE_HOT_CORNER_SIZE;
	bool in_corner = local_x >= 0 && local_y >= 0 &&
		local_x <= size && local_y <= size;
	if (in_corner) {
		if (server->hot_corner_armed && !server->launcher_open) {
			launcher_open_overlay(server, false,
				ORANGE_LAUNCHER_DISPLAY_FULL);
			server_mark_overlay_dirty(server);
		}
		server->hot_corner_armed = false;
		return;
	}
	if (local_x > size * 3 || local_y > size * 3) {
		server->hot_corner_armed = true;
	}
}

static bool output_ensure_shell_buffer(struct orange_output *output) {
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);

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
		wlr_log(WLR_ERROR, "%s", "failed to allocate shell buffer");
		return false;
	}

	output->shell_scene_buffer = wlr_scene_buffer_create(
		output->server->shell_tree,
		&output->shell_buffer->base);
	if (output->shell_scene_buffer == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to create shell scene buffer");
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
	if (output->backdrop_buffer != NULL) {
		wlr_buffer_drop(&output->backdrop_buffer->base);
		output->backdrop_buffer = NULL;
	}
	output->overlay_buffer = orange_buffer_create(width, height);
	if (output->overlay_buffer == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate overlay buffer");
	} else {
		output->overlay_scene_buffer = wlr_scene_buffer_create(
			output->server->overlay_tree,
			&output->overlay_buffer->base);
		if (output->overlay_scene_buffer == NULL) {
			wlr_log(WLR_ERROR, "%s", "failed to create overlay scene buffer");
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
	output->backdrop_buffer = orange_buffer_create(width, height);
	if (output->backdrop_buffer == NULL) {
		wlr_log(WLR_ERROR, "%s",
			"failed to allocate overlay backdrop buffer");
	}

	output->width = width;
	output->height = height;
	output->shell_dirty = true;
	output->dock_dirty = false;
	output->dock_dirty_bounds = (struct orange_rect){0, 0, 0, 0};
	output->overlay_dirty = true;
	output->overlay_bounds_valid = false;
	output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
	server_reload_cursor_scales(output->server);
	return true;
}

static void output_redraw_shell(struct orange_output *output) {
	if (!output_ensure_shell_buffer(output)) {
		return;
	}
	if (!output->shell_dirty && !output->dock_dirty) {
		return;
	}

	double cursor_x = output->server->cursor->x;
	double cursor_y = output->server->cursor->y;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);
	server_update_app_menu_model(output->server);
	server_refresh_dock_state(output->server);
	bool dock_auto_hide_blocked =
		dock_auto_hide_blocked_for_output(output->server, output);
	dock_auto_hide_update_animation(output->server, monotonic_time_msec());
	if (!output->shell_dirty && output->dock_dirty) {
		struct orange_shell_state state = {
			.hot_dock_index = output->server->hot_dock_index,
			.dock_pointer_x = (int)cursor_x,
			.dock_pointer_y = (int)cursor_y,
			.dock_auto_hide_revealed =
				dock_auto_hide_runtime_revealed(output->server),
			.dock_auto_hide_blocked = dock_auto_hide_blocked,
			.dock_auto_hide_animating =
				output->server->dock_auto_hide_animating,
			.dock_auto_hide_progress =
				output->server->dock_auto_hide_progress,
			.now = time(NULL),
			.assets = &output->server->assets,
			.config = &output->server->config,
			.trash_full = output->server->trash_full,
			.status = output->server->status,
			.desktop_entries = output->server->desktop_entries,
			.desktop_entry_count = (int)output->server->desktop_entry_count,
			.volumes = output->server->volumes,
			.volume_count = output->server->volume_count,
			.desktop_volume_count = output->server->desktop_volume_count,
			.desktop_files = output->server->desktop_files,
			.desktop_file_count = output->server->desktop_file_count,
			.desktop_selection_active = output->server->desktop_select_active &&
				output->server->desktop_select_moved,
			.desktop_selection_rect = normalized_rect_from_points(
				output->server->desktop_select_start_x,
				output->server->desktop_select_start_y,
				output->server->desktop_select_x,
				output->server->desktop_select_y),
			.desktop_rename_active = output->server->desktop_rename.active,
			.desktop_rename_index = output->server->desktop_rename.layout_index,
			.dock_drag_index = -1,
			.dock_drag_insert_before = -1,
			.dock_bounce_active = output->server->dock_bounce_active,
			.dock_bounce_launcher_idx =
				output->server->dock_bounce_launcher_idx,
			.dock_bounce_start_time = output->server->dock_bounce_start,
			.dock_temporary_count = output->server->dock_temporary_count,
			.dock_minimized_count =
				server_minimized_dock_count(output->server, NULL),
		};
		memcpy(state.dock_open, output->server->dock_open,
			sizeof(state.dock_open));
		memcpy(state.dock_temporary_app_ids,
			output->server->dock_temporary_app_ids,
			sizeof(state.dock_temporary_app_ids));
		memcpy(state.dock_temporary_open,
			output->server->dock_temporary_open,
			sizeof(state.dock_temporary_open));
		server_populate_minimized_dock_titles(output->server, &state);
		memcpy(state.desktop_selected, output->server->desktop_selected,
			sizeof(state.desktop_selected));
		memcpy(state.desktop_rename_text,
			output->server->desktop_rename.text,
			sizeof(state.desktop_rename_text));
		server_active_app_label(output->server,
			state.active_app_label, sizeof(state.active_app_label));
	state.app_menu = output->server->app_menu;

	state.app_switcher_active = output->server->app_switcher_active;
	state.app_switcher_index = output->server->app_switcher_index;
	state.app_switcher_count = output->server->app_switcher_count;
	for (int i = 0; i < state.app_switcher_count && i < 128; i++) {
		struct orange_view *v = output->server->app_switcher_views[i];
		if (v != NULL && v->mapped) {
			const char *title = view_get_title(v);
			const char *app_id = view_get_app_id(v);
			if (title != NULL) {
				snprintf(state.app_switcher_titles[i],
					sizeof(state.app_switcher_titles[i]),
					"%s", title);
			} else {
				state.app_switcher_titles[i][0] = '\0';
			}
			if (app_id != NULL) {
				snprintf(state.app_switcher_app_ids[i],
					sizeof(state.app_switcher_app_ids[i]),
					"%s", app_id);
			} else {
				state.app_switcher_app_ids[i][0] = '\0';
			}
		} else {
			state.app_switcher_titles[i][0] = '\0';
			state.app_switcher_app_ids[i][0] = '\0';
		}
	}
		const struct orange_shell_draw_options options = {
			.draw_wallpaper = true,
			.skip_transient_overlays = true,
			.skip_dock = output->server->config.dock_auto_hide &&
				dock_auto_hide_blocked,
			.draw_only_dock = true,
			.clip_to_bounds = true,
			.clip_bounds = output->dock_dirty_bounds,
		};
		orange_shell_draw_with_options(output->shell_buffer->pixels,
			output->width,
			output->height,
			output->shell_buffer->stride,
			&state,
			&options);
		if (!options.skip_dock) {
			struct orange_shell_layout snapshot_layout;
			compute_shell_layout_for_output(output->server, output,
				&snapshot_layout);
			draw_minimized_dock_snapshots(output, &snapshot_layout,
				&state, output->shell_buffer, &output->dock_dirty_bounds);
		}
		pixman_region32_t damage;
		pixman_region32_init_rect(&damage,
			output->dock_dirty_bounds.x,
			output->dock_dirty_bounds.y,
			output->dock_dirty_bounds.width,
			output->dock_dirty_bounds.height);
		wlr_scene_buffer_set_buffer_with_damage(output->shell_scene_buffer,
			&output->shell_buffer->base,
			&damage);
		pixman_region32_fini(&damage);
		output->dock_dirty = false;
		output->dock_dirty_bounds = (struct orange_rect){0, 0, 0, 0};
		return;
	}
	struct orange_shell_state state = {
		.system_menu_open = output->server->system_menu_open,
		.notification_center_open = output->server->notification_center_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_pointer_x = (int)cursor_x,
		.dock_pointer_y = (int)cursor_y,
		.dock_auto_hide_revealed =
			dock_auto_hide_runtime_revealed(output->server),
		.dock_auto_hide_blocked = dock_auto_hide_blocked,
		.dock_auto_hide_animating =
			output->server->dock_auto_hide_animating,
		.dock_auto_hide_progress =
			output->server->dock_auto_hide_progress,
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
		.trash_full = output->server->trash_full,
		.status = output->server->status,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.volumes = output->server->volumes,
		.volume_count = output->server->volume_count,
		.desktop_volume_count = output->server->desktop_volume_count,
		.desktop_files = output->server->desktop_files,
		.desktop_file_count = output->server->desktop_file_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
		.open_with_app_count = output->server->open_with_app_count,
		.open_with_submenu_open = output->server->open_with_submenu_open,
		.dock_options_submenu_open = output->server->dock_options_submenu_open,
		.dock_minimize_submenu_open =
			output->server->dock_minimize_submenu_open,
		.dock_position_submenu_open =
			output->server->dock_position_submenu_open,
		.context_menu_alt_pressed = output->server->context_menu_alt_pressed,
		.context_menu_dock_temporary = output->server->context_menu_dock_temporary,
		.desktop_selection_active = output->server->desktop_select_active &&
			output->server->desktop_select_moved,
			.desktop_selection_rect = normalized_rect_from_points(
				output->server->desktop_select_start_x,
				output->server->desktop_select_start_y,
				output->server->desktop_select_x,
				output->server->desktop_select_y),
			.desktop_rename_active = output->server->desktop_rename.active,
			.desktop_rename_index = output->server->desktop_rename.layout_index,
			.launcher_open = output->server->launcher_open,
		.launcher_display_mode = output->server->launcher_display_mode,
		.launcher_position_set = output->server->launcher_position_set,
		.launcher_x = output->server->launcher_x,
		.launcher_y = output->server->launcher_y,
		.launcher_hot_app = output->server->launcher_hot_app,
		.launcher_app_count = output->server->launcher_app_count,
		.launcher_result_count = output->server->launcher_result_count,
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
		.dock_temporary_count = output->server->dock_temporary_count,
		.dock_minimized_count =
			server_minimized_dock_count(output->server, NULL),
	};
	server_populate_status_notifier_items(output->server, &state);
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	memcpy(state.dock_temporary_app_ids, output->server->dock_temporary_app_ids,
		sizeof(state.dock_temporary_app_ids));
	memcpy(state.dock_temporary_open, output->server->dock_temporary_open,
		sizeof(state.dock_temporary_open));
	server_populate_minimized_dock_titles(output->server, &state);
	memcpy(state.desktop_selected, output->server->desktop_selected,
		sizeof(state.desktop_selected));
	memcpy(state.desktop_rename_text, output->server->desktop_rename.text,
		sizeof(state.desktop_rename_text));
	memcpy(state.open_with_app_labels, output->server->open_with_app_labels,
		sizeof(state.open_with_app_labels));
	memcpy(state.open_with_app_icons, output->server->open_with_app_icons,
		sizeof(state.open_with_app_icons));
	memcpy(state.launcher_query, output->server->launcher_query,
		sizeof(state.launcher_query));
	memcpy(state.launcher_app_indices, output->server->launcher_app_indices,
		sizeof(state.launcher_app_indices));
	memcpy(state.launcher_results, output->server->launcher_results,
		sizeof(state.launcher_results));
	memcpy(state.launcher_categories, output->server->launcher_categories,
		sizeof(state.launcher_categories));
	server_active_app_label(output->server,
		state.active_app_label, sizeof(state.active_app_label));
	state.app_menu = output->server->app_menu;
	const struct orange_shell_draw_options options = {
		.draw_wallpaper = true,
		.skip_transient_overlays = true,
		.skip_dock = output->server->config.dock_auto_hide &&
			dock_auto_hide_blocked,
	};
	orange_shell_draw_with_options(output->shell_buffer->pixels,
		output->width,
		output->height,
		output->shell_buffer->stride,
		&state,
		&options);
	if (!options.skip_dock) {
		struct orange_shell_layout snapshot_layout;
		compute_shell_layout_for_output(output->server, output,
			&snapshot_layout);
		draw_minimized_dock_snapshots(output, &snapshot_layout,
			&state, output->shell_buffer, NULL);
	}

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, 0, 0, output->width, output->height);
	wlr_scene_buffer_set_buffer_with_damage(output->shell_scene_buffer,
		&output->shell_buffer->base,
		&damage);
	pixman_region32_fini(&damage);
	output->shell_dirty = false;
	output->dock_dirty = false;
	output->dock_dirty_bounds = (struct orange_rect){0, 0, 0, 0};
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

static struct orange_rect output_clamp_rect(
		struct orange_rect rect,
		int width,
		int height) {
	int x1 = rect.x;
	int y1 = rect.y;
	int x2 = rect.x + rect.width;
	int y2 = rect.y + rect.height;
	if (x1 < 0) {
		x1 = 0;
	}
	if (y1 < 0) {
		y1 = 0;
	}
	if (x2 > width) {
		x2 = width;
	}
	if (y2 > height) {
		y2 = height;
	}
	if (x2 <= x1 || y2 <= y1) {
		return (struct orange_rect){0, 0, 0, 0};
	}
	return (struct orange_rect){x1, y1, x2 - x1, y2 - y1};
}

static void output_mark_dock_visual_dirty(
		struct orange_output *output,
		const struct orange_shell_layout *layout) {
	if (output == NULL || layout == NULL) {
		return;
	}
	if (output->server != NULL && output->server->config.dock_auto_hide &&
			layout->dock_auto_hide_blocked &&
			dock_auto_hide_overlay_active(output->server)) {
		output->overlay_dirty = true;
		if (!output->commit_pending && !output->wlr_output->frame_pending &&
				!output->commit_failed) {
			wlr_output_schedule_frame(output->wlr_output);
		}
		return;
	}
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	struct orange_rect bounds =
		orange_dock_visual_dirty_bounds(layout, width, height);
	if (!output_rect_has_area(&bounds)) {
		return;
	}
	output->dock_dirty_bounds = output->dock_dirty ?
		output_union_rect(output->dock_dirty_bounds, bounds) : bounds;
	output->dock_dirty = true;
	if (!output->commit_pending && !output->wlr_output->frame_pending &&
			!output->commit_failed) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static void server_mark_dock_visual_dirty(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct orange_shell_layout layout;
		compute_shell_layout_for_output(server, output, &layout);
		output_mark_dock_visual_dirty(output, &layout);
	}
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

static void output_normalize_readback_pixels(
		uint32_t *pixels,
		int width,
		int height,
		int stride,
		uint32_t format) {
	if (pixels == NULL || width <= 0 || height <= 0 || stride < width * 4) {
		return;
	}
	for (int y = 0; y < height; y++) {
		uint32_t *row = (uint32_t *)((unsigned char *)pixels +
			(size_t)y * (size_t)stride);
		for (int x = 0; x < width; x++) {
			uint32_t p = row[x];
			switch (format) {
			case DRM_FORMAT_ARGB8888:
				if ((p & 0xff000000u) == 0) {
					p |= 0xff000000u;
				}
				break;
			case DRM_FORMAT_XRGB8888:
				p |= 0xff000000u;
				break;
			case DRM_FORMAT_ABGR8888:
				p = (p & 0xff000000u) |
					((p & 0x000000ffu) << 16) |
					(p & 0x0000ff00u) |
					((p & 0x00ff0000u) >> 16);
				if ((p & 0xff000000u) == 0) {
					p |= 0xff000000u;
				}
				break;
			case DRM_FORMAT_XBGR8888:
				p = 0xff000000u |
					((p & 0x000000ffu) << 16) |
					(p & 0x0000ff00u) |
					((p & 0x00ff0000u) >> 16);
				break;
			default:
				p |= 0xff000000u;
				break;
			}
			row[x] = p;
		}
	}
}

static bool output_read_backdrop_from_state(
		struct orange_output *output,
		struct wlr_output_state *state,
		const struct orange_rect *bounds,
		uint32_t format) {
	if (output == NULL || state == NULL || state->buffer == NULL ||
			output->backdrop_buffer == NULL || !output_rect_has_area(bounds)) {
		return false;
	}
	struct wlr_texture *texture = wlr_texture_from_buffer(
		output->server->renderer, state->buffer);
	if (texture == NULL) {
		return false;
	}

	uint32_t *dst = (uint32_t *)((unsigned char *)output->backdrop_buffer->pixels +
		(size_t)bounds->y * (size_t)output->backdrop_buffer->stride +
		(size_t)bounds->x * sizeof(uint32_t));
	struct wlr_texture_read_pixels_options read_options = {
		.data = dst,
		.format = format,
		.stride = (uint32_t)output->backdrop_buffer->stride,
		.dst_x = 0,
		.dst_y = 0,
		.src_box = {
			.x = bounds->x,
			.y = bounds->y,
			.width = bounds->width,
			.height = bounds->height,
		},
	};
	bool ok = wlr_texture_read_pixels(texture, &read_options);
	wlr_texture_destroy(texture);
	if (!ok) {
		return false;
	}

	output_normalize_readback_pixels(dst, bounds->width, bounds->height,
		output->backdrop_buffer->stride, format);
	return true;
}

static bool output_capture_composed_backdrop(
		struct orange_output *output,
		const struct orange_rect *bounds) {
	if (output == NULL || output->backdrop_buffer == NULL ||
			output->scene_output == NULL || output->overlay_scene_buffer == NULL ||
			!output_rect_has_area(bounds)) {
		return false;
	}

	bool overlay_enabled = output->overlay_scene_buffer->node.enabled;
	if (overlay_enabled) {
		wlr_scene_node_set_enabled(&output->overlay_scene_buffer->node, false);
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	bool built = wlr_scene_output_build_state(output->scene_output,
		&state, NULL);
	if (overlay_enabled) {
		wlr_scene_node_set_enabled(&output->overlay_scene_buffer->node, true);
	}
	if (!built || state.buffer == NULL) {
		wlr_output_state_finish(&state);
		return false;
	}

	bool ok = output_read_backdrop_from_state(output, &state, bounds,
		DRM_FORMAT_ARGB8888);
	if (!ok) {
		struct wlr_texture *texture = wlr_texture_from_buffer(
			output->server->renderer, state.buffer);
		uint32_t preferred = texture != NULL ?
			wlr_texture_preferred_read_format(texture) : DRM_FORMAT_ARGB8888;
		if (texture != NULL) {
			wlr_texture_destroy(texture);
		}
		if (preferred != DRM_FORMAT_ARGB8888) {
			ok = output_read_backdrop_from_state(output, &state, bounds,
				preferred);
		}
	}
	wlr_output_state_finish(&state);
	return ok;
}

static bool rects_intersect_local(struct orange_rect a, struct orange_rect b) {
	return a.x < b.x + b.width &&
		a.x + a.width > b.x &&
		a.y < b.y + b.height &&
		a.y + a.height > b.y;
}

static struct orange_rect rect_inset_clamped(struct orange_rect rect, int pad) {
	if (pad < 0) {
		pad = 0;
	}
	if (pad * 2 >= rect.width) {
		pad = rect.width > 2 ? rect.width / 3 : 0;
	}
	if (pad * 2 >= rect.height) {
		pad = rect.height > 2 ? rect.height / 3 : 0;
	}
	return (struct orange_rect){
		rect.x + pad,
		rect.y + pad,
		rect.width - pad * 2,
		rect.height - pad * 2,
	};
}

static struct orange_rect dock_item_inset_rect(
		struct orange_rect item,
		int pad_divisor,
		int min_pad) {
	if (pad_divisor <= 0) {
		pad_divisor = 1;
	}
	int pad_x = item.width / pad_divisor;
	int pad_y = item.height / pad_divisor;
	if (pad_x < min_pad) {
		pad_x = min_pad;
	}
	if (pad_y < min_pad) {
		pad_y = min_pad;
	}
	return rect_inset_clamped(item, pad_x < pad_y ? pad_x : pad_y);
}

static struct orange_rect fit_image_rect_to_box(
		int image_width,
		int image_height,
		struct orange_rect rect) {
	if (image_width <= 0 || image_height <= 0 ||
			rect.width <= 0 || rect.height <= 0) {
		return rect;
	}
	double image_aspect = (double)image_width / (double)image_height;
	double box_aspect = (double)rect.width / (double)rect.height;
	if (box_aspect > image_aspect) {
		int width = (int)lround((double)rect.height * image_aspect);
		if (width < 1) {
			width = 1;
		}
		if (width > rect.width) {
			width = rect.width;
		}
		return (struct orange_rect){
			rect.x + (rect.width - width) / 2,
			rect.y,
			width,
			rect.height,
		};
	}
	int height = (int)lround((double)rect.width / image_aspect);
	if (height < 1) {
		height = 1;
	}
	if (height > rect.height) {
		height = rect.height;
	}
	return (struct orange_rect){
		rect.x,
		rect.y + (rect.height - height) / 2,
		rect.width,
		height,
	};
}

static void append_round_rect_path(
		cairo_t *cr,
		struct orange_rect rect,
		double radius) {
	double x = rect.x;
	double y = rect.y;
	double w = rect.width;
	double h = rect.height;
	if (radius < 0.0) {
		radius = 0.0;
	}
	double max_radius = fmin(w, h) / 2.0;
	if (radius > max_radius) {
		radius = max_radius;
	}
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - radius, y + radius, radius,
		-M_PI / 2.0, 0.0);
	cairo_arc(cr, x + w - radius, y + h - radius, radius,
		0.0, M_PI / 2.0);
	cairo_arc(cr, x + radius, y + h - radius, radius,
		M_PI / 2.0, M_PI);
	cairo_arc(cr, x + radius, y + radius, radius,
		M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static void draw_minimized_dock_snapshots(
		struct orange_output *output,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		struct orange_buffer *buffer,
		const struct orange_rect *clip) {
	if (output == NULL || output->server == NULL || layout == NULL ||
			buffer == NULL || buffer->pixels == NULL) {
		return;
	}
	struct orange_dock_visual_item visual[ORANGE_DOCK_MAX];
	orange_dock_compute_visual_items(layout, state,
		&output->server->config, visual);
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)buffer->pixels,
		CAIRO_FORMAT_ARGB32,
		output->width,
		output->height,
		buffer->stride);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}
	cairo_t *cr = cairo_create(surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		return;
	}

	for (int i = 0; i < layout->dock_item_count && i < ORANGE_DOCK_MAX; i++) {
		if (!layout->dock_minimized[i]) {
			continue;
		}
		struct orange_view *view = server_minimized_view_for_dock_index(
			output->server, layout->dock_minimized_indices[i]);
		if (view == NULL || view->minimized_snapshot == NULL ||
				view->minimized_snapshot->pixels == NULL) {
			continue;
		}
		struct orange_minimize_animation *anim =
			&output->server->minimize_animation;
		if (anim->active && !anim->restoring && anim->view == view) {
			continue;
		}

		struct orange_rect item = visual[i].rect;
		struct orange_rect frame = orange_dock_minimized_thumbnail_rect(item);
		if (!output_rect_has_area(&frame) ||
				(clip != NULL && !rects_intersect_local(frame, *clip))) {
			continue;
		}
		struct orange_rect image = fit_image_rect_to_box(
			view->minimized_snapshot->base.width,
			view->minimized_snapshot->base.height,
			frame);
		if (!output_rect_has_area(&image)) {
			continue;
		}

		cairo_surface_t *snapshot = cairo_image_surface_create_for_data(
			(unsigned char *)view->minimized_snapshot->pixels,
			CAIRO_FORMAT_ARGB32,
			view->minimized_snapshot->base.width,
			view->minimized_snapshot->base.height,
			view->minimized_snapshot->stride);
		if (cairo_surface_status(snapshot) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(snapshot);
			continue;
		}

		cairo_save(cr);
		append_round_rect_path(cr, frame, fmax(5.0, frame.width / 9.0));
		cairo_clip(cr);
		cairo_translate(cr, image.x, image.y);
		cairo_scale(cr,
			(double)image.width /
				(double)view->minimized_snapshot->base.width,
			(double)image.height /
				(double)view->minimized_snapshot->base.height);
		cairo_set_source_surface(cr, snapshot, 0, 0);
		cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
		cairo_paint(cr);
		cairo_restore(cr);

		cairo_surface_destroy(snapshot);
	}

	cairo_destroy(cr);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

static void view_drop_minimized_snapshot(struct orange_view *view) {
	if (view == NULL || view->minimized_snapshot == NULL) {
		return;
	}
	wlr_buffer_drop(&view->minimized_snapshot->base);
	view->minimized_snapshot = NULL;
}

static bool server_view_is_live(
		const struct orange_server *server,
		const struct orange_view *needle) {
	if (server == NULL || needle == NULL) {
		return false;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view == needle) {
			return true;
		}
	}
	return false;
}

static void minimize_animation_mark_dirty(struct orange_minimize_animation *anim) {
	if (anim == NULL || anim->output == NULL) {
		return;
	}
	anim->output->overlay_dirty = true;
	if (!anim->output->commit_pending &&
			!anim->output->wlr_output->frame_pending &&
			!anim->output->commit_failed) {
		wlr_output_schedule_frame(anim->output->wlr_output);
	}
}

static void minimize_animation_cancel_internal(
		struct orange_server *server,
		bool complete,
		bool mark_dirty) {
	if (server == NULL || !server->minimize_animation.active) {
		return;
	}
	struct orange_minimize_animation *anim = &server->minimize_animation;
	struct orange_output *output = anim->output;
	bool was_restoring = anim->restoring;
	bool focus_on_complete = complete && was_restoring &&
		anim->focus_on_complete;
	struct orange_view *view = anim->view;
	if (was_restoring && server_view_is_live(server, view) &&
			view->mapped && !view->minimized &&
			view->scene_tree != NULL) {
		wlr_scene_node_set_enabled(&view->scene_tree->node,
			view_is_visible_on_current_workspace(view));
	}
	if (anim->snapshot != NULL) {
		wlr_buffer_unlock(&anim->snapshot->base);
	}
	*anim = (struct orange_minimize_animation){0};
	if (!was_restoring && mark_dirty) {
		server_mark_shell_dirty(server);
	}
	if (focus_on_complete && server_view_is_live(server, view) &&
			view->mapped && !view->minimized) {
		struct wlr_surface *wsurface = view_get_wlr_surface(view);
		if (wsurface != NULL) {
			focus_view(view, wsurface);
		}
	}
	if (output != NULL && mark_dirty) {
		output->overlay_dirty = true;
		if (!output->commit_pending && !output->wlr_output->frame_pending &&
				!output->commit_failed) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void minimize_animation_cancel(
		struct orange_server *server,
		bool complete) {
	minimize_animation_cancel_internal(server, complete, true);
}

static struct orange_output *output_for_view(struct orange_view *view) {
	if (view == NULL || view->server == NULL) {
		return NULL;
	}
	struct orange_server *server = view->server;
	struct orange_output *fallback = NULL;
	double lx = (double)view->x + (double)view->width / 2.0;
	double ly = (double)view->y + (double)view->height / 2.0;
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (fallback == NULL) {
			fallback = output;
		}
		double ox = lx;
		double oy = ly;
		wlr_output_layout_output_coords(server->output_layout,
			output->wlr_output, &ox, &oy);
		int width = 0;
		int height = 0;
		output_effective_size(output, &width, &height);
		if (ox >= 0.0 && oy >= 0.0 &&
				ox < (double)width && oy < (double)height) {
			return output;
		}
	}
	return fallback;
}

static struct orange_output *server_primary_output(
		struct orange_server *server) {
	if (server == NULL || wl_list_empty(&server->outputs)) {
		return NULL;
	}
	struct orange_output *output =
		wl_container_of(server->outputs.next, output, link);
	return output;
}

static struct orange_output *output_for_cursor(
		struct orange_server *server) {
	if (server == NULL || server->cursor == NULL) {
		return server_primary_output(server);
	}
	double cx = server->cursor->x;
	double cy = server->cursor->y;
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		double ox = cx;
		double oy = cy;
		wlr_output_layout_output_coords(server->output_layout,
			output->wlr_output, &ox, &oy);
		int width = 0;
		int height = 0;
		output_effective_size(output, &width, &height);
		if (ox >= 0.0 && oy >= 0.0 &&
				ox < (double)width && oy < (double)height) {
			return output;
		}
	}
	return server_primary_output(server);
}

static bool view_is_workspace_scoped(const struct orange_view *view) {
	if (view == NULL || view->server == NULL) {
		return false;
	}
	struct orange_server *server = view->server;
	if (!server->workspaces_only_on_primary) {
		return true;
	}
	struct orange_output *primary = server_primary_output(server);
	if (primary == NULL) {
		return true;
	}
	return output_for_view((struct orange_view *)view) == primary;
}

static int output_current_workspace(struct orange_output *output) {
	if (output == NULL) {
		return 0;
	}
	return output->current_workspace;
}

static int server_effective_current_workspace(
		struct orange_server *server) {
	if (server == NULL) {
		return 0;
	}
	if (!server->workspaces_only_on_primary) {
		struct orange_output *focused = server->focused_view != NULL ?
			output_for_view(server->focused_view) :
			output_for_cursor(server);
		if (focused == NULL) {
			focused = server_primary_output(server);
		}
		if (focused != NULL) {
			return focused->current_workspace;
		}
	}
	return server->current_workspace;
}

static bool view_is_visible_on_current_workspace(
		const struct orange_view *view) {
	if (view == NULL || view->server == NULL || !view->mapped) {
		return false;
	}
	if (!view_is_workspace_scoped(view)) {
		return true;
	}
	if (!view->server->workspaces_only_on_primary) {
		struct orange_output *output = output_for_view(
			(struct orange_view *)view);
		if (output != NULL) {
			return view->workspace == output->current_workspace;
		}
	}
	return view->workspace == view->server->current_workspace;
}

static bool view_is_scene_visible_for_workspace(
		const struct orange_view *view) {
	if (view == NULL || view->server == NULL || !view->mapped ||
			view->minimized) {
		return false;
	}
	if (!view_is_workspace_scoped(view)) {
		return true;
	}
	const struct orange_workspace_animation *anim =
		&view->server->workspace_animation;
	if (anim->active &&
			(view->workspace == anim->from_workspace ||
			 view->workspace == anim->to_workspace)) {
		return true;
	}
	if (!view->server->workspaces_only_on_primary) {
		struct orange_output *output = output_for_view(
			(struct orange_view *)view);
		if (output != NULL) {
			return view->workspace == output->current_workspace;
		}
	}
	return view->workspace == view->server->current_workspace;
}

static struct orange_view *server_first_visible_view(
		struct orange_server *server) {
	if (server == NULL) {
		return NULL;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view_is_visible_on_current_workspace(view) && !view->minimized) {
			return view;
		}
	}
	return NULL;
}

static void server_schedule_scene_frames(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output->commit_pending && !output->wlr_output->frame_pending &&
				!output->commit_failed) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static int server_workspace_animation_span(struct orange_server *server) {
	if (server == NULL) {
		return 0;
	}
	if (server->workspaces_only_on_primary) {
		struct orange_output *primary = server_primary_output(server);
		struct orange_rect area;
		if (primary != NULL &&
				output_area_in_layout(server, primary, &area) &&
				area.width > 0) {
			return area.width;
		}
	}
	if (!server->workspaces_only_on_primary) {
		struct orange_output *focused = output_for_cursor(server);
		if (focused != NULL) {
			struct orange_rect area;
			if (output_area_in_layout(server, focused, &area) &&
					area.width > 0) {
				return area.width;
			}
		}
	}
	struct wlr_box box = {0};
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	if (box.width > 0) {
		return box.width;
	}
	return server->options != NULL ? server->options->width : 0;
}

static void server_reset_workspace_animation_positions(
		struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->scene_tree != NULL) {
			wlr_scene_node_set_position(&view->scene_tree->node,
				view->x, view->y);
		}
	}
}

static void server_apply_workspace_animation_positions(
		struct orange_server *server,
		double progress) {
	if (server == NULL) {
		return;
	}
	struct orange_workspace_animation *anim = &server->workspace_animation;
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->scene_tree == NULL) {
			continue;
		}
		int offset = 0;
		if (view_is_workspace_scoped(view)) {
			if (!server->workspaces_only_on_primary &&
					anim->output != NULL &&
					output_for_view(view) != anim->output) {
				wlr_scene_node_set_position(&view->scene_tree->node,
					view->x, view->y);
				continue;
			}
			if (view->workspace == anim->from_workspace) {
				offset = orange_shell_workspace_animation_offset(
					anim->span, anim->direction, progress, false);
			} else if (view->workspace == anim->to_workspace) {
				offset = orange_shell_workspace_animation_offset(
					anim->span, anim->direction, progress, true);
			}
		}
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x + offset, view->y);
	}
}

static void server_finish_workspace_animation(struct orange_server *server) {
	if (server == NULL || !server->workspace_animation.active) {
		return;
	}
	server->workspace_animation.active = false;
	server_reset_workspace_animation_positions(server);
	server_apply_workspace_visibility(server);
	server_schedule_scene_frames(server);
}

static bool server_update_workspace_animation(struct orange_server *server) {
	if (server == NULL || !server->workspace_animation.active) {
		return false;
	}
	struct orange_workspace_animation *anim = &server->workspace_animation;
	uint32_t now = monotonic_time_msec();
	uint32_t elapsed = now - anim->start_ms;
	if (elapsed >= anim->duration_ms || anim->duration_ms == 0) {
		server_finish_workspace_animation(server);
		return true;
	}
	double progress = (double)elapsed / (double)anim->duration_ms;
	server_apply_workspace_animation_positions(server, progress);
	server_schedule_scene_frames(server);
	return true;
}

static bool server_begin_workspace_animation(
		struct orange_server *server,
		int from_workspace,
		int to_workspace,
		struct orange_output *output) {
	if (server == NULL || !server->animations_enabled ||
			from_workspace == to_workspace) {
		return false;
	}
	int span = server_workspace_animation_span(server);
	if (span <= 0) {
		return false;
	}
	struct orange_workspace_animation *anim = &server->workspace_animation;
	*anim = (struct orange_workspace_animation){
		.active = true,
		.start_ms = monotonic_time_msec(),
		.duration_ms = WORKSPACE_ANIMATION_DURATION_MS,
		.from_workspace = from_workspace,
		.to_workspace = to_workspace,
		.direction = to_workspace > from_workspace ? 1 : -1,
		.span = span,
		.output = output,
	};
	server_apply_workspace_visibility(server);
	server_apply_workspace_animation_positions(server, 0.0);
	server_schedule_scene_frames(server);
	return true;
}

static void server_apply_workspace_visibility(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		bool visible = view_is_scene_visible_for_workspace(view);
		if (view->scene_tree != NULL) {
			wlr_scene_node_set_enabled(&view->scene_tree->node, visible);
		}
		if (!visible && server->focused_view == view) {
			view_configure_activated(view, false);
			server->focused_view = NULL;
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
	}
	server_mark_shell_dirty(server);
	server_mark_dock_dirty(server);
}

static void server_switch_workspace(struct orange_server *server, int workspace) {
	if (server == NULL) {
		return;
	}
	if (server->workspace_animation.active) {
		server_finish_workspace_animation(server);
	}
	int previous;
	int next = clamp_workspace_index(workspace, server->workspace_count);
	if (!server->workspaces_only_on_primary) {
		struct orange_output *focused = server->focused_view != NULL ?
			output_for_view(server->focused_view) :
			output_for_cursor(server);
		if (focused == NULL) {
			focused = server_primary_output(server);
		}
		if (focused == NULL) {
			return;
		}
		previous = focused->current_workspace;
		if (next == previous) {
			return;
		}
		focused->current_workspace = next;
		server->current_workspace = next;
		server_normalize_workspaces(server);
		if (!server_begin_workspace_animation(server, previous,
				server->current_workspace, focused)) {
			server_apply_workspace_visibility(server);
		}
	} else {
		previous = server->current_workspace;
		if (next == previous) {
			return;
		}
		server->current_workspace = next;
		server_normalize_workspaces(server);
		if (!server_begin_workspace_animation(server, previous,
				server->current_workspace, NULL)) {
			server_apply_workspace_visibility(server);
		}
	}
	struct orange_view *view = server_first_visible_view(server);
	if (view != NULL && view_get_wlr_surface(view) != NULL) {
		focus_view(view, view_get_wlr_surface(view));
	} else {
		focus_desktop_files(server);
	}
}

static void server_move_focused_view_to_workspace(
		struct orange_server *server,
		int workspace,
		bool follow) {
	if (server == NULL || server->focused_view == NULL ||
			!server->focused_view->mapped) {
		return;
	}
	int count = server->workspace_count;
	if (server->dynamic_workspaces_enabled &&
			workspace >= count && count < ORANGE_WORKSPACE_MAX) {
		count = workspace + 1;
	}
	workspace = clamp_workspace_index(workspace, count);
	server->workspace_count = count;
	server->focused_view->workspace = workspace;
	server_normalize_workspaces(server);
	if (follow) {
		server_switch_workspace(server, workspace);
	} else {
		server_apply_workspace_visibility(server);
	}
}

static bool view_rect_in_output(
		struct orange_view *view,
		struct orange_output *output,
		struct orange_rect *out_rect) {
	if (view == NULL || output == NULL || out_rect == NULL ||
			view->width <= 0 || view->height <= 0) {
		return false;
	}
	double x = view->x;
	double y = view->y;
	wlr_output_layout_output_coords(view->server->output_layout,
		output->wlr_output, &x, &y);
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	struct orange_rect rect = {
		(int)lround(x),
		(int)lround(y),
		view->width,
		view->height,
	};
	rect = output_clamp_rect(rect, width, height);
	if (!output_rect_has_area(&rect)) {
		return false;
	}
	*out_rect = rect;
	return true;
}

static struct orange_rect minimize_target_from_item(struct orange_rect item) {
	return dock_item_inset_rect(item, 8, 3);
}

static struct orange_rect minimize_target_from_thumbnail_item(
		struct orange_rect item) {
	return orange_dock_minimized_thumbnail_rect(item);
}

static struct orange_rect minimize_target_from_dock(
		const struct orange_shell_layout *layout) {
	int size = layout->dock.width < layout->dock.height ?
		layout->dock.width : layout->dock.height;
	if (size > 84) {
		size = 84;
	}
	if (size < 24) {
		size = 24;
	}
	return (struct orange_rect){
		layout->dock.x + (layout->dock.width - size) / 2,
		layout->dock.y + (layout->dock.height - size) / 2,
		size,
		size,
	};
}

static bool minimize_animation_target_for_view(
		struct orange_server *server,
		struct orange_output *output,
		struct orange_view *view,
		bool restoring,
		struct orange_rect *out_target,
		enum orange_dock_position *out_position) {
	if (server == NULL || output == NULL || view == NULL ||
			out_target == NULL || out_position == NULL) {
		return false;
	}

	struct orange_shell_layout layout;
	server_refresh_dock_state(server);
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	const struct orange_view *pending_view = restoring ? NULL : view;
	int minimized_count = server_minimized_dock_count(server, pending_view);
	orange_shell_layout_compute_with_dock_state(width, height,
		server->system_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		server->desktop_volume_count,
		server->desktop_files,
		server->desktop_file_count,
		server->dock_temporary_count,
		server->dock_temporary_app_ids,
		minimized_count,
		&layout);
	bool dock_auto_hide_blocked =
		dock_auto_hide_blocked_for_layout(server, output, &layout);
	orange_shell_layout_apply_dock_auto_hide_progress(&layout,
		&server->config,
		dock_auto_hide_blocked,
		dock_auto_hide_runtime_revealed(server),
		server->dock_auto_hide_progress);
	*out_position = layout.dock_position;
	int minimized_index = server_minimized_dock_index(server, view,
		pending_view);
	if (minimized_index >= 0) {
		for (int i = 0; i < layout.dock_item_count; i++) {
			if (layout.dock_minimized[i] &&
					layout.dock_minimized_indices[i] == minimized_index) {
				*out_target = minimize_target_from_thumbnail_item(
					layout.dock_items[i]);
				return true;
			}
		}
	}

	int launcher_idx = dock_launcher_for_view(server, view);
	if (launcher_idx >= 0) {
		for (int i = 0; i < layout.dock_item_count; i++) {
			if (!layout.dock_temporary[i] &&
					layout.dock_launcher_indices[i] == launcher_idx) {
				*out_target = minimize_target_from_item(layout.dock_items[i]);
				return true;
			}
		}
	}

	char identity[128];
	if (view_dock_identity(server, view, identity, sizeof(identity))) {
		for (int i = 0; i < layout.dock_item_count; i++) {
			if (layout.dock_temporary[i] &&
					dock_identity_matches(
						layout.dock_temporary_app_ids[i], identity)) {
				*out_target = minimize_target_from_item(layout.dock_items[i]);
				return true;
			}
		}
	}

	if (output_rect_has_area(&layout.dock)) {
		*out_target = minimize_target_from_dock(&layout);
		return true;
	}
	return false;
}

static struct orange_buffer *capture_view_snapshot(
		struct orange_view *view,
		struct orange_output *output,
		struct orange_rect *out_rect) {
	if (view == NULL || output == NULL || out_rect == NULL ||
			!output_ensure_shell_buffer(output) ||
			!view_rect_in_output(view, output, out_rect)) {
		return NULL;
	}
	if (!output_capture_composed_backdrop(output, out_rect)) {
		return NULL;
	}

	struct orange_buffer *snapshot =
		orange_buffer_create(out_rect->width, out_rect->height);
	if (snapshot == NULL) {
		return NULL;
	}
	for (int y = 0; y < out_rect->height; y++) {
		const unsigned char *src =
			(const unsigned char *)output->backdrop_buffer->pixels +
			(size_t)(out_rect->y + y) *
				(size_t)output->backdrop_buffer->stride +
			(size_t)out_rect->x * sizeof(uint32_t);
		unsigned char *dst =
			(unsigned char *)snapshot->pixels +
			(size_t)y * (size_t)snapshot->stride;
		memcpy(dst, src, (size_t)out_rect->width * sizeof(uint32_t));
	}
	return snapshot;
}

static bool minimize_animation_begin(
		struct orange_view *view,
		bool restoring,
		bool focus_on_restore) {
	if (view == NULL || view->server == NULL || !view->mapped) {
		return false;
	}
	struct orange_server *server = view->server;
	struct orange_output *output = output_for_view(view);
	if (output == NULL) {
		return false;
	}

	struct orange_rect view_rect;
	if (!view_rect_in_output(view, output, &view_rect)) {
		return false;
	}

	struct orange_buffer *snapshot = NULL;
	if (restoring) {
		if (view->minimized_snapshot == NULL) {
			return false;
		}
		snapshot = orange_buffer_from_wlr_buffer(
			wlr_buffer_lock(&view->minimized_snapshot->base));
	} else {
		snapshot = capture_view_snapshot(view, output, &view_rect);
		if (snapshot == NULL) {
			return false;
		}
		view_drop_minimized_snapshot(view);
		view->minimized_snapshot = snapshot;
		snapshot = orange_buffer_from_wlr_buffer(
			wlr_buffer_lock(&view->minimized_snapshot->base));
	}
	if (snapshot == NULL) {
		return false;
	}

	struct orange_rect dock_target;
	enum orange_dock_position dock_position = ORANGE_DOCK_POSITION_BOTTOM;
	if (!minimize_animation_target_for_view(server, output, view,
			restoring, &dock_target, &dock_position)) {
		wlr_buffer_unlock(&snapshot->base);
		return false;
	}

	minimize_animation_cancel(server, false);
	server->minimize_animation = (struct orange_minimize_animation){
		.active = true,
		.restoring = restoring,
		.focus_on_complete = restoring && focus_on_restore,
		.effect = server->config.minimize_effect,
		.dock_position = dock_position,
		.start_ms = monotonic_time_msec(),
		.duration_ms = MINIMIZE_ANIMATION_DURATION_MS,
		.output = output,
		.view = view,
		.snapshot = snapshot,
		.from_rect = restoring ? dock_target : view_rect,
		.to_rect = restoring ? view_rect : dock_target,
	};
	minimize_animation_mark_dirty(&server->minimize_animation);
	return true;
}

static double minimize_animation_progress(
		const struct orange_minimize_animation *anim,
		uint32_t now) {
	if (anim == NULL || !anim->active || anim->duration_ms == 0) {
		return 1.0;
	}
	uint32_t elapsed = now - anim->start_ms;
	if (elapsed >= anim->duration_ms) {
		return 1.0;
	}
	return (double)elapsed / (double)anim->duration_ms;
}

static bool genie_animation_uses_vertical_strips(
		const struct orange_minimize_animation *anim) {
	return anim != NULL &&
		(anim->dock_position == ORANGE_DOCK_POSITION_LEFT ||
		 anim->dock_position == ORANGE_DOCK_POSITION_RIGHT);
}

static double genie_strip_progress(
		const struct orange_minimize_animation *anim,
		double progress,
		double axis_progress) {
	progress = clamp(progress, 0.0, 1.0);
	axis_progress = clamp(axis_progress, 0.0, 1.0);
	if (anim == NULL) {
		return progress;
	}

	double direction = 1.0;
	switch (anim->dock_position) {
	case ORANGE_DOCK_POSITION_LEFT:
		direction = -1.0;
		break;
	case ORANGE_DOCK_POSITION_RIGHT:
	case ORANGE_DOCK_POSITION_BOTTOM:
	default:
		direction = 1.0;
		break;
	}
	if (anim->restoring) {
		direction = -direction;
	}

	double warp = 0.64 * sin(progress * M_PI);
	return clamp(progress + (axis_progress - 0.5) * direction * warp,
		0.0, 1.0);
}

static struct orange_rect minimize_animation_fitted_rect_at_progress(
		const struct orange_minimize_animation *anim,
		double progress) {
	if (anim == NULL) {
		return (struct orange_rect){0, 0, 0, 0};
	}
	struct orange_rect rect = orange_shell_minimize_animation_rect(
		anim->from_rect, anim->to_rect, progress);
	if (anim->snapshot == NULL ||
			anim->snapshot->base.width <= 0 ||
			anim->snapshot->base.height <= 0 ||
			rect.width <= 0 || rect.height <= 0) {
		return rect;
	}
	return fit_image_rect_to_box(anim->snapshot->base.width,
		anim->snapshot->base.height, rect);
}

static struct orange_rect genie_animation_bounds(
		const struct orange_minimize_animation *anim,
		double progress) {
	struct orange_rect bounds = {0, 0, 0, 0};
	if (anim == NULL || anim->snapshot == NULL) {
		return bounds;
	}
	for (int i = 0; i <= GENIE_ANIMATION_BOUND_SAMPLES; i++) {
		double axis = (double)i / (double)GENIE_ANIMATION_BOUND_SAMPLES;
		double strip_progress = genie_strip_progress(anim, progress, axis);
		struct orange_rect rect =
			minimize_animation_fitted_rect_at_progress(anim, strip_progress);
		bounds = output_union_rect(bounds, rect);
	}
	return bounds;
}

static bool minimize_animation_bounds(
		const struct orange_minimize_animation *anim,
		struct orange_rect *out_bounds) {
	if (anim == NULL || !anim->active || out_bounds == NULL) {
		return false;
	}
	double progress = minimize_animation_progress(anim, monotonic_time_msec());
	struct orange_rect rect =
		anim->effect == ORANGE_MINIMIZE_EFFECT_GENIE ?
			genie_animation_bounds(anim, progress) :
			orange_shell_minimize_animation_rect(
				anim->from_rect, anim->to_rect, progress);
	rect.x -= 3;
	rect.y -= 3;
	rect.width += 6;
	rect.height += 6;
	if (anim->output != NULL) {
		int width = 0;
		int height = 0;
		output_effective_size(anim->output, &width, &height);
		rect = output_clamp_rect(rect, width, height);
	}
	if (!output_rect_has_area(&rect)) {
		return false;
	}
	*out_bounds = rect;
	return true;
}

static struct orange_rect fit_snapshot_rect(
		const struct orange_minimize_animation *anim,
		struct orange_rect rect) {
	if (anim == NULL || anim->snapshot == NULL ||
			anim->snapshot->base.width <= 0 ||
			anim->snapshot->base.height <= 0 ||
			rect.width <= 0 || rect.height <= 0) {
		return rect;
	}
	return fit_image_rect_to_box(anim->snapshot->base.width,
		anim->snapshot->base.height, rect);
}

static void append_genie_warp_clip_path(
		cairo_t *cr,
		const struct orange_minimize_animation *anim,
		double progress) {
	cairo_new_path(cr);
	if (anim == NULL) {
		return;
	}

	if (genie_animation_uses_vertical_strips(anim)) {
		for (int i = 0; i <= GENIE_ANIMATION_BOUND_SAMPLES; i++) {
			double u = (double)i / (double)GENIE_ANIMATION_BOUND_SAMPLES;
			struct orange_rect rect = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, u));
			double x = (double)rect.x + (double)rect.width * u;
			double y = (double)rect.y;
			if (i == 0) {
				cairo_move_to(cr, x, y);
			} else {
				cairo_line_to(cr, x, y);
			}
		}
		for (int i = GENIE_ANIMATION_BOUND_SAMPLES; i >= 0; i--) {
			double u = (double)i / (double)GENIE_ANIMATION_BOUND_SAMPLES;
			struct orange_rect rect = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, u));
			double x = (double)rect.x + (double)rect.width * u;
			double y = (double)rect.y + (double)rect.height;
			cairo_line_to(cr, x, y);
		}
	} else {
		for (int i = 0; i <= GENIE_ANIMATION_BOUND_SAMPLES; i++) {
			double v = (double)i / (double)GENIE_ANIMATION_BOUND_SAMPLES;
			struct orange_rect rect = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, v));
			double x = (double)rect.x + (double)rect.width;
			double y = (double)rect.y + (double)rect.height * v;
			if (i == 0) {
				cairo_move_to(cr, x, y);
			} else {
				cairo_line_to(cr, x, y);
			}
		}
		for (int i = GENIE_ANIMATION_BOUND_SAMPLES; i >= 0; i--) {
			double v = (double)i / (double)GENIE_ANIMATION_BOUND_SAMPLES;
			struct orange_rect rect = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, v));
			double x = (double)rect.x;
			double y = (double)rect.y + (double)rect.height * v;
			cairo_line_to(cr, x, y);
		}
	}
	cairo_close_path(cr);
}

static void draw_snapshot_strip(
		cairo_t *cr,
		cairo_surface_t *snapshot,
		double src_x,
		double src_y,
		double src_w,
		double src_h,
		double dst_x,
		double dst_y,
		double dst_w,
		double dst_h) {
	if (cr == NULL || snapshot == NULL ||
			src_w <= 0.0 || src_h <= 0.0 ||
			dst_w <= 0.0 || dst_h <= 0.0) {
		return;
	}
	double scale_x = dst_w / src_w;
	double scale_y = dst_h / src_h;
	cairo_save(cr);
	cairo_rectangle(cr, dst_x, dst_y, dst_w, dst_h);
	cairo_clip(cr);
	cairo_translate(cr,
		dst_x - src_x * scale_x,
		dst_y - src_y * scale_y);
	cairo_scale(cr, scale_x, scale_y);
	cairo_set_source_surface(cr, snapshot, 0.0, 0.0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
	cairo_paint_with_alpha(cr, 0.96);
	cairo_restore(cr);
}

static void draw_genie_animation(
		cairo_t *cr,
		cairo_surface_t *snapshot,
		const struct orange_minimize_animation *anim,
		double progress) {
	if (cr == NULL || snapshot == NULL || anim == NULL ||
			anim->snapshot == NULL ||
			anim->snapshot->base.width <= 0 ||
			anim->snapshot->base.height <= 0) {
		return;
	}

	double src_width = (double)anim->snapshot->base.width;
	double src_height = (double)anim->snapshot->base.height;
	cairo_save(cr);
	append_genie_warp_clip_path(cr, anim, progress);
	cairo_clip(cr);

	if (genie_animation_uses_vertical_strips(anim)) {
		for (int i = 0; i < GENIE_ANIMATION_STRIPS; i++) {
			double u0 = (double)i / (double)GENIE_ANIMATION_STRIPS;
			double u1 = (double)(i + 1) / (double)GENIE_ANIMATION_STRIPS;
			double uc = (u0 + u1) * 0.5;
			struct orange_rect r0 = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, u0));
			struct orange_rect r1 = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, u1));
			struct orange_rect rc = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, uc));
			double x0 = (double)r0.x + (double)r0.width * u0;
			double x1 = (double)r1.x + (double)r1.width * u1;
			double dst_x = x0 < x1 ? x0 : x1;
			double dst_w = fabs(x1 - x0);
			draw_snapshot_strip(cr, snapshot,
				src_width * u0, 0.0,
				src_width * (u1 - u0), src_height,
				dst_x, (double)rc.y,
				dst_w, (double)rc.height);
		}
	} else {
		for (int i = 0; i < GENIE_ANIMATION_STRIPS; i++) {
			double v0 = (double)i / (double)GENIE_ANIMATION_STRIPS;
			double v1 = (double)(i + 1) / (double)GENIE_ANIMATION_STRIPS;
			double vc = (v0 + v1) * 0.5;
			struct orange_rect r0 = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, v0));
			struct orange_rect r1 = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, v1));
			struct orange_rect rc = minimize_animation_fitted_rect_at_progress(
				anim, genie_strip_progress(anim, progress, vc));
			double y0 = (double)r0.y + (double)r0.height * v0;
			double y1 = (double)r1.y + (double)r1.height * v1;
			double dst_y = y0 < y1 ? y0 : y1;
			double dst_h = fabs(y1 - y0);
			draw_snapshot_strip(cr, snapshot,
				0.0, src_height * v0,
				src_width, src_height * (v1 - v0),
				(double)rc.x, dst_y,
				(double)rc.width, dst_h);
		}
	}

	cairo_restore(cr);
}

static void draw_minimize_animation(
		struct orange_output *output,
		double progress) {
	if (output == NULL || output->overlay_buffer == NULL ||
			output->server == NULL) {
		return;
	}
	struct orange_minimize_animation *anim =
		&output->server->minimize_animation;
	if (!anim->active || anim->output != output || anim->snapshot == NULL) {
		return;
	}
	struct orange_rect raw_rect = orange_shell_minimize_animation_rect(
		anim->from_rect, anim->to_rect, progress);
	struct orange_rect rect = fit_snapshot_rect(anim, raw_rect);
	if (!output_rect_has_area(&rect)) {
		return;
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)output->overlay_buffer->pixels,
		CAIRO_FORMAT_ARGB32,
		output->width,
		output->height,
		output->overlay_buffer->stride);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}
	cairo_surface_t *snapshot = cairo_image_surface_create_for_data(
		(unsigned char *)anim->snapshot->pixels,
		CAIRO_FORMAT_ARGB32,
		anim->snapshot->base.width,
		anim->snapshot->base.height,
		anim->snapshot->stride);
	if (cairo_surface_status(snapshot) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(snapshot);
		cairo_surface_destroy(surface);
		return;
	}
	cairo_t *cr = cairo_create(surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		cairo_destroy(cr);
		cairo_surface_destroy(snapshot);
		cairo_surface_destroy(surface);
		return;
	}
	cairo_save(cr);
	if (anim->effect == ORANGE_MINIMIZE_EFFECT_GENIE) {
		draw_genie_animation(cr, snapshot, anim, progress);
	} else {
		cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
		cairo_clip(cr);
		cairo_translate(cr, rect.x, rect.y);
		cairo_scale(cr,
			(double)rect.width / (double)anim->snapshot->base.width,
			(double)rect.height / (double)anim->snapshot->base.height);
		cairo_set_source_surface(cr, snapshot, 0, 0);
		cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
		cairo_paint_with_alpha(cr, 0.96);
	}
	cairo_restore(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(snapshot);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
}

static bool minimize_animation_active_on_output(
		const struct orange_server *server,
		const struct orange_output *output) {
	return server != NULL && output != NULL &&
		server->minimize_animation.active &&
		server->minimize_animation.output == output;
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
	server_update_app_menu_model(output->server);
	server_refresh_dock_state(output->server);
	bool dock_auto_hide_blocked =
		dock_auto_hide_blocked_for_output(server, output);
	dock_auto_hide_update_animation(server, monotonic_time_msec());
	bool dock_auto_hide_overlay =
		dock_auto_hide_blocked &&
		dock_auto_hide_overlay_active(server);
	if (minimize_animation_active_on_output(server, output) &&
			minimize_animation_progress(&server->minimize_animation,
				monotonic_time_msec()) >= 1.0) {
		minimize_animation_cancel(server, true);
	}
	bool minimize_overlay = minimize_animation_active_on_output(server, output);
	if (server->context_menu_kind == ORANGE_CONTEXT_MENU_NONE &&
			!server->launcher_open &&
			!server->system_menu_open &&
			!server->notification_center_open &&
			!server->dock_drag_active &&
			!server->launcher_app_drag_active &&
			!dock_auto_hide_overlay &&
			!minimize_overlay &&
			server->hot_dock_index < 0 &&
			!server->app_switcher_active) {
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

	struct orange_shell_state state = {
		.system_menu_open = output->server->system_menu_open,
		.notification_center_open = output->server->notification_center_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_pointer_x = (int)cursor_x,
		.dock_pointer_y = (int)cursor_y,
		.dock_auto_hide_revealed =
			dock_auto_hide_runtime_revealed(output->server),
		.dock_auto_hide_blocked = dock_auto_hide_blocked,
		.dock_auto_hide_animating =
			output->server->dock_auto_hide_animating,
		.dock_auto_hide_progress =
			output->server->dock_auto_hide_progress,
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
		.trash_full = output->server->trash_full,
		.status = output->server->status,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.volumes = output->server->volumes,
		.volume_count = output->server->volume_count,
		.desktop_volume_count = output->server->desktop_volume_count,
		.desktop_files = output->server->desktop_files,
		.desktop_file_count = output->server->desktop_file_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
		.open_with_app_count = output->server->open_with_app_count,
		.open_with_submenu_open = output->server->open_with_submenu_open,
		.dock_options_submenu_open = output->server->dock_options_submenu_open,
		.dock_minimize_submenu_open =
			output->server->dock_minimize_submenu_open,
		.dock_position_submenu_open =
			output->server->dock_position_submenu_open,
		.context_menu_alt_pressed = output->server->context_menu_alt_pressed,
		.context_menu_dock_temporary = output->server->context_menu_dock_temporary,
		.desktop_selection_active = output->server->desktop_select_active &&
			output->server->desktop_select_moved,
			.desktop_selection_rect = normalized_rect_from_points(
				output->server->desktop_select_start_x,
				output->server->desktop_select_start_y,
				output->server->desktop_select_x,
				output->server->desktop_select_y),
			.desktop_rename_active = output->server->desktop_rename.active,
			.desktop_rename_index = output->server->desktop_rename.layout_index,
			.launcher_open = output->server->launcher_open,
		.launcher_display_mode = output->server->launcher_display_mode,
		.launcher_position_set = output->server->launcher_position_set,
		.launcher_x = output->server->launcher_x,
		.launcher_y = output->server->launcher_y,
		.launcher_hot_app = output->server->launcher_hot_app,
		.launcher_app_count = output->server->launcher_app_count,
		.launcher_result_count = output->server->launcher_result_count,
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
		.dock_bounce_active = output->server->dock_bounce_active,
		.dock_bounce_launcher_idx = output->server->dock_bounce_launcher_idx,
		.dock_bounce_start_time = output->server->dock_bounce_start,
		.dock_temporary_count = output->server->dock_temporary_count,
		.dock_minimized_count =
			server_minimized_dock_count(output->server, NULL),
	};
	server_populate_status_notifier_items(output->server, &state);
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	memcpy(state.dock_temporary_app_ids, output->server->dock_temporary_app_ids,
		sizeof(state.dock_temporary_app_ids));
	memcpy(state.dock_temporary_open, output->server->dock_temporary_open,
		sizeof(state.dock_temporary_open));
	server_populate_minimized_dock_titles(output->server, &state);
	memcpy(state.desktop_selected, output->server->desktop_selected,
		sizeof(state.desktop_selected));
	memcpy(state.desktop_rename_text, output->server->desktop_rename.text,
		sizeof(state.desktop_rename_text));
	memcpy(state.open_with_app_labels, output->server->open_with_app_labels,
		sizeof(state.open_with_app_labels));
	memcpy(state.open_with_app_icons, output->server->open_with_app_icons,
		sizeof(state.open_with_app_icons));
	memcpy(state.launcher_query, output->server->launcher_query,
		sizeof(state.launcher_query));
	memcpy(state.launcher_app_indices, output->server->launcher_app_indices,
		sizeof(state.launcher_app_indices));
	memcpy(state.launcher_results, output->server->launcher_results,
		sizeof(state.launcher_results));
	memcpy(state.launcher_categories, output->server->launcher_categories,
		sizeof(state.launcher_categories));
	server_active_app_label(output->server,
		state.active_app_label, sizeof(state.active_app_label));
	state.app_menu = output->server->app_menu;

	struct orange_rect shell_bounds = {0};
	bool have_shell_bounds = orange_shell_overlay_bounds(
		output->width, output->height, &state, &shell_bounds);
	struct orange_rect animation_bounds = {0};
	bool have_animation_bounds = minimize_animation_bounds(
		&server->minimize_animation, &animation_bounds) &&
		minimize_animation_active_on_output(server, output);
	if (!have_shell_bounds && !have_animation_bounds) {
		if (output->overlay_bounds_valid) {
			output_clear_overlay_region(output, &output->overlay_bounds);
			output_set_overlay_damage(output, &output->overlay_bounds);
			output->overlay_bounds_valid = false;
			output->overlay_bounds = (struct orange_rect){0, 0, 0, 0};
		}
		output->overlay_dirty = false;
		return;
	}
	struct orange_rect current_bounds = have_shell_bounds ?
		shell_bounds : animation_bounds;
	if (have_shell_bounds && have_animation_bounds) {
		current_bounds = output_union_rect(shell_bounds, animation_bounds);
	}
	struct orange_rect damage_bounds = current_bounds;
	if (output->overlay_bounds_valid) {
		damage_bounds = output_union_rect(damage_bounds,
			output->overlay_bounds);
	}
	output_clear_overlay_region(output, &damage_bounds);
	if (have_shell_bounds) {
		const uint32_t *backdrop_pixels = output->shell_buffer->pixels;
		int backdrop_stride = output->shell_buffer->stride;
		if (output_capture_composed_backdrop(output, &shell_bounds)) {
			backdrop_pixels = output->backdrop_buffer->pixels;
			backdrop_stride = output->backdrop_buffer->stride;
		}
		orange_shell_draw_overlay_with_backdrop(output->overlay_buffer->pixels,
			output->width,
			output->height,
			output->overlay_buffer->stride,
			backdrop_pixels,
			backdrop_stride,
			&state);
		if (dock_auto_hide_overlay) {
			struct orange_shell_layout snapshot_layout;
			compute_shell_layout_for_output(server, output,
				&snapshot_layout);
			draw_minimized_dock_snapshots(output, &snapshot_layout,
				&state, output->overlay_buffer, &shell_bounds);
		}
	}
	if (have_animation_bounds) {
		draw_minimize_animation(output,
			minimize_animation_progress(&server->minimize_animation,
				monotonic_time_msec()));
	}

	output_set_overlay_damage(output, &damage_bounds);
	output->overlay_bounds = current_bounds;
	output->overlay_bounds_valid = true;
	output->overlay_dirty = false;
}

static struct wlr_xdg_toplevel *view_toplevel(struct orange_view *view) {
	if (view == NULL || view->xdg_surface == NULL) {
		return NULL;
	}
	return view->xdg_surface->toplevel;
}

static bool view_configure_ready(struct orange_view *view) {
	struct wlr_xdg_toplevel *toplevel = view_toplevel(view);
	return toplevel != NULL && toplevel->base != NULL &&
		toplevel->base->initialized;
}

static bool view_configure_activated(
		struct orange_view *view,
		bool activated) {
	if (view_is_xwayland(view)) {
		wlr_xwayland_surface_activate(view->xwayland_surface, activated);
		return true;
	}
	if (!view_configure_ready(view)) {
		return false;
	}
	wlr_xdg_toplevel_set_activated(view->xdg_surface->toplevel, activated);
	return true;
}

static bool view_configure_size(
		struct orange_view *view,
		int width,
		int height) {
	if (view_is_xwayland(view)) {
		wlr_xwayland_surface_configure(view->xwayland_surface,
			view->x, view->y, width, height);
		return true;
	}
	if (!view_configure_ready(view)) {
		return false;
	}
	wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, width, height);
	return true;
}

static bool view_configure_maximized(
		struct orange_view *view,
		bool maximized) {
	if (view_is_xwayland(view)) {
		wlr_xwayland_surface_set_maximized(view->xwayland_surface,
			maximized, maximized);
		return true;
	}
	if (!view_configure_ready(view)) {
		return false;
	}
	wlr_xdg_toplevel_set_maximized(view->xdg_surface->toplevel, maximized);
	return true;
}

static bool view_configure_fullscreen(
		struct orange_view *view,
		bool fullscreen) {
	if (view_is_xwayland(view)) {
		wlr_xwayland_surface_set_fullscreen(view->xwayland_surface, fullscreen);
		return true;
	}
	if (!view_configure_ready(view)) {
		return false;
	}
	wlr_xdg_toplevel_set_fullscreen(view->xdg_surface->toplevel, fullscreen);
	return true;
}

static bool view_configure_wm_capabilities(struct orange_view *view) {
	if (view_is_xwayland(view)) {
		return true;
	}
	struct wlr_xdg_toplevel *toplevel = view_toplevel(view);
	if (toplevel == NULL || toplevel->resource == NULL ||
			!view_configure_ready(view)) {
		return false;
	}
	if (wl_resource_get_version(toplevel->resource) <
			XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
		return true;
	}
	wlr_xdg_toplevel_set_wm_capabilities(toplevel,
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
	return true;
}

static bool view_send_initial_configure(struct orange_view *view) {
	if (view_is_xwayland(view)) {
		return true;
	}
	if (!view_configure_ready(view)) {
		return false;
	}
	view_configure_wm_capabilities(view);
	view_configure_size(view, 900, 620);
	return true;
}

static void handle_view_initial_configure_idle(void *data) {
	struct orange_view *view = data;
	view->initial_configure_idle = NULL;
	view_send_initial_configure(view);
}

static void view_schedule_initial_configure(struct orange_view *view) {
	if (view == NULL || view->initial_configure_idle != NULL ||
			view->server == NULL || view->server->display == NULL) {
		return;
	}
	struct wl_event_loop *loop = wl_display_get_event_loop(view->server->display);
	view->initial_configure_idle = wl_event_loop_add_idle(loop,
		handle_view_initial_configure_idle, view);
}

static void view_cancel_initial_configure(struct orange_view *view) {
	if (view == NULL || view->initial_configure_idle == NULL) {
		return;
	}
	wl_event_source_remove(view->initial_configure_idle);
	view->initial_configure_idle = NULL;
}

static void focus_view(struct orange_view *view, struct wlr_surface *surface) {
	if (view == NULL || surface == NULL || !view->mapped) {
		return;
	}

	struct orange_server *server = view->server;
	if (view_is_workspace_scoped(view)) {
		if (!server->workspaces_only_on_primary) {
			struct orange_output *output = output_for_view(view);
			if (output != NULL &&
					view->workspace != output->current_workspace) {
				server_switch_workspace(server, view->workspace);
			}
		} else if (view->workspace != server->current_workspace) {
			server_switch_workspace(server, view->workspace);
		}
	}
	if (!view_is_visible_on_current_workspace(view)) {
		return;
	}
	if (server->minimize_animation.active &&
			server->minimize_animation.restoring &&
			server->minimize_animation.view == view) {
		server->minimize_animation.focus_on_complete = true;
		return;
	}
	if (view->minimized) {
		set_view_minimized_with_focus(view, false, true);
		return;
	}
	if (server->focused_view != NULL && server->focused_view != view) {
		view_configure_activated(server->focused_view, false);
	}

	server->focused_view = view;
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	view_configure_activated(view, true);

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

static void focus_desktop_files(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	if (server->focused_view != NULL) {
		view_configure_activated(server->focused_view, false);
		server->focused_view = NULL;
	}
	wlr_seat_keyboard_notify_clear_focus(server->seat);
	app_menu_model_reset(&server->app_menu);
	memset(server->atspi_actions, 0, sizeof(server->atspi_actions));
	server->atspi_action_count = 0;
	server->app_menu_cached_app_id = NULL;
	server->app_menu_cached_pid = -1;
	server->app_menu_next_probe_ms = 0;
	server->app_menu_retry_until_ms = 0;
	server->app_menu_tried_atspi = false;
	server->app_menu_bus_name[0] = '\0';
	server->app_menu_action_path[0] = '\0';
	server_mark_shell_dirty(server);
}

static struct orange_view *view_for_xdg_surface_chain(
		struct wlr_surface *surface) {
	while (surface != NULL) {
		struct wlr_xdg_toplevel *toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(surface);
		if (toplevel != NULL && toplevel->base != NULL) {
			return toplevel->base->surface->data;
		}

		struct wlr_xdg_popup *popup =
			wlr_xdg_popup_try_from_wlr_surface(surface);
		if (popup != NULL) {
			surface = popup->parent;
			continue;
		}

		struct wlr_surface *root =
			wlr_surface_get_root_surface(surface);
		if (root == NULL || root == surface) {
			break;
		}
		surface = root;
	}
	return NULL;
}

static struct wlr_scene_tree *popup_parent_scene_tree(
		struct wlr_surface *surface) {
	while (surface != NULL) {
		struct wlr_xdg_toplevel *toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(surface);
		if (toplevel != NULL && toplevel->base != NULL) {
			struct orange_view *view = toplevel->base->surface->data;
			return view != NULL ? view->scene_tree : NULL;
		}

		struct wlr_xdg_popup *popup =
			wlr_xdg_popup_try_from_wlr_surface(surface);
		if (popup != NULL && popup->base != NULL) {
			struct orange_popup *orange_popup = popup->base->data;
			if (orange_popup != NULL) {
				return orange_popup->scene_tree;
			}
			surface = popup->parent;
			continue;
		}

		struct wlr_surface *root =
			wlr_surface_get_root_surface(surface);
		if (root == NULL || root == surface) {
			break;
		}
		surface = root;
	}
	return NULL;
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
	*view = view_for_xdg_surface_chain(root);
	return surface;
}

static bool output_area_in_layout(
		struct orange_server *server,
		struct orange_output *output,
		struct orange_rect *rect) {
	if (server == NULL || output == NULL || output->wlr_output == NULL ||
			rect == NULL) {
		return false;
	}

	struct wlr_box box = {0};
	wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);
	int width = 0;
	int height = 0;
	output_effective_size(output, &width, &height);
	if (box.width <= 0) {
		box.width = width;
	}
	if (box.height <= 0) {
		box.height = height;
	}
	*rect = (struct orange_rect){
		.x = box.x,
		.y = box.y,
		.width = box.width,
		.height = box.height,
	};
	if (rect->width < 0) {
		rect->width = 0;
	}
	if (rect->height < 0) {
		rect->height = 0;
	}
	return true;
}

static bool rect_intersects_area(
		int x, int y, int width, int height,
		struct orange_rect area) {
	return width > 0 && height > 0 &&
		area.width > 0 && area.height > 0 &&
		x + width > area.x &&
		x < area.x + area.width &&
		y + height > area.y &&
		y < area.y + area.height;
}

static bool output_work_area_in_layout(
		struct orange_server *server,
		struct orange_output *output,
		struct orange_rect *rect) {
	if (server == NULL || output == NULL || output->wlr_output == NULL ||
			rect == NULL) {
		return false;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct orange_rect work = orange_shell_layout_work_area(&layout);
	struct orange_rect output_area;
	if (!output_area_in_layout(server, output, &output_area)) {
		return false;
	}

	*rect = (struct orange_rect){
		.x = output_area.x + work.x,
		.y = output_area.y + work.y,
		.width = work.width,
		.height = work.height,
	};
	if (rect->width < 0) {
		rect->width = 0;
	}
	if (rect->height < 0) {
		rect->height = 0;
	}
	return true;
}

static void constrain_view_position(
		struct orange_server *server,
		int *x, int *y, int width, int height) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct orange_rect output_area;
		if (!output_area_in_layout(server, output, &output_area)) {
			continue;
		}
		if (rect_intersects_area(*x, *y, width, height, output_area)) {
			struct orange_rect work;
			if (!output_work_area_in_layout(server, output, &work)) {
				return;
			}
			int work_left = work.x;
			int work_top = work.y;
			int work_right = work.x + work.width;
			int work_bottom = work.y + work.height;
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

static void constrain_view_resize(
		struct orange_server *server,
		int *x, int *y, int *width, int *height,
		uint32_t edges) {
	if (server == NULL || x == NULL || y == NULL ||
			width == NULL || height == NULL) {
		return;
	}

	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct orange_rect output_area;
		if (!output_area_in_layout(server, output, &output_area)) {
			continue;
		}
		if (!rect_intersects_area(*x, *y, *width, *height, output_area)) {
			continue;
		}

		struct orange_rect work;
		if (!output_work_area_in_layout(server, output, &work)) {
			return;
		}
		int work_left = work.x;
		int work_top = work.y;
		int work_right = work.x + work.width;
		int work_bottom = work.y + work.height;

		if ((edges & WLR_EDGE_LEFT) && *x < work_left) {
			int delta = work_left - *x;
			*x = work_left;
			*width -= delta;
		}
		if ((edges & WLR_EDGE_RIGHT) && *x + *width > work_right) {
			*width = work_right - *x;
		}
		if ((edges & WLR_EDGE_TOP) && *y < work_top) {
			int delta = work_top - *y;
			*y = work_top;
			*height -= delta;
		}
		if ((edges & WLR_EDGE_BOTTOM) && *y + *height > work_bottom) {
			*height = work_bottom - *y;
		}

		if (*width > work.width) {
			*width = work.width;
			*x = work_left;
		}
		if (*height > work.height) {
			*height = work.height;
			*y = work_top;
		}
		if (*width < 1) {
			*width = 1;
		}
		if (*height < 1) {
			*height = 1;
		}
		constrain_view_position(server, x, y, *width, *height);
		return;
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
		if (server->config.dock_auto_hide) {
			server_mark_shell_dirty(server);
		}
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

		constrain_view_resize(server, &x, &y, &width, &height,
			server->resize_edges);

		view->x = x;
		view->y = y;
		view->width = width;
		view->height = height;
		wlr_scene_node_set_position(&view->scene_tree->node, x, y);
		view_configure_size(view, width, height);
		if (server->config.dock_auto_hide) {
			server_mark_shell_dirty(server);
		}
		return;
	}

	if (server->dock_resize_active) {
		process_dock_resize(server);
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

	if (server->desktop_select_active) {
		process_desktop_selection(server);
		return;
	}

	update_dock_auto_hide_for_pointer(server);
	update_hot_corner_for_pointer(server);

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
		if (!has_active_xdg_popup_grab(server)) {
			wlr_seat_pointer_notify_clear_focus(server->seat);
		}
	}

	if (server->cursor_loading_active && server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager, "progress");
	}
	if (surface == NULL && has_active_xdg_popup_grab(server)) {
		return;
	}

	/* Skip dock-hover layout computation when the cursor is over a client
	 * surface — the dock is behind the window and magnification is invisible. */
	if (surface == NULL ||
			(server->config.dock_auto_hide &&
				dock_auto_hide_overlay_active(server))) {
		int local_x = 0;
		int local_y = 0;
		struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
		struct orange_shell_layout layout;
		bool have_layout = false;
		int new_hot = -1;
		int dock_redraw_threshold = 16;
		bool separator_hover = false;
		enum orange_dock_position separator_position =
			server->config.dock_position;
		if (output != NULL) {
			compute_shell_layout_for_output(server, output, &layout);
			have_layout = true;
			struct orange_shell_hit hit =
				orange_shell_hit_test(&layout, local_x, local_y);
			if (hit.kind == ORANGE_HIT_DOCK_ITEM) {
				new_hot = hit.index;
			} else if (hit.kind == ORANGE_HIT_DOCK_SEPARATOR) {
				separator_hover = true;
				separator_position = layout.dock_position;
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
		if (separator_hover && !server->cursor_loading_active &&
				server->xcursor_manager != NULL) {
			wlr_cursor_set_xcursor(server->cursor,
				server->xcursor_manager,
				dock_resize_cursor_name(separator_position));
		}
		if (new_hot != server->hot_dock_index) {
			server->hot_dock_index = new_hot;
			server->last_dock_pointer_x = local_x;
			server->last_dock_pointer_y = local_y;
			if (server->config.dock_auto_hide &&
					dock_auto_hide_overlay_active(server)) {
				server_mark_overlay_dirty(server);
			} else if (have_layout) {
				output_mark_dock_visual_dirty(output, &layout);
			} else {
				server_mark_dock_visual_dirty(server);
			}
			server_mark_overlay_dirty(server);
		} else if (new_hot >= 0 && server->config.dock_magnification) {
			int dx = abs(local_x - server->last_dock_pointer_x);
			int dy = abs(local_y - server->last_dock_pointer_y);
			if (dx >= dock_redraw_threshold || dy >= dock_redraw_threshold) {
				server->last_dock_pointer_x = local_x;
				server->last_dock_pointer_y = local_y;
				if (server->config.dock_auto_hide &&
						dock_auto_hide_overlay_active(server)) {
					server_mark_overlay_dirty(server);
				} else if (have_layout) {
					output_mark_dock_visual_dirty(output, &layout);
				} else {
					server_mark_dock_visual_dirty(server);
				}
				server_mark_overlay_dirty(server);
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
		if (view_is_xwayland(server->focused_view)) {
			wlr_xwayland_surface_close(
				server->focused_view->xwayland_surface);
		} else {
			wlr_xdg_toplevel_send_close(
				server->focused_view->xdg_surface->toplevel);
		}
	}
}

static struct orange_keyboard *orange_keyboard_for_wlr(
		struct orange_server *server,
		struct wlr_keyboard *wlr_keyboard) {
	struct orange_keyboard *kb;
	wl_list_for_each(kb, &server->keyboards, link) {
		if (kb->keyboard == wlr_keyboard) {
			return kb;
		}
	}
	return NULL;
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

	struct orange_keyboard *orange_kb =
		orange_keyboard_for_wlr(server, keyboard);
	if (orange_kb == NULL) {
		return false;
	}

	struct wlr_keyboard_modifiers saved = keyboard->modifiers;
	struct wlr_keyboard_modifiers next = saved;
	if (ctrl && orange_kb->mod_ctrl != XKB_MOD_INVALID &&
			orange_kb->mod_ctrl < sizeof(xkb_mod_mask_t) * 8) {
		next.depressed |= ((xkb_mod_mask_t)1 << orange_kb->mod_ctrl);
	}
	if (shift && orange_kb->mod_shift != XKB_MOD_INVALID &&
			orange_kb->mod_shift < sizeof(xkb_mod_mask_t) * 8) {
		next.depressed |= ((xkb_mod_mask_t)1 << orange_kb->mod_shift);
	}
	if (alt && orange_kb->mod_alt != XKB_MOD_INVALID &&
			orange_kb->mod_alt < sizeof(xkb_mod_mask_t) * 8) {
		next.depressed |= ((xkb_mod_mask_t)1 << orange_kb->mod_alt);
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
		if ((server->app_switcher_current_workspace_only ||
				server->window_switcher_current_workspace_only) &&
				!view_is_visible_on_current_workspace(view)) {
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
		focus_view(candidate, view_get_wlr_surface(candidate));
	}
}

static struct orange_output *find_output_for_point(
		struct orange_server *server, int x, int y) {
	struct orange_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct orange_rect output_area;
		if (!output_area_in_layout(server, output, &output_area)) {
			continue;
		}
		if (x >= output_area.x &&
				x < output_area.x + output_area.width &&
				y >= output_area.y &&
				y < output_area.y + output_area.height) {
			return output;
		}
	}
	return NULL;
}

static void apply_maximize(struct orange_view *view, bool maximized) {
	if (view == NULL) {
		return;
	}
	struct orange_server *server = view->server;
	view->maximized = maximized;
	view_configure_maximized(view, maximized);
	if (!maximized || server == NULL || !view->mapped ||
			view->scene_tree == NULL) {
		return;
	}

	struct orange_output *output = find_output_for_point(server,
		view->x + view->width / 2, view->y + view->height / 2);
	if (output == NULL && !wl_list_empty(&server->outputs)) {
		output = wl_container_of(server->outputs.next, output, link);
	}
	if (output == NULL) {
		return;
	}

	struct orange_rect work;
	if (!output_work_area_in_layout(server, output, &work)) {
		return;
	}
	int margin = 8;
	view->x = work.x + margin;
	view->y = work.y + margin;
	view->width = work.width - margin * 2;
	view->height = work.height - margin * 2;
	if (view->width < 1) {
		view->width = 1;
	}
	if (view->height < 1) {
		view->height = 1;
	}
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	view_configure_size(view, view->width, view->height);
}

static void apply_fullscreen(struct orange_view *view, bool fullscreen) {
	if (view == NULL) {
		return;
	}
	struct orange_server *server = view->server;
	bool was_fullscreen = view->fullscreen;
	bool changed = was_fullscreen != fullscreen;
	if (fullscreen && changed && view->mapped &&
			view->width > 0 && view->height > 0) {
		view->fullscreen_restore_x = view->x;
		view->fullscreen_restore_y = view->y;
		view->fullscreen_restore_width = view->width;
		view->fullscreen_restore_height = view->height;
		view->fullscreen_restore_valid = true;
	}
	view->fullscreen = fullscreen;
	view_configure_fullscreen(view, fullscreen);
	if (changed && server != NULL && server->config.dock_auto_hide) {
		server_mark_shell_dirty(server);
	}
	if (!fullscreen && !was_fullscreen) {
		view->fullscreen_restore_valid = false;
		return;
	}
	if (!view->mapped || server == NULL ||
			view->scene_tree == NULL) {
		return;
	}

	struct orange_output *output = find_output_for_point(server,
		view->x + view->width / 2, view->y + view->height / 2);
	if (output == NULL && !wl_list_empty(&server->outputs)) {
		output = wl_container_of(server->outputs.next, output, link);
	}
	if (output == NULL) {
		return;
	}

	if (!fullscreen) {
		struct orange_rect target = {0};
		if (view->fullscreen_restore_valid) {
			target = (struct orange_rect){
				.x = view->fullscreen_restore_x,
				.y = view->fullscreen_restore_y,
				.width = view->fullscreen_restore_width,
				.height = view->fullscreen_restore_height,
			};
			view->fullscreen_restore_valid = false;
		} else {
			struct orange_rect work;
			if (!output_work_area_in_layout(server, output, &work)) {
				return;
			}
			target.width = work.width < 900 ? work.width : 900;
			target.height = work.height < 620 ? work.height : 620;
			target.x = work.x + (work.width - target.width) / 2;
			target.y = work.y + 30;
			if (target.y + target.height > work.y + work.height) {
				target.y = work.y;
			}
		}
		constrain_view_resize(server, &target.x, &target.y,
			&target.width, &target.height,
			WLR_EDGE_LEFT | WLR_EDGE_RIGHT |
			WLR_EDGE_TOP | WLR_EDGE_BOTTOM);
		view->x = target.x;
		view->y = target.y;
		view->width = target.width;
		view->height = target.height;
		wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
		view_configure_size(view, view->width, view->height);
		return;
	}

	struct orange_rect output_area;
	if (!output_area_in_layout(server, output, &output_area)) {
		return;
	}
	view->x = output_area.x;
	view->y = output_area.y;
	view->width = output_area.width;
	view->height = output_area.height;
	if (view->width < 1) {
		view->width = 1;
	}
	if (view->height < 1) {
		view->height = 1;
	}
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	view_configure_size(view, view->width, view->height);
}

static enum orange_edge_tile_mode edge_tile_mode_for_point(
		struct orange_server *server,
		int x,
		int y,
		struct orange_output **out_output) {
	if (out_output != NULL) {
		*out_output = NULL;
	}
	if (server == NULL || !server->edge_tiling_enabled) {
		return ORANGE_EDGE_TILE_NONE;
	}
	struct orange_output *output = find_output_for_point(server, x, y);
	if (output == NULL) {
		return ORANGE_EDGE_TILE_NONE;
	}
	struct orange_rect work;
	if (!output_work_area_in_layout(server, output, &work) ||
			work.width <= 0 || work.height <= 0) {
		return ORANGE_EDGE_TILE_NONE;
	}
	if (out_output != NULL) {
		*out_output = output;
	}
	int margin = ORANGE_EDGE_TILE_MARGIN;
	if (y <= work.y + margin) {
		return ORANGE_EDGE_TILE_TOP;
	}
	if (x <= work.x + margin) {
		return ORANGE_EDGE_TILE_LEFT;
	}
	if (x >= work.x + work.width - margin) {
		return ORANGE_EDGE_TILE_RIGHT;
	}
	return ORANGE_EDGE_TILE_NONE;
}

static struct orange_rect edge_tile_rect_for_mode(
		struct orange_rect work,
		enum orange_edge_tile_mode mode) {
	int gap = 8;
	struct orange_rect rect = work;
	if (mode == ORANGE_EDGE_TILE_LEFT) {
		rect.x = work.x + gap;
		rect.y = work.y + gap;
		rect.width = work.width / 2 - gap - gap / 2;
		rect.height = work.height - gap * 2;
	} else if (mode == ORANGE_EDGE_TILE_RIGHT) {
		int left_width = work.width / 2;
		rect.x = work.x + left_width + gap / 2;
		rect.y = work.y + gap;
		rect.width = work.width - left_width - gap - gap / 2;
		rect.height = work.height - gap * 2;
	}
	if (rect.width < 180) {
		rect.width = work.width > 180 ? 180 : work.width;
	}
	if (rect.height < 120) {
		rect.height = work.height > 120 ? 120 : work.height;
	}
	return rect;
}

static bool finish_interactive_move(struct orange_server *server) {
	if (server == NULL || server->grabbed_view == NULL) {
		return false;
	}
	struct orange_view *view = server->grabbed_view;
	if (view->fullscreen) {
		return false;
	}
	bool moved = view->x != server->grab_view_x ||
		view->y != server->grab_view_y;
	if (!moved) {
		return false;
	}

	struct orange_output *output = NULL;
	enum orange_edge_tile_mode mode = edge_tile_mode_for_point(server,
		(int)server->cursor->x,
		(int)server->cursor->y,
		&output);
	if (mode == ORANGE_EDGE_TILE_NONE || output == NULL) {
		return false;
	}
	if (mode == ORANGE_EDGE_TILE_TOP) {
		apply_maximize(view, true);
		return true;
	}

	struct orange_rect work;
	if (!output_work_area_in_layout(server, output, &work)) {
		return false;
	}
	struct orange_rect target = edge_tile_rect_for_mode(work, mode);
	view->maximized = false;
	view_configure_maximized(view, false);
	view->x = target.x;
	view->y = target.y;
	view->width = target.width;
	view->height = target.height;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	view_configure_size(view, view->width, view->height);
	server_mark_shell_dirty(server);
	return true;
}

static void handle_shell_menu_action(struct orange_server *server, int index) {
	switch (index) {
	case 0:
		if (!focus_view_for_app_id(server, ".About") &&
				!focus_view_for_app_id(server, "orange-about")) {
			launch_session_command(orange_about_command);
		}
		break;
	case 1:
		if (!focus_view_for_app_id(server, "orange-settings") &&
				!focus_view_for_app_id(server, "settings")) {
			launch_session_command(orange_settings_command);
		}
		break;
	case 2:
		launch_command("gnome-software || plasma-discover || true");
		break;
	case 3:
		launch_command("gio open recent:/// || xdg-open \"$HOME/.local/share/recently-used.xbel\" || true");
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
	server->open_with_file_index = -1;
	server->open_with_app_count = 0;
	server->open_with_submenu_open = false;
	server->dock_options_submenu_open = false;
	server->dock_minimize_submenu_open = false;
	server->dock_position_submenu_open = false;
	server->context_menu_alt_pressed = false;
	server->context_menu_dock_temporary = false;
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

static void set_dock_position(
		struct orange_server *server,
		enum orange_dock_position position) {
	server->config.dock_position = position;
	orange_config_save(&server->config, server->options->config_path);
}

static void set_minimize_effect(
		struct orange_server *server,
		enum orange_minimize_effect effect) {
	server->config.minimize_effect = effect;
	orange_config_save(&server->config, server->options->config_path);
}

static void cycle_desktop_sort_mode(struct orange_server *server) {
	switch (server->config.desktop_sort_by) {
	case ORANGE_DESKTOP_SORT_NONE:
	case ORANGE_DESKTOP_SORT_SNAP_TO_GRID:
		server->config.desktop_sort_by = ORANGE_DESKTOP_SORT_NAME;
		break;
	case ORANGE_DESKTOP_SORT_NAME:
		server->config.desktop_sort_by = ORANGE_DESKTOP_SORT_KIND;
		break;
	case ORANGE_DESKTOP_SORT_KIND:
	case ORANGE_DESKTOP_SORT_DATE_ADDED:
	case ORANGE_DESKTOP_SORT_DATE_MODIFIED:
	case ORANGE_DESKTOP_SORT_SIZE:
	default:
		server->config.desktop_sort_by = ORANGE_DESKTOP_SORT_NONE;
		break;
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void clean_up_desktop_grid(struct orange_server *server) {
	server->config.desktop_sort_by = ORANGE_DESKTOP_SORT_SNAP_TO_GRID;
	for (int i = 0; i < ORANGE_DESKTOP_POSITION_MAX; i++) {
		server->config.desktop_positions[i] =
			(struct orange_desktop_icon_position){0};
	}
	orange_config_save(&server->config, server->options->config_path);
}

static void open_launcher_mode(
		struct orange_server *server,
		int mode,
		enum orange_launcher_mode display_mode) {
	launcher_open_overlay(server, true, display_mode);
	if (mode >= 0 && mode < ORANGE_LAUNCHER_MODE_COUNT) {
		server->launcher_current_mode = mode;
		server->launcher_scroll_row = 0;
		server->launcher_hot_app = -1;
		launcher_refresh_filter(server);
	}
}

static void hide_focused_app(struct orange_server *server) {
	if (server != NULL && server->focused_view != NULL) {
		set_view_minimized(server->focused_view, true);
	}
}

static void hide_other_apps(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->mapped && view != server->focused_view) {
			set_view_minimized(view, true);
		}
	}
}

static void show_all_apps(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	struct orange_view *view;
	struct orange_view *focus_target = server->focused_view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		if (view->minimized) {
			set_view_minimized(view, false);
		}
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
		if (focus_target == NULL) {
			focus_target = view;
		}
	}
	if (focus_target != NULL) {
		focus_view(focus_target, view_get_wlr_surface(focus_target));
	}
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

	GDBusConnection *connection = dbus_cached_connection(G_BUS_TYPE_SESSION);
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
				return true;
			}
			g_object_unref(group);
		}
	}
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

	GDBusConnection *connection = dbus_cached_connection(G_BUS_TYPE_SESSION);
	if (connection == NULL) {
		return false;
	}
	if (strncmp(action, "atspi:", strlen("atspi:")) == 0) {
		char *end = NULL;
		long ref_index = strtol(action + strlen("atspi:"), &end, 10);
		if (end == action + strlen("atspi:") || ref_index < 0 ||
				ref_index >= server->atspi_action_count ||
				ref_index >= ORANGE_APP_MENU_ITEM_MAX) {
			return false;
		}
		const struct orange_atspi_action_ref *ref =
			&server->atspi_actions[ref_index];
		GDBusConnection *atspi = atspi_bus_connection();
		if (atspi == NULL) {
			return false;
		}
		GError *call_error = NULL;
		GVariant *reply = g_dbus_connection_call_sync(
			atspi,
			ref->bus_name,
			ref->object_path,
			"org.a11y.atspi.Action",
			"DoAction",
			g_variant_new("(i)", ref->action_index),
			G_VARIANT_TYPE("(b)"),
			G_DBUS_CALL_FLAGS_NONE,
			500,
			NULL,
			&call_error);
		if (call_error != NULL) {
			g_error_free(call_error);
		}
		bool activated = false;
		if (reply != NULL) {
			g_variant_get(reply, "(b)", &activated);
			g_variant_unref(reply);
		}
		g_object_unref(atspi);
		return activated;
	}

	GDBusActionGroup *group = g_dbus_action_group_get(connection,
		server->app_menu_bus_name,
		server->app_menu_action_path[0] != '\0' ?
			server->app_menu_action_path : "/org/gtk/Actions");
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
	case 2:
		launch_command("gio open recent:/// || xdg-open \"$HOME/.local/share/recently-used.xbel\" || true");
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
	case 3:
		if (server->focused_view != NULL) {
			apply_maximize(server->focused_view, true);
		}
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
	case 2:
		open_launcher_mode(server, ORANGE_LAUNCHER_MODE_ACTIONS,
			ORANGE_LAUNCHER_DISPLAY_FULL);
		break;
	case 3:
		send_focused_shortcut(server, KEY_COMMA, true, false, false);
		break;
	default:
		break;
	}
}

static void show_dock_app_in_files(struct orange_server *server, int launcher_idx) {
	if (server == NULL || launcher_idx < 0 || launcher_idx >= ORANGE_DOCK_MAX ||
			server->config.dock_apps[launcher_idx][0] == '\0') {
		return;
	}
	const char *app_id = server->config.dock_apps[launcher_idx];
	const char *home = getenv("HOME");
	if (home == NULL) {
		home = "";
	}
	char cmd[2048];
	snprintf(cmd, sizeof(cmd),
		"if [ -d \"%s/.config/%s\" ]; then xdg-open \"%s/.config/%s\"; "
		"else xdg-open \"%s/.config\"; fi || true",
		home, app_id, home, app_id, home);
	launch_command(cmd);
}

static void toggle_open_at_login(struct orange_server *server, int launcher_idx) {
	if (server == NULL || launcher_idx < 0 || launcher_idx >= ORANGE_DOCK_MAX ||
			server->config.dock_apps[launcher_idx][0] == '\0') {
		return;
	}
	const char *app_id = server->config.dock_apps[launcher_idx];
	const char *home = getenv("HOME");
	if (home == NULL || home[0] == '\0') {
		return;
	}
	char path[576];
	snprintf(path, sizeof(path),
		"%s/.config/autostart/orange-%s.desktop", home, app_id);
	struct stat st;
	if (stat(path, &st) == 0) {
		char cmd[640];
		snprintf(cmd, sizeof(cmd), "rm -f '%s'", path);
		launch_command(cmd);
		return;
	}
	const struct orange_desktop_entry *entry =
		server_desktop_entry_for_app_id(server, app_id);
	const char *name = (entry != NULL) ? entry->name : app_id;
	char exec_buf[1024] = "";
	const char *exec = NULL;
	if (entry != NULL &&
			orange_desktop_entry_expand_exec(entry, exec_buf, sizeof(exec_buf))) {
		exec = exec_buf;
	}
	if (exec == NULL) {
		exec = orange_dock_command(launcher_idx,
			server->desktop_entries, (int)server->desktop_entry_count,
			&server->config);
	}
	if (exec == NULL || exec[0] == '\0') {
		return;
	}
	char cmd[4096];
	snprintf(cmd, sizeof(cmd),
		"mkdir -p '%s/.config/autostart' && cat > '%s' << 'ORANGE_EOF'\n"
		"[Desktop Entry]\n"
		"Type=Application\n"
		"Name=%s\n"
		"Exec=%s\n"
		"X-GNOME-Autostart-enabled=true\n"
		"ORANGE_EOF",
		home, path, name, exec);
	launch_command(cmd);
}

static void launch_desktop_file_open(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd), "gio open %s || xdg-open %s || true",
		quoted, quoted);
	launch_command(cmd);
}

static bool desktop_file_content_type(
		const char *path,
		char *out,
		size_t out_size) {
	if (out == NULL || out_size == 0) {
		return false;
	}
	out[0] = '\0';
	if (path == NULL || path[0] == '\0') {
		return false;
	}

	GFile *file = g_file_new_for_path(path);
	if (file != NULL) {
		GError *error = NULL;
		GFileInfo *info = g_file_query_info(file,
			G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			G_FILE_QUERY_INFO_NONE,
			NULL,
			&error);
		if (info != NULL) {
			const char *content_type = g_file_info_get_content_type(info);
			if (content_type != NULL && content_type[0] != '\0') {
				snprintf(out, out_size, "%s", content_type);
			}
			g_object_unref(info);
		}
		if (error != NULL) {
			g_error_free(error);
		}
		g_object_unref(file);
	}
	if (out[0] != '\0') {
		return true;
	}

	gboolean uncertain = false;
	char *guessed = g_content_type_guess(path, NULL, 0, &uncertain);
	if (guessed != NULL && guessed[0] != '\0') {
		snprintf(out, out_size, "%s", guessed);
		g_free(guessed);
		return true;
	}
	g_free(guessed);
	return false;
}

static const char *icon_name_from_gicon(
		GIcon *icon,
		char *out,
		size_t out_size) {
	if (out == NULL || out_size == 0) {
		return NULL;
	}
	out[0] = '\0';
	if (icon == NULL) {
		return NULL;
	}
	if (G_IS_THEMED_ICON(icon)) {
		const char *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
		if (names != NULL && names[0] != NULL && names[0][0] != '\0') {
			snprintf(out, out_size, "%s", names[0]);
			return out;
		}
	}
	return NULL;
}

static bool open_with_app_id_seen(
		struct orange_server *server,
		const char *id) {
	if (id == NULL || id[0] == '\0') {
		return false;
	}
	for (int i = 0; i < server->open_with_app_count; i++) {
		if (strcmp(server->open_with_app_ids[i], id) == 0) {
			return true;
		}
	}
	return false;
}

static void add_open_with_app(
		struct orange_server *server,
		GAppInfo *app) {
	if (server == NULL || app == NULL ||
			server->open_with_app_count >= ORANGE_OPEN_WITH_APP_MAX) {
		return;
	}
	const char *id = g_app_info_get_id(app);
	const char *name = g_app_info_get_display_name(app);
	if (name == NULL || name[0] == '\0') {
		name = g_app_info_get_name(app);
	}
	if (id == NULL || id[0] == '\0' ||
			name == NULL || name[0] == '\0' ||
			open_with_app_id_seen(server, id)) {
		return;
	}

	int idx = server->open_with_app_count++;
	snprintf(server->open_with_app_ids[idx],
		sizeof(server->open_with_app_ids[idx]), "%s", id);
	snprintf(server->open_with_app_labels[idx],
		sizeof(server->open_with_app_labels[idx]), "%s", name);
	char icon_name[ORANGE_ASSET_ICON_NAME_MAX];
	const char *icon = icon_name_from_gicon(
		g_app_info_get_icon(app), icon_name, sizeof(icon_name));
	snprintf(server->open_with_app_icons[idx],
		sizeof(server->open_with_app_icons[idx]), "%s",
		icon != NULL ? icon : "application-x-executable");
}

static void add_open_with_app_list(
		struct orange_server *server,
		GList *apps) {
	for (GList *node = apps; node != NULL; node = node->next) {
		if (server->open_with_app_count >= ORANGE_OPEN_WITH_APP_MAX) {
			break;
		}
		add_open_with_app(server, G_APP_INFO(node->data));
	}
	g_list_free_full(apps, g_object_unref);
}

static void populate_open_with_apps(
		struct orange_server *server,
		int file_idx) {
	if (server == NULL) {
		return;
	}
	server->open_with_file_index = file_idx;
	server->open_with_app_count = 0;
	memset(server->open_with_app_ids, 0, sizeof(server->open_with_app_ids));
	memset(server->open_with_app_labels, 0, sizeof(server->open_with_app_labels));
	memset(server->open_with_app_icons, 0, sizeof(server->open_with_app_icons));
	if (file_idx < 0 || file_idx >= server->desktop_file_count) {
		return;
	}

	char content_type[256];
	if (!desktop_file_content_type(server->desktop_files[file_idx].path,
			content_type, sizeof(content_type))) {
		return;
	}
	add_open_with_app_list(server,
		g_app_info_get_recommended_for_type(content_type));
	if (server->open_with_app_count < ORANGE_OPEN_WITH_APP_MAX) {
		add_open_with_app_list(server,
			g_app_info_get_all_for_type(content_type));
	}
}

static void launch_desktop_file_open_with(
		struct orange_server *server,
		int app_index) {
	if (server == NULL ||
			app_index < 0 || app_index >= server->open_with_app_count ||
			app_index >= ORANGE_OPEN_WITH_APP_MAX ||
			server->open_with_file_index < 0 ||
			server->open_with_file_index >= server->desktop_file_count) {
		return;
	}
	const char *id = server->open_with_app_ids[app_index];
	const char *path = server->desktop_files[server->open_with_file_index].path;
	if (id == NULL || id[0] == '\0' || path == NULL || path[0] == '\0') {
		return;
	}

	GFile *file = g_file_new_for_path(path);
	char *uri = file != NULL ? g_file_get_uri(file) : NULL;
	if (file != NULL) {
		g_object_unref(file);
	}
	if (uri == NULL || uri[0] == '\0') {
		g_free(uri);
		return;
	}

	char quoted_id[512];
	char quoted_uri[4096];
	if (shell_quote_arg(id, quoted_id, sizeof(quoted_id)) &&
			shell_quote_arg(uri, quoted_uri, sizeof(quoted_uri))) {
		char cmd[9200];
		snprintf(cmd, sizeof(cmd), "gtk-launch %s %s || gio open %s || true",
			quoted_id, quoted_uri, quoted_uri);
		launch_command(cmd);
	}
	g_free(uri);
}

static void launch_desktop_file_show_in_files(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd),
		"p=%s; " ORANGE_GNOME_APP_ENV "nautilus --select \"$p\" 2>/dev/null || "
		"gio open \"$(dirname -- \"$p\")\" || "
		"xdg-open \"$(dirname -- \"$p\")\" || true",
		quoted);
	launch_command(cmd);
}

static bool desktop_directory_path(char *out, size_t out_size) {
	const char *home = getenv("HOME");
	if (out == NULL || out_size == 0 ||
			home == NULL || home[0] == '\0') {
		return false;
	}
	int n = snprintf(out, out_size, "%s/Desktop", home);
	return n > 0 && n < (int)out_size;
}

static bool write_all_fd(int fd, const char *data, size_t len) {
	size_t written = 0;
	while (written < len) {
		ssize_t n = write(fd, data + written, len - written);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		if (n == 0) {
			return false;
		}
		written += (size_t)n;
	}
	return true;
}

static bool write_temp_payload_file(
		const char *payload,
		char *path_out,
		size_t path_out_size) {
	if (payload == NULL || path_out == NULL || path_out_size == 0) {
		return false;
	}
	char path[] = "/tmp/orange-clipboard-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		return false;
	}
	bool ok = write_all_fd(fd, payload, strlen(payload));
	if (close(fd) != 0) {
		ok = false;
	}
	if (!ok) {
		unlink(path);
		return false;
	}
	int n = snprintf(path_out, path_out_size, "%s", path);
	if (n <= 0 || n >= (int)path_out_size) {
		unlink(path);
		return false;
	}
	return true;
}

static void launch_clipboard_copy_payload(const char *payload) {
	char tmp_path[128];
	if (!write_temp_payload_file(payload, tmp_path, sizeof(tmp_path))) {
		return;
	}
	char quoted[256];
	if (!shell_quote_arg(tmp_path, quoted, sizeof(quoted))) {
		unlink(tmp_path);
		return;
	}
	char cmd[2400];
	snprintf(cmd, sizeof(cmd),
		"if command -v wl-copy >/dev/null 2>&1; then "
		"wl-copy --type x-special/gnome-copied-files < %s || "
		"tail -n +2 %s | wl-copy --type text/uri-list || true; "
		"elif command -v xclip >/dev/null 2>&1; then "
		"xclip -selection clipboard -t x-special/gnome-copied-files < %s || "
		"tail -n +2 %s | xclip -selection clipboard -t text/uri-list || true; "
		"fi; rm -f %s; true",
		quoted, quoted, quoted, quoted, quoted);
	launch_command(cmd);
}

static void launch_desktop_paths_copy(
		const char *const *paths,
		int path_count) {
	if (paths == NULL || path_count <= 0) {
		return;
	}
	GString *payload = g_string_new("copy\n");
	int copied = 0;
	for (int i = 0; i < path_count; i++) {
		if (paths[i] == NULL || paths[i][0] == '\0') {
			continue;
		}
		GFile *file = g_file_new_for_path(paths[i]);
		char *uri = g_file_get_uri(file);
		if (uri != NULL && uri[0] != '\0') {
			g_string_append(payload, uri);
			g_string_append_c(payload, '\n');
			copied++;
		}
		g_free(uri);
		g_object_unref(file);
	}
	if (copied > 0) {
		launch_clipboard_copy_payload(payload->str);
	}
	g_string_free(payload, TRUE);
}

static void launch_desktop_file_copy(const char *path) {
	const char *paths[1] = {path};
	launch_desktop_paths_copy(paths, 1);
}

static void launch_desktop_paste_to_desktop(struct orange_server *server) {
	char desktop_path[1024];
	if (!desktop_directory_path(desktop_path, sizeof(desktop_path))) {
		return;
	}
	if (mkdir(desktop_path, 0755) < 0 && errno != EEXIST) {
		return;
	}
	GFile *desktop = g_file_new_for_path(desktop_path);
	char *desktop_uri = g_file_get_uri(desktop);
	g_object_unref(desktop);
	if (desktop_uri == NULL || desktop_uri[0] == '\0') {
		g_free(desktop_uri);
		return;
	}
	char quoted_path[4096];
	char quoted_uri[4096];
	if (!shell_quote_arg(desktop_path, quoted_path, sizeof(quoted_path)) ||
			!shell_quote_arg(desktop_uri, quoted_uri, sizeof(quoted_uri))) {
		g_free(desktop_uri);
		return;
	}
	g_free(desktop_uri);
	char cmd[12000];
	snprintf(cmd, sizeof(cmd),
		"dest=%s; dest_uri=%s; tmp=$(mktemp) || exit 0; "
		"if command -v wl-paste >/dev/null 2>&1; then "
		"wl-paste --type x-special/gnome-copied-files > \"$tmp\" 2>/dev/null || "
		"wl-paste --type text/uri-list > \"$tmp\" 2>/dev/null || "
		"{ rm -f \"$tmp\"; exit 0; }; "
		"elif command -v xclip >/dev/null 2>&1; then "
		"xclip -selection clipboard -o -t x-special/gnome-copied-files > \"$tmp\" 2>/dev/null || "
		"xclip -selection clipboard -o -t text/uri-list > \"$tmp\" 2>/dev/null || "
		"{ rm -f \"$tmp\"; exit 0; }; "
		"else rm -f \"$tmp\"; exit 0; fi; "
		"while IFS= read -r line; do "
		"case \"$line\" in \"\"|\\#*|copy|cut) continue;; "
		"file://*) gio copy --preserve \"$line\" \"$dest_uri\" 2>/dev/null || true;; "
		"/*) cp -a -- \"$line\" \"$dest/\" 2>/dev/null || true;; "
		"esac; done < \"$tmp\"; rm -f \"$tmp\"; true",
		quoted_path, quoted_uri);
	launch_command(cmd);
	if (server != NULL) {
		server->desktop_file_poll_ticks = 0;
	}
}

static bool create_new_desktop_folder(struct orange_server *server) {
	char desktop_path[1024];
	if (!desktop_directory_path(desktop_path, sizeof(desktop_path))) {
		return false;
	}
	if (mkdir(desktop_path, 0755) < 0 && errno != EEXIST) {
		return false;
	}
	for (int n = 0; n < 100; n++) {
		char path[1200];
		int wrote = n == 0 ?
			snprintf(path, sizeof(path), "%s/New Folder", desktop_path) :
			snprintf(path, sizeof(path), "%s/New Folder %d",
				desktop_path, n + 1);
		if (wrote <= 0 || wrote >= (int)sizeof(path)) {
			return false;
		}
		if (mkdir(path, 0755) == 0) {
			if (server != NULL) {
				update_desktop_files(server);
				server_mark_shell_dirty(server);
			}
			return true;
		}
		if (errno != EEXIST) {
			return false;
		}
	}
	return false;
}

static void launch_desktop_file_get_info(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd),
		"p=%s; " ORANGE_GNOME_APP_ENV "nautilus --properties \"$p\" 2>/dev/null || "
		"gio info \"$p\" || true",
		quoted);
	launch_command(cmd);
}

static void launch_desktop_file_duplicate(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[9000];
	snprintf(cmd, sizeof(cmd),
		"p=%s; dir=$(dirname -- \"$p\"); base=$(basename -- \"$p\"); "
		"n=0; while [ $n -lt 100 ]; do "
		"if [ $n -eq 0 ]; then target=\"$dir/$base copy\"; "
		"else target=\"$dir/$base copy $n\"; fi; "
		"if ! [ -e \"$target\" ]; then cp -a -- \"$p\" \"$target\"; break; fi; "
		"n=$((n + 1)); done; true",
		quoted);
	launch_command(cmd);
}

static void launch_desktop_file_quick_look(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd),
		"p=%s; sushi \"$p\" 2>/dev/null || "
		"gio open \"$p\" || xdg-open \"$p\" || true",
		quoted);
	launch_command(cmd);
}

static void launch_desktop_file_share(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd),
		"p=%s; xdg-email --attach \"$p\" 2>/dev/null || "
		"gio open \"$(dirname -- \"$p\")\" || true",
		quoted);
	launch_command(cmd);
}

static void launch_desktop_file_trash(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[4300];
	snprintf(cmd, sizeof(cmd), "gio trash %s || true", quoted);
	launch_command(cmd);
}

static void cancel_desktop_rename(struct orange_server *server) {
	if (server == NULL) {
		return;
	}
	orange_desktop_rename_reset(&server->desktop_rename);
	server_mark_shell_dirty(server);
}

static void finish_desktop_rename(
		struct orange_server *server,
		bool commit) {
	if (server == NULL || !server->desktop_rename.active) {
		return;
	}
	char old_path[sizeof(server->desktop_rename.path)];
	char new_name[sizeof(server->desktop_rename.text)];
	snprintf(old_path, sizeof(old_path), "%s", server->desktop_rename.path);
	snprintf(new_name, sizeof(new_name), "%s", server->desktop_rename.text);
	cancel_desktop_rename(server);
	if (!commit || old_path[0] == '\0' || new_name[0] == '\0' ||
			strchr(new_name, '/') != NULL) {
		return;
	}
	const char *old_base = strrchr(old_path, '/');
	old_base = old_base != NULL ? old_base + 1 : old_path;
	if (strcmp(old_base, new_name) == 0) {
		return;
	}
	char new_path[1024];
	const char *slash = strrchr(old_path, '/');
	if (slash == NULL) {
		snprintf(new_path, sizeof(new_path), "%s", new_name);
	} else {
		size_t dir_len = (size_t)(slash - old_path);
		if (dir_len + 1 + strlen(new_name) >= sizeof(new_path)) {
			return;
		}
		memcpy(new_path, old_path, dir_len);
		new_path[dir_len] = '/';
		snprintf(new_path + dir_len + 1,
			sizeof(new_path) - dir_len - 1, "%s", new_name);
	}
	if (rename(old_path, new_path) == 0) {
		update_desktop_files(server);
		server_mark_shell_dirty(server);
	}
}

static void start_desktop_rename(
		struct orange_server *server,
		int layout_index,
		const struct orange_desktop_item_info *info) {
	if (server == NULL || info == NULL ||
			info->kind != ORANGE_DESKTOP_ITEM_FILE ||
			info->index < 0 || info->index >= server->desktop_file_count) {
		return;
	}
	const struct orange_file_info *file = &server->desktop_files[info->index];
	orange_desktop_rename_begin(&server->desktop_rename,
		layout_index,
		info->index,
		file->name,
		file->path,
		file->is_directory);
	server->desktop_last_click_index = -1;
	server->desktop_last_click_time_ms = 0;
	server_mark_shell_dirty(server);
}

static void launch_volume_open_path(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[8400];
	snprintf(cmd, sizeof(cmd), "gio open %s || xdg-open %s || true",
		quoted, quoted);
	launch_command(cmd);
}

static void launch_volume_get_info_path(const char *path) {
	char quoted[4096];
	if (!shell_quote_arg(path, quoted, sizeof(quoted))) {
		return;
	}
	char cmd[9000];
	snprintf(cmd, sizeof(cmd),
		ORANGE_GNOME_APP_ENV "nautilus --properties %s 2>/dev/null || gio info %s || true",
		quoted, quoted);
	launch_command(cmd);
}

static const char *desktop_item_path_for_info(
		struct orange_server *server,
		struct orange_desktop_item_info info) {
	if (info.kind == ORANGE_DESKTOP_ITEM_FILE &&
			info.index >= 0 && info.index < server->desktop_file_count) {
		return server->desktop_files[info.index].path;
	}
	if (info.kind == ORANGE_DESKTOP_ITEM_VOLUME &&
			info.index >= 0 && info.index < server->desktop_volume_count) {
		return server->volumes[info.index].mount_path;
	}
	return NULL;
}

static bool current_desktop_layout(
		struct orange_server *server,
		struct orange_shell_layout *layout) {
	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	(void)local_x;
	(void)local_y;
	if (output == NULL) {
		return false;
	}
	compute_shell_layout_for_output(server, output, layout);
	return true;
}

static void launch_selected_desktop_copy(struct orange_server *server,
		const struct orange_shell_layout *layout) {
	const char *paths[ORANGE_DESKTOP_MAX];
	int path_count = 0;
	for (int i = 0; i < layout->desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (!server->desktop_selected[i]) {
			continue;
		}
		const char *path = desktop_item_path_for_info(server,
			layout->desktop_item_info[i]);
		if (path == NULL || path[0] == '\0') {
			continue;
		}
		paths[path_count++] = path;
	}
	launch_desktop_paths_copy(paths, path_count);
}

static void handle_desktop_selection_action(
		struct orange_server *server,
		enum orange_context_menu_kind kind,
		int item_index) {
	struct orange_shell_layout layout;
	if (!current_desktop_layout(server, &layout)) {
		return;
	}
	for (int i = 0; i < layout.desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (!server->desktop_selected[i]) {
			continue;
		}
		struct orange_desktop_item_info info = layout.desktop_item_info[i];
		const char *path = desktop_item_path_for_info(server, info);
		if (path == NULL || path[0] == '\0') {
			continue;
		}
		if (kind == ORANGE_CONTEXT_MENU_DESKTOP_SELECTION) {
			switch (item_index) {
			case 0:
				if (info.kind == ORANGE_DESKTOP_ITEM_VOLUME) {
					launch_volume_open_path(path);
				} else {
					launch_desktop_file_open(path);
				}
				break;
			case 2:
				if (info.kind == ORANGE_DESKTOP_ITEM_VOLUME) {
					launch_volume_get_info_path(path);
				} else {
					launch_desktop_file_get_info(path);
				}
				break;
			case 3:
				launch_desktop_file_share(path);
				break;
			default:
				break;
			}
		} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION &&
				info.kind == ORANGE_DESKTOP_ITEM_FILE) {
			switch (item_index) {
			case 0:
				launch_desktop_file_open(path);
				break;
			case 1:
				launch_desktop_file_show_in_files(path);
				break;
			case 3:
				launch_desktop_file_get_info(path);
				break;
			case 4:
				launch_desktop_file_quick_look(path);
				break;
			case 5:
				launch_desktop_file_share(path);
				break;
			case 6:
				launch_desktop_file_trash(path);
				server->trash_poll_ticks = 0;
				break;
			default:
				break;
			}
		}
	}
	if ((kind == ORANGE_CONTEXT_MENU_DESKTOP_SELECTION && item_index == 1) ||
			(kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION &&
			 item_index == 2)) {
		launch_selected_desktop_copy(server, &layout);
	}
}

static void handle_context_menu_action(struct orange_server *server, int item_index) {
	enum orange_context_menu_kind kind = server->context_menu_kind;
	int target = server->context_menu_index;
	bool was_alt = server->context_menu_alt_pressed;
	bool was_dock_temporary = server->context_menu_dock_temporary;
	char context_app_id[sizeof(server->context_menu_app_id)];
	snprintf(context_app_id, sizeof(context_app_id), "%s",
		server->context_menu_app_id);
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;
	server->open_with_submenu_open = false;
	server->dock_options_submenu_open = false;
	server->dock_minimize_submenu_open = false;
	server->dock_position_submenu_open = false;
	server->context_menu_alt_pressed = false;
	server->context_menu_dock_temporary = false;

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
				launch_session_command(applications_settings_command);
			}
			break;
		case 2:
			open_launcher_mode(server, ORANGE_LAUNCHER_MODE_ACTIONS,
				ORANGE_LAUNCHER_DISPLAY_FULL);
			break;
		case 3:
			hide_focused_app(server);
			break;
		case 4:
			hide_other_apps(server);
			break;
		case 5:
			show_all_apps(server);
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
		int launcher_idx = was_dock_temporary ? -1 :
			dock_launcher_for_visible_index(server, target);
		switch (item_index) {
		case 0:
			if (was_dock_temporary) {
				launch_dock_app_id(server, context_app_id);
			} else {
				launch_dock_launcher(server, launcher_idx);
			}
			break;
		case 1:
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK;
			server->context_menu_index = target;
			server->dock_options_submenu_open = true;
			server->context_menu_dock_temporary = was_dock_temporary;
			snprintf(server->context_menu_app_id,
				sizeof(server->context_menu_app_id), "%s",
				context_app_id);
			server_mark_overlay_dirty(server);
			break;
		case 2:
			break;
		case 3:
			if (was_dock_temporary) {
				if (orange_dock_temporary_remove(
						server->dock_temporary_app_ids,
						&server->dock_temporary_count,
						context_app_id)) {
					server->launcher_filter_cache_key[0] = '\0';
					server_mark_dock_dirty(server);
				}
			} else if (orange_dock_config_remove_visible(&server->config, target)) {
				orange_config_save(&server->config,
					server->options->config_path);
				server_mark_dock_dirty(server);
			}
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_RUNNING) {
		int launcher_idx = was_dock_temporary ? -1 :
			dock_launcher_for_visible_index(server, target);
		bool alt = was_alt;
		switch (item_index) {
		case 0:
			if (was_dock_temporary) {
				show_windows_for_dock_identity(server, context_app_id);
			} else {
				show_windows_for_dock_launcher(server, launcher_idx);
			}
			break;
		case 1:
			if (alt) {
				hide_other_apps(server);
			} else if (was_dock_temporary) {
				hide_windows_for_dock_identity(server, context_app_id);
			} else {
				hide_windows_for_dock_launcher(server, launcher_idx);
			}
			break;
		case 2:
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_RUNNING;
			server->context_menu_index = target;
			server->dock_options_submenu_open = true;
			server->context_menu_alt_pressed = was_alt;
			server->context_menu_dock_temporary = was_dock_temporary;
			snprintf(server->context_menu_app_id,
				sizeof(server->context_menu_app_id), "%s",
				context_app_id);
			server_mark_overlay_dirty(server);
			break;
		case 3:
			if (was_dock_temporary) {
				close_windows_for_dock_identity(server, context_app_id);
			} else {
				close_windows_for_dock_launcher(server, launcher_idx);
			}
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_LAUNCHER) {
		int launcher_idx = dock_launcher_for_visible_index(server, target);
		switch (item_index) {
		case 0:
			launch_dock_launcher(server, launcher_idx);
			break;
		case 1:
			launch_session_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_TRASH) {
		switch (item_index) {
		case 0:
			launch_command(orange_trash_command);
			break;
		case 1:
			launch_command("gio trash --empty || trash-empty || true");
			server->trash_poll_ticks = 0;
			break;
		case 2:
			launch_session_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR) {
		switch (item_index) {
		case 0:
			server->config.dock_auto_hide =
				!server->config.dock_auto_hide;
			server->dock_auto_hide_revealed = false;
			dock_auto_hide_update_animation(server, monotonic_time_msec());
			orange_config_save(&server->config,
				server->options->config_path);
			server_mark_shell_dirty(server);
			break;
		case 1:
			server->config.dock_magnification =
				!server->config.dock_magnification;
			orange_config_save(&server->config,
				server->options->config_path);
			break;
		case 2:
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_SEPARATOR;
			server->context_menu_index = target;
			server->dock_minimize_submenu_open = true;
			server_mark_overlay_dirty(server);
			break;
		case 3:
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_SEPARATOR;
			server->context_menu_index = target;
			server->dock_position_submenu_open = true;
			server_mark_overlay_dirty(server);
			break;
		case 4:
			launch_session_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_WIDGET) {
		switch (item_index) {
		case 0:
			launch_session_command(orange_settings_command);
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
		case 1: {
			if (target >= 0 && target < (int)server->desktop_entry_count) {
				const char *app_id = server->desktop_entries[target].id;
				const char *home = getenv("HOME");
				if (home == NULL) home = "";
				char cmd[1024];
				snprintf(cmd, sizeof(cmd),
					"if [ -d \"%s/.config/%s\" ]; then xdg-open \"%s/.config/%s\"; "
					"else xdg-open \"%s/.config\"; fi || true",
					home, app_id, home, app_id, home);
				launch_command(cmd);
			}
			break;
		}
		case 3:
			launch_session_command(applications_settings_command);
			break;
		case 6: {
			if (target >= 0 && target < (int)server->desktop_entry_count) {
				const char *app_id = server->desktop_entries[target].id;
				const char *home = getenv("HOME");
				if (home == NULL) home = "";
				char cmd[1024];
				snprintf(cmd, sizeof(cmd),
					"if [ -d \"%s/.config/%s\" ]; then xdg-open \"%s/.config/%s\"; "
					"else xdg-open \"%s/.config\"; fi || true",
					home, app_id, home, app_id, home);
				launch_command(cmd);
			}
			break;
		}
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE) {
		int file_idx = desktop_file_index_for_context_target(server, target);
		if (file_idx >= 0 && file_idx < server->desktop_file_count) {
			const struct orange_file_info *info = &server->desktop_files[file_idx];
			switch (item_index) {
			case 0:
				launch_desktop_file_open(info->path);
				break;
			case 1:
				populate_open_with_apps(server, file_idx);
				server->context_menu_kind =
					ORANGE_CONTEXT_MENU_DESKTOP_FILE;
				server->context_menu_index = target;
				server->open_with_submenu_open = true;
				server_mark_overlay_dirty(server);
				break;
			case 2:
				launch_desktop_file_show_in_files(info->path);
				break;
			case 3:
				launch_desktop_file_copy(info->path);
				break;
			case 4:
				launch_desktop_file_get_info(info->path);
				break;
			case 5: {
				struct orange_desktop_item_info item_info;
				if (desktop_item_info_for_context_target(server,
						target, &item_info)) {
					start_desktop_rename(server, target, &item_info);
				}
				break;
			}
			case 6:
				launch_desktop_file_duplicate(info->path);
				break;
			case 7:
				launch_desktop_file_quick_look(info->path);
				break;
			case 8:
				launch_desktop_file_share(info->path);
				break;
			case 9:
				launch_desktop_file_trash(info->path);
				server->trash_poll_ticks = 0;
				break;
			default:
				break;
			}
		}
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH) {
		launch_desktop_file_open_with(server, item_index);
		server->open_with_file_index = -1;
		server->open_with_app_count = 0;
		server->open_with_submenu_open = false;
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_SELECTION ||
			kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION) {
		handle_desktop_selection_action(server, kind, item_index);
	} else if (kind == ORANGE_CONTEXT_MENU_DESKTOP_VOLUME) {
		int vol_idx = desktop_volume_index_for_context_target(server, target);
		if (vol_idx >= 0 && vol_idx < server->desktop_volume_count) {
			const struct orange_volume_info *vol = &server->volumes[vol_idx];
			char quoted[4096];
			switch (item_index) {
			case 0: /* Open */
				if (vol->mount_path[0] != '\0') {
					launch_volume_open_path(vol->mount_path);
				}
				break;
			case 1: /* Get Info */
				if (vol->mount_path[0] != '\0') {
					launch_volume_get_info_path(vol->mount_path);
				}
				break;
			case 2: /* Eject */
				if (vol->mount_path[0] != '\0' &&
						shell_quote_arg(vol->mount_path, quoted,
							sizeof(quoted))) {
					char cmd[4300];
					snprintf(cmd, sizeof(cmd), "gio mount -u %s || true",
						quoted);
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
			create_new_desktop_folder(server);
			break;
		case 1:
			launch_desktop_paste_to_desktop(server);
			break;
		case 2:
			launch_command(ORANGE_GNOME_APP_ENV "nautilus --properties \"$HOME/Desktop\" 2>/dev/null || xdg-open \"$HOME/Desktop\" || true");
			break;
		case 3:
			launch_session_command(background_settings_command);
			break;
		case 4:
			launch_session_command(orange_settings_command);
			break;
		case 5:
			server->config.desktop_use_stacks =
				!server->config.desktop_use_stacks;
			orange_config_save(&server->config,
				server->options->config_path);
			break;
		case 6:
			cycle_desktop_sort_mode(server);
			break;
		case 7:
			clean_up_desktop_grid(server);
			break;
		case 8:
			launch_session_command(orange_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_WIFI) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_session_command(network_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_SOUND) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_session_command(sound_settings_command);
			break;
		default:
			break;
		}
	} else if (kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY) {
		switch (item_index) {
		case 0:
		case 1:
		case 2:
			launch_session_command(power_settings_command);
			break;
		default:
			break;
		}
	}
	server_mark_shell_dirty(server);
}

static void handle_context_submenu_action(
		struct orange_server *server,
		int item_index) {
	if (server->context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE &&
			server->open_with_submenu_open) {
		launch_desktop_file_open_with(server, item_index);
	} else if (server->dock_options_submenu_open) {
		int launcher_idx = dock_launcher_for_visible_index(server,
			server->context_menu_index);
		switch (item_index) {
		case 0:
			if (server->context_menu_dock_temporary) {
				if (orange_dock_config_insert_app(&server->config,
						server->context_menu_app_id, -1)) {
					orange_config_save(&server->config,
						server->options->config_path);
					server_mark_dock_dirty(server);
				}
			} else if (launcher_idx >= 0 &&
					launcher_idx < ORANGE_DOCK_MAX) {
				if (orange_dock_config_remove_visible(&server->config,
						server->context_menu_index)) {
					orange_config_save(&server->config,
						server->options->config_path);
					server_mark_dock_dirty(server);
				}
			}
			break;
		case 1:
			show_dock_app_in_files(server, launcher_idx);
			break;
		case 2:
			toggle_open_at_login(server, launcher_idx);
			break;
		default:
			break;
		}
	} else if (server->dock_minimize_submenu_open) {
		switch (item_index) {
		case 0:
			set_minimize_effect(server, ORANGE_MINIMIZE_EFFECT_GENIE);
			break;
		case 1:
			set_minimize_effect(server, ORANGE_MINIMIZE_EFFECT_SCALE);
			break;
		default:
			break;
		}
	} else if (server->dock_position_submenu_open) {
		switch (item_index) {
		case 0:
			set_dock_position(server, ORANGE_DOCK_POSITION_LEFT);
			break;
		case 1:
			set_dock_position(server, ORANGE_DOCK_POSITION_BOTTOM);
			break;
		case 2:
			set_dock_position(server, ORANGE_DOCK_POSITION_RIGHT);
			break;
		default:
			break;
		}
	}
	clear_context_menu(server);
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
	if (index < ORANGE_DESKTOP_MAX && !server->desktop_selected[index]) {
		for (int i = 0; i < ORANGE_DESKTOP_MAX; i++) {
			server->desktop_selected[i] = false;
		}
		server->desktop_selected[index] = true;
	}
	for (int i = 0; i < layout->desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (server->desktop_selected[i]) {
			server->desktop_drag_initial_x[i] = layout->desktop_items[i].x;
			server->desktop_drag_initial_y[i] = layout->desktop_items[i].y;
		}
	}
}

static struct orange_rect normalized_rect_from_points(
		int x1,
		int y1,
		int x2,
		int y2) {
	int x = x1 < x2 ? x1 : x2;
	int y = y1 < y2 ? y1 : y2;
	int right = x1 > x2 ? x1 : x2;
	int bottom = y1 > y2 ? y1 : y2;
	return (struct orange_rect){x, y, right - x, bottom - y};
}

static bool rects_intersect(struct orange_rect a, struct orange_rect b) {
	return a.x < b.x + b.width &&
		a.x + a.width > b.x &&
		a.y < b.y + b.height &&
		a.y + a.height > b.y;
}

static bool rect_contains_point(struct orange_rect rect, int x, int y) {
	return x >= rect.x && y >= rect.y &&
		x < rect.x + rect.width && y < rect.y + rect.height;
}

static struct orange_shell_hit desktop_item_context_hit(
		const struct orange_shell_layout *layout,
		int x,
		int y) {
	if (layout == NULL) {
		return (struct orange_shell_hit){ORANGE_HIT_NONE, -1};
	}
	for (int i = 0; i < layout->desktop_item_count; i++) {
		if (rect_contains_point(layout->desktop_items[i], x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_DESKTOP_ITEM, i};
		}
	}
	for (int i = 0; i < layout->desktop_item_count; i++) {
		struct orange_rect hit =
			orange_shell_desktop_item_context_rect(layout, i);
		if (rect_contains_point(hit, x, y)) {
			return (struct orange_shell_hit){ORANGE_HIT_DESKTOP_ITEM, i};
		}
	}
	return (struct orange_shell_hit){ORANGE_HIT_NONE, -1};
}

static void update_desktop_selection_from_rect(
		struct orange_server *server,
		const struct orange_shell_layout *layout,
		struct orange_rect selection) {
	for (int i = 0; i < ORANGE_DESKTOP_MAX; i++) {
		server->desktop_selected[i] = false;
	}
	for (int i = 0; i < layout->desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (rects_intersect(selection, layout->desktop_items[i])) {
			server->desktop_selected[i] = true;
		}
	}
}

static void start_desktop_selection(struct orange_server *server,
		int local_x,
		int local_y) {
	server->desktop_select_active = true;
	server->desktop_select_moved = false;
	server->desktop_select_start_x = local_x;
	server->desktop_select_start_y = local_y;
	server->desktop_select_x = local_x;
	server->desktop_select_y = local_y;
	for (int i = 0; i < ORANGE_DESKTOP_MAX; i++) {
		server->desktop_selected[i] = false;
	}
}

static void process_desktop_selection(struct orange_server *server) {
	if (!server->desktop_select_active) {
		return;
	}
	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		return;
	}
	int dx = local_x - server->desktop_select_start_x;
	int dy = local_y - server->desktop_select_start_y;
	if (abs(dx) > 4 || abs(dy) > 4) {
		server->desktop_select_moved = true;
	}
	server->desktop_select_x = local_x;
	server->desktop_select_y = local_y;
	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct orange_rect selection = normalized_rect_from_points(
		server->desktop_select_start_x,
		server->desktop_select_start_y,
		server->desktop_select_x,
		server->desktop_select_y);
	update_desktop_selection_from_rect(server, &layout, selection);
	server_mark_shell_dirty(server);
}

static void finish_desktop_selection(struct orange_server *server) {
	if (!server->desktop_select_active) {
		return;
	}
	bool moved = server->desktop_select_moved;
	server->desktop_select_active = false;
	server->desktop_select_moved = false;
	if (!moved) {
		for (int i = 0; i < ORANGE_DESKTOP_MAX; i++) {
			server->desktop_selected[i] = false;
		}
	}
	server_mark_shell_dirty(server);
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

	if (server->config.desktop_sort_by != ORANGE_DESKTOP_SORT_NONE) {
		server->config.desktop_sort_by = ORANGE_DESKTOP_SORT_NONE;
	}

	struct orange_rect work = orange_shell_layout_work_area(&layout);
	for (int i = 0; i < layout.desktop_item_count && i < ORANGE_DESKTOP_MAX; i++) {
		if (!server->desktop_selected[i]) {
			continue;
		}
		int next_x = server->desktop_drag_initial_x[i] + dx;
		int next_y = server->desktop_drag_initial_y[i] + dy;
		int min_x = work.x;
		int min_y = work.y;
		int max_x = work.x + work.width - layout.desktop_items[i].width;
		int max_y = work.y + work.height - layout.desktop_items[i].height;
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
		server->config.desktop_positions[i].valid = true;
		server->config.desktop_positions[i].x = next_x;
		server->config.desktop_positions[i].y = next_y;
	}
	server_mark_shell_dirty(server);
}

static void launch_desktop_item(
		struct orange_server *server,
		const struct orange_desktop_item_info *info) {
	if (server == NULL || info == NULL) {
		return;
	}
	if (info->kind == ORANGE_DESKTOP_ITEM_ENTRY &&
			info->index >= 0 &&
			(size_t)info->index < server->desktop_entry_count) {
		start_cursor_loading(server, -1);
		launch_desktop_entry(&server->desktop_entries[info->index]);
	} else if (info->kind == ORANGE_DESKTOP_ITEM_FILE &&
			info->index >= 0 && info->index < server->desktop_file_count) {
		start_cursor_loading(server, -1);
		launch_desktop_file_open(server->desktop_files[info->index].path);
	} else if (info->kind == ORANGE_DESKTOP_ITEM_VOLUME &&
			info->index >= 0 && info->index < server->desktop_volume_count) {
		const struct orange_volume_info *vol = &server->volumes[info->index];
		if (vol->mount_path[0] == '\0') {
			return;
		}
		char quoted[4096];
		if (!shell_quote_arg(vol->mount_path, quoted, sizeof(quoted))) {
			return;
		}
		char cmd[8400];
		snprintf(cmd, sizeof(cmd),
			"gio open %s || xdg-open %s || true",
			quoted, quoted);
		start_cursor_loading(server, -1);
		launch_command(cmd);
	}
}

static void finish_desktop_drag(
		struct orange_server *server,
		uint32_t time_msec) {
	if (!server->desktop_drag_active) {
		return;
	}
	int index = server->desktop_drag_index;
	bool moved = server->desktop_drag_moved;
	server->desktop_drag_active = false;
	server->desktop_drag_moved = false;
	server->desktop_drag_index = -1;

	if (moved) {
		server->desktop_last_click_index = -1;
		server->desktop_last_click_time_ms = 0;
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else {
		struct orange_desktop_item_info info;
		if (!desktop_item_info_for_context_target(server, index, &info)) {
			return;
		}
		bool double_click = index == server->desktop_last_click_index &&
			(uint32_t)(time_msec - server->desktop_last_click_time_ms) <=
				DESKTOP_DOUBLE_CLICK_MS;
		if (double_click) {
			server->desktop_last_click_index = -1;
			server->desktop_last_click_time_ms = 0;
			launch_desktop_item(server, &info);
		} else {
			server->desktop_last_click_index = index;
			server->desktop_last_click_time_ms = time_msec;
		}
		server_mark_shell_dirty(server);
	}
}

static void start_dock_drag(struct orange_server *server,
		const struct orange_shell_layout *layout, int index) {
	server->dock_drag_active = true;
	server->dock_drag_moved = false;
	server->dock_drag_index = index;
	server->dock_drag_temporary = layout != NULL &&
		index >= 0 && index < layout->dock_item_count &&
		layout->dock_temporary[index];
	server->dock_drag_app_id[0] = '\0';
	if (server->dock_drag_temporary) {
		snprintf(server->dock_drag_app_id,
			sizeof(server->dock_drag_app_id), "%s",
			layout->dock_temporary_app_ids[index]);
	}
	server->dock_drag_start_x = (int)server->cursor->x;
	server->dock_drag_start_y = (int)server->cursor->y;
	server->dock_drag_insert_before = -1;
	server->dock_drag_remove = false;
	(void)layout;
}

static const char *dock_resize_cursor_name(enum orange_dock_position position) {
	(void)position;
	return "ns-resize";
}

static void start_dock_resize(struct orange_server *server,
		const struct orange_shell_layout *layout) {
	if (server == NULL || layout == NULL) {
		return;
	}
	server->dock_resize_active = true;
	server->dock_resize_moved = false;
	server->dock_resize_position = layout->dock_position;
	server->dock_resize_start_x = (int)server->cursor->x;
	server->dock_resize_start_y = (int)server->cursor->y;
	server->dock_resize_start_scale = server->config.dock_scale;
	server->hot_dock_index = -1;
	server->last_dock_pointer_x = INT_MIN;
	server->last_dock_pointer_y = INT_MIN;
	if (server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager,
			dock_resize_cursor_name(server->dock_resize_position));
	}
	server_mark_dock_visual_dirty(server);
}

static void process_dock_resize(struct orange_server *server) {
	if (server == NULL || !server->dock_resize_active) {
		return;
	}
	int current_x = (int)server->cursor->x;
	int current_y = (int)server->cursor->y;
	int dx = current_x - server->dock_resize_start_x;
	int dy = current_y - server->dock_resize_start_y;
	double next = orange_shell_dock_scale_for_separator_drag(
		server->dock_resize_position,
		server->dock_resize_start_scale,
		server->dock_resize_start_x,
		server->dock_resize_start_y,
		current_x,
		current_y);
	if (abs(dx) > 2 || abs(dy) > 2 ||
			fabs(next - server->dock_resize_start_scale) >= 0.005) {
		server->dock_resize_moved = true;
	}
	if (fabs(next - server->config.dock_scale) < 0.005) {
		if (server->xcursor_manager != NULL) {
			wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager,
				dock_resize_cursor_name(server->dock_resize_position));
		}
		return;
	}
	server->config.dock_scale = next;
	server->hot_dock_index = -1;
	server->last_dock_pointer_x = INT_MIN;
	server->last_dock_pointer_y = INT_MIN;
	if (server->xcursor_manager != NULL) {
		wlr_cursor_set_xcursor(server->cursor, server->xcursor_manager,
			dock_resize_cursor_name(server->dock_resize_position));
	}
	server_mark_shell_dirty(server);
}

static void finish_dock_resize(struct orange_server *server) {
	if (server == NULL || !server->dock_resize_active) {
		return;
	}
	bool moved = server->dock_resize_moved;
	server->dock_resize_active = false;
	server->dock_resize_moved = false;
	if (moved) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	}
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
	bool dragged_temp = dragged >= 0 && dragged < layout.dock_item_count &&
		layout.dock_temporary[dragged];
	bool remove = (dragged_temp ||
			dock_visible_index_is_removable(server, dragged)) &&
		dock_remove_zone_contains(&layout, dragged, local_x, local_y);
	int insert = remove || dragged_temp ? -1 :
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
	bool temporary = server->dock_drag_temporary;
	char temporary_app_id[sizeof(server->dock_drag_app_id)];
	snprintf(temporary_app_id, sizeof(temporary_app_id), "%s",
		server->dock_drag_app_id);
	server->dock_drag_active = false;
	server->dock_drag_moved = false;
	server->dock_drag_index = -1;
	server->dock_drag_temporary = false;
	server->dock_drag_app_id[0] = '\0';
	server->dock_drag_insert_before = -1;
	server->dock_drag_remove = false;
	server_mark_overlay_dirty(server);
	if (moved) {
		server_mark_shell_dirty(server);
	}

	if (temporary) {
		if (moved && remove && temporary_app_id[0] != '\0' &&
				!server_dock_temporary_open_for_id(server,
					temporary_app_id) &&
				orange_dock_temporary_remove(
					server->dock_temporary_app_ids,
					&server->dock_temporary_count,
					temporary_app_id)) {
			server->launcher_filter_cache_key[0] = '\0';
			server_mark_dock_dirty(server);
		} else if (!moved && temporary_app_id[0] != '\0') {
			if (!focus_view_for_dock_identity(server, temporary_app_id)) {
				start_cursor_loading(server, -1);
				launch_dock_app_id(server, temporary_app_id);
			}
			server_mark_shell_dirty(server);
		}
		return;
	}

	int dock_count = orange_dock_count(&server->config);
	if (moved && remove && index >= 0 && index < dock_count &&
			orange_dock_config_remove_visible(&server->config, index)) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_dock_dirty(server);
	} else if (moved && index >= 0 && index < dock_count &&
			insert >= 0 &&
			orange_dock_config_reorder_visible(&server->config, index, insert)) {
		orange_config_save(&server->config, server->options->config_path);
		server_mark_dock_dirty(server);
	} else if (!moved && index >= 0 && index < dock_count) {
		int launcher_idx = dock_launcher_for_visible_index(server, index);
		if (launcher_idx < 0 || launcher_idx >= ORANGE_DOCK_MAX ||
				server->config.dock_apps[launcher_idx][0] == '\0') {
			server_mark_shell_dirty(server);
			return;
		}
		if (!focus_view_for_dock_launcher(server, launcher_idx)) {
			if (!orange_dock_app_is_permanent(
					server->config.dock_apps[launcher_idx]) &&
					server->config.animate_opening_applications) {
				server->dock_bounce_active = true;
				server->dock_bounce_start = monotonic_time_msec();
				server->dock_bounce_launcher_idx = launcher_idx;
			}
			if (!orange_dock_app_is_permanent(
					server->config.dock_apps[launcher_idx])) {
				start_cursor_loading(server, launcher_idx);
			}
			launch_dock_launcher(server, launcher_idx);
		}
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
		server_mark_dock_dirty(server);
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

static bool status_notifier_item_for_hit(
		struct orange_server *server,
		int index,
		struct orange_status_notifier_item *item) {
	if (server == NULL || item == NULL ||
			index < 0 || index >= ORANGE_STATUS_NOTIFIER_ITEM_MAX) {
		return false;
	}
	struct orange_status_notifier_item items[ORANGE_STATUS_NOTIFIER_ITEM_MAX];
	int count = server_copy_status_notifier_items(server, items,
		ORANGE_STATUS_NOTIFIER_ITEM_MAX);
	if (index >= count) {
		return false;
	}
	*item = items[index];
	return true;
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
	if (server->desktop_rename.active) {
		bool keep_renaming = false;
		if (hit.kind == ORANGE_HIT_DESKTOP_ITEM &&
				hit.index == server->desktop_rename.layout_index) {
			struct orange_rect label_rect =
				orange_shell_desktop_label_rect(&layout,
					&server->config, hit.index);
			keep_renaming = rect_contains_point(label_rect, local_x, local_y);
		}
		if (!keep_renaming) {
			finish_desktop_rename(server, true);
		}
		if (keep_renaming) {
			server_mark_shell_dirty(server);
			return;
		}
	}

	switch (hit.kind) {
	case ORANGE_HIT_CONTEXT_MENU_ITEM:
		handle_context_menu_action(server, hit.index);
		break;
	case ORANGE_HIT_CONTEXT_SUBMENU_ITEM:
		handle_context_submenu_action(server, hit.index);
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
			default:
				break;
			}
		}
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_TRAY_ITEM:
		clear_context_menu(server);
		server->system_menu_open = false;
		server->notification_center_open = false;
		{
			struct orange_status_notifier_item item;
			if (status_notifier_item_for_hit(server, hit.index, &item)) {
				orange_session_services_activate_status_notifier_item(
					&server->session_services,
					item.id,
					local_x,
					local_y);
			}
		}
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_STATUS_AREA:
		break;
	case ORANGE_HIT_MENU_BAR:
		clear_context_menu(server);
		if (server->notification_center_open) {
			server->notification_center_open = false;
		}
		if (server->system_menu_open) {
			server->system_menu_open = false;
		}
		server_mark_overlay_dirty(server);
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
		launch_session_command(orange_settings_command);
		server_mark_overlay_dirty(server);
		break;
	case ORANGE_HIT_DOCK_ITEM:
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		{
			struct orange_view *view =
				server_minimized_view_for_visible_dock_index(server,
					&layout, hit.index);
			if (view != NULL) {
				set_view_minimized_with_focus(view, false, true);
				server_mark_shell_dirty(server);
				server_mark_overlay_dirty(server);
				break;
			}
		}
		start_dock_drag(server, &layout, hit.index);
		server_mark_overlay_dirty(server);
		break;
		case ORANGE_HIT_DOCK_SEPARATOR:
			server->notification_center_open = false;
			clear_context_menu(server);
			server->system_menu_open = false;
			start_dock_resize(server, &layout);
			server_mark_overlay_dirty(server);
			break;
	case ORANGE_HIT_DOCK:
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
		focus_desktop_files(server);
		server->notification_center_open = false;
		clear_context_menu(server);
		server->system_menu_open = false;
		if (hit.index >= 0 && hit.index < ORANGE_DESKTOP_MAX &&
				server->desktop_selected[hit.index]) {
			struct orange_desktop_item_info info = {0};
			if (hit.index < layout.desktop_item_count) {
				info = layout.desktop_item_info[hit.index];
			}
			struct orange_rect label_rect =
				orange_shell_desktop_label_rect(&layout,
					&server->config, hit.index);
			if (rect_contains_point(label_rect, local_x, local_y) &&
					info.kind == ORANGE_DESKTOP_ITEM_FILE) {
				start_desktop_rename(server, hit.index, &info);
				break;
			}
		}
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
		if (launcher_uses_list_results(server)) {
			launcher_launch_index(server, hit.index);
		} else {
			start_launcher_app_drag(server, hit.index);
		}
		break;
	case ORANGE_HIT_LAUNCHER_BACKGROUND:
		/* Clicking the dimmed backdrop closes the overlay, like macOS. */
		launcher_close_overlay(server);
		break;
	case ORANGE_HIT_DESKTOP:
		focus_desktop_files(server);
		clear_context_menu(server);
		server->desktop_last_click_index = -1;
		server->desktop_last_click_time_ms = 0;
		if (server->notification_center_open) {
			server->notification_center_open = false;
		}
		if (server->system_menu_open) {
			server->system_menu_open = false;
		}
		start_desktop_selection(server, local_x, local_y);
		server_mark_shell_dirty(server);
		server_mark_overlay_dirty(server);
		break;
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

	if (server->desktop_rename.active) {
		finish_desktop_rename(server, true);
	}
	clear_context_menu(server);
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard != NULL) {
		uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
		server->context_menu_alt_pressed =
			(modifiers & WLR_MODIFIER_ALT) != 0;
	}

	struct orange_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, local_x, local_y);
	struct orange_shell_hit desktop_hit =
		desktop_item_context_hit(&layout, local_x, local_y);
	if (desktop_hit.kind == ORANGE_HIT_DESKTOP_ITEM) {
		hit = desktop_hit;
	}

	server->system_menu_open = false;
	server->notification_center_open = false;
	if (server->launcher_open) {
		launcher_close_overlay(server);
		return;
	}
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	if (hit.kind == ORANGE_HIT_DOCK_SEPARATOR) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_SEPARATOR;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DOCK_ITEM) {
		if (server_minimized_view_for_visible_dock_index(server,
				&layout, hit.index) != NULL) {
			server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
			server->context_menu_index = -1;
			server_mark_overlay_dirty(server);
			return;
		}
		bool is_temp = hit.index >= 0 && hit.index < layout.dock_item_count &&
			layout.dock_temporary[hit.index];
		int launcher_idx = -1;
		if (!is_temp) {
			launcher_idx = dock_launcher_for_visible_index(server, hit.index);
		}
		const char *app_id =
			!is_temp && launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX ?
			server->config.dock_apps[launcher_idx] : "";
		if (is_temp) {
			app_id = hit.index >= 0 && hit.index < layout.dock_item_count ?
				layout.dock_temporary_app_ids[hit.index] : "";
		}
		snprintf(server->context_menu_app_id,
			sizeof(server->context_menu_app_id), "%s", app_id);
		if (strcmp(app_id, "__launcher__") == 0) {
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_LAUNCHER;
			server->context_menu_dock_temporary = false;
		} else if (strcmp(app_id, "__trash__") == 0) {
			server->context_menu_kind = ORANGE_CONTEXT_MENU_DOCK_TRASH;
			server->context_menu_dock_temporary = false;
		} else if (is_temp) {
			server->context_menu_kind =
				server_dock_temporary_open_for_id(server, app_id) ?
				ORANGE_CONTEXT_MENU_DOCK_RUNNING :
				ORANGE_CONTEXT_MENU_DOCK;
			server->context_menu_dock_temporary = true;
		} else {
			server->context_menu_kind =
				launcher_idx >= 0 && launcher_idx < ORANGE_DOCK_MAX &&
					server->dock_open[launcher_idx] ?
				ORANGE_CONTEXT_MENU_DOCK_RUNNING : ORANGE_CONTEXT_MENU_DOCK;
			server->context_menu_dock_temporary = false;
		}
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
	} else if (hit.kind == ORANGE_HIT_TRAY_ITEM) {
		struct orange_status_notifier_item item;
		if (status_notifier_item_for_hit(server, hit.index, &item)) {
			orange_session_services_context_menu_status_notifier_item(
				&server->session_services,
				item.id,
				local_x,
				local_y);
		}
		server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
	} else if (hit.kind == ORANGE_HIT_APP_MENU) {
		open_app_menu(server, hit.index, local_x, local_y);
	} else if (hit.kind == ORANGE_HIT_WIDGET) {
		server->context_menu_kind = ORANGE_CONTEXT_MENU_WIDGET;
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP_ITEM) {
		focus_desktop_files(server);
		struct orange_desktop_item_info info = {0};
		if (hit.index >= 0 && hit.index < layout.desktop_item_count) {
			info = layout.desktop_item_info[hit.index];
		}
		struct desktop_selection_summary selection =
			desktop_selection_summary_for_layout(server, &layout, hit.index);
		if (selection.count > 1 && selection.includes_target) {
			server->context_menu_kind =
				selection.volume_count == 0 ?
				ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION :
				ORANGE_CONTEXT_MENU_DESKTOP_SELECTION;
		} else {
			if (hit.index >= 0 && hit.index < ORANGE_DESKTOP_MAX &&
					!selection.includes_target) {
				for (int i = 0; i < ORANGE_DESKTOP_MAX; i++) {
					server->desktop_selected[i] = false;
				}
				server->desktop_selected[hit.index] = true;
				server_mark_shell_dirty(server);
			}
			if (info.kind == ORANGE_DESKTOP_ITEM_FILE) {
				server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_FILE;
			} else if (info.kind == ORANGE_DESKTOP_ITEM_VOLUME) {
				server->context_menu_kind = ORANGE_CONTEXT_MENU_DESKTOP_VOLUME;
			} else {
				server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
			}
		}
		server->context_menu_index = hit.index;
	} else if (hit.kind == ORANGE_HIT_DESKTOP) {
		focus_desktop_files(server);
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

static bool has_active_xdg_popup_grab(struct orange_server *server) {
	return server != NULL && server->xdg_shell != NULL &&
		!wl_list_empty(&server->xdg_shell->popup_grabs);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			server->cursor_mode != ORANGE_CURSOR_PASSTHROUGH) {
		if (event->button == BTN_LEFT &&
				server->cursor_mode == ORANGE_CURSOR_MOVE) {
			finish_interactive_move(server);
		}
		server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
		server->grabbed_view = NULL;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->dock_drag_active) {
		finish_dock_drag(server);
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->dock_resize_active) {
		finish_dock_resize(server);
		return;
	}

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->desktop_drag_active) {
		finish_desktop_drag(server, event->time_msec);
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->desktop_select_active) {
		finish_desktop_selection(server);
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_app_drag_active) {
		finish_launcher_app_drag(server);
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_drag_active) {
		server->launcher_drag_active = false;
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
			event->button == BTN_LEFT &&
			server->launcher_scroll_drag_active) {
		server->launcher_scroll_drag_active = false;
		return;
	}

	bool popup_grab_active = has_active_xdg_popup_grab(server);
	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
			event->button == BTN_LEFT && !popup_grab_active) {
		bool dock_auto_hide_overlay =
			dock_auto_hide_overlay_active(server);
		bool overlay_active = server->context_menu_kind != ORANGE_CONTEXT_MENU_NONE ||
			server->launcher_open || server->system_menu_open ||
			server->notification_center_open ||
			dock_auto_hide_overlay;
		if (overlay_active) {
			int local_x = 0, local_y = 0;
			struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
			if (output != NULL) {
				struct orange_shell_layout layout;
				compute_shell_layout_for_output(server, output, &layout);
				struct orange_shell_hit hit = orange_shell_hit_test(&layout, local_x, local_y);
				switch (hit.kind) {
				case ORANGE_HIT_DOCK:
				case ORANGE_HIT_DOCK_ITEM:
				case ORANGE_HIT_DOCK_SEPARATOR:
				case ORANGE_HIT_CONTEXT_MENU_ITEM:
				case ORANGE_HIT_CONTEXT_SUBMENU_ITEM:
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

	/* An explicit xdg_popup grab is active (e.g. a GTK4 context menu).
	 * GTK4's GtkPopoverMenu installs an invisible "catcher" popup that
	 * typically spans the entire output specifically so it can detect a
	 * click anywhere, including bare desktop with no client surface
	 * under it — so surface_at() here may return NULL (true desktop),
	 * the catcher's own surface, or even land on a totally unrelated
	 * surface. In every one of those cases the correct, protocol-safe
	 * thing to do is hand the event to wlr_seat_pointer_notify_button(),
	 * which wlroots automatically routes through the active grab's own
	 * button handler. That handler is what's responsible for detecting
	 * "click outside the popup chain" and dismissing it in the correct
	 * topmost-first order — it already does this correctly and safely.
	 *
	 * We must NOT call our own focus_view()/handle_shell_click() logic
	 * here: those manually drive wlr_seat_keyboard_notify_enter and
	 * wlr_seat_pointer_notify_enter, which stomps on focus/grab state
	 * that wlroots' popup grab object owns while it's still alive, and
	 * is what was actually crashing — not popup destruction order. */
	if (popup_grab_active) {
		if (surface != NULL) {
			wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
			wlr_seat_pointer_notify_button(server->seat,
				event->time_msec,
				event->button,
				event->state);
		} else {
			wlr_seat_pointer_notify_clear_focus(server->seat);
		}
		return;
	}

	bool right_click_desktop_item = false;
	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
			event->button == BTN_RIGHT) {
		int local_x = 0;
		int local_y = 0;
		struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
		if (output != NULL) {
			struct orange_shell_layout layout;
			compute_shell_layout_for_output(server, output, &layout);
			struct orange_shell_hit shell_hit =
				orange_shell_hit_test(&layout, local_x, local_y);
			if (shell_hit.kind == ORANGE_HIT_DOCK ||
					shell_hit.kind == ORANGE_HIT_DOCK_ITEM ||
					shell_hit.kind == ORANGE_HIT_DOCK_SEPARATOR) {
				wlr_seat_pointer_notify_clear_focus(server->seat);
				handle_shell_right_click(server,
					(int)server->cursor->x,
					(int)server->cursor->y);
				return;
			}
			struct orange_shell_hit hit =
				desktop_item_context_hit(&layout, local_x, local_y);
			right_click_desktop_item =
				hit.kind == ORANGE_HIT_DESKTOP_ITEM;
		}
	}

	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
			event->button == BTN_LEFT) {
		if (surface != NULL && view != NULL) {
			clear_context_menu(server);
			struct wlr_surface *root =
				wlr_surface_get_root_surface(surface);
			struct wlr_surface *keyboard_surface = root != NULL &&
				wlr_xdg_popup_try_from_wlr_surface(root) != NULL ?
				view_get_wlr_surface(view) : surface;
			focus_view(view, keyboard_surface);
			wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		} else {
			wlr_seat_pointer_notify_clear_focus(server->seat);
			handle_shell_click(server, (int)server->cursor->x, (int)server->cursor->y);
			return;
		}
	} else if (event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
			event->button == BTN_RIGHT) {
		if (surface == NULL || right_click_desktop_item) {
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
			event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL) {
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
		event->source,
		event->relative_direction);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, cursor_frame);
	(void)data;
	wlr_seat_pointer_notify_frame(server->seat);
}

static void app_switcher_activate(struct orange_server *server) {
	if (server->app_switcher_active) {
		return;
	}
	server->app_switcher_active = true;
	server->app_switcher_index = 0;
	server->app_switcher_count = 0;

	struct orange_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		if ((server->app_switcher_current_workspace_only ||
				server->window_switcher_current_workspace_only) &&
				!view_is_visible_on_current_workspace(view)) {
			continue;
		}
		if (server->app_switcher_count < 128) {
			server->app_switcher_views[server->app_switcher_count] = view;
			server->app_switcher_count++;
		}
	}
	if (server->app_switcher_count > 0) {
		server_mark_overlay_dirty(server);
	}
}

static void app_switcher_cycle(struct orange_server *server) {
	if (!server->app_switcher_active || server->app_switcher_count == 0) {
		return;
	}
	server->app_switcher_index =
		(server->app_switcher_index + 1) % server->app_switcher_count;
	server_mark_overlay_dirty(server);
}

static void app_switcher_commit(struct orange_server *server) {
	if (!server->app_switcher_active) {
		return;
	}
	server->app_switcher_active = false;
	if (server->app_switcher_count > 0 &&
			server->app_switcher_index >= 0 &&
			server->app_switcher_index < server->app_switcher_count) {
		struct orange_view *view =
			server->app_switcher_views[server->app_switcher_index];
		if (view != NULL && view->mapped) {
			focus_view(view, view_get_wlr_surface(view));
		}
	}
	server->app_switcher_count = 0;
	server_mark_overlay_dirty(server);
}

static void app_switcher_cancel(struct orange_server *server) {
	if (!server->app_switcher_active) {
		return;
	}
	server->app_switcher_active = false;
	server->app_switcher_count = 0;
	server_mark_overlay_dirty(server);
}

static bool handle_keybinding(
		struct orange_server *server,
		xkb_keysym_t sym,
		uint32_t modifiers) {
	bool shift = (modifiers & WLR_MODIFIER_SHIFT) != 0;
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
		app_switcher_activate(server);
		return true;
	case XKB_KEY_Page_Up:
	case XKB_KEY_Left:
		if (shift) {
			server_move_focused_view_to_workspace(server,
				server_effective_current_workspace(server) - 1,
				true);
		} else {
			server_switch_workspace(server,
				server_effective_current_workspace(server) - 1);
		}
		return true;
	case XKB_KEY_Page_Down:
	case XKB_KEY_Right:
		if (shift) {
			server_move_focused_view_to_workspace(server,
				server_effective_current_workspace(server) + 1,
				true);
		} else {
			server_switch_workspace(server,
				server_effective_current_workspace(server) + 1);
		}
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
	bool alt = modifiers & WLR_MODIFIER_ALT;

	if (server->app_switcher_active) {
		if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
			if (!(modifiers & WLR_MODIFIER_LOGO) &&
					!(modifiers & WLR_MODIFIER_ALT)) {
				app_switcher_commit(server);
				return;
			}
			return;
		}
		bool consumed = false;
		for (int i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Tab) {
				app_switcher_cycle(server);
				consumed = true;
				break;
			}
			if (syms[i] == XKB_KEY_Escape) {
				app_switcher_cancel(server);
				consumed = true;
				break;
			}
		}
		if (consumed) {
			return;
		}
		if (!logo && !alt) {
			app_switcher_commit(server);
		}
		return;
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
			server->desktop_rename.active) {
		bool consumed = false;
		for (int i = 0; i < nsyms; i++) {
			xkb_keysym_t sym = syms[i];
			if (sym == XKB_KEY_Escape) {
				finish_desktop_rename(server, false);
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
				finish_desktop_rename(server, true);
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_BackSpace) {
				if (orange_desktop_rename_backspace(
						&server->desktop_rename)) {
					server_mark_shell_dirty(server);
				}
				consumed = true;
				break;
			}
			if (sym == XKB_KEY_Delete || sym == XKB_KEY_KP_Delete) {
				if (orange_desktop_rename_delete_forward(
						&server->desktop_rename)) {
					server_mark_shell_dirty(server);
				}
				consumed = true;
				break;
			}
		}
		if (!consumed && !logo) {
			uint32_t cp = xkb_state_key_get_utf32(
				keyboard->keyboard->xkb_state, keycode);
			if (orange_desktop_rename_insert_codepoint(
					&server->desktop_rename, cp)) {
				server_mark_shell_dirty(server);
				consumed = true;
			}
		}
		if (consumed) {
			return;
		}
	}

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
			/* Ctrl+1/2/3/4 for browse mode switching. */
			if (logo && sym >= XKB_KEY_1 && sym <= XKB_KEY_4) {
				int new_mode = sym - XKB_KEY_1;
				if (new_mode >= 0 && new_mode < ORANGE_LAUNCHER_MODE_COUNT) {
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
				if (launcher_uses_list_results(server)) {
					if (server->launcher_hot_app < 0 &&
							server->launcher_app_count > 0) {
						server->launcher_hot_app = 0;
					} else if (server->launcher_hot_app > 0) {
						server->launcher_hot_app--;
					}
					if (server->launcher_hot_app >= 0 &&
							server->launcher_hot_app < server->launcher_scroll_row) {
						server->launcher_scroll_row = server->launcher_hot_app;
					}
					server_mark_overlay_dirty(server);
				} else if (server->launcher_hot_app < 0 && server->launcher_app_count > 0) {
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
				if (launcher_uses_list_results(server)) {
					if (server->launcher_hot_app < 0 &&
							server->launcher_app_count > 0) {
						server->launcher_hot_app = 0;
					} else if (server->launcher_hot_app + 1 <
							server->launcher_app_count) {
						server->launcher_hot_app++;
					}
					int last_visible = server->launcher_scroll_row + 6;
					if (server->launcher_hot_app > last_visible) {
						server->launcher_scroll_row++;
					}
					server_mark_overlay_dirty(server);
				} else if (server->launcher_hot_app < 0 && server->launcher_app_count > 0) {
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
						if (server->launcher_display_mode ==
								ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY) {
							server->launcher_display_mode =
								ORANGE_LAUNCHER_DISPLAY_FULL;
						}
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
			handled = handle_keybinding(server, syms[i], modifiers);
			if (handled) {
				break;
			}
		}
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && alt &&
			!handled) {
		for (int i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Tab) {
				app_switcher_activate(server);
				handled = true;
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
		keyboard->mod_ctrl = xkb_keymap_mod_get_index(
			wlr_keyboard->keymap, XKB_MOD_NAME_CTRL);
		keyboard->mod_shift = xkb_keymap_mod_get_index(
			wlr_keyboard->keymap, XKB_MOD_NAME_SHIFT);
		keyboard->mod_alt = xkb_keymap_mod_get_index(
			wlr_keyboard->keymap, XKB_MOD_NAME_ALT);
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
	case WLR_INPUT_DEVICE_TABLET:
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

static void set_view_minimized_with_focus(
		struct orange_view *view,
		bool minimized,
		bool focus_on_restore) {
	if (view == NULL || !view->mapped) {
		return;
	}
	if (view->minimized == minimized) {
		return;
	}
	struct orange_server *server = view->server;
	if (minimized && server->minimize_animation.active &&
			server->minimize_animation.view == view &&
			server->minimize_animation.restoring) {
		server->minimize_animation.focus_on_complete = false;
		minimize_animation_cancel(server, true);
	}
	bool animated = minimize_animation_begin(view, !minimized,
		focus_on_restore);
	view->minimized = minimized;
	if (view->scene_tree != NULL) {
		wlr_scene_node_set_enabled(&view->scene_tree->node,
			!minimized && !animated &&
			view_is_visible_on_current_workspace(view));
	}
	if (!minimized) {
		view_drop_minimized_snapshot(view);
	}
	if (minimized) {
		view_configure_activated(view, false);
		if (view->server->focused_view == view) {
			view->server->focused_view = NULL;
			wlr_seat_keyboard_notify_clear_focus(view->server->seat);
		}
		if (view->server->grabbed_view == view) {
			view->server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
			view->server->grabbed_view = NULL;
		}
	}
	server_mark_shell_dirty(view->server);
}

static void set_view_minimized(struct orange_view *view, bool minimized) {
	set_view_minimized_with_focus(view, minimized, false);
}

static void handle_view_commit(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, commit);
	(void)data;

	if (view_is_xwayland(view)) {
		if (!view->mapped) {
			return;
		}
		struct wlr_surface *surface = view->xwayland_surface->surface;
		if (surface->current.width > 0 && surface->current.height > 0 &&
				view->server->cursor_mode != ORANGE_CURSOR_RESIZE) {
			bool size_changed = (int)surface->current.width != view->width ||
				(int)surface->current.height != view->height;
			view->width = surface->current.width;
			view->height = surface->current.height;
			if (size_changed && view->server->config.dock_auto_hide) {
				server_mark_shell_dirty(view->server);
			}
		}
		return;
	}

	if (view->xdg_surface->initial_commit) {
		if (!view_send_initial_configure(view)) {
			view_schedule_initial_configure(view);
		}
		return;
	}

	if (!view->mapped) {
		return;
	}
	struct wlr_box geom;
	geom = view->xdg_surface->current.geometry;
	if (geom.width > 0 && geom.height > 0 &&
			view->server->cursor_mode != ORANGE_CURSOR_RESIZE) {
		bool size_changed = geom.width != view->width ||
			geom.height != view->height;
		view->width = geom.width;
		view->height = geom.height;
		if (size_changed && view->server->config.dock_auto_hide) {
			server_mark_shell_dirty(view->server);
		}
	}
}

static void handle_view_map(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, map);
	(void)data;

	struct orange_server *server = view->server;

	if (view_is_xwayland(view)) {
		struct wlr_xwayland_surface *xwayland_surface = view->xwayland_surface;

		if (view->scene_tree == NULL) {
			view->scene_tree = wlr_scene_tree_create(
				server->window_tree);
			if (view->scene_tree == NULL) {
				return;
			}
			wlr_scene_surface_create(view->scene_tree,
				xwayland_surface->surface);
			view->scene_tree->node.data = view;
			xwayland_surface->surface->data = view;
		}

		view->width = xwayland_surface->surface->current.width;
		view->height = xwayland_surface->surface->current.height;
		if (view->width <= 0) {
			view->width = 900;
		}
		if (view->height <= 0) {
			view->height = 620;
		}

		int local_x = 0;
		int local_y = 0;
		struct orange_output *output = output_at_cursor(server, &local_x, &local_y);
		(void)local_x;
		(void)local_y;
		if (output == NULL && !wl_list_empty(&server->outputs)) {
			output = wl_container_of(server->outputs.next, output, link);
		}
		if (output != NULL) {
			struct orange_rect work;
			if (!output_work_area_in_layout(server, output, &work)) {
				return;
			}
			int work_left = work.x;
			int work_top = work.y;
			int work_right = work.x + work.width;
			int work_bottom = work.y + work.height;
			view->x = work_left + (work_right - work_left - view->width) / 2;
			view->y = work_top + 30;
			constrain_view_position(server, &view->x, &view->y,
				view->width, view->height);
			if (view->y + view->height > work_bottom) {
				view->y = work_top;
			}
		} else {
			struct wlr_box layout_box;
			wlr_output_layout_get_box(server->output_layout, NULL, &layout_box);
			if (layout_box.width <= 0 || layout_box.height <= 0) {
				layout_box = (struct wlr_box){0, 0,
					server->options->width, server->options->height};
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
		view->workspace = clamp_workspace_index(
			server_effective_current_workspace(server), server->workspace_count);
		if (server->launcher_open) {
			launcher_close_overlay(server);
		}
		server_normalize_workspaces(server);
		server_apply_workspace_visibility(server);
		focus_view(view, xwayland_surface->surface);
		server_mark_dock_dirty(server);
		return;
	}

	struct wlr_xdg_surface *xdg_surface = view->xdg_surface;
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;

	if (view->scene_tree == NULL) {
		view->scene_tree = wlr_scene_xdg_surface_create(
			server->window_tree, xdg_surface);
		if (view->scene_tree == NULL) {
			return;
		}
		view->scene_tree->node.data = view;
		xdg_surface->surface->data = view;
	}

	view_configure_wm_capabilities(view);
	if (!view->maximized && !view->fullscreen) {
		view_configure_size(view, 900, 620);
	}

	struct wlr_box geom;
	geom = view->xdg_surface->current.geometry;
	view->width = geom.width > 0 ? geom.width : 900;
	view->height = geom.height > 0 ? geom.height : 620;

	int local_x = 0;
	int local_y = 0;
	struct orange_output *output = output_at_cursor(
		server, &local_x, &local_y);
	(void)local_x;
	(void)local_y;
	if (output == NULL && !wl_list_empty(&server->outputs)) {
		output = wl_container_of(server->outputs.next, output, link);
	}
	if (output != NULL) {
		struct orange_rect work;
		if (!output_work_area_in_layout(server, output, &work)) {
			return;
		}
		int work_left = work.x;
		int work_top = work.y;
		int work_right = work.x + work.width;
		int work_bottom = work.y + work.height;
		view->x = work_left + (work_right - work_left - view->width) / 2;
		view->y = work_top + 30;
		constrain_view_position(server, &view->x, &view->y,
			view->width, view->height);
		if (view->y + view->height > work_bottom) {
			view->y = work_top;
		}
	} else {
		struct wlr_box layout_box;
		wlr_output_layout_get_box(server->output_layout, NULL,
			&layout_box);
		if (layout_box.width <= 0 || layout_box.height <= 0) {
			layout_box = (struct wlr_box){0, 0,
				server->options->width,
				server->options->height};
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
	view->workspace = clamp_workspace_index(
		server_effective_current_workspace(server),
		server->workspace_count);
	if (toplevel->requested.fullscreen) {
		view->fullscreen = true;
	}
	if (toplevel->requested.maximized) {
		view->maximized = true;
	}
	if (view->fullscreen) {
		apply_fullscreen(view, true);
	} else if (view->maximized) {
		apply_maximize(view, true);
	}
	/* A newly mapped app dismisses the launcher overlay: the launch action is
	 * complete, so the grid/Spotlight view should give way to the app. */
	if (server->launcher_open) {
		launcher_close_overlay(server);
	}
	server_normalize_workspaces(server);
	server_apply_workspace_visibility(server);
	focus_view(view, view_get_wlr_surface(view));
	if (server->cursor_loading_active) {
		int loading_launcher_idx = server->cursor_loading_launcher_idx;
		if (loading_launcher_idx < 0 ||
				dock_launcher_for_view(server, view) ==
					loading_launcher_idx) {
			stop_cursor_loading(server);
		}
	}
	server_mark_dock_dirty(server);
}

static void handle_view_unmap(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, unmap);
	(void)data;
	if (view->server->minimize_animation.view == view) {
		minimize_animation_cancel(view->server, false);
	}
	view->mapped = false;
	view->minimized = false;
	view_drop_minimized_snapshot(view);
	if (view->scene_tree != NULL) {
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	}
	if (view->server->app_switcher_active) {
		app_switcher_cancel(view->server);
	}
	if (view->server->focused_view == view) {
		view->server->focused_view = NULL;
		wlr_seat_keyboard_notify_clear_focus(view->server->seat);
	}
	if (view->server->grabbed_view == view) {
		view->server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}
	server_normalize_workspaces(view->server);
	server_apply_workspace_visibility(view->server);
	server_mark_dock_dirty(view->server);
}

static void handle_view_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, toplevel_destroy);
	(void)data;
	/* Fires from wlr_xdg_toplevel.events.destroy, before wlroots asserts
	 * the toplevel's listener lists are empty. Remove toplevel-owned
	 * listeners here so the asserts pass. The toplevel is still alive. */
	orange_listener_remove(&view->toplevel_destroy);
	orange_listener_remove(&view->set_title);
	orange_listener_remove(&view->set_app_id);
	orange_listener_remove(&view->request_move);
	orange_listener_remove(&view->request_resize);
	orange_listener_remove(&view->request_maximize);
	orange_listener_remove(&view->request_fullscreen);
	orange_listener_remove(&view->request_minimize);
}

static void handle_view_destroy(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, destroy);
	(void)data;
	if (view->server->minimize_animation.view == view) {
		minimize_animation_cancel(view->server, false);
	}
	view_drop_minimized_snapshot(view);
	view_cancel_initial_configure(view);
	wl_list_remove(&view->link);
	orange_listener_remove(&view->map);
	orange_listener_remove(&view->unmap);
	orange_listener_remove(&view->destroy);
	orange_listener_remove(&view->commit);
	orange_listener_remove(&view->toplevel_destroy);
	orange_listener_remove(&view->set_title);
	orange_listener_remove(&view->set_app_id);
	orange_listener_remove(&view->request_move);
	orange_listener_remove(&view->request_resize);
	orange_listener_remove(&view->request_maximize);
	orange_listener_remove(&view->request_fullscreen);
	orange_listener_remove(&view->request_minimize);
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

static void handle_view_set_metadata(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, set_title);
	(void)data;
	view->dock_launcher_index = -1;
	if (view->mapped) {
		server_mark_dock_dirty(view->server);
	}
}

static void handle_view_set_app_id(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, set_app_id);
	(void)data;
	view->dock_launcher_index = -1;
	if (view->mapped) {
		server_mark_dock_dirty(view->server);
	}
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	(void)listener;
	(void)data;
}

static bool popup_configure_ready(struct orange_popup *popup) {
	return popup != NULL && popup->popup != NULL &&
		popup->popup->base != NULL &&
		popup->popup->base->initialized;
}

static bool popup_send_initial_configure(struct orange_popup *popup) {
	if (!popup_configure_ready(popup)) {
		return false;
	}
	struct orange_view *view = popup->owner_view != NULL &&
			popup->owner_view->xdg_surface != NULL &&
			popup->owner_view->mapped ?
		popup->owner_view : view_for_xdg_surface_chain(popup->popup->parent);
	if (view == NULL || view->xdg_surface == NULL) {
		return false;
	}

	struct wlr_box box = view->xdg_surface->current.geometry;
	if (box.width <= 0 || box.height <= 0) {
		box = (struct wlr_box){
			.x = 0,
			.y = 0,
			.width = view->width > 0 ? view->width : 1,
			.height = view->height > 0 ? view->height : 1,
		};
	}
	wlr_xdg_popup_unconstrain_from_box(popup->popup, &box);
	return true;
}

static void handle_popup_initial_configure_idle(void *data) {
	struct orange_popup *popup = data;
	popup->initial_configure_idle = NULL;
	popup_send_initial_configure(popup);
}

static void popup_schedule_initial_configure(struct orange_popup *popup) {
	if (popup == NULL || popup->initial_configure_idle != NULL ||
			popup->server == NULL || popup->server->display == NULL) {
		return;
	}
	struct wl_event_loop *loop =
		wl_display_get_event_loop(popup->server->display);
	popup->initial_configure_idle = wl_event_loop_add_idle(loop,
		handle_popup_initial_configure_idle, popup);
}

static void popup_cancel_initial_configure(struct orange_popup *popup) {
	if (popup == NULL || popup->initial_configure_idle == NULL) {
		return;
	}
	wl_event_source_remove(popup->initial_configure_idle);
	popup->initial_configure_idle = NULL;
}

static void handle_popup_commit(struct wl_listener *listener, void *data) {
	struct orange_popup *popup = wl_container_of(listener, popup, commit);
	(void)data;
	if (popup->popup == NULL || popup->popup->base == NULL) {
		return;
	}
	if (popup->popup->base->initial_commit &&
			!popup_send_initial_configure(popup)) {
		popup_schedule_initial_configure(popup);
	}
}

static void handle_popup_destroy(struct wl_listener *listener, void *data) {
	struct orange_popup *popup = wl_container_of(listener, popup, destroy);
	(void)data;
	popup_cancel_initial_configure(popup);
	orange_listener_remove(&popup->commit);
	orange_listener_remove(&popup->destroy);
	/*
	 * NOTE: popup->popup (the struct wlr_xdg_popup*) is owned by wlroots
	 * and may already be freed/invalidated by the time this destroy
	 * listener runs (destroy_xdg_surface() tears down the role object
	 * before emitting events.destroy). Do NOT dereference popup->popup
	 * here. The struct wlr_xdg_surface itself (cached below at creation
	 * time) is still valid for the duration of this callback, so use
	 * that instead.
	 */
	if (popup->xdg_surface != NULL && popup->xdg_surface->data == popup) {
		popup->xdg_surface->data = NULL;
	}
	if (popup->xdg_surface != NULL && popup->xdg_surface->surface != NULL &&
			popup->xdg_surface->surface->data == popup->owner_view) {
		popup->xdg_surface->surface->data = NULL;
	}
	free(popup);
}

static void handle_new_xdg_popup(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xdg_popup);
	struct wlr_xdg_popup *popup = data;
	if (popup == NULL || popup->base == NULL) {
		return;
	}
	struct wlr_xdg_surface *xdg_surface = popup->base;
	struct wlr_scene_tree *parent_tree =
		popup_parent_scene_tree(popup->parent);
	struct orange_view *owner_view =
		view_for_xdg_surface_chain(popup->parent);

	struct wlr_scene_tree *scene_tree = wlr_scene_xdg_surface_create(
		parent_tree ? parent_tree : server->window_tree, xdg_surface);
	if (scene_tree == NULL) {
		return;
	}

	struct orange_popup *orange_popup = calloc(1, sizeof(*orange_popup));
	if (orange_popup == NULL) {
		wlr_scene_node_destroy(&scene_tree->node);
		return;
	}
	orange_popup->server = server;
	orange_popup->popup = popup;
	orange_popup->xdg_surface = xdg_surface;
	orange_popup->scene_tree = scene_tree;
	orange_popup->owner_view = owner_view;
	orange_listener_init(&orange_popup->commit);
	orange_listener_init(&orange_popup->destroy);
	orange_popup->commit.notify = handle_popup_commit;
	wl_signal_add(&xdg_surface->surface->events.commit,
		&orange_popup->commit);
	orange_popup->destroy.notify = handle_popup_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &orange_popup->destroy);
	xdg_surface->data = orange_popup;
	xdg_surface->surface->data = owner_view;
}

static void handle_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *toplevel = data;
	struct wlr_xdg_surface *xdg_surface = toplevel->base;

	struct orange_view *view = calloc(1, sizeof(*view));
	if (view == NULL) {
		return;
	}

	view->server = server;
	view->xdg_surface = xdg_surface;
	view->scene_tree = NULL;
	view->dock_launcher_index = -1;

	view->map.notify = handle_view_map;
	wl_signal_add(&xdg_surface->surface->events.map, &view->map);
	view->unmap.notify = handle_view_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_view_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
	view->commit.notify = handle_view_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->commit);

	view->toplevel_destroy.notify = handle_view_toplevel_destroy;
	wl_signal_add(&xdg_surface->toplevel->events.destroy,
		&view->toplevel_destroy);
	view->set_title.notify = handle_view_set_metadata;
	wl_signal_add(&xdg_surface->toplevel->events.set_title,
		&view->set_title);
	view->set_app_id.notify = handle_view_set_app_id;
	wl_signal_add(&xdg_surface->toplevel->events.set_app_id,
		&view->set_app_id);

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

	wl_list_insert(&server->views, &view->link);
}

static void handle_xwayland_ready(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, xwayland_ready);
	struct wlr_xwayland *xwayland = data;
	(void)xwayland;

	setenv("DISPLAY", server->xwayland->display_name, true);
	snprintf(orange_server_xwayland_display,
		sizeof(orange_server_xwayland_display), "%s",
		server->xwayland->display_name);
	wlr_log(WLR_INFO, "xwayland ready, DISPLAY=%s",
		server->xwayland->display_name);
	server_export_client_environment();
}

static void handle_xwayland_surface_associate(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, xwayland_associate);
	struct wlr_xwayland_surface *xwayland_surface = data;
	(void)xwayland_surface;

	view->map.notify = handle_view_map;
	wl_signal_add(&view->xwayland_surface->surface->events.map, &view->map);
	view->unmap.notify = handle_view_unmap;
	wl_signal_add(&view->xwayland_surface->surface->events.unmap, &view->unmap);
	view->commit.notify = handle_view_commit;
	wl_signal_add(&view->xwayland_surface->surface->events.commit, &view->commit);
}

static void handle_xwayland_surface_dissociate(struct wl_listener *listener, void *data) {
	struct orange_view *view = wl_container_of(listener, view, xwayland_dissociate);
	(void)data;
	orange_listener_remove(&view->map);
	orange_listener_remove(&view->unmap);
	orange_listener_remove(&view->commit);
}

static void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xwayland_surface = data;

	if (xwayland_surface->override_redirect) {
		return;
	}

	struct orange_view *view = calloc(1, sizeof(*view));
	if (view == NULL) {
		return;
	}

	view->server = server;
	view->xwayland_surface = xwayland_surface;
	view->scene_tree = NULL;
	view->dock_launcher_index = -1;

	view->destroy.notify = handle_view_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);

	view->xwayland_associate.notify = handle_xwayland_surface_associate;
	wl_signal_add(&xwayland_surface->events.associate, &view->xwayland_associate);
	view->xwayland_dissociate.notify = handle_xwayland_surface_dissociate;
	wl_signal_add(&xwayland_surface->events.dissociate, &view->xwayland_dissociate);

	if (xwayland_surface->surface != NULL) {
		view->map.notify = handle_view_map;
		wl_signal_add(&xwayland_surface->surface->events.map, &view->map);
		view->unmap.notify = handle_view_unmap;
		wl_signal_add(&xwayland_surface->surface->events.unmap, &view->unmap);
		view->commit.notify = handle_view_commit;
		wl_signal_add(&xwayland_surface->surface->events.commit, &view->commit);
	}

	wl_list_insert(&server->views, &view->link);
}

static void handle_decoration_request_mode(
		struct wl_listener *listener,
		void *data) {
	struct orange_decoration *decoration =
		wl_container_of(listener, decoration, request_mode);
	(void)data;
	if (!decoration->decoration->toplevel->base->initialized) {
		return;
	}
	wlr_xdg_toplevel_decoration_v1_set_mode(decoration->decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void handle_decoration_map(struct wl_listener *listener, void *data) {
	struct orange_decoration *decoration =
		wl_container_of(listener, decoration, map);
	(void)data;
	orange_listener_remove(&decoration->map);
	wlr_xdg_toplevel_decoration_v1_set_mode(decoration->decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void handle_decoration_destroy(struct wl_listener *listener, void *data) {
	struct orange_decoration *decoration =
		wl_container_of(listener, decoration, destroy);
	(void)data;
	orange_listener_remove(&decoration->request_mode);
	orange_listener_remove(&decoration->destroy);
	orange_listener_remove(&decoration->map);
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
	decoration->map.notify = handle_decoration_map;
	wl_signal_add(&wlr_decoration->toplevel->base->surface->events.map,
		&decoration->map);
}

static void handle_output_frame(struct wl_listener *listener, void *data) {
	struct orange_output *output = wl_container_of(listener, output, frame);
	(void)data;

	if (output->commit_pending || output->wlr_output->frame_pending) {
		return;
	}
	if (output->commit_failed) {
		return;
	}
	if (!output->wlr_output->needs_frame && !output->server->options->once) {
		return;
	}
	output_redraw_shell(output);
	output_redraw_overlay(output);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!wlr_scene_output_commit(output->scene_output, NULL)) {
		if (!output->commit_failed) {
			wlr_log(WLR_ERROR, "%s",
				"failed to commit scene output (rate-limited)");
			output->commit_failed = true;
		}
		return;
	}
	output->commit_failed = false;
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
	output->commit_failed = false;
	struct orange_server *server = output->server;
	if (server->dock_bounce_active) {
		uint32_t now = monotonic_time_msec();
		uint32_t elapsed = now - server->dock_bounce_start;
		if (elapsed >= ORANGE_DOCK_BOUNCE_DURATION_MS) {
			server->dock_bounce_active = false;
		} else {
			int width = 0;
			int height = 0;
			output_effective_size(output, &width, &height);
			struct orange_shell_layout layout;
			compute_shell_layout_for_output(server, output, &layout);
			struct orange_rect bounds =
				orange_dock_visual_dirty_bounds(&layout, width, height);
			if (output_rect_has_area(&bounds)) {
				output_mark_dock_visual_dirty(output, &layout);
			} else {
				output->shell_dirty = true;
			}
		}
	}
	if (server->config.dock_auto_hide) {
		if (dock_auto_hide_update_animation(server, monotonic_time_msec())) {
			output->overlay_dirty = true;
		}
	}
	if (server->cursor_loading_active) {
		uint32_t now = monotonic_time_msec();
		uint32_t elapsed = now - server->cursor_loading_start_ms;
		if (elapsed >= CURSOR_LOADING_TIMEOUT_MS) {
			stop_cursor_loading(server);
		}
	}
	bool minimize_active = minimize_animation_active_on_output(server, output);
	if (minimize_active) {
		output->overlay_dirty = true;
	}
	if (output->shell_dirty || output->dock_dirty ||
			output->overlay_dirty ||
			server->dock_auto_hide_animating ||
			minimize_active ||
			output->wlr_output->needs_frame) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct orange_output *output =
		wl_container_of(listener, output, destroy);
	(void)data;
	struct orange_server *server = output->server;
	if (server->minimize_animation.output == output) {
		minimize_animation_cancel_internal(server, true, false);
	}
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->present.link);
	wl_list_remove(&output->destroy.link);
	if (output->shell_scene_buffer != NULL) {
		wlr_scene_node_destroy(&output->shell_scene_buffer->node);
	}
	if (output->overlay_scene_buffer != NULL) {
		wlr_scene_node_destroy(&output->overlay_scene_buffer->node);
	}
	if (output->shell_buffer != NULL) {
		wlr_buffer_drop(&output->shell_buffer->base);
	}
	if (output->overlay_buffer != NULL) {
		wlr_buffer_drop(&output->overlay_buffer->base);
	}
	if (output->backdrop_buffer != NULL) {
		wlr_buffer_drop(&output->backdrop_buffer->base);
	}
	if (output->scene_output != NULL) {
		wlr_scene_output_destroy(output->scene_output);
	}
	free(output);
	orange_session_services_emit_monitors_changed(
		&server->session_services);
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
		wlr_log(WLR_ERROR, "%s",
			"failed to commit initial output state");
	}
	return ok;
}

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct orange_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
		wlr_log(WLR_ERROR, "%s",
			"failed to initialize output renderer");
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
	output->current_workspace = 0;
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
	orange_session_services_emit_monitors_changed(
		&server->session_services);

	if (output_ensure_shell_buffer(output)) {
		wlr_output_schedule_frame(wlr_output);
	}

	server_reload_cursor_scales(server);

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

static int handle_glib_timer(void *data) {
	struct orange_server *server = data;
	while (g_main_context_pending(NULL)) {
		g_main_context_iteration(NULL, false);
	}
	if (server != NULL) {
		server_update_workspace_animation(server);
	}
	if (server != NULL && server->glib_timer != NULL) {
		wl_event_source_timer_update(server->glib_timer, 10);
	}
	return 0;
}

static int handle_clock_timer(void *data) {
	struct orange_server *server = data;
	while (g_main_context_pending(NULL)) {
		g_main_context_iteration(NULL, false);
	}
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
			update_volumes(server, true);
			need_redraw = volumes_changed_since(
				server, prev_vc, prev_dc, prev);
			server->volume_poll_ticks = VOLUME_RELOAD_TICKS;
		} else {
			server->volume_poll_ticks--;
		}
		if (server->desktop_file_poll_ticks <= 0) {
			update_desktop_files(server);
			need_redraw = true;
			server->desktop_file_poll_ticks = DESKTOP_FILE_RELOAD_TICKS;
		} else {
			server->desktop_file_poll_ticks--;
		}
		bool status_changed = false;
		bool trash_changed = server_poll_trash_state(server);
		if (server->status_poll_ticks <= 0) {
			status_changed = server_update_status(server);
			server->status_poll_ticks = 15;
		} else {
			server->status_poll_ticks--;
		}
		if (server->background_settings != NULL) {
			if (server->background_poll_ticks <= 0) {
				need_redraw = server_refresh_background_settings(server) ||
					need_redraw;
				server->background_poll_ticks = 3;
			} else {
				server->background_poll_ticks--;
			}
		}
		if (server->gnome_interface_settings != NULL) {
			if (server->interface_poll_ticks <= 0) {
				need_redraw =
					server_sync_appearance_from_gnome_interface(server) ||
					need_redraw;
				server->interface_poll_ticks = 1;
			} else {
				server->interface_poll_ticks--;
			}
		}
		if (server->gnome_interface_settings != NULL ||
				server->gnome_mutter_settings != NULL ||
				server->gnome_wm_settings != NULL ||
				server->gnome_app_switcher_settings != NULL ||
				server->gnome_window_switcher_settings != NULL) {
			if (server->multitasking_poll_ticks <= 0) {
				need_redraw =
					server_sync_multitasking_settings(server) ||
					need_redraw;
				server->multitasking_poll_ticks = 1;
			} else {
				server->multitasking_poll_ticks--;
			}
		}
		if (need_redraw || status_changed) {
			server_mark_shell_dirty(server);
		} else if (trash_changed) {
			server_mark_dock_visual_dirty(server);
		}
	} else {
		bool status_changed = false;
		bool trash_changed = server_poll_trash_state(server);
		if (server->status_poll_ticks <= 0) {
			status_changed = server_update_status(server);
			server->status_poll_ticks = 15;
		} else {
			server->status_poll_ticks--;
		}
		if (server->background_settings != NULL) {
			if (server->background_poll_ticks <= 0) {
				server_refresh_background_settings(server);
				server->background_poll_ticks = 3;
			} else {
				server->background_poll_ticks--;
			}
		}
		if (server->gnome_interface_settings != NULL) {
			if (server->interface_poll_ticks <= 0) {
				server_sync_appearance_from_gnome_interface(server);
				server->interface_poll_ticks = 1;
			} else {
				server->interface_poll_ticks--;
			}
		}
		if (server->gnome_interface_settings != NULL ||
				server->gnome_mutter_settings != NULL ||
				server->gnome_wm_settings != NULL ||
				server->gnome_app_switcher_settings != NULL ||
				server->gnome_window_switcher_settings != NULL) {
			if (server->multitasking_poll_ticks <= 0) {
				server_sync_multitasking_settings(server);
				server->multitasking_poll_ticks = 1;
			} else {
				server->multitasking_poll_ticks--;
			}
		}
		if (status_changed) {
			server_mark_shell_dirty(server);
		} else if (trash_changed) {
			server_mark_dock_visual_dirty(server);
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

static void ensure_home_child_dir(const char *home, const char *name) {
	if (home == NULL || home[0] == '\0' ||
			name == NULL || name[0] == '\0') {
		return;
	}
	char path[4096];
	int n = snprintf(path, sizeof(path), "%s/%s", home, name);
	if (n > 0 && n < (int)sizeof(path)) {
		mkdir(path, 0755);
	}
}

static void ensure_xdg_user_dirs_config(
		const char *config_dir,
		const char *home) {
	if (config_dir == NULL || config_dir[0] == '\0' ||
			home == NULL || home[0] == '\0') {
		return;
	}
	mkdir(config_dir, 0755);
	ensure_home_child_dir(home, "Desktop");
	ensure_home_child_dir(home, "Documents");
	ensure_home_child_dir(home, "Downloads");
	ensure_home_child_dir(home, "Music");
	ensure_home_child_dir(home, "Pictures");
	ensure_home_child_dir(home, "Public");
	ensure_home_child_dir(home, "Templates");
	ensure_home_child_dir(home, "Videos");

	char path[4096];
	int n = snprintf(path, sizeof(path), "%s/user-dirs.dirs", config_dir);
	if (n <= 0 || n >= (int)sizeof(path) || access(path, F_OK) == 0) {
		return;
	}
	FILE *f = fopen(path, "w");
	if (f == NULL) {
		return;
	}
	fprintf(f,
		"XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n"
		"XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n"
		"XDG_TEMPLATES_DIR=\"$HOME/Templates\"\n"
		"XDG_PUBLICSHARE_DIR=\"$HOME/Public\"\n"
		"XDG_DOCUMENTS_DIR=\"$HOME/Documents\"\n"
		"XDG_MUSIC_DIR=\"$HOME/Music\"\n"
		"XDG_PICTURES_DIR=\"$HOME/Pictures\"\n"
		"XDG_VIDEOS_DIR=\"$HOME/Videos\"\n");
	fclose(f);
}

static void ensure_gtk_bookmarks_file(const char *home) {
	if (home == NULL || home[0] == '\0') {
		return;
	}
	char path[4096];
	int n = snprintf(path, sizeof(path), "%s/.gtk-bookmarks", home);
	if (n <= 0 || n >= (int)sizeof(path) || access(path, F_OK) == 0) {
		return;
	}
	FILE *f = fopen(path, "w");
	if (f != NULL) {
		fclose(f);
	}
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
	const char *parent_wayland_display = getenv("WAYLAND_DISPLAY");
	const char *parent_x11_display = getenv("DISPLAY");
	bool inherited_graphical_session =
		(parent_wayland_display != NULL && parent_wayland_display[0] != '\0') ||
		(parent_x11_display != NULL && parent_x11_display[0] != '\0');
	server->options = options;
	server->hot_dock_index = -1;
	server->last_dock_pointer_x = INT_MIN;
	server->last_dock_pointer_y = INT_MIN;
	server->dock_drag_index = -1;
	server->dock_resize_position = ORANGE_DOCK_POSITION_BOTTOM;
	server->dock_state_dirty = true;
	server->status_poll_ticks = 0;
	server->desktop_entry_poll_ticks = 0;
	server->volume_poll_ticks = 0;
	server->desktop_file_poll_ticks = 0;
	server->trash_poll_ticks = 0;
	server->interface_poll_ticks = 0;
	server->multitasking_poll_ticks = 0;
	server->background_poll_ticks = 0;
	server->hot_corners_enabled = true;
	server->hot_corner_armed = true;
	server->edge_tiling_enabled = true;
	server->animations_enabled = true;
	server->dynamic_workspaces_enabled = true;
	server->workspaces_only_on_primary = false;
	server->app_switcher_current_workspace_only = false;
	server->window_switcher_current_workspace_only = false;
	server->workspace_count = 4;
	server->current_workspace = 0;
	server->context_menu_kind = ORANGE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;
	server->open_with_file_index = -1;
	server->open_with_app_count = 0;
	server->open_with_submenu_open = false;
	server->dock_options_submenu_open = false;
	server->dock_minimize_submenu_open = false;
	server->dock_position_submenu_open = false;
	server->context_menu_alt_pressed = false;
	server->context_menu_dock_temporary = false;
	server->desktop_drag_index = -1;
	server->desktop_last_click_index = -1;
	orange_desktop_rename_reset(&server->desktop_rename);
	server->cursor_loading_launcher_idx = -1;
	server->volume_count = 0;
	server->desktop_volume_count = 0;
	server->desktop_file_count = 0;
	server->volume_timer = NULL;
	server->desktop_file_timer = NULL;
	server->cursor_mode = ORANGE_CURSOR_PASSTHROUGH;

	orange_session_services_init(&server->session_services);
	orange_session_services_set_status_notifier_changed_handler(
		&server->session_services, server_status_notifier_changed, server);
	wl_list_init(&server->outputs);
	wl_list_init(&server->views);
	wl_list_init(&server->keyboards);
	server_init_listener_links(server);
	orange_config_set_defaults(&server->config);
	server_load_config(server, false);
	orange_session_services_setup(&server->session_services,
		server->config.appearance,
		server_display_config_current_state,
		server_display_config_apply,
		server);
	status_set_defaults(&server->status);
	server_update_status(server);
	server_reload_desktop_entries(server);
	update_volumes(server, false);
	update_desktop_files(server);
	server_update_trash_state(server);
	orange_assets_init(&server->assets);
	orange_assets_load(&server->assets, server->config.icon_theme);
	server_setup_gnome_interface_settings(server);
	server_setup_multitasking_settings(server);
	server_setup_background_settings(server);
	orange_menubar_warm_assets(&server->assets);
	server_preload_app_icons(server);

	server->display = wl_display_create();
	if (server->display == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to create Wayland display");
		return false;
	}
	struct wl_event_loop *event_loop =
		wl_display_get_event_loop(server->display);

	if (options->headless) {
		server->backend = wlr_headless_backend_create(event_loop);
	} else {
		server->backend = wlr_backend_autocreate(event_loop, NULL);
	}
	if (server->backend == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to create backend");
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
		wlr_log(WLR_ERROR, "%s",
			"failed to create renderer - no renderer is available");
		return false;
	}
	if (!wlr_renderer_init_wl_display(server->renderer, server->display)) {
		wlr_log(WLR_ERROR, "%s",
			"failed to initialize renderer protocols");
		return false;
	}

#ifdef ORANGE_HAVE_VULKAN
	if (wlr_renderer_is_vk(server->renderer)) {
		wlr_log(WLR_INFO, "%s", "wlroots selected the Vulkan renderer");
	} else {
		wlr_log(WLR_INFO, "%s",
			"wlroots selected a non-Vulkan renderer");
	}
#else
	wlr_log(WLR_INFO, "%s",
		"wlroots selected a non-Vulkan renderer (Vulkan not available at build time)");
#endif

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (server->allocator == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to create allocator");
		return false;
	}

	server->wlr_compositor = wlr_compositor_create(server->display, 5, server->renderer);
	wlr_subcompositor_create(server->display);
	server->viewporter = wlr_viewporter_create(server->display);
	if (server->viewporter == NULL) {
		wlr_log(WLR_ERROR, "%s", "failed to create wp_viewporter global");
		return false;
	}
	server->fractional_scale_manager =
		wlr_fractional_scale_manager_v1_create(server->display, 1);
	if (server->fractional_scale_manager == NULL) {
		wlr_log(WLR_ERROR, "%s",
			"failed to create wp_fractional_scale_v1 global");
		return false;
	}
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
	server->new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&server->xdg_shell->events.new_toplevel,
		&server->new_xdg_toplevel);
	server->new_xdg_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&server->xdg_shell->events.new_popup,
		&server->new_xdg_popup);

	server->xwayland = wlr_xwayland_create(server->display, server->wlr_compositor, false);
	if (server->xwayland != NULL) {
		server->new_xwayland_surface.notify = handle_new_xwayland_surface;
		wl_signal_add(&server->xwayland->events.new_surface,
			&server->new_xwayland_surface);
		server->xwayland_ready.notify = handle_xwayland_ready;
		wl_signal_add(&server->xwayland->events.ready,
			&server->xwayland_ready);
		wlr_log(WLR_INFO, "xwayland created%s", "");
	} else {
		wlr_log(WLR_ERROR, "%s", "failed to create xwayland");
	}

	server->scene = wlr_scene_create();
	server->shell_tree = wlr_scene_tree_create(&server->scene->tree);
	server->window_tree = wlr_scene_tree_create(&server->scene->tree);
	server->overlay_tree = wlr_scene_tree_create(&server->scene->tree);
	server->output_layout = wlr_output_layout_create(server->display);
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
	server->glib_timer = wl_event_loop_add_timer(loop, handle_glib_timer, server);
	wl_event_source_timer_update(server->glib_timer, 10);

	const char *socket = NULL;
	if (!options->once) {
		if (!ensure_runtime_dir()) {
			return false;
		}

		socket = wl_display_add_socket_auto(server->display);
		if (socket == NULL) {
			wlr_log(WLR_ERROR, "%s",
				"failed to create Wayland socket");
			return false;
		}
		setenv("WAYLAND_DISPLAY", socket, true);
		snprintf(orange_client_wayland_display,
			sizeof(orange_client_wayland_display), "%s", socket);
		const char *private_dbus = getenv("ORANGE_CLIENT_PRIVATE_DBUS");
		if (private_dbus != NULL && private_dbus[0] != '\0') {
			orange_client_launch_private_dbus =
				strcmp(private_dbus, "0") != 0 &&
				strcasecmp(private_dbus, "false") != 0 &&
				strcasecmp(private_dbus, "no") != 0;
		} else {
			orange_client_launch_private_dbus =
				inherited_graphical_session;
		}
		wlr_log(WLR_INFO,
			"client launches target WAYLAND_DISPLAY=%s%s",
			socket,
			orange_client_launch_private_dbus ?
			" with private DBus sessions" : "");
		server_export_client_environment();
	} else {
		wlr_log(WLR_INFO, "%s",
			"one-shot mode skips Wayland client socket creation");
	}

	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "%s", "failed to start backend");
		return false;
	}

	if (options->headless) {
		int count = options->num_outputs;
		if (count < 1) {
			count = 1;
		}
		for (int i = 0; i < count; i++) {
			wlr_headless_add_output(server->backend,
				(unsigned int)options->width,
				(unsigned int)options->height);
		}
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
	if (server->xwayland != NULL) {
		wlr_xwayland_destroy(server->xwayland);
		server->xwayland = NULL;
	}
	if (server->display != NULL) {
		wl_display_destroy_clients(server->display);
	}
	server_remove_wlroots_listeners(server);
	if (server->clock_timer != NULL) {
		wl_event_source_remove(server->clock_timer);
		server->clock_timer = NULL;
	}
	if (server->glib_timer != NULL) {
		wl_event_source_remove(server->glib_timer);
		server->glib_timer = NULL;
	}
	orange_session_services_finish(&server->session_services);

	if (server->background_settings != NULL) {
		g_object_unref(server->background_settings);
		server->background_settings = NULL;
	}
	if (server->gnome_interface_settings != NULL) {
		g_object_unref(server->gnome_interface_settings);
		server->gnome_interface_settings = NULL;
	}
	if (server->gnome_mutter_settings != NULL) {
		g_object_unref(server->gnome_mutter_settings);
		server->gnome_mutter_settings = NULL;
	}
	if (server->gnome_wm_settings != NULL) {
		g_object_unref(server->gnome_wm_settings);
		server->gnome_wm_settings = NULL;
	}
	if (server->gnome_app_switcher_settings != NULL) {
		g_object_unref(server->gnome_app_switcher_settings);
		server->gnome_app_switcher_settings = NULL;
	}
	if (server->gnome_window_switcher_settings != NULL) {
		g_object_unref(server->gnome_window_switcher_settings);
		server->gnome_window_switcher_settings = NULL;
	}
	orange_assets_finish(&server->assets);
	if (server->backend != NULL) {
		wlr_backend_destroy(server->backend);
		server->backend = NULL;
	}
	if (server->scene != NULL) {
		wlr_scene_node_destroy(&server->scene->tree.node);
		server->scene = NULL;
		server->shell_tree = NULL;
		server->window_tree = NULL;
		server->overlay_tree = NULL;
		server->scene_layout = NULL;
	}
	if (server->cursor != NULL) {
		wlr_cursor_destroy(server->cursor);
		server->cursor = NULL;
	}
	if (server->xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	if (server->seat != NULL) {
		wlr_seat_destroy(server->seat);
		server->seat = NULL;
	}
	if (server->output_layout != NULL) {
		wlr_output_layout_destroy(server->output_layout);
		server->output_layout = NULL;
	}
	if (server->allocator != NULL) {
		wlr_allocator_destroy(server->allocator);
		server->allocator = NULL;
	}
	if (server->renderer != NULL) {
		wlr_renderer_destroy(server->renderer);
		server->renderer = NULL;
	}
	if (server->display != NULL) {
		wl_display_destroy(server->display);
		server->display = NULL;
	}
}

static void orange_log_handler(enum wlr_log_importance importance,
		const char *fmt, va_list args) {
	(void)importance;
	if (strstr(fmt, "No free output buffer slot") != NULL ||
			strstr(fmt, "Failed to render cursor buffer") != NULL ||
			strstr(fmt, "Falling back to software cursor") != NULL ||
			strstr(fmt, "Unsupported buffer format") != NULL) {
		return;
	}
	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, args);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(stderr, "%02ld:%02ld:%02ld.%03ld %s\n",
		(ts.tv_sec / 3600) % 24, (ts.tv_sec / 60) % 60,
		ts.tv_sec % 60, ts.tv_nsec / 1000000, buf);
}

int orange_compositor_run(const struct orange_options *options) {
	struct sigaction sigchld = {
		.sa_handler = SIG_IGN,
		.sa_flags = SA_NOCLDWAIT,
	};
	sigemptyset(&sigchld.sa_mask);
	sigaction(SIGCHLD, &sigchld, NULL);

	wlr_log_init(WLR_INFO, orange_log_handler);

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
	ensure_xdg_user_dirs_config(config_dir, home);
	ensure_gtk_bookmarks_file(home);
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