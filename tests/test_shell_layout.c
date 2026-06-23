#include "orange/dock.h"
#include "orange/launcher.h"
#include "orange/shell.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_roughly_double(int large, int small) {
	assert(abs(large - small * 2) <= 2);
}

static void assert_app_menu_tabs_do_not_overlap(
		const struct orange_shell_layout *layout) {
	int last_right = 0;
	for (int i = 0; i < layout->app_menu_item_count; i++) {
		struct orange_rect item = layout->app_menu_items[i];
		if (item.width <= 0) {
			continue;
		}
		assert(item.x >= last_right);
		assert(item.x + item.width <= layout->status_area.x);
		last_right = item.x + item.width;
	}
}

static void test_dock_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	assert(layout.dock_item_count >= 10);
	struct orange_rect first = layout.dock_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_ITEM);
	assert(hit.index == 0);
	struct orange_desktop_entry entries[1];
	assert(orange_dock_command(hit.index, entries, 0, &config) != NULL);

	assert(layout.dock_separator.width > 0);
	hit = orange_shell_hit_test(
		&layout,
		layout.dock_separator.x + layout.dock_separator.width / 2,
		layout.dock_separator.y + layout.dock_separator.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_SEPARATOR);
	assert(hit.index == -1);
	hit = orange_shell_hit_test(
		&layout,
		layout.dock.x + 4,
		layout.dock.y + layout.dock.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK);
	assert(hit.index == -1);
	struct orange_rect work = orange_shell_layout_work_area(&layout);
	assert(work.x == 0);
	assert(work.y == layout.menu_bar.height);
	assert(work.x + work.width == layout.width);
	assert(work.y + work.height == layout.dock.y);
}

static void test_reordered_dock_hit_resolves_launcher(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_order[0] = 1;
	config.dock_order[1] = 0;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	assert(layout.dock_item_count >= 2);
	struct orange_rect first = layout.dock_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_ITEM);
	assert(hit.index == 0);
	int launcher_idx = orange_dock_launcher_index(&layout, hit.index);
	assert(launcher_idx == 1);
	assert(strcmp(orange_dock_label(launcher_idx, NULL, 0, &config),
		"Launcher") == 0);
	const char *command = orange_dock_command(launcher_idx,
		NULL, 0, &config);
	assert(command != NULL);
	assert(strstr(command, "ORANGE_APP_PICKER") != NULL);
}

static void test_dock_position_layout(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_position = ORANGE_DOCK_POSITION_LEFT;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	assert(layout.dock_position == ORANGE_DOCK_POSITION_LEFT);
	assert(layout.dock.x < 80);
	assert(layout.dock.height > layout.dock.width);
	assert(layout.dock_items[1].y > layout.dock_items[0].y);
	assert(layout.dock_separator.width > layout.dock_separator.height);
	struct orange_rect work = orange_shell_layout_work_area(&layout);
	assert(work.x == layout.dock.x + layout.dock.width);
	assert(work.y == layout.menu_bar.height);
	assert(work.x + work.width == layout.width);
	assert(work.y + work.height == layout.height);
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		layout.dock_separator.x + layout.dock_separator.width / 2,
		layout.dock_separator.y + layout.dock_separator.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_SEPARATOR);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR);
	assert(layout.context_menu_panel.x >=
		layout.dock.x + layout.dock.width);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	assert(layout.dock_position == ORANGE_DOCK_POSITION_RIGHT);
	assert(layout.dock.x > layout.width / 2);
	assert(layout.dock.height > layout.dock.width);
	assert(layout.dock_items[1].y > layout.dock_items[0].y);
	work = orange_shell_layout_work_area(&layout);
	assert(work.x == 0);
	assert(work.y == layout.menu_bar.height);
	assert(work.x + work.width == layout.dock.x);
	assert(work.y + work.height == layout.height);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_menu_panel.x + layout.context_menu_panel.width <=
		layout.dock.x);
}

static void test_default_dock_apps_have_fallback_commands(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_dock_count(&config);
	assert(dock_count > 0);
	for (int i = 0; i < dock_count; i++) {
		const char *command = orange_dock_command(
			i, NULL, 0, &config);
		assert(command != NULL);
		assert(command[0] != '\0');
		if (strstr(command, "gnome-control-center") != NULL) {
			assert(strstr(command,
				"XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu") != NULL);
			assert(strstr(command,
				"env -u GTK_THEME -u GTK_ICON_THEME") != NULL);
		}
	}
	const char *settings_command =
		orange_dock_builtin_command("org.gnome.Settings.desktop");
	assert(settings_command != NULL);
	assert(strstr(settings_command, "gnome-control-center") != NULL);
	assert(strstr(settings_command,
		"XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu") != NULL);
	assert(strstr(settings_command,
		"env -u GTK_THEME -u GTK_ICON_THEME") != NULL);
}

static void test_firefox_dock_matches_snap_desktop_entry(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	const struct orange_desktop_entry entries[] = {
		{
			.id = "firefox_firefox",
			.name = "Firefox",
			.icon = "/snap/firefox/current/default256.png",
			.exec = "/snap/bin/firefox %u",
		},
	};

	assert(strcmp(config.dock_apps[2], "firefox.desktop") == 0);
	assert(strcmp(orange_dock_label(2, entries, 1, &config),
		"Firefox") == 0);
	const char *command = orange_dock_command(2, entries, 1, &config);
	assert(command != NULL);
	assert(strstr(command, "/snap/bin/firefox") != NULL);
}

static void test_dock_trash_spacing_is_balanced(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	int first_leading = layout.dock_items[0].x - layout.dock.x;
	int last = layout.dock_item_count - 1;
	int trailing = layout.dock.x + layout.dock.width -
		(layout.dock_items[last].x + layout.dock_items[last].width);
	assert(abs(first_leading - trailing) <= 1);

	snprintf(config.dock_apps[last], sizeof(config.dock_apps[last]), "%s", "");
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	first_leading = layout.dock_items[0].x - layout.dock.x;
	last = layout.dock_item_count - 1;
	trailing = layout.dock.x + layout.dock.width -
		(layout.dock_items[last].x + layout.dock_items[last].width);
	assert(abs(first_leading - trailing) <= 1);
}

static void test_dock_remove_visible_compacts_aliases(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int original_count = orange_dock_count(&config);
	assert(original_count > 4);
	assert(orange_dock_config_remove_visible(&config, 2));
	assert(orange_dock_count(&config) == original_count - 1);
	assert(!orange_dock_config_contains_app(&config, "firefox.desktop"));
	for (int i = 0; i < orange_dock_count(&config); i++) {
		assert(config.dock_apps[i][0] != '\0');
		assert(config.dock_order[i] == i);
	}
	assert(!orange_dock_config_remove_visible(&config, 1));
	assert(orange_dock_config_contains_app(&config, "__launcher__"));
	assert(!orange_dock_config_remove_visible(&config,
		orange_dock_count(&config) - 1));
	assert(orange_dock_config_contains_app(&config, "__trash__"));
}

static void test_dock_insert_from_launcher_clamps_before_trash(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int original_count = orange_dock_count(&config);
	assert(original_count > 2);
	assert(orange_dock_config_insert_app(&config,
		"org.example.NewApp.desktop", original_count));
	assert(orange_dock_count(&config) == original_count + 1);
	assert(strcmp(config.dock_apps[original_count - 1],
		"org.example.NewApp.desktop") == 0);
	assert(strcmp(config.dock_apps[original_count], "__trash__") == 0);
	assert(!orange_dock_config_insert_app(&config,
		"org.example.NewApp.desktop", 0));
	assert(!orange_dock_config_insert_app(&config,
		"org.example.NewApp", 0));
}

static void test_dock_reorder_visible_keeps_permanent_items_fixed(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int original_count = orange_dock_count(&config);
	assert(original_count > 4);
	assert(!orange_dock_config_reorder_visible(&config, 1, 0));
	assert(strcmp(config.dock_apps[1], "__launcher__") == 0);
	assert(orange_dock_config_reorder_visible(&config, 2, 0));
	assert(strcmp(config.dock_apps[0], "firefox.desktop") == 0);
	assert(orange_dock_config_reorder_visible(&config, 0, original_count));
	assert(strcmp(config.dock_apps[original_count - 2], "firefox.desktop") == 0);
	assert(strcmp(config.dock_apps[original_count - 1], "__trash__") == 0);
}

static void test_desktop_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 2, &layout);
	struct orange_rect item = layout.desktop_items[1];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
}

static void test_desktop_requires_volumes(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 0, &layout);
	assert(layout.desktop_item_count == 0);
}

static void test_desktop_custom_position_snaps_to_grid(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 2, &layout);
	assert(layout.desktop_item_count == 2);
	/* Items are placed in grid from right side */
	assert(layout.desktop_items[0].x >= 1700);
	assert(layout.desktop_items[0].y >= layout.menu_bar.height);
}

static void test_desktop_custom_position_clamps_to_visible_area(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 50000, 50000};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 1, &layout);
	struct orange_rect item = layout.desktop_items[0];
	assert(item.x >= 0);
	assert(item.y >= layout.menu_bar.height);
	assert(item.x + item.width <= layout.width);
	assert(item.y + item.height <= layout.dock.y);
}

static void test_system_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout closed;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &closed);
	struct orange_shell_hit hit = orange_shell_hit_test(&closed, 36, 16);
	assert(hit.kind == ORANGE_HIT_SYSTEM_MENU);
	hit = orange_shell_hit_test(&closed,
		closed.app_menu_button.x + closed.app_menu_button.width / 2,
		closed.app_menu_button.y + closed.app_menu_button.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);

	struct orange_shell_layout open;
	orange_shell_layout_compute(1920, 1080, true, &config, 2, 0, &open);
	assert(open.system_menu_item_count > 4);
	struct orange_rect item = open.system_menu_items[3];
	hit = orange_shell_hit_test(&open, item.x + 8, item.y + 10);
	assert(hit.kind == ORANGE_HIT_SYSTEM_MENU_ITEM);
	assert(hit.index == 3);
	assert(orange_menubar_menu_label(hit.index) != NULL);
}

static void test_app_menu_layout_and_labels(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	orange_shell_layout_set_app_menu_tabs(&layout, "Photoshop", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);

	struct orange_shell_state default_state = {0};
	assert(strcmp(orange_menubar_active_app_label(&default_state), "Files") == 0);
	struct orange_shell_state active_state = {0};
	snprintf(active_state.active_app_label,
		sizeof(active_state.active_app_label), "%s", "Terminal");
	assert(strcmp(orange_menubar_active_app_label(&active_state), "Terminal") == 0);

	struct orange_rect app_tab =
		layout.app_menu_items[ORANGE_APP_MENU_TAB_APP];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		app_tab.x + app_tab.width / 2,
		app_tab.y + app_tab.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);
	assert(hit.index == ORANGE_APP_MENU_TAB_APP);

	struct orange_rect file_tab =
		layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE];
	hit = orange_shell_hit_test(
		&layout,
		file_tab.x + file_tab.width / 2,
		file_tab.y + file_tab.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);
	assert(hit.index == ORANGE_APP_MENU_TAB_FILE);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP);
	assert(layout.context_menu_item_count == 7);
	assert(layout.context_menu_panel.x == layout.app_menu_items[
		ORANGE_APP_MENU_TAB_APP].x);
	assert(layout.context_menu_panel.y > layout.menu_bar.height);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP, 0), "About App") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP, 6), "Quit App") == 0);
	assert(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_APP, 0) != NULL);

	struct orange_rect item = layout.context_menu_items[6];
	hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_CONTEXT_MENU_ITEM);
	assert(hit.index == 6);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_FILE, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_FILE);
	assert(layout.context_menu_item_count == 7);
	assert(layout.context_menu_panel.x == layout.app_menu_items[
		ORANGE_APP_MENU_TAB_FILE].x);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_FILE, 1), "Open...") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_FILE, 4), "Save") == 0);
	assert(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_APP_FILE, 1) != NULL);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_EDIT, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_EDIT);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_EDIT, 3), "Copy") == 0);

	orange_shell_layout_set_app_menu_tabs(&layout, "Firefox", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_GO].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW].width >
		layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width > 0);
	struct orange_rect bookmarks_tab =
		layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW];
	hit = orange_shell_hit_test(
		&layout,
		bookmarks_tab.x + bookmarks_tab.width / 2,
		bookmarks_tab.y + bookmarks_tab.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);
	assert(hit.index == ORANGE_APP_MENU_TAB_WINDOW);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_BOOKMARKS, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_BOOKMARKS);
	assert(layout.context_menu_item_count == 4);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_BOOKMARKS, 0),
		"Bookmark Current Tab") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_TOOLS, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_TOOLS);
	assert(layout.context_menu_item_count == 4);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_TOOLS, 1),
		"Add-ons and Themes") == 0);

	struct orange_app_menu_model native_menu = {
		.available = true,
		.native = true,
		.tab_count = 2,
	};
	snprintf(native_menu.tab_labels[ORANGE_APP_MENU_TAB_APP],
		sizeof(native_menu.tab_labels[ORANGE_APP_MENU_TAB_APP]),
		"%s", "NativeApp");
	snprintf(native_menu.tab_labels[ORANGE_APP_MENU_TAB_FILE],
		sizeof(native_menu.tab_labels[ORANGE_APP_MENU_TAB_FILE]),
		"%s", "Project");
	native_menu.item_counts[ORANGE_APP_MENU_TAB_FILE] = 2;
	snprintf(native_menu.items[ORANGE_APP_MENU_TAB_FILE][0].label,
		sizeof(native_menu.items[ORANGE_APP_MENU_TAB_FILE][0].label),
		"%s", "Native Open");
	snprintf(native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].label,
		sizeof(native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].label),
		"%s", "Native Close");
	orange_shell_layout_set_app_menu_tabs(&layout, "Firefox", &native_menu);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_EDIT].width == 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_HELP].width == 0);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_FILE, -1, 0, 0, &native_menu);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_FILE);
	assert(layout.context_menu_item_count == 2);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_HELP, -1, 0, 0, &native_menu);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_NONE);
}

static void test_status_area_hit_and_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	struct orange_rect clock =
		layout.status_items[ORANGE_STATUS_ITEM_CLOCK];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		clock.x + clock.width / 2,
		clock.y + clock.height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_ITEM);
	assert(hit.index == ORANGE_STATUS_ITEM_CLOCK);

	struct orange_rect control =
		layout.status_items[ORANGE_STATUS_ITEM_CONTROL_CENTER];
	hit = orange_shell_hit_test(&layout,
		control.x + control.width / 2,
		control.y + control.height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_ITEM);
	assert(hit.index == ORANGE_STATUS_ITEM_CONTROL_CENTER);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_STATUS, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_STATUS);
	assert(layout.context_menu_item_count == 10);
	assert(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS, 0) != NULL);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS, 9), "Control Center Settings...") == 0);
	assert(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_STATUS, 0) != NULL);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_STATUS_WIFI, ORANGE_STATUS_ITEM_WIFI, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_STATUS_WIFI);
	assert(layout.context_menu_item_count == 3);
	assert(layout.context_menu_panel.y > layout.menu_bar.height);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS_WIFI, 2), "Network Settings...") == 0);
	assert(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_STATUS_WIFI, 0) != NULL);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_STATUS_SOUND, ORANGE_STATUS_ITEM_SOUND, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_STATUS_SOUND);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS_SOUND, 2), "Sound Settings...") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_STATUS_BATTERY, ORANGE_STATUS_ITEM_BATTERY, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_STATUS_BATTERY);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_STATUS_BATTERY, 1), "Power Settings...") == 0);
}

static void test_notification_center_layout_and_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 0, &layout);
	orange_shell_layout_set_notification_center(&layout, 2);

	assert(layout.notification_center_panel.width > 0);
	assert(layout.notification_center_panel.x >= 0);
	assert(layout.notification_center_panel.x +
		layout.notification_center_panel.width <= layout.width);
	assert(layout.notification_center_panel.y > layout.menu_bar.height);
	assert(layout.notification_center_card_count >= 4);
	assert(layout.notification_center_card_kinds[0] ==
		ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION);
	assert(layout.notification_center_card_kinds[1] ==
		ORANGE_NOTIFICATION_CENTER_CARD_NOTIFICATION);
	assert(layout.notification_center_card_kinds[2] ==
		ORANGE_NOTIFICATION_CENTER_CARD_CALENDAR_WIDGET);
	assert(layout.notification_center_edit_button.y >
		layout.notification_center_cards[
			layout.notification_center_card_count - 1].y);

	struct orange_rect card = layout.notification_center_cards[0];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		card.x + card.width / 2,
		card.y + card.height / 2);
	assert(hit.kind == ORANGE_HIT_NOTIFICATION_CENTER);

	struct orange_rect edit = layout.notification_center_edit_button;
	hit = orange_shell_hit_test(&layout,
		edit.x + edit.width / 2,
		edit.y + edit.height / 2);
	assert(hit.kind == ORANGE_HIT_NOTIFICATION_CENTER_EDIT);

	struct orange_shell_layout empty;
	orange_shell_layout_compute(1920, 1080, false, &config, 0, 0, &empty);
	orange_shell_layout_set_notification_center(&empty, 0);
	assert(empty.notification_center_card_count >= 3);
	assert(empty.notification_center_card_kinds[0] ==
		ORANGE_NOTIFICATION_CENTER_CARD_EMPTY);

	struct orange_rect clock = layout.status_items[ORANGE_STATUS_ITEM_CLOCK];
	hit = orange_shell_hit_test(&layout,
		clock.x + clock.width / 2,
		clock.y + clock.height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_ITEM);
	assert(hit.index == ORANGE_STATUS_ITEM_CLOCK);
}

static void test_launcher_full_layout_scrolls_all_apps(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 6;
	layout.launcher_category_active = 2;
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_FULL, 80);

	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width <= (int)(layout.width * 0.58));
	assert(layout.launcher_panel.height <= (int)(layout.height * 0.70));
	assert(layout.launcher_viewport.x - layout.launcher_panel.x <= 24);
	assert(layout.launcher_panel.y + layout.launcher_panel.height -
		(layout.launcher_viewport.y + layout.launcher_viewport.height) <= 4);
	assert(layout.launcher_grid_cols == ORANGE_LAUNCHER_COLS);
	assert(layout.launcher_section_count == 1);
	assert(layout.launcher_category_count == 6);
	assert(layout.launcher_category_active == 2);
	assert(layout.launcher_category_tabs[0].width > 0);
	assert(layout.launcher_viewport.y >
		layout.launcher_category_tabs[0].y +
		layout.launcher_category_tabs[0].height);
	assert(layout.launcher_grid_cell_count > 0);
	assert(layout.launcher_grid_indices[0] == 0);
	assert(layout.launcher_max_scroll > 0);
	assert(layout.launcher_scroll_track.width > 0);
	assert(layout.launcher_scroll_thumb.height > 0);

	struct orange_rect first = layout.launcher_grid_cells[0];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_APP);
	assert(hit.index == 0);

	struct orange_rect tab = layout.launcher_category_tabs[2];
	hit = orange_shell_hit_test(&layout,
		tab.x + tab.width / 2,
		tab.y + tab.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_CATEGORY);
	assert(hit.index == 2);

	struct orange_rect thumb = layout.launcher_scroll_thumb;
	hit = orange_shell_hit_test(&layout,
		thumb.x + thumb.width / 2,
		thumb.y + thumb.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_SCROLLBAR);

	layout.launcher_scroll_row = 2;
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 6;
	layout.launcher_category_active = 2;
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_FULL, 80);
	assert(layout.launcher_scroll_row == 2);
	assert(layout.launcher_grid_indices[0] == ORANGE_LAUNCHER_COLS * 2);
	first = layout.launcher_grid_cells[0];
	hit = orange_shell_hit_test(&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_APP);
	assert(hit.index == ORANGE_LAUNCHER_COLS * 2);
}

static void test_launcher_search_mode_transforms_to_overlay(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 6;
	layout.launcher_category_active = 0;
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY, 0);

	assert(layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width == layout.launcher_search_field.width);
	assert(layout.launcher_panel.height == layout.launcher_search_field.height);
	assert(layout.launcher_grid_cell_count == 0);
	assert(layout.launcher_viewport.width == 0);
	for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
		assert(layout.launcher_mode_buttons[i].width > 0);
		assert(layout.launcher_mode_buttons[i].x >
			layout.launcher_search_field.x + layout.launcher_search_field.width);
	}
	assert(layout.launcher_search_field.x > layout.width / 4);
	assert(layout.launcher_search_field.x < layout.width / 2);
	assert(layout.launcher_search_field.y > layout.height / 4);
	assert(layout.launcher_search_field.y < layout.height / 2);

	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.launcher_search_field.x + layout.launcher_search_field.width / 2,
		layout.launcher_search_field.y + layout.launcher_search_field.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_SEARCH);

	struct orange_rect mode = layout.launcher_mode_buttons[0];
	hit = orange_shell_hit_test(&layout,
		mode.x + mode.width / 2,
		mode.y + mode.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_MODE);
	assert(hit.index == 0);

	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 6;
	layout.launcher_category_active = 0;
	orange_shell_layout_set_launcher(&layout, true,
		ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY, 24);

	assert(layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY);
	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width == layout.launcher_search_field.width);
	assert(layout.launcher_viewport.width == 0);
	assert(layout.launcher_grid_cell_count == 0);
	assert(layout.launcher_mode_buttons[0].width > 0);
	assert(layout.launcher_mode_buttons[1].width > 0);

	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 6;
	layout.launcher_category_active = 0;
	orange_shell_layout_set_launcher(&layout, true,
		ORANGE_LAUNCHER_DISPLAY_FULL, 24);

	assert(layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_FULL);
	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width < layout.width / 2);
	assert(layout.launcher_panel.height < layout.height / 2);
	assert(layout.launcher_viewport.width > 0);
	assert(layout.launcher_grid_cell_count > 0);
	assert(layout.launcher_mode_buttons[0].width > 0);
	assert(layout.launcher_mode_buttons[1].width == 0);
	assert(layout.launcher_search_field.x >= layout.launcher_panel.x);
	assert(layout.launcher_search_field.y >= layout.launcher_panel.y);
	assert(layout.launcher_search_field.x + layout.launcher_search_field.width <=
		layout.launcher_panel.x + layout.launcher_panel.width);

	hit = orange_shell_hit_test(&layout,
		layout.launcher_search_field.x + layout.launcher_search_field.width / 2,
		layout.launcher_search_field.y + layout.launcher_search_field.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_SEARCH);

	struct orange_rect first = layout.launcher_grid_cells[0];
	hit = orange_shell_hit_test(&layout,
		first.x + first.width / 2,
		first.y + first.height / 2);
	assert(hit.kind == ORANGE_HIT_LAUNCHER_APP);
	assert(hit.index == 0);

	orange_shell_layout_compute(1440, 900, false, &config, 0, 0, &layout);
	layout.launcher_position_set = true;
	layout.launcher_x = 320;
	layout.launcher_y = 280;
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	orange_shell_layout_set_launcher(&layout, true,
		ORANGE_LAUNCHER_DISPLAY_FULL, 24);
	assert(layout.launcher_search_field.x == 320);
	assert(layout.launcher_search_field.y == 280);
}

static bool filter_contains_id(
		const struct orange_desktop_entry *entries,
		const int *indices,
		int count,
		const char *id) {
	for (int i = 0; i < count; i++) {
		if (strcmp(entries[indices[i]].id, id) == 0) {
			return true;
		}
	}
	return false;
}

static void test_launcher_category_filter(void) {
	struct orange_desktop_entry entries[4] = {
		{
			.id = "terminal",
			.name = "Terminal",
			.icon = "utilities-terminal",
			.exec = "terminal",
			.categories = "Utility;System;",
		},
		{
			.id = "contacts",
			.name = "Contacts",
			.icon = "x-office-address-book",
			.exec = "contacts",
			.categories = "Office;ContactManagement;",
		},
		{
			.id = "viewer",
			.name = "Image Viewer",
			.icon = "image-viewer",
			.exec = "viewer",
			.categories = "Graphics;Photography;",
		},
		{
			.id = "chat",
			.name = "Chat",
			.icon = "chat",
			.exec = "chat",
			.categories = "Network;Chat;",
		},
	};
	int indices[4];
	int count = orange_launcher_filter(entries, 4, "", "Utilities",
		indices, 4);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "terminal"));
	count = orange_launcher_filter(entries, 4, "", "Productivity",
		indices, 4);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "contacts"));
	count = orange_launcher_filter(entries, 4, "", "Media", indices, 4);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "viewer"));
	count = orange_launcher_filter(entries, 4, "cha", "Social", indices, 4);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "chat"));
}

static void test_small_width_dock_fits(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(900, 700, false, &config, 2, 0, &layout);
	assert(layout.dock.x >= 0);
	assert(layout.dock.x + layout.dock.width <= layout.width);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].x >= layout.dock.x);
		assert(layout.dock_items[i].x + layout.dock_items[i].width <=
			layout.dock.x + layout.dock.width);
	}
}

static void test_invalid_visible_dock_order_has_no_blank_slot(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_dock_count(&config);
	assert(dock_count > 1);
	config.dock_order[dock_count - 1] = dock_count;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440, 900, false, &config, 2, 0, &layout);
	assert(layout.dock_item_count == dock_count);
	for (int i = 0; i < layout.dock_item_count; i++) {
		assert(layout.dock_items[i].width > 0);
		assert(layout.dock_items[i].height > 0);
		assert(orange_dock_launcher_index(&layout, i) >= 0);
		assert(orange_dock_launcher_index(&layout, i) < dock_count);
	}
}

static void test_resolution_scaling(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout base;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &base);
	orange_shell_layout_compute(3840, 2160, false, &config, 2, 0, &large);

	assert(large.menu_bar.height > base.menu_bar.height);
	assert(large.calendar_widget.width > base.calendar_widget.width);
	assert(large.weather_widget.height > base.weather_widget.height);
	assert(large.dock_items[0].width > base.dock_items[0].width);
	assert(large.dock.x + large.dock.width <= large.width);
}

static void test_major_surfaces_scale_by_resolution(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout small;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1440, 900, true, &config, 0, 2, &small);
	orange_shell_layout_compute(2880, 1800, true, &config, 0, 2, &large);

	assert_roughly_double(large.menu_bar.height, small.menu_bar.height);
	assert_roughly_double(large.system_menu_button.width,
		small.system_menu_button.width);
	assert_roughly_double(large.system_menu_panel.width,
		small.system_menu_panel.width);
	assert_roughly_double(large.system_menu_items[0].height,
		small.system_menu_items[0].height);
	assert_roughly_double(large.status_items[ORANGE_STATUS_ITEM_WIFI].width,
		small.status_items[ORANGE_STATUS_ITEM_WIFI].width);
	assert_roughly_double(large.status_items[ORANGE_STATUS_ITEM_CLOCK].width,
		small.status_items[ORANGE_STATUS_ITEM_CLOCK].width);
	assert_roughly_double(large.desktop_items[0].width,
		small.desktop_items[0].width);
	assert_roughly_double(large.desktop_items[0].height,
		small.desktop_items[0].height);
	assert_roughly_double(large.dock.width, small.dock.width);
	assert_roughly_double(large.dock.height, small.dock.height);
	assert_roughly_double(large.dock_items[0].width,
		small.dock_items[0].width);
	orange_shell_layout_set_context_menu(&small,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450, NULL);
	orange_shell_layout_set_context_menu(&large,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 1440, 900, NULL);
	assert_roughly_double(large.context_menu_panel.width,
		small.context_menu_panel.width);
	assert_roughly_double(large.context_menu_panel.height,
		small.context_menu_panel.height);
	assert_roughly_double(large.context_menu_items[0].height,
		small.context_menu_items[0].height);

	orange_shell_layout_set_notification_center(&small, 1);
	orange_shell_layout_set_notification_center(&large, 1);
	assert_roughly_double(large.notification_center_panel.width,
		small.notification_center_panel.width);
	assert_roughly_double(large.notification_center_cards[0].height,
		small.notification_center_cards[0].height);
	assert_roughly_double(large.notification_center_edit_button.height,
		small.notification_center_edit_button.height);
}

static void test_desktop_background_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(2048, 1153, false, &config, 2, 0, &layout);
	/* click on empty desktop area below menu bar */
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, 500, 500);
	assert(hit.kind == ORANGE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_chrome_background_blocks_desktop_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);

	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.status_area.x - 4,
		layout.menu_bar.y + layout.menu_bar.height / 2);
	assert(hit.kind == ORANGE_HIT_MENU_BAR);
	assert(hit.index == -1);

	hit = orange_shell_hit_test(&layout,
		layout.dock.x + 4,
		layout.dock.y + layout.dock.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK);
	assert(hit.index == -1);

	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	hit = orange_shell_hit_test(&layout,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + 4);
	assert(hit.kind == ORANGE_HIT_DOCK);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	hit = orange_shell_hit_test(&layout,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + 4);
	assert(hit.kind == ORANGE_HIT_DOCK);
}

static void test_context_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 5);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	/* index 1 is "Show in Files" (first non-separator item after index 0) */
	struct orange_rect item = layout.context_menu_items[1];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_CONTEXT_MENU_ITEM);
	assert(hit.index == 1);
	assert(orange_menubar_context_menu_label(ORANGE_CONTEXT_MENU_DOCK, hit.index) != NULL);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 8);
	assert(layout.context_menu_separator[3]);
	assert(layout.context_menu_separator[6]);
	assert(layout.context_menu_separator[7]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 1),
		"Show All Windows") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 7), "Quit") == 0);
	assert(strcmp(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 7),
		"application-exit") == 0);

	orange_shell_layout_set_context_menu(&layout, ORANGE_CONTEXT_MENU_DOCK,
		layout.dock_item_count - 1, layout.width, layout.height, NULL);
	assert(layout.context_menu_panel.x >= 0);
	assert(layout.context_menu_panel.x + layout.context_menu_panel.width <=
		layout.width);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR);
	assert(layout.context_menu_item_count == 5);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	assert(layout.context_menu_separator[1]);
	assert(layout.context_menu_separator[4]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 0),
		"Turn Magnification On/Off") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 2),
		"Position on Screen") == 0);
	assert(strcmp(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 3),
		"window-minimize") == 0);
}

static void test_widget_hit_and_context_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	struct orange_rect widget = layout.widgets[0].rect;
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2);
	assert(hit.kind == ORANGE_HIT_WIDGET);
	assert(hit.index == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_WIDGET,
		hit.index,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2,
		NULL);
	assert(layout.context_menu_item_count == 5);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_WIDGET, 0), "Edit Widget") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_WIDGET, 4), "Remove Widget") == 0);
}

static void test_hidden_widget_not_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = false;
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	struct orange_rect widget = layout.widgets[0].rect;
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		widget.x + widget.width / 2,
		widget.y + widget.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_state state = {
		.system_menu_open = true,
		.hot_dock_index = 0,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
	};
	orange_shell_draw(pixels, width, height, stride, &state);

	size_t non_zero = 0;
	for (size_t i = 0; i < (size_t)width * (size_t)height; i++) {
		if (pixels[i] != 0) {
			non_zero++;
		}
	}
	assert(non_zero > (size_t)width * (size_t)height / 2);
	free(pixels);
}

static void test_dark_render_smoke(void) {
	int width = 960;
	int height = 540;
	int stride = width * 4;
	uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
	assert(pixels != NULL);

	struct orange_config config;
	orange_config_set_defaults(&config);
	config.appearance = ORANGE_APPEARANCE_DARK;

	struct orange_shell_state state = {
		.system_menu_open = false,
		.hot_dock_index = -1,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.now = 1757641980,
		.assets = NULL,
		.config = &config,
	};
	orange_shell_draw(pixels, width, height, stride, &state);

	size_t non_zero = 0;
	for (size_t i = 0; i < (size_t)width * (size_t)height; i++) {
		if (pixels[i] != 0) {
			non_zero++;
		}
	}
	assert(non_zero > (size_t)width * (size_t)height / 2);
	free(pixels);
}

static void test_widget_layer_exists(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920, 1080, false, &config, 2, 0, &layout);
	assert(layout.widget_count == 2);
	assert(layout.widgets[0].type == ORANGE_WIDGET_CALENDAR);
	assert(layout.widgets[1].type == ORANGE_WIDGET_WEATHER);
	assert(layout.widgets[0].visible);
	assert(layout.widgets[1].visible);
}

static void test_dock_bounce_offset(void) {
	struct orange_dock_bounce bounce = {
		.active = true,
		.launcher_idx = 3,
		.start_time = 1000000,
	};
	double scale = 1.0;

	double t0 = orange_dock_bounce_offset(&bounce, 1000000, scale);
	assert(t0 == 0.0);

	double t_mid = orange_dock_bounce_offset(&bounce,
		1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS / 6, scale);
	assert(t_mid < 0.0);
	struct orange_dock_bounce_displacement bottom =
		orange_dock_bounce_displacement(&bounce,
			1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS / 6,
			ORANGE_DOCK_POSITION_BOTTOM, scale);
	assert(bottom.x == 0.0);
	assert(bottom.y < 0.0);
	struct orange_dock_bounce_displacement left =
		orange_dock_bounce_displacement(&bounce,
			1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS / 6,
			ORANGE_DOCK_POSITION_LEFT, scale);
	assert(left.x > 0.0);
	assert(left.y == 0.0);
	struct orange_dock_bounce_displacement right =
		orange_dock_bounce_displacement(&bounce,
			1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS / 6,
			ORANGE_DOCK_POSITION_RIGHT, scale);
	assert(right.x < 0.0);
	assert(right.y == 0.0);

	double t_end = orange_dock_bounce_offset(&bounce,
		1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS, scale);
	assert(t_end == 0.0);

	double t_past = orange_dock_bounce_offset(&bounce,
		1000000 + ORANGE_DOCK_BOUNCE_DURATION_MS + 100, scale);
	assert(t_past == 0.0);

	bounce.active = false;
	double t_inactive = orange_dock_bounce_offset(&bounce, 1000000, scale);
	assert(t_inactive == 0.0);
}

int main(void) {
	test_dock_hit();
	test_reordered_dock_hit_resolves_launcher();
	test_dock_position_layout();
	test_default_dock_apps_have_fallback_commands();
	test_firefox_dock_matches_snap_desktop_entry();
	test_dock_trash_spacing_is_balanced();
	test_dock_remove_visible_compacts_aliases();
	test_dock_insert_from_launcher_clamps_before_trash();
	test_dock_reorder_visible_keeps_permanent_items_fixed();
	test_desktop_hit();
	test_desktop_requires_volumes();
	test_desktop_custom_position_snaps_to_grid();
	test_desktop_custom_position_clamps_to_visible_area();
	test_system_menu_hit();
	test_app_menu_layout_and_labels();
	test_status_area_hit_and_menu();
	test_notification_center_layout_and_hit();
	test_launcher_full_layout_scrolls_all_apps();
	test_launcher_search_mode_transforms_to_overlay();
	test_launcher_category_filter();
	test_small_width_dock_fits();
	test_invalid_visible_dock_order_has_no_blank_slot();
	test_resolution_scaling();
	test_major_surfaces_scale_by_resolution();
	test_context_menu_hit();
	test_widget_hit_and_context_menu();
	test_hidden_widget_not_hit();
	test_desktop_background_hit();
	test_render_smoke();
	test_dark_render_smoke();
	test_widget_layer_exists();
	test_dock_bounce_offset();
	test_chrome_background_blocks_desktop_hit();
	puts("shell layout tests passed");
	return 0;
}
