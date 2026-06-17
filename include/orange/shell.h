#ifndef ORANGE_SHELL_H
#define ORANGE_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"

#define ORANGE_DESKTOP_MAX 32
#define ORANGE_DESKTOP_GRID_COLS 6
#define ORANGE_MENU_ITEM_MAX 20
#define ORANGE_WIDGET_MAX 16
#define ORANGE_CONTEXT_MENU_ITEM_MAX 16
#define ORANGE_NOTIFICATION_CENTER_CARD_MAX 8
#define ORANGE_STATUS_ITEM_COUNT 6
#define ORANGE_STATUS_TEXT_MAX 64
#define ORANGE_APP_MENU_LABEL_MAX 64
#define ORANGE_APP_MENU_TAB_COUNT 8
#define ORANGE_APP_MENU_ITEM_MAX 16
#define ORANGE_APP_MENU_ACTION_MAX 128

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
	ORANGE_HIT_NOTIFICATION_CENTER,
	ORANGE_HIT_NOTIFICATION_CENTER_EDIT,
	ORANGE_HIT_DOCK_ITEM,
	ORANGE_HIT_WIDGET,
	ORANGE_HIT_DESKTOP_ITEM,
	ORANGE_HIT_DESKTOP,
	ORANGE_HIT_CONTEXT_MENU_ITEM,
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
	ORANGE_CONTEXT_MENU_WIDGET,
	ORANGE_CONTEXT_MENU_DESKTOP,
	ORANGE_CONTEXT_MENU_DESKTOP_ICON,
	ORANGE_CONTEXT_MENU_DESKTOP_VOLUME,
	ORANGE_CONTEXT_MENU_STATUS,
	ORANGE_CONTEXT_MENU_STATUS_WIFI,
	ORANGE_CONTEXT_MENU_STATUS_SOUND,
	ORANGE_CONTEXT_MENU_STATUS_BATTERY,
};

enum orange_status_item {
	ORANGE_STATUS_ITEM_WIFI,
	ORANGE_STATUS_ITEM_SOUND,
	ORANGE_STATUS_ITEM_BATTERY,
	ORANGE_STATUS_ITEM_SEARCH,
	ORANGE_STATUS_ITEM_CONTROL_CENTER,
	ORANGE_STATUS_ITEM_CLOCK,
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

/* Desktop item descriptor: either a .desktop entry index or a volume index. */
enum orange_desktop_item_kind {
	ORANGE_DESKTOP_ITEM_ENTRY,
	ORANGE_DESKTOP_ITEM_VOLUME,
};

struct orange_desktop_item_info {
	enum orange_desktop_item_kind kind;
	int index;
};

struct orange_shell_layout {
	int width;
	int height;

	struct orange_rect menu_bar;
	struct orange_rect system_menu_button;
	struct orange_rect app_menu_button;
	struct orange_rect app_menu_items[ORANGE_APP_MENU_TAB_COUNT];
	int app_menu_item_count;
	struct orange_rect status_area;
	struct orange_rect status_items[ORANGE_STATUS_ITEM_COUNT];
	struct orange_rect notification_center_panel;
	struct orange_rect notification_center_cards[ORANGE_NOTIFICATION_CENTER_CARD_MAX];
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
	int dock_launcher_indices[ORANGE_DOCK_MAX];
	int dock_item_count;

	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	struct orange_rect context_menu_panel;
	struct orange_rect context_menu_items[ORANGE_CONTEXT_MENU_ITEM_MAX];
	bool context_menu_separator[ORANGE_CONTEXT_MENU_ITEM_MAX];
	int context_menu_item_count;
};

struct orange_shell_state {
	bool system_menu_open;
	bool notification_center_open;
	int hot_dock_index;
	int dock_pointer_x;
	int dock_pointer_y;
	time_t now;
	struct orange_assets *assets;
	const struct orange_config *config;
	struct orange_status_state status;
	const struct orange_desktop_entry *desktop_entries;
	int desktop_entry_count;
	const struct orange_volume_info *volumes;
	int volume_count;
	int desktop_volume_count;
	struct orange_app_menu_model app_menu;
	bool dock_open[ORANGE_DOCK_MAX];
	char active_app_label[ORANGE_APP_MENU_LABEL_MAX];
	int dock_drag_index;
	int dock_drag_insert_before;
	int dock_drag_x;
	int dock_drag_y;
	enum orange_context_menu_kind context_menu_kind;
	int context_menu_index;
	int context_menu_cursor_x;
	int context_menu_cursor_y;
	int desktop_drag_target_index;
	bool desktop_drag_swap;
};

struct orange_shell_draw_options {
	bool draw_wallpaper;
};

void orange_shell_layout_compute(
	int width,
	int height,
	bool system_menu_open,
	const struct orange_config *config,
	int desktop_entry_count,
	int desktop_volume_count,
	struct orange_shell_layout *layout);
void orange_shell_layout_clean_up(
	struct orange_shell_layout *layout,
	const struct orange_config *config);
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
void orange_shell_layout_set_context_menu(
	struct orange_shell_layout *layout,
	enum orange_context_menu_kind kind,
	int index,
	int cursor_x,
	int cursor_y,
	const struct orange_app_menu_model *app_menu);
void orange_shell_layout_set_notification_center(
	struct orange_shell_layout *layout);

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

int orange_shell_dock_count(const struct orange_config *config);
int orange_shell_dock_launcher_index(
	const struct orange_shell_layout *layout,
	int visible_index);
const char *orange_shell_dock_label(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_shell_builtin_command(const char *app_id);
const char *orange_shell_dock_command(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_shell_active_app_label(const struct orange_shell_state *state);
const char *orange_shell_menu_label(int index);
const char *orange_shell_context_menu_label(
	enum orange_context_menu_kind kind,
	int index);
const char *orange_shell_context_menu_icon_name(
	enum orange_context_menu_kind kind,
	int index);
const char *orange_shell_volume_label(const struct orange_volume_info *volumes,
	int volume_count, int index);
const char *orange_shell_volume_icon_name(const struct orange_volume_info *volumes,
	int volume_count, int index);

#endif
