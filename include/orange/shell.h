#ifndef ORANGE_SHELL_H
#define ORANGE_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/notifications.h"
#include "orange/status_notifier.h"
#include "orange/widget_data.h"
#define ORANGE_DESKTOP_MAX 64
#define ORANGE_DESKTOP_FILE_MAX 48
#define ORANGE_DESKTOP_GRID_COLS 6
#define ORANGE_MENU_ITEM_MAX 20
#define ORANGE_WIDGET_MAX 16
#define ORANGE_CONTEXT_MENU_ITEM_MAX 16
#define ORANGE_OPEN_WITH_APP_MAX 16
#define ORANGE_NOTIFICATION_CENTER_CARD_MAX 8
#define ORANGE_STATUS_ITEM_COUNT 5
#define ORANGE_STATUS_TEXT_MAX 64
#define ORANGE_APP_MENU_LABEL_MAX 64
#define ORANGE_APP_MENU_TAB_COUNT 8
#define ORANGE_APP_MENU_ITEM_MAX 16
#define ORANGE_APP_MENU_ACTION_MAX 128
#define ORANGE_LAUNCHER_COLS 6
#define ORANGE_LAUNCHER_ROWS 4
#define ORANGE_LAUNCHER_CELL_MAX 512
#define ORANGE_LAUNCHER_MODE_COUNT 4
#define ORANGE_LAUNCHER_MODE_APPS 0
#define ORANGE_LAUNCHER_MODE_FILES 1
#define ORANGE_LAUNCHER_MODE_ACTIONS 2
#define ORANGE_LAUNCHER_MODE_CLIPBOARD 3
#define ORANGE_LAUNCHER_QUERY_MAX 64
#define ORANGE_LAUNCHER_APP_MAX 1024
#define ORANGE_LAUNCHER_CATEGORY_MAX 16
#define ORANGE_LAUNCHER_CATEGORY_NAME_MAX 64
#define ORANGE_LAUNCHER_SECTION_MAX 4
#define ORANGE_LAUNCHER_RESULT_MAX 64
#define ORANGE_LAUNCHER_RESULT_LABEL_MAX 128
#define ORANGE_LAUNCHER_RESULT_SUBTITLE_MAX 192
#define ORANGE_LAUNCHER_RESULT_ACTION_MAX 128

enum orange_launcher_mode {
	ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY,
	ORANGE_LAUNCHER_DISPLAY_FULL,
};

enum orange_launcher_result_kind {
	ORANGE_LAUNCHER_RESULT_APP,
	ORANGE_LAUNCHER_RESULT_FILE,
	ORANGE_LAUNCHER_RESULT_ACTION,
	ORANGE_LAUNCHER_RESULT_WEB,
};

struct orange_rect {
	int x;
	int y;
	int width;
	int height;
};

enum orange_shell_hit_kind {
	ORANGE_HIT_NONE,
	ORANGE_HIT_SYSTEM_MENU,
	ORANGE_HIT_SYSTEM_MENU_ITEM,
	ORANGE_HIT_APP_MENU,
	ORANGE_HIT_STATUS_AREA,
	ORANGE_HIT_STATUS_ITEM,
	ORANGE_HIT_TRAY_ITEM,
	ORANGE_HIT_MENU_BAR,
	ORANGE_HIT_NOTIFICATION_CENTER,
	ORANGE_HIT_NOTIFICATION_CENTER_EDIT,
	ORANGE_HIT_DOCK,
	ORANGE_HIT_DOCK_ITEM,
	ORANGE_HIT_DOCK_SEPARATOR,
	ORANGE_HIT_WIDGET,
	ORANGE_HIT_DESKTOP_ITEM,
	ORANGE_HIT_DESKTOP,
	ORANGE_HIT_CONTEXT_MENU_ITEM,
	ORANGE_HIT_CONTEXT_SUBMENU_ITEM,
	ORANGE_HIT_LAUNCHER_SEARCH,
	ORANGE_HIT_LAUNCHER_MODE,
	ORANGE_HIT_LAUNCHER_APP,
	ORANGE_HIT_LAUNCHER_CATEGORY,
	ORANGE_HIT_LAUNCHER_SCROLLBAR,
	ORANGE_HIT_LAUNCHER_BACKGROUND,
};

struct orange_shell_hit {
	enum orange_shell_hit_kind kind;
	int index;
};

enum orange_app_menu_tab {
	ORANGE_APP_MENU_TAB_APP,
	ORANGE_APP_MENU_TAB_FILE,
	ORANGE_APP_MENU_TAB_EDIT,
	ORANGE_APP_MENU_TAB_VIEW,
	ORANGE_APP_MENU_TAB_GO,
	ORANGE_APP_MENU_TAB_WINDOW,
	ORANGE_APP_MENU_TAB_TOOLS,
	ORANGE_APP_MENU_TAB_HELP,
};

enum orange_widget_type {
	ORANGE_WIDGET_CALENDAR,
	ORANGE_WIDGET_WEATHER,
};

struct orange_widget {
	int id;
	enum orange_widget_type type;
	bool visible;
	struct orange_rect rect;
};

enum orange_context_menu_kind {
	ORANGE_CONTEXT_MENU_NONE,
	ORANGE_CONTEXT_MENU_APP,
	ORANGE_CONTEXT_MENU_APP_FILE,
	ORANGE_CONTEXT_MENU_APP_EDIT,
	ORANGE_CONTEXT_MENU_APP_VIEW,
	ORANGE_CONTEXT_MENU_APP_GO,
	ORANGE_CONTEXT_MENU_APP_WINDOW,
	ORANGE_CONTEXT_MENU_APP_HISTORY,
	ORANGE_CONTEXT_MENU_APP_BOOKMARKS,
	ORANGE_CONTEXT_MENU_APP_TOOLS,
	ORANGE_CONTEXT_MENU_APP_HELP,
	ORANGE_CONTEXT_MENU_DOCK,
	ORANGE_CONTEXT_MENU_DOCK_RUNNING,
	ORANGE_CONTEXT_MENU_DOCK_SEPARATOR,
	ORANGE_CONTEXT_MENU_DOCK_LAUNCHER,
	ORANGE_CONTEXT_MENU_DOCK_TRASH,
	ORANGE_CONTEXT_MENU_WIDGET,
	ORANGE_CONTEXT_MENU_DESKTOP,
	ORANGE_CONTEXT_MENU_DESKTOP_ICON,
	ORANGE_CONTEXT_MENU_DESKTOP_VOLUME,
	ORANGE_CONTEXT_MENU_DESKTOP_FILE,
	ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH,
	ORANGE_CONTEXT_MENU_DOCK_OPTIONS,
	ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING,
	ORANGE_CONTEXT_MENU_DOCK_POSITION,
	ORANGE_CONTEXT_MENU_DESKTOP_SELECTION,
	ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION,
	ORANGE_CONTEXT_MENU_STATUS_WIFI,
	ORANGE_CONTEXT_MENU_STATUS_SOUND,
	ORANGE_CONTEXT_MENU_STATUS_BATTERY,
};

enum orange_status_item {
	ORANGE_STATUS_ITEM_WIFI,
	ORANGE_STATUS_ITEM_SOUND,
	ORANGE_STATUS_ITEM_BATTERY,
	ORANGE_STATUS_ITEM_SEARCH,
	ORANGE_STATUS_ITEM_CLOCK,
};

enum orange_notification_center_card_kind {
	ORANGE_NOTIFICATION_CENTER_CARD_EMPTY,
	ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION,
	ORANGE_NOTIFICATION_CENTER_CARD_CALENDAR_WIDGET,
	ORANGE_NOTIFICATION_CENTER_CARD_SCREEN_TIME_WIDGET,
	ORANGE_NOTIFICATION_CENTER_CARD_WEATHER_WIDGET,
};

struct orange_status_state {
	char network_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char network_label[ORANGE_STATUS_TEXT_MAX];
	bool network_connected;
	bool network_available;

	char sound_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char sound_label[ORANGE_STATUS_TEXT_MAX];
	bool sound_muted;
	int sound_percent;

	char battery_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char battery_label[ORANGE_STATUS_TEXT_MAX];
	bool battery_present;
	bool battery_charging;
	int battery_percent;
};

struct orange_app_menu_item {
	char label[ORANGE_APP_MENU_LABEL_MAX];
	char action[ORANGE_APP_MENU_ACTION_MAX];
	bool enabled;
	bool separator;
};

struct orange_app_menu_model {
	bool available;
	bool native;
	int tab_count;
	char tab_labels[ORANGE_APP_MENU_TAB_COUNT][ORANGE_APP_MENU_LABEL_MAX];
	int item_counts[ORANGE_APP_MENU_TAB_COUNT];
	struct orange_app_menu_item items[ORANGE_APP_MENU_TAB_COUNT][ORANGE_APP_MENU_ITEM_MAX];
};

/* Desktop item descriptor: either a .desktop entry index, a volume index, or a file index. */
enum orange_desktop_item_kind {
	ORANGE_DESKTOP_ITEM_ENTRY,
	ORANGE_DESKTOP_ITEM_VOLUME,
	ORANGE_DESKTOP_ITEM_FILE,
};

struct orange_file_info {
	char name[256];
	char path[1024];
	char icon_name[128];
	bool is_directory;
	bool is_image;
};

struct orange_launcher_result {
	enum orange_launcher_result_kind kind;
	int source_index;
	char label[ORANGE_LAUNCHER_RESULT_LABEL_MAX];
	char subtitle[ORANGE_LAUNCHER_RESULT_SUBTITLE_MAX];
	char icon_name[ORANGE_ASSET_ICON_NAME_MAX];
	char action[ORANGE_LAUNCHER_RESULT_ACTION_MAX];
};

struct orange_desktop_item_info {
	enum orange_desktop_item_kind kind;
	int index;
};

struct orange_shell_layout {
	int width;
	int height;
	double menu_scale;

	struct orange_rect menu_bar;
	struct orange_rect system_menu_button;
	struct orange_rect app_menu_button;
	struct orange_rect app_menu_items[ORANGE_APP_MENU_TAB_COUNT];
	int app_menu_item_count;
	struct orange_rect status_area;
	struct orange_rect status_items[ORANGE_STATUS_ITEM_COUNT];
	struct orange_rect status_notifier_items[
		ORANGE_STATUS_NOTIFIER_ITEM_MAX];
	int status_notifier_item_count;
	struct orange_rect notification_center_panel;
	struct orange_rect notification_center_cards[ORANGE_NOTIFICATION_CENTER_CARD_MAX];
	enum orange_notification_center_card_kind notification_center_card_kinds[
		ORANGE_NOTIFICATION_CENTER_CARD_MAX];
	int notification_center_card_count;
	struct orange_rect notification_center_edit_button;
	struct orange_rect system_menu_panel;
	struct orange_rect system_menu_items[ORANGE_MENU_ITEM_MAX];
	bool system_menu_separator[ORANGE_MENU_ITEM_MAX];
	int system_menu_item_count;

	struct orange_rect calendar_widget;
	struct orange_rect weather_widget;
	struct orange_widget widgets[ORANGE_WIDGET_MAX];
	int widget_count;

	struct orange_rect desktop_items[ORANGE_DESKTOP_MAX];
	struct orange_desktop_item_info desktop_item_info[ORANGE_DESKTOP_MAX];
	int desktop_item_count;

	struct orange_rect dock;
	struct orange_rect dock_items[ORANGE_DOCK_MAX];
	struct orange_rect dock_separator;
	struct orange_rect dock_reveal_zone;
	enum orange_dock_position dock_position;
	bool dock_auto_hide;
	bool dock_auto_hide_blocked;
	bool dock_auto_hidden;
	int dock_launcher_indices[ORANGE_DOCK_MAX];
	int dock_item_count;
	bool dock_temporary[ORANGE_DOCK_MAX];
	char dock_temporary_app_ids[ORANGE_DOCK_MAX][128];
	int dock_temporary_count;
	bool dock_minimized[ORANGE_DOCK_MAX];
	int dock_minimized_indices[ORANGE_DOCK_MAX];
	int dock_minimized_count;
	struct orange_rect dock_temporary_separator;

	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int open_with_app_count;
	bool open_with_submenu_open;
	bool dock_options_submenu_open;
	bool dock_minimize_submenu_open;
	bool dock_position_submenu_open;
	bool context_menu_alt_pressed;
	bool context_menu_dock_temporary;
	struct orange_rect context_menu_panel;
	struct orange_rect context_menu_items[ORANGE_CONTEXT_MENU_ITEM_MAX];
	bool context_menu_separator[ORANGE_CONTEXT_MENU_ITEM_MAX];
	int context_menu_item_count;
	struct orange_rect context_submenu_panel;
	struct orange_rect context_submenu_items[ORANGE_CONTEXT_MENU_ITEM_MAX];
	int context_submenu_item_count;

	bool launcher_visible;
	enum orange_launcher_mode launcher_display_mode;
	bool launcher_position_set;
	int launcher_x;
	int launcher_y;
	struct orange_rect launcher_panel;
	struct orange_rect launcher_search_field;
	struct orange_rect launcher_mode_buttons[ORANGE_LAUNCHER_MODE_COUNT];
	struct orange_rect launcher_category_tabs[ORANGE_LAUNCHER_CATEGORY_MAX];
	int launcher_category_count;
	int launcher_category_active;
	struct orange_rect launcher_section_headers[ORANGE_LAUNCHER_SECTION_MAX];
	int launcher_section_starts[ORANGE_LAUNCHER_SECTION_MAX];
	int launcher_section_lengths[ORANGE_LAUNCHER_SECTION_MAX];
	int launcher_section_count;
	struct orange_rect launcher_viewport;
	struct orange_rect launcher_scroll_track;
	struct orange_rect launcher_scroll_thumb;
	struct orange_rect launcher_grid_cells[ORANGE_LAUNCHER_CELL_MAX];
	int launcher_grid_indices[ORANGE_LAUNCHER_CELL_MAX];
	int launcher_grid_cell_count;
	int launcher_grid_cols;
	int launcher_grid_cell_w;
	int launcher_grid_cell_h;
	int launcher_grid_icon_size;
	int launcher_scroll;
	int launcher_scroll_row;
	int launcher_max_scroll;
	bool launcher_searching;
	bool launcher_list_results;
	int launcher_current_mode;
};

struct orange_shell_state {
	bool system_menu_open;
	bool notification_center_open;
	int hot_dock_index;
	int dock_pointer_x;
	int dock_pointer_y;
	bool dock_auto_hide_revealed;
	bool dock_auto_hide_blocked;
	bool dock_auto_hide_animating;
	double dock_auto_hide_progress;
	time_t now;
	struct orange_assets *assets;
	const struct orange_config *config;
	bool trash_full;
	struct orange_status_state status;
	struct orange_status_notifier_item status_notifier_items[
		ORANGE_STATUS_NOTIFIER_ITEM_MAX];
	int status_notifier_item_count;
	const struct orange_desktop_entry *desktop_entries;
	int desktop_entry_count;
	const struct orange_volume_info *volumes;
	int volume_count;
	int desktop_volume_count;
	const struct orange_file_info *desktop_files;
	int desktop_file_count;
	const struct orange_notification *notifications;
	int notification_count;
	const struct orange_widget_data *widget_data;
	struct orange_app_menu_model app_menu;
	bool dock_open[ORANGE_DOCK_MAX];
	char active_app_label[ORANGE_APP_MENU_LABEL_MAX];
	int dock_drag_index;
	int dock_drag_insert_before;
	int dock_drag_x;
	int dock_drag_y;
	bool dock_drag_remove;
	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
	int open_with_app_count;
	bool open_with_submenu_open;
	bool dock_options_submenu_open;
	bool dock_minimize_submenu_open;
	bool dock_position_submenu_open;
	bool context_menu_alt_pressed;
	bool context_menu_dock_temporary;
	char open_with_app_labels[ORANGE_OPEN_WITH_APP_MAX][ORANGE_APP_MENU_LABEL_MAX];
	char open_with_app_icons[ORANGE_OPEN_WITH_APP_MAX][ORANGE_ASSET_ICON_NAME_MAX];
	bool desktop_selected[ORANGE_DESKTOP_MAX];
	bool desktop_selection_active;
	struct orange_rect desktop_selection_rect;
	bool desktop_rename_active;
	int desktop_rename_index;
	char desktop_rename_text[256];

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
	int launcher_scroll;
	int launcher_scroll_row;
	int launcher_current_mode;
	bool launcher_search_focus;
	bool launcher_app_drag_active;
	int launcher_app_drag_x;
	int launcher_app_drag_y;
	int launcher_app_drag_entry_index;
	char launcher_categories[ORANGE_LAUNCHER_CATEGORY_MAX][ORANGE_LAUNCHER_CATEGORY_NAME_MAX];
	int launcher_category_count;
	int launcher_category_active;

	bool dock_bounce_active;
	int dock_bounce_launcher_idx;
	uint32_t dock_bounce_start_time;
	int dock_temporary_count;
	char dock_temporary_app_ids[ORANGE_DOCK_MAX][128];
	bool dock_temporary_open[ORANGE_DOCK_MAX];
	int dock_minimized_count;
	char dock_minimized_titles[ORANGE_DOCK_MAX][ORANGE_APP_MENU_LABEL_MAX];
};

struct orange_shell_draw_options {
	bool draw_wallpaper;
	bool skip_transient_overlays;
	bool skip_dock;
	bool draw_only_dock;
	bool clip_to_bounds;
	struct orange_rect clip_bounds;
};

#include "orange/dock.h"
#include "orange/launcher.h"
#include "orange/menubar.h"

void orange_shell_layout_compute(
	int width,
	int height,
	bool system_menu_open,
	const struct orange_config *config,
	int desktop_entry_count,
	int desktop_volume_count,
	const struct orange_file_info *desktop_files,
	int desktop_file_count,
	struct orange_shell_layout *layout);
void orange_shell_layout_compute_with_dock_temporary(
	int width,
	int height,
	bool system_menu_open,
	const struct orange_config *config,
	int desktop_entry_count,
	int desktop_volume_count,
	const struct orange_file_info *desktop_files,
	int desktop_file_count,
	int dock_temporary_count,
	const void *dock_temporary_app_ids,
	struct orange_shell_layout *layout);
void orange_shell_layout_compute_with_dock_state(
	int width,
	int height,
	bool system_menu_open,
	const struct orange_config *config,
	int desktop_entry_count,
	int desktop_volume_count,
	const struct orange_file_info *desktop_files,
	int desktop_file_count,
	int dock_temporary_count,
	const void *dock_temporary_app_ids,
	int dock_minimized_count,
	struct orange_shell_layout *layout);
void orange_shell_layout_clean_up(
	struct orange_shell_layout *layout,
	const struct orange_config *config);
void orange_shell_layout_apply_dock_auto_hide(
	struct orange_shell_layout *layout,
	const struct orange_config *config,
	bool blocked,
	bool revealed);
void orange_shell_layout_apply_dock_auto_hide_progress(
	struct orange_shell_layout *layout,
	const struct orange_config *config,
	bool blocked,
	bool revealed,
	double visible_progress);
bool orange_shell_layout_dock_auto_hide_blocked_by_view(
	const struct orange_shell_layout *layout,
	struct orange_rect view_rect,
	bool fullscreen);
double orange_shell_dock_scale_for_separator_drag(
	enum orange_dock_position position,
	double start_scale,
	int start_x,
	int start_y,
	int current_x,
	int current_y);
double orange_shell_minimize_animation_ease(double progress);
struct orange_rect orange_shell_minimize_animation_rect(
	struct orange_rect start,
	struct orange_rect target,
	double progress);
double orange_shell_workspace_animation_ease(double progress);
int orange_shell_workspace_animation_offset(
	int span,
	int direction,
	double progress,
	bool incoming);
void orange_shell_layout_sort_by(
	struct orange_shell_layout *layout,
	enum orange_desktop_sort_by sort_by,
	const struct orange_config *config,
	const struct orange_desktop_entry *entries,
	int entry_count,
	const struct orange_volume_info *volumes,
	int volume_count);
void orange_shell_layout_snap_to_grid(
	struct orange_shell_layout *layout,
	int index,
	int *x,
	int *y,
	const struct orange_config *config);
void orange_shell_layout_set_app_menu_tabs(
	struct orange_shell_layout *layout,
	const char *active_app_label,
	const struct orange_app_menu_model *app_menu);
void orange_shell_layout_set_status_notifier_items(
	struct orange_shell_layout *layout,
	const struct orange_status_notifier_item *items,
	int item_count);
void orange_shell_layout_set_context_menu(
	struct orange_shell_layout *layout,
	enum orange_context_menu_kind kind,
	int index,
	int cursor_x,
	int cursor_y,
	const struct orange_app_menu_model *app_menu);
void orange_shell_layout_set_notification_center(
	struct orange_shell_layout *layout,
	int notification_count);
struct orange_rect orange_shell_layout_work_area(
	const struct orange_shell_layout *layout);
struct orange_rect orange_shell_desktop_label_rect(
	const struct orange_shell_layout *layout,
	const struct orange_config *config,
	int index);
struct orange_rect orange_shell_desktop_item_context_rect(
	const struct orange_shell_layout *layout,
	int index);
void orange_shell_layout_set_launcher(
	struct orange_shell_layout *layout,
	bool searching,
	enum orange_launcher_mode display_mode,
	int app_count);

struct orange_shell_hit orange_shell_hit_test(
	const struct orange_shell_layout *layout,
	int x,
	int y);

void orange_shell_draw(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct orange_shell_state *state);
void orange_shell_draw_with_options(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct orange_shell_state *state,
	const struct orange_shell_draw_options *options);
void orange_shell_draw_overlay(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const struct orange_shell_state *state);
bool orange_shell_overlay_bounds(
	int width,
	int height,
	const struct orange_shell_state *state,
	struct orange_rect *out_bounds);
void orange_shell_draw_overlay_with_backdrop(
	uint32_t *pixels,
	int width,
	int height,
	int stride,
	const uint32_t *backdrop_pixels,
	int backdrop_stride,
	const struct orange_shell_state *state);

const char *orange_shell_volume_label(const struct orange_volume_info *volumes,
	int volume_count, int index);
const char *orange_shell_volume_icon_name(const struct orange_volume_info *volumes,
	int volume_count, int index);

#endif
