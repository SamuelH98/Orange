#include "tahoe/buffer.h"
#include "tahoe/compositor.h"
#include "tahoe/config.h"
#include "tahoe/desktop_entry.h"
#include "tahoe/shell.h"

#include <assert.h>
#include <linux/input-event-codes.h>
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
#ifdef TAHOE_HAVE_VULKAN
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

enum tahoe_cursor_mode {
	TAHOE_CURSOR_PASSTHROUGH,
	TAHOE_CURSOR_MOVE,
	TAHOE_CURSOR_RESIZE,
};

struct tahoe_server;

struct tahoe_output {
	struct wl_list link;
	struct tahoe_server *server;
	struct wlr_output *wlr_output;
	struct wlr_output_layout_output *layout_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_buffer *shell_scene_buffer;
	struct tahoe_buffer *shell_buffer;
	struct wl_listener frame;
	struct wl_listener destroy;
	int width;
	int height;
	bool shell_dirty;
};

struct tahoe_view {
	struct wl_list link;
	struct tahoe_server *server;
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

struct tahoe_keyboard {
	struct wl_list link;
	struct tahoe_server *server;
	struct wlr_keyboard *keyboard;
	struct wl_listener key;
	struct wl_listener modifiers;
	struct wl_listener destroy;
};

struct tahoe_decoration {
	struct wl_listener request_mode;
	struct wl_listener destroy;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
};

struct tahoe_server {
	const struct tahoe_options *options;
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
	struct tahoe_assets assets;
	struct tahoe_config config;
	struct tahoe_desktop_entry desktop_entries[TAHOE_DESKTOP_MAX];
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

	struct tahoe_view *focused_view;
	enum tahoe_cursor_mode cursor_mode;
	struct tahoe_view *grabbed_view;
	double grab_cursor_x;
	double grab_cursor_y;
	int grab_view_x;
	int grab_view_y;
	int grab_view_width;
	int grab_view_height;
	uint32_t resize_edges;

	bool apple_menu_open;
	int hot_dock_index;
	bool dock_open[TAHOE_DOCK_MAX];
	enum tahoe_context_menu_kind context_menu_kind;
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

static void server_mark_shell_dirty(struct tahoe_server *server);
static void server_apply_cursor_config(struct tahoe_server *server);
static void process_desktop_drag(struct tahoe_server *server);
static void start_dock_drag(struct tahoe_server *server,
	const struct tahoe_shell_layout *layout, int index);
static void process_dock_drag(struct tahoe_server *server);
static void finish_dock_drag(struct tahoe_server *server);

static void server_apply_theme_env(struct tahoe_server *server) {
	const char *theme = server->config.appearance == TAHOE_APPEARANCE_DARK ?
		"TahoeGTK-dark" : "TahoeGTK";
	setenv("GTK_THEME", theme, true);
	setenv("GTK_ICON_THEME", "TahoeIcons", true);
	setenv("GTK_CSD", "1", true);
	if (server->options->theme_root != NULL) {
		const char *old_dirs = getenv("XDG_DATA_DIRS");
		char dirs[4096];
		if (old_dirs != NULL && old_dirs[0] != '\0') {
			snprintf(dirs, sizeof(dirs), "%s:%s",
				server->options->theme_root, old_dirs);
		} else {
			snprintf(dirs, sizeof(dirs), "%s:/usr/local/share:/usr/share",
				server->options->theme_root);
		}
		setenv("XDG_DATA_DIRS", dirs, true);
	}
}

static void server_apply_cursor_config(struct tahoe_server *server) {
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

static void server_load_config(struct tahoe_server *server, bool force_dirty) {
	struct tahoe_config next;
	tahoe_config_load(&next, server->options->config_path);
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
	const char *terminal = getenv("TAHOE_TERMINAL");
	if (terminal != NULL && terminal[0] != '\0') {
		launch_command(terminal);
		return;
	}
	launch_command("foot || alacritty || kitty || weston-terminal || xterm");
}

static void launch_app_picker(void) {
	const char *picker = getenv("TAHOE_APP_PICKER");
	if (picker != NULL && picker[0] != '\0') {
		launch_command(picker);
		return;
	}
	launch_command("wofi --show drun || rofi -show drun || true");
}

static bool contains_case(const char *haystack, const char *needle) {
	return haystack != NULL && needle != NULL && strcasestr(haystack, needle) != NULL;
}

static int dock_index_for_view(struct tahoe_view *view) {
	if (view == NULL || view->xdg_surface == NULL ||
			view->xdg_surface->toplevel == NULL) {
		return -1;
	}
	const char *app_id = view->xdg_surface->toplevel->app_id;
	const char *title = view->xdg_surface->toplevel->title;
	if (contains_case(app_id, "finder") || contains_case(app_id, "files") ||
			contains_case(app_id, "nautilus") || contains_case(title, "files")) {
		return 0;
	}
	if (contains_case(app_id, "safari") || contains_case(app_id, "browser") ||
			contains_case(app_id, "firefox") || contains_case(app_id, "chrom") ||
			contains_case(title, "browser")) {
		return 2;
	}
	if (contains_case(app_id, "mail") || contains_case(title, "mail")) {
		return 4;
	}
	if (contains_case(app_id, "map") || contains_case(title, "map")) {
		return 5;
	}
	if (contains_case(app_id, "photo") || contains_case(title, "photo")) {
		return 6;
	}
	if (contains_case(app_id, "calendar") || contains_case(title, "calendar")) {
		return 9;
	}
	if (contains_case(app_id, "contact") || contains_case(title, "contact")) {
		return 10;
	}
	if (contains_case(app_id, "todo") || contains_case(app_id, "endeavour") ||
			contains_case(app_id, "reminder") || contains_case(title, "reminder")) {
		return 11;
	}
	if (contains_case(app_id, "text") || contains_case(app_id, "gedit") ||
			contains_case(app_id, "mousepad") || contains_case(title, "notes")) {
		return 12;
	}
	if (contains_case(app_id, "music") || contains_case(title, "music")) {
		return 14;
	}
	if (contains_case(app_id, "software") || contains_case(app_id, "discover") ||
			contains_case(title, "software")) {
		return 16;
	}
	if (contains_case(app_id, "calculator") || contains_case(app_id, "kcalc") ||
			contains_case(title, "calculator")) {
		return 17;
	}
	if (contains_case(app_id, "settings") || contains_case(app_id, "control-center") ||
			contains_case(title, "settings")) {
		return 18;
	}
	return -1;
}

static void server_update_dock_open(struct tahoe_server *server) {
	memset(server->dock_open, 0, sizeof(server->dock_open));
	struct tahoe_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		int index = dock_index_for_view(view);
		if (index >= 0 && index < TAHOE_DOCK_MAX) {
			server->dock_open[index] = true;
		}
	}
}

static struct tahoe_output *output_from_wlr_output(
		struct tahoe_server *server,
		struct wlr_output *wlr_output) {
	struct tahoe_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			return output;
		}
	}
	return NULL;
}

static struct tahoe_output *output_at_cursor(
		struct tahoe_server *server,
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
		struct tahoe_server *server,
		struct tahoe_output *output,
		struct tahoe_shell_layout *layout) {
	tahoe_shell_layout_compute(output->width, output->height,
		server->apple_menu_open,
		&server->config,
		(int)server->desktop_entry_count,
		layout);
	tahoe_shell_layout_set_context_menu(layout,
		server->context_menu_kind,
		server->context_menu_index,
		server->context_menu_cursor_x,
		server->context_menu_cursor_y);
}

static void server_mark_shell_dirty(struct tahoe_server *server) {
	struct tahoe_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output->shell_dirty = true;
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static bool output_ensure_shell_buffer(struct tahoe_output *output) {
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

	output->shell_buffer = tahoe_buffer_create(width, height);
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

static void output_redraw_shell(struct tahoe_output *output) {
	if (!output_ensure_shell_buffer(output)) {
		return;
	}
	if (!output->shell_dirty) {
		return;
	}

	server_update_dock_open(output->server);
	struct tahoe_shell_state state = {
		.apple_menu_open = output->server->apple_menu_open,
		.hot_dock_index = output->server->hot_dock_index,
		.dock_drag_index = output->server->dock_drag_active ?
			output->server->dock_drag_index : -1,
		.dock_drag_insert_before = output->server->dock_drag_active ?
			output->server->dock_drag_insert_before : -1,
		.dock_drag_x = output->server->dock_drag_active ?
			(int)output->server->cursor->x : 0,
		.dock_drag_y = output->server->dock_drag_active ?
			(int)output->server->cursor->y : 0,
		.now = time(NULL),
		.assets = &output->server->assets,
		.config = &output->server->config,
		.desktop_entries = output->server->desktop_entries,
		.desktop_entry_count = (int)output->server->desktop_entry_count,
		.context_menu_kind = output->server->context_menu_kind,
		.context_menu_index = output->server->context_menu_index,
		.context_menu_cursor_x = output->server->context_menu_cursor_x,
		.context_menu_cursor_y = output->server->context_menu_cursor_y,
	};
	memcpy(state.dock_open, output->server->dock_open, sizeof(state.dock_open));
	tahoe_shell_draw(output->shell_buffer->pixels,
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

static void focus_view(struct tahoe_view *view, struct wlr_surface *surface) {
	if (view == NULL || surface == NULL || !view->mapped) {
		return;
	}

	struct tahoe_server *server = view->server;
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
		struct tahoe_server *server,
		double lx,
		double ly,
		double *sx,
		double *sy,
		struct tahoe_view **view) {
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

static void process_cursor_motion(struct tahoe_server *server, uint32_t time_msec) {
	if (server->cursor_mode == TAHOE_CURSOR_MOVE && server->grabbed_view != NULL) {
		struct tahoe_view *view = server->grabbed_view;
		view->x = (int)(server->cursor->x - server->grab_cursor_x);
		view->y = (int)(server->cursor->y - server->grab_cursor_y);
		wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
		return;
	}

	if (server->cursor_mode == TAHOE_CURSOR_RESIZE && server->grabbed_view != NULL) {
		struct tahoe_view *view = server->grabbed_view;
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
	struct tahoe_view *view = NULL;
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
	struct tahoe_output *output = output_at_cursor(server, &local_x, &local_y);
	int new_hot = -1;
	if (output != NULL) {
		struct tahoe_shell_layout layout;
		compute_shell_layout_for_output(server, output, &layout);
		struct tahoe_shell_hit hit =
			tahoe_shell_hit_test(&layout, local_x, local_y);
		if (hit.kind == TAHOE_HIT_DOCK_ITEM) {
			new_hot = hit.index;
		}
	}
	if (new_hot != server->hot_dock_index) {
		server->hot_dock_index = new_hot;
		server_mark_shell_dirty(server);
	}
}

static void begin_interactive(
		struct tahoe_view *view,
		enum tahoe_cursor_mode mode,
		uint32_t edges) {
	struct tahoe_server *server = view->server;
	server->cursor_mode = mode;
	server->grabbed_view = view;
	server->grab_cursor_x = server->cursor->x;
	server->grab_cursor_y = server->cursor->y;
	if (mode == TAHOE_CURSOR_MOVE) {
		server->grab_cursor_x = server->cursor->x - view->x;
		server->grab_cursor_y = server->cursor->y - view->y;
	}
	server->grab_view_x = view->x;
	server->grab_view_y = view->y;
	server->grab_view_width = view->width;
	server->grab_view_height = view->height;
	server->resize_edges = edges;
}

static void close_focused_view(struct tahoe_server *server) {
	if (server->focused_view != NULL && server->focused_view->mapped) {
		wlr_xdg_toplevel_send_close(server->focused_view->xdg_surface->toplevel);
	}
}

static void cycle_focus(struct tahoe_server *server) {
	if (wl_list_empty(&server->views)) {
		return;
	}

	struct tahoe_view *first_mapped = NULL;
	struct tahoe_view *candidate = NULL;
	bool seen_focused = server->focused_view == NULL;

	struct tahoe_view *view;
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

static void apply_maximize(struct tahoe_view *view, bool maximized) {
	struct tahoe_server *server = view->server;
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

static void apply_fullscreen(struct tahoe_view *view, bool fullscreen) {
	struct tahoe_server *server = view->server;
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

static void handle_shell_menu_action(struct tahoe_server *server, int index) {
	switch (index) {
	case 1:
		launch_command("build/tahoe-settings tahoe.conf || true");
		break;
	case 2:
		launch_app_picker();
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
	server->apple_menu_open = false;
	server_mark_shell_dirty(server);
}

static void clear_context_menu(struct tahoe_server *server) {
	if (server->context_menu_kind != TAHOE_CONTEXT_MENU_NONE) {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
		server->context_menu_cursor_x = 0;
		server->context_menu_cursor_y = 0;
		server_mark_shell_dirty(server);
	}
}

static void set_widget_size(
		struct tahoe_server *server,
		int target,
		enum tahoe_widget_size size) {
	if (target == 0) {
		server->config.calendar_widget_size = size;
	} else if (target == 1) {
		server->config.weather_widget_size = size;
	} else {
		return;
	}
	tahoe_config_save(&server->config, server->options->config_path);
}

static void remove_widget(struct tahoe_server *server, int target) {
	if (target == 0) {
		server->config.calendar_widget_visible = false;
	} else if (target == 1) {
		server->config.weather_widget_visible = false;
	} else {
		return;
	}
	tahoe_config_save(&server->config, server->options->config_path);
}

static void handle_context_menu_action(struct tahoe_server *server, int item_index) {
	enum tahoe_context_menu_kind kind = server->context_menu_kind;
	int target = server->context_menu_index;
	server->context_menu_kind = TAHOE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;

	if (kind == TAHOE_CONTEXT_MENU_DOCK) {
		int launcher_idx = target >= 0 && target < TAHOE_DOCK_MAX ?
			server->config.dock_order[target] : target;
		switch (item_index) {
		case 0:
			launch_command(tahoe_shell_dock_command(launcher_idx));
			break;
		case 1:
			launch_command("xdg-open \"$HOME\" || true");
			break;
		case 4:
			launch_command("build/tahoe-settings tahoe.conf || true");
			break;
		default:
			break;
		}
	} else if (kind == TAHOE_CONTEXT_MENU_WIDGET) {
		switch (item_index) {
		case 0:
			launch_command("build/tahoe-settings tahoe.conf || true");
			break;
		case 1:
			set_widget_size(server, target, TAHOE_WIDGET_SIZE_SMALL);
			break;
		case 2:
			set_widget_size(server, target, TAHOE_WIDGET_SIZE_MEDIUM);
			break;
		case 3:
			set_widget_size(server, target, TAHOE_WIDGET_SIZE_LARGE);
			break;
		case 4:
			remove_widget(server, target);
			break;
		default:
			break;
		}
	} else if (kind == TAHOE_CONTEXT_MENU_DESKTOP_ICON) {
		switch (item_index) {
		case 0:
			if (target >= 0 && target < (int)server->desktop_entry_count) {
				launch_command(server->desktop_entries[target].exec);
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
	} else if (kind == TAHOE_CONTEXT_MENU_DESKTOP) {
		switch (item_index) {
		case 0:
			launch_command("xdg-open \"$HOME/Desktop\" || true");
			break;
		case 6:
			launch_command("build/tahoe-settings tahoe.conf || true");
			break;
		case 7:
			launch_command("build/tahoe-settings tahoe.conf || true");
			break;
		default:
			break;
		}
	}
	server_mark_shell_dirty(server);
}

static void start_desktop_drag(struct tahoe_server *server,
		struct tahoe_output *output,
		const struct tahoe_shell_layout *layout,
		int index,
		int local_x,
		int local_y) {
	if (index < 0 || index >= layout->desktop_item_count ||
			index >= TAHOE_DESKTOP_POSITION_MAX) {
		return;
	}
	struct tahoe_rect item = layout->desktop_items[index];
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

static void process_desktop_drag(struct tahoe_server *server) {
	if (!server->desktop_drag_active) {
		return;
	}

	int local_x = 0;
	int local_y = 0;
	struct tahoe_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		return;
	}

	struct tahoe_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	int index = server->desktop_drag_index;
	if (index < 0 || index >= layout.desktop_item_count ||
			index >= TAHOE_DESKTOP_POSITION_MAX) {
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

	struct tahoe_rect item = layout.desktop_items[index];
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
	if (next_y + item.height > output->height) {
		next_y = output->height - item.height;
	}
	server->config.desktop_positions[index].valid = true;
	server->config.desktop_positions[index].x = next_x;
	server->config.desktop_positions[index].y = next_y;
	server_mark_shell_dirty(server);
}

static void finish_desktop_drag(struct tahoe_server *server) {
	if (!server->desktop_drag_active) {
		return;
	}
	int index = server->desktop_drag_index;
	bool moved = server->desktop_drag_moved;
	server->desktop_drag_active = false;
	server->desktop_drag_moved = false;
	server->desktop_drag_index = -1;

	if (moved) {
		tahoe_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (index >= 0 && index < (int)server->desktop_entry_count) {
		launch_command(server->desktop_entries[index].exec);
	}
}

static void start_dock_drag(struct tahoe_server *server,
		const struct tahoe_shell_layout *layout, int index) {
	server->dock_drag_active = true;
	server->dock_drag_moved = false;
	server->dock_drag_index = index;
	server->dock_drag_start_x = (int)server->cursor->x;
	server->dock_drag_insert_before = -1;
	(void)layout;
}

static void process_dock_drag(struct tahoe_server *server) {
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
	struct tahoe_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		return;
	}

	struct tahoe_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	int dragged = server->dock_drag_index;
	int insert = layout.dock_item_count;
	for (int i = 0; i < layout.dock_item_count; i++) {
		if (i == dragged) {
			continue;
		}
		struct tahoe_rect r = layout.dock_items[i];
		if (local_x < r.x + r.width / 2) {
			insert = i;
			break;
		}
	}
	if (insert > dragged && insert < layout.dock_item_count) {
		insert--;
	}
	if (insert != server->dock_drag_insert_before) {
		server->dock_drag_insert_before = insert;
		server_mark_shell_dirty(server);
	}
}

static void finish_dock_drag(struct tahoe_server *server) {
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

	if (moved && index >= 0 && index < TAHOE_DOCK_MAX &&
			insert >= 0 && insert < TAHOE_DOCK_MAX) {
		int order[TAHOE_DOCK_MAX];
		memcpy(order, server->config.dock_order, sizeof(order));
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
		memcpy(server->config.dock_order, order, sizeof(order));
		tahoe_config_save(&server->config, server->options->config_path);
		server_mark_shell_dirty(server);
	} else if (!moved && index >= 0 && index < TAHOE_DOCK_MAX) {
		int launcher_idx = server->config.dock_order[index];
		launch_command(tahoe_shell_dock_command(launcher_idx));
		server_mark_shell_dirty(server);
	}
}

static void handle_shell_click(struct tahoe_server *server, int x, int y) {
	int local_x = 0;
	int local_y = 0;
	struct tahoe_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		(void)x;
		(void)y;
		return;
	}

	struct tahoe_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(&layout, local_x, local_y);

	switch (hit.kind) {
	case TAHOE_HIT_CONTEXT_MENU_ITEM:
		handle_context_menu_action(server, hit.index);
		break;
	case TAHOE_HIT_APPLE_MENU:
		clear_context_menu(server);
		server->apple_menu_open = !server->apple_menu_open;
		server_mark_shell_dirty(server);
		break;
	case TAHOE_HIT_APPLE_MENU_ITEM:
		clear_context_menu(server);
		handle_shell_menu_action(server, hit.index);
		break;
	case TAHOE_HIT_DOCK_ITEM:
		clear_context_menu(server);
		server->apple_menu_open = false;
		start_dock_drag(server, &layout, hit.index);
		server_mark_shell_dirty(server);
		break;
	case TAHOE_HIT_WIDGET:
		clear_context_menu(server);
		server->apple_menu_open = false;
		server_mark_shell_dirty(server);
		break;
	case TAHOE_HIT_DESKTOP_ITEM:
		clear_context_menu(server);
		server->apple_menu_open = false;
		start_desktop_drag(server, output, &layout, hit.index, local_x, local_y);
		server_mark_shell_dirty(server);
		break;
	case TAHOE_HIT_DESKTOP:
	case TAHOE_HIT_NONE:
		clear_context_menu(server);
		if (server->apple_menu_open) {
			server->apple_menu_open = false;
			server_mark_shell_dirty(server);
		}
		break;
	}
}

static void handle_shell_right_click(struct tahoe_server *server, int x, int y) {
	int local_x = 0;
	int local_y = 0;
	struct tahoe_output *output = output_at_cursor(server, &local_x, &local_y);
	if (output == NULL) {
		(void)x;
		(void)y;
		return;
	}

	struct tahoe_shell_layout layout;
	compute_shell_layout_for_output(server, output, &layout);
	struct tahoe_shell_hit hit = tahoe_shell_hit_test(&layout, local_x, local_y);

	server->apple_menu_open = false;
	server->context_menu_cursor_x = local_x;
	server->context_menu_cursor_y = local_y;
	if (hit.kind == TAHOE_HIT_DOCK_ITEM) {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_DOCK;
		server->context_menu_index = hit.index;
	} else if (hit.kind == TAHOE_HIT_WIDGET) {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_WIDGET;
		server->context_menu_index = hit.index;
	} else if (hit.kind == TAHOE_HIT_DESKTOP_ITEM) {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_DESKTOP_ICON;
		server->context_menu_index = hit.index;
	} else if (hit.kind == TAHOE_HIT_DESKTOP) {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_DESKTOP;
		server->context_menu_index = -1;
	} else {
		server->context_menu_kind = TAHOE_CONTEXT_MENU_NONE;
		server->context_menu_index = -1;
	}
	server_mark_shell_dirty(server);
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor,
		&event->pointer->base,
		event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	if (event->state == WLR_BUTTON_RELEASED &&
			server->cursor_mode != TAHOE_CURSOR_PASSTHROUGH) {
		server->cursor_mode = TAHOE_CURSOR_PASSTHROUGH;
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
	struct tahoe_view *view = NULL;
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
	struct tahoe_server *server =
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
	struct tahoe_server *server =
		wl_container_of(listener, server, cursor_frame);
	(void)data;
	wlr_seat_pointer_notify_frame(server->seat);
}

static bool handle_keybinding(
		struct tahoe_server *server,
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
		if (server->apple_menu_open) {
			server->apple_menu_open = false;
			server_mark_shell_dirty(server);
			return true;
		}
		return false;
	default:
		return false;
	}
}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
	struct tahoe_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct tahoe_server *server = keyboard->server;
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
	struct tahoe_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	(void)data;
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->keyboard);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->keyboard->modifiers);
}

static void handle_keyboard_destroy(struct wl_listener *listener, void *data) {
	struct tahoe_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	(void)data;
	wl_list_remove(&keyboard->link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->destroy.link);
	free(keyboard);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
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

		struct tahoe_keyboard *keyboard = calloc(1, sizeof(*keyboard));
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
	struct tahoe_server *server =
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
	struct tahoe_server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void handle_request_set_primary_selection(
		struct wl_listener *listener,
		void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

static void handle_view_commit(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, commit);
	(void)data;
	if (!view->mapped) {
		return;
	}
	struct wlr_box geom;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geom);
	if (geom.width > 0 && geom.height > 0 &&
			view->server->cursor_mode != TAHOE_CURSOR_RESIZE) {
		view->width = geom.width;
		view->height = geom.height;
	}
}

static void handle_view_map(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, map);
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
	struct tahoe_view *view = wl_container_of(listener, view, unmap);
	(void)data;
	view->mapped = false;
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	if (view->server->focused_view == view) {
		view->server->focused_view = NULL;
		wlr_seat_keyboard_notify_clear_focus(view->server->seat);
	}
	if (view->server->grabbed_view == view) {
		view->server->cursor_mode = TAHOE_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}
	server_mark_shell_dirty(view->server);
}

static void handle_view_destroy(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, destroy);
	(void)data;
	handle_view_unmap(&view->unmap, NULL);
	wl_list_remove(&view->link);
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->commit.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wlr_scene_node_destroy(&view->scene_tree->node);
	free(view);
}

static void handle_view_request_move(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, request_move);
	(void)data;
	begin_interactive(view, TAHOE_CURSOR_MOVE, 0);
}

static void handle_view_request_resize(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, request_resize);
	struct wlr_xdg_toplevel_resize_event *event = data;
	begin_interactive(view, TAHOE_CURSOR_RESIZE, event->edges);
}

static void handle_view_request_maximize(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, request_maximize);
	(void)data;
	apply_maximize(view, view->xdg_surface->toplevel->requested.maximized);
}

static void handle_view_request_fullscreen(struct wl_listener *listener, void *data) {
	struct tahoe_view *view = wl_container_of(listener, view, request_fullscreen);
	(void)data;
	apply_fullscreen(view, view->xdg_surface->toplevel->requested.fullscreen);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	struct tahoe_view *view = calloc(1, sizeof(*view));
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

	wlr_xdg_toplevel_set_wm_capabilities(xdg_surface->toplevel,
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
	wlr_xdg_toplevel_set_size(xdg_surface->toplevel, 900, 620);
	wl_list_insert(&server->views, &view->link);
}

static void handle_decoration_request_mode(
		struct wl_listener *listener,
		void *data) {
	struct tahoe_decoration *decoration =
		wl_container_of(listener, decoration, request_mode);
	(void)data;
	wlr_xdg_toplevel_decoration_v1_set_mode(decoration->decoration,
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void handle_decoration_destroy(struct wl_listener *listener, void *data) {
	struct tahoe_decoration *decoration =
		wl_container_of(listener, decoration, destroy);
	(void)data;
	wl_list_remove(&decoration->request_mode.link);
	wl_list_remove(&decoration->destroy.link);
	free(decoration);
}

static void handle_new_xdg_decoration(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, new_xdg_decoration);
	(void)server;
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration = data;

	struct tahoe_decoration *decoration = calloc(1, sizeof(*decoration));
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
	struct tahoe_output *output = wl_container_of(listener, output, frame);
	(void)data;

	output_redraw_shell(output);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!wlr_scene_output_commit(output->scene_output, NULL)) {
		wlr_log(WLR_ERROR, "failed to commit scene output");
		return;
	}
	wlr_scene_output_send_frame_done(output->scene_output, &now);

	if (output->server->options->once && !output->server->once_committed) {
		output->server->once_committed = true;
		wl_display_terminate(output->server->display);
	}
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct tahoe_output *output =
		wl_container_of(listener, output, destroy);
	(void)data;
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
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

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct tahoe_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
		wlr_log(WLR_ERROR, "failed to initialize output renderer");
		return;
	}

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	wlr_output_enable(wlr_output, true);
	if (mode != NULL) {
		wlr_output_set_mode(wlr_output, mode);
	} else if (wlr_output->width == 0 || wlr_output->height == 0) {
		wlr_output_set_custom_mode(wlr_output,
			server->options->width,
			server->options->height,
			60000);
	}

	if (!wlr_output_commit(wlr_output)) {
		wlr_log(WLR_ERROR, "failed to commit output state");
		return;
	}

	struct tahoe_output *output = calloc(1, sizeof(*output));
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
	struct tahoe_server *server = data;
	if (!server->desktop_drag_active) {
		server_load_config(server, false);
	}
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
	snprintf(fallback, sizeof(fallback), "/tmp/tahoe-wlroots-runtime-%ld",
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

static bool server_init(struct tahoe_server *server,
		const struct tahoe_options *options) {
	memset(server, 0, sizeof(*server));
	server->options = options;
	server->hot_dock_index = -1;
	server->context_menu_kind = TAHOE_CONTEXT_MENU_NONE;
	server->context_menu_index = -1;
	server->desktop_drag_index = -1;
	server->cursor_mode = TAHOE_CURSOR_PASSTHROUGH;
	wl_list_init(&server->outputs);
	wl_list_init(&server->views);
	wl_list_init(&server->keyboards);
	tahoe_config_set_defaults(&server->config);
	server_load_config(server, false);
	tahoe_desktop_entry_load_all(options->desktop_entry_dir,
		server->desktop_entries,
		TAHOE_DESKTOP_MAX,
		&server->desktop_entry_count);
	tahoe_assets_init(&server->assets);
	tahoe_assets_load(&server->assets, options->asset_root);

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

#ifdef TAHOE_HAVE_VULKAN
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

	server->xdg_shell = wlr_xdg_shell_create(server->display, 3);
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

static void server_finish(struct tahoe_server *server) {
	tahoe_assets_finish(&server->assets);
	if (server->xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	if (server->display != NULL) {
		wl_display_destroy_clients(server->display);
		wl_display_destroy(server->display);
	}
}

int tahoe_compositor_run(const struct tahoe_options *options) {
	struct sigaction sigchld = {
		.sa_handler = SIG_IGN,
		.sa_flags = SA_NOCLDWAIT,
	};
	sigemptyset(&sigchld.sa_mask);
	sigaction(SIGCHLD, &sigchld, NULL);

	wlr_log_init(WLR_INFO, NULL);

	struct tahoe_server server;
	if (!server_init(&server, options)) {
		server_finish(&server);
		return 1;
	}

	wl_display_run(server.display);
	server_finish(&server);
	return 0;
}
