#include "orange/dock.h"
#include "orange/launcher.h"
#include "orange/menubar.h"
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

static bool rects_overlap(struct orange_rect a, struct orange_rect b) {
	return a.x < b.x + b.width &&
		a.x + a.width > b.x &&
		a.y < b.y + b.height &&
		a.y + a.height > b.y;
}

static void test_dock_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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

static void test_dock_auto_hide_layout(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_auto_hide = true;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	struct orange_rect visible_dock = layout.dock;
	struct orange_rect work = orange_shell_layout_work_area(&layout);
	assert(work.x == 0);
	assert(work.y == layout.menu_bar.height);
	assert(work.x + work.width == layout.width);
	assert(work.y + work.height == layout.dock.y);
	struct orange_rect centered_view = {
		work.x + 120,
		work.y + 80,
		640,
		420,
	};
	assert(!orange_shell_layout_dock_auto_hide_blocked_by_view(
		&layout, centered_view, false));
	assert(orange_shell_layout_dock_auto_hide_blocked_by_view(
		&layout, centered_view, true));
	assert(!orange_shell_layout_dock_auto_hide_blocked_by_view(
		&layout,
		(struct orange_rect){
			layout.width + 8,
			centered_view.y,
			centered_view.width,
			centered_view.height,
		},
		true));

	orange_shell_layout_apply_dock_auto_hide(&layout, &config, false, false);
	assert(layout.dock_auto_hidden);
	assert(layout.dock_auto_hide_blocked);
	assert(layout.dock.y >= layout.height);
	assert(layout.dock_reveal_zone.y >= layout.height - 12);
	work = orange_shell_layout_work_area(&layout);
	assert(work.y + work.height == layout.height);
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		layout.width / 2,
		layout.height - 1);
	assert(hit.kind == ORANGE_HIT_DOCK);
	hit = orange_shell_hit_test(
		&layout,
		visible_dock.x + visible_dock.width / 2,
		visible_dock.y + visible_dock.height / 2);
	assert(hit.kind != ORANGE_HIT_DOCK_ITEM);

	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_apply_dock_auto_hide_progress(&layout, &config,
		false, false, 0.5);
	assert(layout.dock_auto_hidden);
	assert(layout.dock_auto_hide_blocked);
	assert(layout.dock.y > visible_dock.y);
	assert(layout.dock.y < layout.height);

	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_apply_dock_auto_hide(&layout, &config, false, true);
	assert(!layout.dock_auto_hidden);
	assert(layout.dock_auto_hide_blocked);
	assert(layout.dock.y == visible_dock.y);
	work = orange_shell_layout_work_area(&layout);
	assert(work.y + work.height == layout.height);
	hit = orange_shell_hit_test(
		&layout,
		layout.dock_items[0].x + layout.dock_items[0].width / 2,
		layout.dock_items[0].y + layout.dock_items[0].height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_ITEM);

	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_apply_dock_auto_hide(&layout, &config, false, false);
	assert(layout.dock_auto_hidden);
	assert(layout.dock_reveal_zone.x == 0);
	hit = orange_shell_hit_test(
		&layout,
		0,
		layout.menu_bar.height + 40);
	assert(hit.kind == ORANGE_HIT_DOCK);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_apply_dock_auto_hide(&layout, &config, false, false);
	assert(layout.dock_auto_hidden);
	assert(layout.dock_reveal_zone.x + layout.dock_reveal_zone.width ==
		layout.width);
	hit = orange_shell_hit_test(
		&layout,
		layout.width - 1,
		layout.menu_bar.height + 40);
	assert(hit.kind == ORANGE_HIT_DOCK);
}

static void test_reordered_dock_hit_resolves_launcher(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_order[0] = 1;
	config.dock_order[1] = 0;

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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
	assert(layout.context_menu_panel.x -
		(layout.dock.x + layout.dock.width) >= 16);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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
	assert(layout.dock.x -
		(layout.context_menu_panel.x + layout.context_menu_panel.width) >= 16);
}

static void test_dock_visual_dirty_bounds_include_hover_labels(void) {
	const int width = 1440;
	const int height = 900;
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout layout;
	orange_shell_layout_compute(width, height, false, &config,
		2, 0, NULL, 0, &layout);
	struct orange_rect bounds =
		orange_dock_visual_dirty_bounds(&layout, width, height);
	assert(bounds.x == 0);
	assert(bounds.width == width);
	assert(bounds.y < layout.dock.y);
	assert(bounds.y + bounds.height == height);

	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	orange_shell_layout_compute(width, height, false, &config,
		2, 0, NULL, 0, &layout);
	bounds = orange_dock_visual_dirty_bounds(&layout, width, height);
	assert(bounds.x == 0);
	assert(bounds.width == width);
	assert(bounds.y == layout.menu_bar.y + layout.menu_bar.height);
	assert(bounds.y + bounds.height <= height);
	assert(bounds.y + bounds.height >= layout.dock.y + layout.dock.height);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(width, height, false, &config,
		2, 0, NULL, 0, &layout);
	bounds = orange_dock_visual_dirty_bounds(&layout, width, height);
	assert(bounds.x == 0);
	assert(bounds.width == width);
	assert(bounds.y == layout.menu_bar.y + layout.menu_bar.height);
	assert(bounds.y + bounds.height <= height);
	assert(bounds.y + bounds.height >= layout.dock.y + layout.dock.height);
}

static void test_dock_separator_drag_scale_math(void) {
	double bottom_grow = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_BOTTOM, 1.0, 500, 900, 500, 846);
	assert(bottom_grow > 1.29 && bottom_grow < 1.31);
	double bottom_shrink = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_BOTTOM, 1.0, 500, 900, 500, 954);
	assert(bottom_shrink > 0.69 && bottom_shrink < 0.71);
	double left_grow = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_LEFT, 1.0, 80, 500, 80, 554);
	assert(left_grow > 1.29 && left_grow < 1.31);
	double left_shrink = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_LEFT, 1.0, 80, 500, 80, 446);
	assert(left_shrink > 0.69 && left_shrink < 0.71);
	double right_grow = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_RIGHT, 1.0, 1840, 500, 1840, 554);
	assert(right_grow > 1.29 && right_grow < 1.31);
	double right_horizontal_drift = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_RIGHT, 1.0, 1840, 500, 1786, 500);
	assert(right_horizontal_drift > 0.99 && right_horizontal_drift < 1.01);
	double clamped_small = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_BOTTOM, 1.0, 0, 0, 0, 1000);
	assert(clamped_small > 0.59 && clamped_small < 0.61);
	double clamped_large = orange_shell_dock_scale_for_separator_drag(
		ORANGE_DOCK_POSITION_BOTTOM, 1.0, 0, 1000, 0, 0);
	assert(clamped_large > 1.99 && clamped_large < 2.01);
}

static void test_minimize_animation_rect_math(void) {
	double start = orange_shell_minimize_animation_ease(0.0);
	double mid = orange_shell_minimize_animation_ease(0.5);
	double end = orange_shell_minimize_animation_ease(1.0);
	assert(start == 0.0);
	assert(mid > 0.5 && mid < 0.9);
	assert(end == 1.0);

	struct orange_rect window = {100, 80, 800, 520};
	struct orange_rect dock = {460, 990, 82, 82};
	struct orange_rect first = orange_shell_minimize_animation_rect(
		window, dock, 0.0);
	struct orange_rect half = orange_shell_minimize_animation_rect(
		window, dock, 0.5);
	struct orange_rect last = orange_shell_minimize_animation_rect(
		window, dock, 1.0);
	assert(first.x == window.x);
	assert(first.y == window.y);
	assert(first.width == window.width);
	assert(first.height == window.height);
	assert(half.x > window.x && half.x < dock.x);
	assert(half.y > window.y && half.y < dock.y);
	assert(half.width < window.width && half.width > dock.width);
	assert(half.height < window.height && half.height > dock.height);
	assert(last.x == dock.x);
	assert(last.y == dock.y);
	assert(last.width == dock.width);
	assert(last.height == dock.height);
}

static void test_workspace_animation_offset_math(void) {
	double start = orange_shell_workspace_animation_ease(0.0);
	double mid = orange_shell_workspace_animation_ease(0.5);
	double end = orange_shell_workspace_animation_ease(1.0);
	assert(start == 0.0);
	assert(mid > 0.5 && mid < 0.9);
	assert(end == 1.0);

	int span = 1440;
	assert(orange_shell_workspace_animation_offset(span, 1, 0.0, false) == 0);
	assert(orange_shell_workspace_animation_offset(span, 1, 1.0, false) ==
		-span);
	assert(orange_shell_workspace_animation_offset(span, 1, 0.0, true) ==
		span);
	assert(orange_shell_workspace_animation_offset(span, 1, 1.0, true) == 0);
	assert(orange_shell_workspace_animation_offset(span, -1, 0.0, true) ==
		-span);
	assert(orange_shell_workspace_animation_offset(span, -1, 1.0, false) ==
		span);
	int incoming_mid = orange_shell_workspace_animation_offset(
		span, 1, 0.5, true);
	int outgoing_mid = orange_shell_workspace_animation_offset(
		span, 1, 0.5, false);
	assert(incoming_mid > 0 && incoming_mid < span / 2);
	assert(outgoing_mid < -span / 2 && outgoing_mid > -span);
}

static void test_dock_scale_resizes_separator_layout(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	struct orange_shell_layout normal;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &normal);

	config.dock_scale = 1.30;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &large);
	assert(large.dock.height > normal.dock.height);
	assert(large.dock.width > normal.dock.width);
	assert(large.dock_separator.height > normal.dock_separator.height);

	struct orange_shell_hit hit = orange_shell_hit_test(
		&large,
		large.dock_separator.x + large.dock_separator.width / 2,
		large.dock_separator.y + large.dock_separator.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_SEPARATOR);
}

static void test_dock_separator_menu_fits_minimize_detail(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440,900,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);

	struct orange_rect item = layout.context_menu_items[2];
	assert(item.width > 0);
	assert(item.width >= 184);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 2),
		"Minimize Using") == 0);
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
		if (strcmp(config.dock_apps[i], "org.gnome.Nautilus.desktop") == 0) {
			assert(strstr(command, "nautilus") != NULL);
			assert(strstr(command,
				"XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu") != NULL);
			assert(strstr(command,
				"env -u GTK_THEME -u GTK_ICON_THEME") != NULL);
		}
		if (strcmp(config.dock_apps[i], "__trash__") == 0) {
			assert(strstr(command, "nautilus trash:///") != NULL);
			assert(strstr(command, "gio open trash:///") != NULL);
			assert(strstr(command,
				"env -u GTK_THEME -u GTK_ICON_THEME") != NULL);
		}
	}
	const char *settings_command =
		orange_dock_builtin_command("org.gnome.Settings.desktop");
	assert(settings_command != NULL);
	const char *gnome_display =
		strstr(settings_command, "gnome-control-center display");
	const char *gnome_multitasking =
		strstr(settings_command, "gnome-control-center multitasking");
	const char *gnome_system =
		strstr(settings_command, "gnome-control-center system");
	assert(gnome_multitasking != NULL);
	assert(gnome_display != NULL);
	assert(gnome_system != NULL);
	assert(gnome_multitasking < gnome_display);
	assert(gnome_display < gnome_system);
	assert(strstr(settings_command, "gnome-control-center;") == NULL);
	assert(strstr(settings_command, "build/orange-settings") == NULL);
	assert(strstr(settings_command, "GSETTINGS_SCHEMA_DIR") == NULL);
	assert(strstr(settings_command,
		"XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu") != NULL);
	assert(strstr(settings_command,
		"env -u GTK_THEME -u GTK_ICON_THEME") != NULL);
	assert(strstr(settings_command, "NO_AT_BRIDGE=1") != NULL);
	assert(strstr(settings_command, "GSK_RENDERER=cairo") != NULL);
	assert(strstr(settings_command,
		"DBUS_SYSTEM_BUS_ADDRESS=") == NULL);
}

static void test_firefox_dock_matches_snap_desktop_entry(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	snprintf(config.dock_apps[2], sizeof(config.dock_apps[2]), "%s",
		"firefox_firefox");
	const struct orange_desktop_entry entries[] = {
		{
			.id = "firefox_firefox",
			.name = "Firefox Web Browser",
			.icon = "/snap/firefox/current/default256.png",
			.exec = "/home/samuel/.local/bin/firefox-snap-wrapper %u",
		},
	};

	assert(strcmp(config.dock_apps[2], "firefox_firefox") == 0);
	assert(strcmp(orange_dock_label(2, entries, 1, &config),
		"Firefox Web Browser") == 0);
	const char *command = orange_dock_command(2, entries, 1, &config);
	assert(command != NULL);
	assert(strstr(command, "/usr/bin/firefox") != NULL);
	assert(strstr(command, "firefox-snap-wrapper") == NULL);
}

static void test_dock_lookup_skips_stale_package_desktop_entry(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	snprintf(config.dock_apps[2], sizeof(config.dock_apps[2]), "%s",
		"sample_sample");
	const struct orange_desktop_entry entries[] = {
		{
			.id = "sample_sample",
			.name = "Sample",
			.icon = "/snap/sample/current/icon.png",
			.exec = "/home/samuel/.local/bin/sample-snap-wrapper",
			.snap_instance = "orange-missing-sample-snap",
			.snap_app = "orange-missing-sample-snap",
			.startup_wm_class = "sample_sample",
		},
		{
			.id = "sample",
			.name = "Sample",
			.icon = "sample",
			.exec = "sample %u",
			.startup_wm_class = "sample",
		},
	};

	assert(strcmp(orange_dock_label(2, entries, 2, &config),
		"Sample") == 0);
	const char *command = orange_dock_command(2, entries, 2, &config);
	assert(command != NULL);
	assert(strcmp(command, "sample %u") == 0);
}

static void test_default_dock_lookup_does_not_confuse_calculator_with_maps(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);

	int maps = orange_dock_config_find_app(&config,
		"org.gnome.Maps.desktop");
	int calculator = orange_dock_config_find_app(&config,
		"org.gnome.Calculator.desktop");
	assert(maps >= 0);
	assert(strcmp(config.dock_apps[maps], "org.gnome.Maps.desktop") == 0);
	assert(calculator == -1);
	assert(!orange_dock_config_contains_app(&config,
		"org.gnome.Calculator.desktop"));

	assert(orange_dock_config_insert_app(&config,
		"org.gnome.Calculator.desktop", -1));
	calculator = orange_dock_config_find_app(&config,
		"org.gnome.Calculator");
	assert(calculator >= 0);
	assert(strcmp(config.dock_apps[calculator],
		"org.gnome.Calculator.desktop") == 0);
	assert(calculator != maps);
}

static void test_dock_trash_spacing_is_balanced(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	int first_leading = layout.dock_items[0].x - layout.dock.x;
	int last = layout.dock_item_count - 1;
	int trailing = layout.dock.x + layout.dock.width -
		(layout.dock_items[last].x + layout.dock_items[last].width);
	assert(abs(first_leading - trailing) <= 1);

	snprintf(config.dock_apps[last], sizeof(config.dock_apps[last]), "%s", "");
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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

	orange_config_set_defaults(&config);
	original_count = orange_dock_count(&config);
	assert(orange_dock_config_insert_app(&config,
		"org.example.TempApp.desktop", -1));
	assert(orange_dock_count(&config) == original_count + 1);
	assert(strcmp(config.dock_apps[original_count - 1],
		"org.example.TempApp.desktop") == 0);
	assert(strcmp(config.dock_apps[original_count], "__trash__") == 0);
}

static void test_temporary_dock_apps_appear_before_trash(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_dock_count(&config);
	assert(dock_count > 2);
	assert(strcmp(config.dock_apps[dock_count - 1], "__trash__") == 0);

	char temporary[ORANGE_DOCK_MAX][128] = {{0}};
	snprintf(temporary[0], sizeof(temporary[0]), "%s",
		"org.example.TempApp.desktop");
	snprintf(temporary[1], sizeof(temporary[1]), "%s",
		"org.example.OtherApp.desktop");

	struct orange_shell_layout layout;
	orange_shell_layout_compute_with_dock_temporary(1920, 1080, false,
		&config, 2, 0, NULL, 0, 2, temporary, &layout);
	assert(layout.dock_item_count == dock_count + 2);

	int first_temp = dock_count - 1;
	assert(layout.dock_temporary[first_temp]);
	assert(layout.dock_temporary[first_temp + 1]);
	assert(!layout.dock_temporary[layout.dock_item_count - 1]);
	assert(layout.dock_launcher_indices[layout.dock_item_count - 1] >= 0);
	assert(strcmp(config.dock_apps[
		layout.dock_launcher_indices[layout.dock_item_count - 1]],
		"__trash__") == 0);
	assert(strcmp(layout.dock_temporary_app_ids[first_temp],
		"org.example.TempApp.desktop") == 0);
	assert(strcmp(layout.dock_temporary_app_ids[first_temp + 1],
		"org.example.OtherApp.desktop") == 0);
	assert(layout.dock_temporary_separator.width > 0);
	assert(layout.dock_temporary_separator.height > 0);
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		layout.dock_temporary_separator.x +
			layout.dock_temporary_separator.width / 2,
		layout.dock_temporary_separator.y +
			layout.dock_temporary_separator.height / 2);
	assert(hit.kind == ORANGE_HIT_DOCK_SEPARATOR);
	assert(hit.index == 1);
}

static void test_minimized_dock_screenshots_appear_left_of_trash(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	int dock_count = orange_dock_count(&config);
	assert(dock_count >= 2);
	assert(strcmp(config.dock_apps[dock_count - 1], "__trash__") == 0);

	char temporary[ORANGE_DOCK_MAX][128] = {{0}};
	snprintf(temporary[0], sizeof(temporary[0]), "%s",
		"org.example.TempApp.desktop");

	struct orange_shell_layout layout;
	orange_shell_layout_compute_with_dock_state(1920, 1080, false,
		&config, 2, 0, NULL, 0, 1, temporary, 2, &layout);
	assert(layout.dock_item_count == dock_count + 3);

	int first_special = dock_count - 1;
	assert(layout.dock_temporary[first_special]);
	assert(!layout.dock_minimized[first_special]);
	assert(layout.dock_minimized[first_special + 1]);
	assert(layout.dock_minimized[first_special + 2]);
	assert(layout.dock_minimized_indices[first_special + 1] == 0);
	assert(layout.dock_minimized_indices[first_special + 2] == 1);
	assert(!layout.dock_temporary[layout.dock_item_count - 1]);
	assert(!layout.dock_minimized[layout.dock_item_count - 1]);
	assert(layout.dock_launcher_indices[layout.dock_item_count - 1] >= 0);
	assert(strcmp(config.dock_apps[
		layout.dock_launcher_indices[layout.dock_item_count - 1]],
		"__trash__") == 0);
	assert(layout.dock_items[first_special + 2].x <
		layout.dock_items[layout.dock_item_count - 1].x);
	assert(layout.dock_separator.width > 0);
	assert(layout.dock_separator.x <
		layout.dock_items[first_special + 1].x);
}

static void test_minimized_dock_thumbnails_magnify_and_use_window_title(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_magnification = true;
	config.dock_magnification_scale = 2.0;

	struct orange_shell_layout layout;
	orange_shell_layout_compute_with_dock_state(1920, 1080, false,
		&config, 2, 0, NULL, 0, 0, NULL, 2, &layout);

	int minimized = -1;
	for (int i = 0; i < layout.dock_item_count; i++) {
		if (layout.dock_minimized[i]) {
			minimized = i;
			break;
		}
	}
	assert(minimized >= 0);
	int minimized_index = layout.dock_minimized_indices[minimized];
	assert(minimized_index >= 0 && minimized_index < ORANGE_DOCK_MAX);

	struct orange_shell_state state = {
		.hot_dock_index = minimized,
		.dock_pointer_x = layout.dock_items[minimized].x +
			layout.dock_items[minimized].width / 2,
		.dock_pointer_y = layout.dock_items[minimized].y +
			layout.dock_items[minimized].height / 2,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
		.dock_minimized_count = 2,
	};
	snprintf(state.dock_minimized_titles[minimized_index],
		sizeof(state.dock_minimized_titles[minimized_index]),
		"%s", "Quarterly Results");

	assert(strcmp(orange_dock_visible_label(&layout, &state, &config,
		minimized), "Quarterly Results") == 0);

	struct orange_rect thumb =
		orange_dock_minimized_thumbnail_rect(layout.dock_items[minimized]);
	int old_pad = layout.dock_items[minimized].width <
		layout.dock_items[minimized].height ?
		layout.dock_items[minimized].width / 12 :
		layout.dock_items[minimized].height / 12;
	if (old_pad < 4) {
		old_pad = 4;
	}
	struct orange_rect old_thumb = {
		layout.dock_items[minimized].x + old_pad,
		layout.dock_items[minimized].y + old_pad,
		layout.dock_items[minimized].width - old_pad * 2,
		layout.dock_items[minimized].height - old_pad * 2,
	};
	assert(thumb.width > old_thumb.width);
	assert(thumb.height > old_thumb.height);

	struct orange_dock_visual_item visual[ORANGE_DOCK_MAX];
	orange_dock_compute_visual_items(&layout, &state, &config, visual);
	assert(visual[minimized].scale > 1.5);
	assert(visual[minimized].rect.width >
		layout.dock_items[minimized].width);
	assert(visual[minimized].rect.height >
		layout.dock_items[minimized].height);
	assert(visual[minimized].rect.y <
		layout.dock_items[minimized].y);
}

static void test_temporary_dock_apps_are_session_scoped(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	char temporary[ORANGE_DOCK_MAX][128] = {{0}};
	int temporary_count = 0;

	assert(orange_dock_temporary_add(temporary, &temporary_count,
		"org.gnome.Calculator.desktop", &config));
	assert(temporary_count == 1);
	assert(orange_dock_temporary_contains(
		(const char (*)[128])temporary, temporary_count,
		"org.gnome.Calculator"));
	assert(!orange_dock_temporary_add(temporary, &temporary_count,
		"org.gnome.Calculator", &config));
	assert(temporary_count == 1);

	temporary_count = orange_dock_temporary_prune_config(
		temporary, temporary_count, &config);
	assert(temporary_count == 1);
	assert(strcmp(temporary[0], "org.gnome.Calculator.desktop") == 0);

	assert(orange_dock_config_insert_app(&config,
		"org.gnome.Calculator.desktop", -1));
	temporary_count = orange_dock_temporary_prune_config(
		temporary, temporary_count, &config);
	assert(temporary_count == 0);
	assert(temporary[0][0] == '\0');
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
	orange_shell_layout_compute(1920,1080,false,&config,0,2, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,0,0, NULL, 0, &layout);
	assert(layout.desktop_item_count == 0);
}

static void test_desktop_custom_position_snaps_to_grid(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,2, NULL, 0, &layout);
	assert(layout.desktop_item_count == 2);
	/* Items are placed in grid from right side */
	assert(layout.desktop_items[0].x >= 1700);
	assert(layout.desktop_items[0].y >= layout.menu_bar.height);
}

static void test_desktop_default_items_are_slightly_larger(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	assert(layout.desktop_item_count == 1);
	assert(layout.desktop_items[0].width >= 76);
	assert(layout.desktop_items[0].height >= 100);
}

static void test_desktop_label_rect_targets_rename_area(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[1] = {
		{.name = "Report.txt", .path = "/tmp/Report.txt"},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,0, files, 1, &layout);
	struct orange_rect item = layout.desktop_items[0];
	struct orange_rect label =
		orange_shell_desktop_label_rect(&layout, &config, 0);
	assert(label.width > item.width);
	assert(label.height > 0);
	assert(label.y > item.y);
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		label.x + label.width / 2,
		label.y + label.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 0);
}

static void test_desktop_image_highlight_area_hits_item(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[1] = {
		{
			.name = "Photo.png",
			.path = "/tmp/Photo.png",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,0, files, 1, &layout);
	struct orange_rect item = layout.desktop_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		item.x - 8,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 0);
	assert(layout.desktop_item_info[hit.index].kind == ORANGE_DESKTOP_ITEM_FILE);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, hit.index,
		item.x + item.width / 2, item.y + item.height / 2, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE);
	assert(layout.context_menu_item_count == 10);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 7), "Quick Look") == 0);
}

static void test_desktop_image_hit_wins_over_widget_overlap(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.calendar_widget_visible = true;
	config.weather_widget_visible = false;
	struct orange_file_info files[1] = {
		{
			.name = "Screenshot.png",
			.path = "/tmp/Screenshot.png",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,0, files, 1, &layout);
	assert(layout.calendar_widget.width > 0);
	layout.desktop_items[0] = layout.calendar_widget;
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.calendar_widget.x + layout.calendar_widget.width / 2,
		layout.calendar_widget.y + layout.calendar_widget.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 0);
	assert(layout.desktop_item_info[hit.index].kind == ORANGE_DESKTOP_ITEM_FILE);
}

static void test_desktop_folder_hit_wins_over_neighbor_padding(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[2] = {
		{
			.name = "Photo.png",
			.path = "/tmp/Photo.png",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
		{
			.name = "Projects",
			.path = "/tmp/Projects",
			.icon_name = "folder",
			.is_directory = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,0, files, 2, &layout);
	assert(layout.desktop_item_count == 2);
	layout.desktop_items[0] = (struct orange_rect){100, 100, 80, 100};
	layout.desktop_items[1] = (struct orange_rect){185, 100, 80, 100};
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, 190, 150);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
	assert(layout.desktop_item_info[hit.index].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[hit.index].index == 1);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, hit.index, 190, 150, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE);
	assert(layout.context_menu_item_count == 10);
}

static void test_desktop_folder_hit_after_context_menu_clear(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[2] = {
		{
			.name = "image.jpg",
			.path = "/tmp/image.jpg",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
		{
			.name = "TEST",
			.path = "/tmp/TEST",
			.icon_name = "folder",
			.is_directory = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440,900,false,&config,0,0, files, 2, &layout);
	assert(layout.desktop_item_count == 2);
	layout.desktop_items[0] = (struct orange_rect){1120, 40, 64, 84};
	layout.desktop_items[1] = (struct orange_rect){1265, 40, 64, 84};
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0,
		layout.desktop_items[1].x, layout.desktop_items[1].y, NULL);
	assert(layout.context_menu_panel.width > 0);
	assert(rects_overlap(layout.context_menu_panel, layout.desktop_items[1]));
	struct orange_shell_hit blocked = orange_shell_hit_test(&layout,
		layout.desktop_items[1].x + layout.desktop_items[1].width / 2,
		layout.desktop_items[1].y + layout.desktop_items[1].height / 2);
	assert(blocked.kind != ORANGE_HIT_DESKTOP_ITEM);

	orange_shell_layout_compute(1440,900,false,&config,0,0, files, 2, &layout);
	layout.desktop_items[0] = (struct orange_rect){1120, 40, 64, 84};
	layout.desktop_items[1] = (struct orange_rect){1265, 40, 64, 84};
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.desktop_items[1].x + layout.desktop_items[1].width / 2,
		layout.desktop_items[1].y + layout.desktop_items[1].height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 1);
	assert(layout.desktop_item_info[hit.index].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(files[layout.desktop_item_info[hit.index].index].is_directory);
}

static void test_desktop_folder_fresh_config_hit_and_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[2] = {
		{
			.name = "TEST",
			.path = "/tmp/TEST",
			.icon_name = "folder",
			.is_directory = true,
		},
		{
			.name = "image.jpg",
			.path = "/tmp/image.jpg",
			.icon_name = "image-x-generic",
			.is_image = true,
		},
	};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1440,900,false,&config,0,0, files, 2, &layout);
	assert(layout.desktop_item_count == 2);
	assert(layout.desktop_item_info[0].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(files[layout.desktop_item_info[0].index].is_directory);
	struct orange_rect folder = layout.desktop_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		folder.x + folder.width / 2,
		folder.y + folder.height / 2);
	assert(hit.kind == ORANGE_HIT_DESKTOP_ITEM);
	assert(hit.index == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, hit.index,
		folder.x + folder.width / 2, folder.y + folder.height / 2, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE);
	assert(layout.context_menu_item_count == 10);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0), "Open") == 0);
}

static void test_desktop_custom_position_preserves_dragged_coordinates(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 420, 260};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	struct orange_rect item = layout.desktop_items[0];
	assert(item.x == 420);
	assert(item.y == 260);
	assert(item.y >= layout.menu_bar.height);
	assert(item.y + item.height < layout.dock.y - 4);

	config.desktop_positions[0].x += 30;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	assert(layout.desktop_items[0].x == item.x + 30);
	assert(layout.desktop_items[0].y == item.y);
}

static void test_desktop_free_positions_separate_overlapping_highlights(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 420, 260};
	config.desktop_positions[1] =
		(struct orange_desktop_icon_position){true, 430, 260};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,2, NULL, 0, &layout);
	assert(layout.desktop_item_count == 2);
	struct orange_rect first =
		orange_shell_desktop_item_context_rect(&layout, 0);
	struct orange_rect second =
		orange_shell_desktop_item_context_rect(&layout, 1);
	assert(!rects_overlap(first, second));
	assert(abs(layout.desktop_items[1].x - 430) <=
		layout.desktop_items[1].width * 2);
	assert(abs(layout.desktop_items[1].y - 260) <=
		layout.desktop_items[1].height * 2);
}

static void test_desktop_duplicate_saved_positions_do_not_overlap(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_sort_by = ORANGE_DESKTOP_SORT_SNAP_TO_GRID;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 420, 260};
	config.desktop_positions[1] =
		(struct orange_desktop_icon_position){true, 420, 260};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,2, NULL, 0, &layout);
	assert(layout.desktop_item_count == 2);
	assert(!rects_overlap(layout.desktop_items[0], layout.desktop_items[1]));
}

static void test_desktop_grid_keeps_visible_spacing(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 420, 260};
	config.desktop_positions[1] =
		(struct orange_desktop_icon_position){true, 520, 260};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,2, NULL, 0, &layout);
	assert(layout.desktop_item_count == 2);
	int dx = abs(layout.desktop_items[0].x - layout.desktop_items[1].x);
	int dy = abs(layout.desktop_items[0].y - layout.desktop_items[1].y);
	assert(dx >= layout.desktop_items[0].width / 2 ||
		dy >= layout.desktop_items[0].height / 2);
}

static void test_desktop_grid_reserves_side_dock_space(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 1900, 360};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	struct orange_rect item = layout.desktop_items[0];
	assert(item.x + item.width < layout.dock.x - 4);

	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 0, 360};
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	item = layout.desktop_items[0];
	assert(item.x > layout.dock.x + layout.dock.width + 4);
}

static void test_desktop_grid_uses_available_horizontal_space(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,36, NULL, 0, &layout);
	assert(layout.desktop_item_count == 36);
	assert(layout.desktop_items[0].y > layout.menu_bar.height + 8);
	int row_step = layout.desktop_items[1].y - layout.desktop_items[0].y;
	assert(row_step > layout.desktop_items[0].height);
	assert(row_step <= layout.desktop_items[0].height +
		layout.desktop_items[0].height / 2);

	int first_col_x = layout.desktop_items[0].x;
	int next_col_x = first_col_x;
	bool has_left_edge_column = false;
	bool has_right_edge_column = false;
	for (int i = 1; i < layout.desktop_item_count; i++) {
		if (layout.desktop_items[i].x <
				first_col_x - layout.desktop_items[0].width / 2) {
			next_col_x = layout.desktop_items[i].x;
		}
		struct orange_rect highlight =
			orange_shell_desktop_item_context_rect(&layout, i);
		assert(highlight.x >= 4);
		assert(highlight.x + highlight.width <= layout.width - 4);
		if (highlight.x >= 4 && highlight.x <= 16) {
			has_left_edge_column = true;
		}
		if (highlight.x + highlight.width <= layout.width - 4 &&
				highlight.x + highlight.width >= layout.width - 16) {
			has_right_edge_column = true;
		}
	}
	struct orange_rect first_highlight =
		orange_shell_desktop_item_context_rect(&layout, 0);
	assert(first_highlight.x >= 4);
	assert(first_highlight.x + first_highlight.width <= layout.width - 4);
	if (first_highlight.x + first_highlight.width <= layout.width - 4 &&
			first_highlight.x + first_highlight.width >= layout.width - 16) {
		has_right_edge_column = true;
	}
	assert(next_col_x < first_col_x);
	assert(first_col_x - next_col_x >=
		layout.desktop_items[0].width * 2);
	assert(has_left_edge_column);
	assert(has_right_edge_column);
}

static void test_desktop_grid_reserves_magnified_bottom_dock_space(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_magnification = true;
	config.dock_magnification_scale = 2.0;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 1800, 50000};

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	assert(layout.desktop_item_count == 1);
	int hot = layout.dock_item_count / 2;
	struct orange_shell_state state = {
		.hot_dock_index = hot,
		.dock_pointer_x = layout.dock_items[hot].x +
			layout.dock_items[hot].width / 2,
		.dock_pointer_y = layout.dock_items[hot].y +
			layout.dock_items[hot].height / 2,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
	};
	struct orange_dock_visual_item visual[ORANGE_DOCK_MAX];
	orange_dock_compute_visual_items(&layout, &state, &config, visual);
	assert(layout.desktop_items[0].y + layout.desktop_items[0].height <=
		visual[hot].rect.y - 4);
}

static void test_desktop_grid_moves_for_larger_dock_scale(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_magnification = false;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 1800, 50000};

	struct orange_shell_layout normal;
	orange_shell_layout_compute(1920,600,false,&config,0,1, NULL, 0, &normal);

	config.dock_scale = 2.0;
	struct orange_shell_layout large;
	orange_shell_layout_compute(1920,600,false,&config,0,1, NULL, 0, &large);
	assert(large.dock.y < normal.dock.y);
	assert(large.desktop_items[0].y < normal.desktop_items[0].y);
	assert(large.desktop_items[0].y + large.desktop_items[0].height <=
		large.dock.y - 4);
}

static void test_desktop_grid_reserves_magnified_side_dock_space(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.dock_magnification = true;
	config.dock_magnification_scale = 2.0;
	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 50000, 360};

	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	assert(layout.desktop_item_count == 1);
	int hot = layout.dock_item_count / 2;
	struct orange_shell_state state = {
		.hot_dock_index = hot,
		.dock_pointer_x = layout.dock_items[hot].x +
			layout.dock_items[hot].width / 2,
		.dock_pointer_y = layout.dock_items[hot].y +
			layout.dock_items[hot].height / 2,
		.dock_drag_index = -1,
		.dock_drag_insert_before = -1,
	};
	struct orange_dock_visual_item visual[ORANGE_DOCK_MAX];
	orange_dock_compute_visual_items(&layout, &state, &config, visual);
	assert(layout.desktop_items[0].x + layout.desktop_items[0].width <=
		visual[hot].rect.x - 4);

	config.dock_position = ORANGE_DOCK_POSITION_LEFT;
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 0, 360};
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	hot = layout.dock_item_count / 2;
	state.hot_dock_index = hot;
	state.dock_pointer_x = layout.dock_items[hot].x +
		layout.dock_items[hot].width / 2;
	state.dock_pointer_y = layout.dock_items[hot].y +
		layout.dock_items[hot].height / 2;
	orange_dock_compute_visual_items(&layout, &state, &config, visual);
	assert(layout.desktop_items[0].x >=
		visual[hot].rect.x + visual[hot].rect.width + 4);
}

static void test_desktop_custom_position_clamps_to_visible_area(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	config.desktop_positions[0] =
		(struct orange_desktop_icon_position){true, 50000, 50000};
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,1, NULL, 0, &layout);
	struct orange_rect item = layout.desktop_items[0];
	assert(item.x >= 0);
	assert(item.y >= layout.menu_bar.height);
	assert(item.x + item.width <= layout.width);
	assert(item.y + item.height <= layout.dock.y);
}

static void test_desktop_sort_modes_order_items(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_file_info files[3] = {
		{.name = "Beta.txt", .is_directory = false},
		{.name = "Alpha", .is_directory = true},
		{.name = "Gamma.txt", .is_directory = false},
	};

	struct orange_shell_layout layout;
	config.desktop_sort_by = ORANGE_DESKTOP_SORT_NAME;
	orange_shell_layout_compute(1920,1080,false,&config,0,1,
		files, 3, &layout);
	assert(layout.desktop_item_count == 4);
	assert(layout.desktop_item_info[0].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[0].index == 1);
	assert(layout.desktop_item_info[1].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[1].index == 0);
	assert(layout.desktop_item_info[2].index == 2);
	assert(layout.desktop_item_info[3].kind == ORANGE_DESKTOP_ITEM_VOLUME);

	config.desktop_sort_by = ORANGE_DESKTOP_SORT_KIND;
	orange_shell_layout_compute(1920,1080,false,&config,0,1,
		files, 3, &layout);
	assert(layout.desktop_item_info[0].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[0].index == 1);
	assert(layout.desktop_item_info[1].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[2].kind == ORANGE_DESKTOP_ITEM_FILE);
	assert(layout.desktop_item_info[3].kind == ORANGE_DESKTOP_ITEM_VOLUME);
}

static void test_system_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout closed;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &closed);
	struct orange_shell_hit hit = orange_shell_hit_test(&closed, 36, 16);
	assert(hit.kind == ORANGE_HIT_SYSTEM_MENU);
	hit = orange_shell_hit_test(&closed,
		closed.app_menu_button.x + closed.app_menu_button.width / 2,
		closed.app_menu_button.y + closed.app_menu_button.height / 2);
	assert(hit.kind == ORANGE_HIT_APP_MENU);

	struct orange_shell_layout open;
	orange_shell_layout_compute(1920,1080,true,&config,2,0, NULL, 0, &open);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_set_app_menu_tabs(&layout, "Photoshop", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_EDIT].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_VIEW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_GO].width == 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width == 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_HELP].width > 0);

	struct orange_shell_state default_state = {0};
	assert(strcmp(orange_menubar_active_app_label(&default_state), "Files") == 0);
	struct orange_shell_state active_state = {0};
	snprintf(active_state.active_app_label,
		sizeof(active_state.active_app_label), "%s", "Terminal");
	assert(strcmp(orange_menubar_active_app_label(&active_state), "Terminal") == 0);

	struct orange_app_menu_model stale_native_menu = {
		.available = true,
		.native = true,
		.tab_count = ORANGE_APP_MENU_TAB_COUNT,
	};
	snprintf(stale_native_menu.tab_labels[ORANGE_APP_MENU_TAB_FILE],
		sizeof(stale_native_menu.tab_labels[ORANGE_APP_MENU_TAB_FILE]),
		"%s", "Native File");
	orange_shell_layout_set_app_menu_tabs(&layout, "Files",
		&stale_native_menu);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_EDIT].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_VIEW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_GO].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width == 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_HELP].width > 0);
	orange_shell_layout_set_app_menu_tabs(&layout, "Photoshop", NULL);

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
	assert(file_tab.width > 0);

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

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_EDIT, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_EDIT);
	assert(layout.context_menu_item_count == 7);

	orange_shell_layout_set_app_menu_tabs(&layout, "Files", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_EDIT].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_VIEW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_GO].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width == 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_HELP].width > 0);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_FILE, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_FILE);
	assert(layout.context_menu_item_count == 7);
	assert(layout.context_menu_panel.x == layout.app_menu_items[
		ORANGE_APP_MENU_TAB_FILE].x);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_FILE, 1), "Open...") == 0);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_EDIT, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_EDIT);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_EDIT, 3), "Copy") == 0);

	orange_shell_layout_set_app_menu_tabs(&layout, "Firefox", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width > 0);
	int short_firefox_width =
		layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width;
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_FILE].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_EDIT].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_VIEW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_GO].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_WINDOW].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_TOOLS].width > 0);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_HELP].width > 0);
	assert(strcmp(orange_menubar_app_menu_tab_label(
		NULL, ORANGE_APP_MENU_TAB_GO, "Firefox"), "History") == 0);
	assert(strcmp(orange_menubar_app_menu_tab_label(
		NULL, ORANGE_APP_MENU_TAB_WINDOW, "Firefox"), "Bookmarks") == 0);

	orange_shell_layout_set_app_menu_tabs(&layout,
		"Firefox Web Browser", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
	assert(layout.app_menu_items[ORANGE_APP_MENU_TAB_APP].width >
		short_firefox_width);
	assert(strcmp(orange_menubar_app_menu_tab_label(
		NULL, ORANGE_APP_MENU_TAB_APP,
		"Firefox Web Browser"), "Firefox Web Browser") == 0);
	assert(strcmp(orange_menubar_app_menu_tab_label(
		NULL, ORANGE_APP_MENU_TAB_GO,
		"Firefox Web Browser"), "History") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_HISTORY, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_HISTORY);
	assert(layout.context_menu_item_count == 5);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_APP_HISTORY, 0), "Back") == 0);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_BOOKMARKS, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_BOOKMARKS);
	assert(layout.context_menu_item_count == 4);

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
	assert(strcmp(orange_menubar_app_menu_tab_label(
		&native_menu, ORANGE_APP_MENU_TAB_APP, "Firefox"), "Firefox") == 0);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	struct orange_rect clock =
		layout.status_items[ORANGE_STATUS_ITEM_CLOCK];
	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		clock.x + clock.width / 2,
		clock.y + clock.height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_ITEM);
	assert(hit.index == ORANGE_STATUS_ITEM_CLOCK);

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

static void test_status_notifier_items_layout_and_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	int original_status_x = layout.status_area.x;

	struct orange_status_notifier_item items[2] = {
		{
			.id = ":1.30/StatusNotifierItem",
			.icon_name = "network-vpn",
			.status = "Active",
		},
		{
			.id = ":1.31/StatusNotifierItem",
			.attention_icon_name = "dialog-warning",
			.status = "NeedsAttention",
		},
	};
	orange_shell_layout_set_status_notifier_items(&layout, items, 2);

	assert(layout.status_notifier_item_count == 2);
	assert(layout.status_notifier_items[0].x < original_status_x);
	assert(layout.status_notifier_items[0].width > 0);
	assert(layout.status_notifier_items[1].x >
		layout.status_notifier_items[0].x);
	assert(layout.status_notifier_items[1].x +
		layout.status_notifier_items[1].width <
		layout.status_items[ORANGE_STATUS_ITEM_WIFI].x);
	assert(layout.status_area.x == layout.status_notifier_items[0].x);

	struct orange_shell_hit hit = orange_shell_hit_test(&layout,
		layout.status_notifier_items[1].x +
			layout.status_notifier_items[1].width / 2,
		layout.status_notifier_items[1].y +
			layout.status_notifier_items[1].height / 2);
	assert(hit.kind == ORANGE_HIT_TRAY_ITEM);
	assert(hit.index == 1);

	hit = orange_shell_hit_test(&layout,
		layout.status_items[ORANGE_STATUS_ITEM_WIFI].x +
			layout.status_items[ORANGE_STATUS_ITEM_WIFI].width / 2,
		layout.status_items[ORANGE_STATUS_ITEM_WIFI].y +
			layout.status_items[ORANGE_STATUS_ITEM_WIFI].height / 2);
	assert(hit.kind == ORANGE_HIT_STATUS_ITEM);
	assert(hit.index == ORANGE_STATUS_ITEM_WIFI);

	orange_shell_layout_set_app_menu_tabs(&layout, "Terminal", NULL);
	assert_app_menu_tabs_do_not_overlap(&layout);
}

static void test_modern_menu_affordance_metadata(void) {
	assert(!orange_menubar_context_menu_uses_icons(ORANGE_CONTEXT_MENU_APP));
	assert(!orange_menubar_context_menu_uses_icons(ORANGE_CONTEXT_MENU_DOCK));
	assert(!orange_menubar_context_menu_uses_icons(
		ORANGE_CONTEXT_MENU_DOCK_LAUNCHER));
	assert(!orange_menubar_context_menu_uses_icons(
		ORANGE_CONTEXT_MENU_DOCK_TRASH));
	assert(!orange_menubar_context_menu_uses_icons(ORANGE_CONTEXT_MENU_DESKTOP));
	assert(!orange_menubar_context_menu_uses_icons(
		ORANGE_CONTEXT_MENU_DESKTOP_ICON));
	assert(orange_menubar_context_menu_uses_icons(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH));
	assert(strcmp(orange_menubar_menu_label(1), "System Settings...") == 0);
	assert(strcmp(orange_menubar_menu_shortcut_label(4), "Ctrl+Alt+Esc") == 0);
	assert(!orange_menubar_menu_has_submenu(3));

	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_APP_FILE, 1), "Ctrl+O") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_APP_EDIT, 4), "Ctrl+V") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 0), "Shift+Ctrl+N") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 1), "Ctrl+V") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 2), "Ctrl+I") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 8), "Ctrl+J") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP_ICON, 8), "Ctrl+Del") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 3), "Ctrl+C") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DOCK_TRASH, 1), "Shift+Ctrl+Del") == 0);
	assert(!orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DESKTOP, 3));
	assert(!orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DESKTOP_ICON, 7));
	assert(orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 2));
	assert(orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 3));
}

static void test_native_app_menu_separators_are_preserved(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);

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
	native_menu.item_counts[ORANGE_APP_MENU_TAB_FILE] = 3;
	snprintf(native_menu.items[ORANGE_APP_MENU_TAB_FILE][0].label,
		sizeof(native_menu.items[ORANGE_APP_MENU_TAB_FILE][0].label),
		"%s", "Native Open");
	native_menu.items[ORANGE_APP_MENU_TAB_FILE][0].enabled = true;
	native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].separator = true;
	snprintf(native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].label,
		sizeof(native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].label),
		"%s", "Native Save");
	native_menu.items[ORANGE_APP_MENU_TAB_FILE][1].enabled = true;
	snprintf(native_menu.items[ORANGE_APP_MENU_TAB_FILE][2].label,
		sizeof(native_menu.items[ORANGE_APP_MENU_TAB_FILE][2].label),
		"%s", "Native Close");
	native_menu.items[ORANGE_APP_MENU_TAB_FILE][2].enabled = true;

	orange_shell_layout_set_app_menu_tabs(&layout, "NativeApp", &native_menu);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_APP_FILE, -1, 0, 0, &native_menu);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_APP_FILE);
	assert(layout.context_menu_item_count == 3);
	assert(!layout.context_menu_separator[0]);
	assert(layout.context_menu_separator[1]);
	assert(!layout.context_menu_separator[2]);
}

static void test_notification_center_layout_and_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,0,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,0,0, NULL, 0, &empty);
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
	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 8;
	layout.launcher_category_active = 2;
	orange_shell_layout_set_launcher(&layout, false,
		ORANGE_LAUNCHER_DISPLAY_FULL, 80);

	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width >= (int)(layout.width * 0.58));
	assert(layout.launcher_panel.width <= (int)(layout.width * 0.72));
	assert(layout.launcher_panel.width <= layout.width - 24);
	assert(layout.launcher_panel.height >= (int)(layout.height * 0.38));
	assert(layout.launcher_panel.height <= (int)(layout.height * 0.56));
	assert(layout.launcher_search_field.y >= (int)(layout.height * 0.23));
	assert(layout.launcher_search_field.y <= (int)(layout.height * 0.27));
	assert(layout.launcher_panel.y >= (int)(layout.height * 0.29));
	assert(layout.launcher_panel.y <= (int)(layout.height * 0.36));
	assert(layout.launcher_search_field.y + layout.launcher_search_field.height <
		layout.launcher_panel.y);
	assert(abs((layout.launcher_search_field.x +
			layout.launcher_search_field.width / 2) -
			layout.width / 2) <= 2);
	assert(layout.launcher_viewport.x > layout.launcher_panel.x);
	assert(layout.launcher_viewport.x - layout.launcher_panel.x <= 40);
	assert(layout.launcher_panel.y + layout.launcher_panel.height -
		(layout.launcher_viewport.y + layout.launcher_viewport.height) <= 24);
	assert(layout.launcher_grid_cols == ORANGE_LAUNCHER_COLS);
	assert(layout.launcher_grid_cols == 6);
	assert(layout.launcher_section_count == 1);
	assert(layout.launcher_category_count == 8);
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
	layout.launcher_category_count = 8;
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
	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 8;
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

	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 8;
	layout.launcher_category_active = 0;
	orange_shell_layout_set_launcher(&layout, true,
		ORANGE_LAUNCHER_DISPLAY_SEARCH_ONLY, 24);

	assert(layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_FULL);
	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width > layout.launcher_search_field.width);
	assert(layout.launcher_viewport.width > 0);
	assert(layout.launcher_grid_cell_count > 0);
	assert(layout.launcher_list_results);
	assert(layout.launcher_mode_buttons[0].width > 0);
	assert(layout.launcher_mode_buttons[1].width > 0);

	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &layout);
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 8;
	layout.launcher_category_active = 0;
	orange_shell_layout_set_launcher(&layout, true,
		ORANGE_LAUNCHER_DISPLAY_FULL, 24);

	assert(layout.launcher_display_mode == ORANGE_LAUNCHER_DISPLAY_FULL);
	assert(layout.launcher_panel.width > 0);
	assert(layout.launcher_panel.width >= (int)(layout.width * 0.58));
	assert(layout.launcher_panel.width <= (int)(layout.width * 0.72));
	assert(layout.launcher_panel.height >= (int)(layout.height * 0.35));
	assert(layout.launcher_panel.height <= (int)(layout.height * 0.56));
	assert(layout.launcher_viewport.width > 0);
	assert(layout.launcher_grid_cell_count > 0);
	assert(layout.launcher_list_results);
	assert(layout.launcher_mode_buttons[0].width > 0);
	assert(layout.launcher_mode_buttons[1].width > 0);
	assert(layout.launcher_mode_buttons[2].width > 0);
	assert(layout.launcher_mode_buttons[3].width > 0);
	assert(layout.launcher_search_field.y + layout.launcher_search_field.height <
		layout.launcher_panel.y);
	assert(abs((layout.launcher_search_field.x +
			layout.launcher_search_field.width / 2) -
			layout.width / 2) <= 2);

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

	orange_shell_layout_compute(1440,900,false,&config,0,0, NULL, 0, &layout);
	layout.launcher_position_set = true;
	layout.launcher_x = 320;
	layout.launcher_y = 280;
	layout.launcher_current_mode = ORANGE_LAUNCHER_MODE_APPS;
	layout.launcher_category_count = 8;
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

static bool results_contain_kind_label(
		const struct orange_launcher_result *results,
		int count,
		enum orange_launcher_result_kind kind,
		const char *label) {
	for (int i = 0; i < count; i++) {
		if (results[i].kind == kind &&
				strcmp(results[i].label, label) == 0) {
			return true;
		}
	}
	return false;
}

static bool results_contain_action(
		const struct orange_launcher_result *results,
		int count,
		const char *action) {
	for (int i = 0; i < count; i++) {
		if (results[i].kind == ORANGE_LAUNCHER_RESULT_ACTION &&
				strcmp(results[i].action, action) == 0) {
			return true;
		}
	}
	return false;
}

static void test_launcher_spotlight_results(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_desktop_entry entries[2] = {
		{
			.id = "org.gnome.Calculator.desktop",
			.name = "Calculator",
			.icon = "accessories-calculator",
			.exec = "gnome-calculator",
			.categories = "Utility;",
		},
		{
			.id = "org.example.Notes.desktop",
			.name = "Notes",
			.icon = "accessories-text-editor",
			.exec = "notes",
			.categories = "Office;",
		},
	};
	struct orange_file_info files[2] = {
		{
			.name = "Budget.xlsx",
			.path = "/home/example/Desktop/Budget.xlsx",
			.icon_name = "x-office-spreadsheet",
			.is_directory = false,
			.is_image = false,
		},
		{
			.name = "Screenshots",
			.path = "/home/example/Desktop/Screenshots",
			.icon_name = "folder-pictures",
			.is_directory = true,
			.is_image = false,
		},
	};
	struct orange_launcher_result results[16];
	int count = orange_launcher_build_results(entries, 2, files, 2,
		"calc", ORANGE_LAUNCHER_MODE_APPS, NULL, &config, NULL, 0,
		results, 16);
	assert(results_contain_kind_label(results, count,
		ORANGE_LAUNCHER_RESULT_APP, "Calculator"));
	assert(results_contain_kind_label(results, count,
		ORANGE_LAUNCHER_RESULT_WEB, "Search Web for \"calc\""));

	count = orange_launcher_build_results(entries, 2, files, 2,
		"budget", ORANGE_LAUNCHER_MODE_APPS, NULL, &config, NULL, 0,
		results, 16);
	assert(results_contain_kind_label(results, count,
		ORANGE_LAUNCHER_RESULT_FILE, "Budget.xlsx"));
	assert(results_contain_kind_label(results, count,
		ORANGE_LAUNCHER_RESULT_WEB, "Search Web for \"budget\""));

	count = orange_launcher_build_results(entries, 2, files, 2,
		"terminal", ORANGE_LAUNCHER_MODE_APPS, NULL, &config, NULL, 0,
		results, 16);
	assert(results_contain_action(results, count, "open-terminal"));

	count = orange_launcher_build_results(entries, 2, files, 2,
		"", ORANGE_LAUNCHER_MODE_FILES, NULL, &config, NULL, 0,
		results, 16);
	assert(count == 2);
	assert(results[0].kind == ORANGE_LAUNCHER_RESULT_FILE);
	assert(results[1].kind == ORANGE_LAUNCHER_RESULT_FILE);

	count = orange_launcher_build_results(entries, 2, files, 2,
		"", ORANGE_LAUNCHER_MODE_ACTIONS, NULL, &config, NULL, 0,
		results, 16);
	assert(count > 0);
	assert(results_contain_action(results, count, "open-terminal"));
	assert(results_contain_action(results, count, "open-settings"));
}

static void test_launcher_category_filter(void) {
	struct orange_desktop_entry entries[7] = {
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
			.categories = "Photography;Viewer;",
		},
		{
			.id = "designer",
			.name = "Designer",
			.icon = "applications-graphics",
			.exec = "designer",
			.categories = "Art;Design;",
		},
		{
			.id = "chess",
			.name = "Chess",
			.icon = "gnome-chess",
			.exec = "chess",
			.categories = "Game;",
		},
		{
			.id = "chat",
			.name = "Chat",
			.icon = "chat",
			.exec = "chat",
			.categories = "Network;Chat;",
		},
		{
			.id = "books",
			.name = "Books",
			.icon = "accessories-dictionary",
			.exec = "books",
			.categories = "Education;Literature;",
		},
	};
	int indices[7];
	int count = orange_launcher_filter(entries, 7, "", "Utilities",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "terminal"));
	count = orange_launcher_filter(entries, 7, "", "Productivity & Finance",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "contacts"));
	count = orange_launcher_filter(entries, 7, "", "Photo & Video",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "viewer"));
	count = orange_launcher_filter(entries, 7, "", "Creativity",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "designer"));
	count = orange_launcher_filter(entries, 7, "", "Entertainment",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "chess"));
	count = orange_launcher_filter(entries, 7, "cha", "Social", indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "chat"));
	count = orange_launcher_filter(entries, 7, "", "Information & Reading",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count, "books"));
}

static void test_launcher_hides_desktop_entries_gnome_would_hide(void) {
	const char *previous_desktop = getenv("XDG_CURRENT_DESKTOP");
	char previous_buf[512];
	bool had_previous = previous_desktop != NULL;
	if (had_previous) {
		snprintf(previous_buf, sizeof(previous_buf), "%s", previous_desktop);
	}
	assert(setenv("XDG_CURRENT_DESKTOP", "GNOME:orange", 1) == 0);

	struct orange_desktop_entry entries[7] = {
		{
			.id = "org.example.Editor.desktop",
			.name = "Example Editor",
			.icon = "accessories-text-editor",
			.exec = "example-editor",
			.categories = "Utility;",
		},
		{
			.id = "org.example.MimeHelper.desktop",
			.name = "MIME Helper",
			.icon = "helper",
			.exec = "mime-helper",
			.categories = "Utility;",
			.no_display = true,
		},
		{
			.id = "org.example.Hidden.desktop",
			.name = "Hidden Helper",
			.icon = "helper",
			.exec = "hidden-helper",
			.categories = "Utility;",
			.hidden = true,
		},
		{
			.id = "org.example.KdeOnly.desktop",
			.name = "KDE Only",
			.icon = "helper",
			.exec = "kde-only",
			.categories = "Utility;",
			.only_show_in = "KDE;",
		},
		{
			.id = "org.example.NotGnome.desktop",
			.name = "Not GNOME",
			.icon = "helper",
			.exec = "not-gnome",
			.categories = "Utility;",
			.not_show_in = "GNOME;",
		},
		{
			.id = "org.example.TryMissing.desktop",
			.name = "Try Missing",
			.icon = "helper",
			.exec = "try-missing",
			.categories = "Utility;",
			.try_exec = "/nonexistent-orange-helper",
		},
		{
			.id = "stale_snap",
			.name = "Stale Snap",
			.icon = "helper",
			.exec = "stale-snap",
			.categories = "Utility;",
			.snap_instance = "orange-missing-launcher-snap",
			.snap_app = "orange-missing-launcher-snap",
		},
	};
	int indices[7];
	int count = orange_launcher_filter(entries, 7, "", "Utilities",
		indices, 7);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count,
		"org.example.Editor.desktop"));

	if (had_previous) {
		assert(setenv("XDG_CURRENT_DESKTOP", previous_buf, 1) == 0);
	} else {
		assert(unsetenv("XDG_CURRENT_DESKTOP") == 0);
	}
}

static void test_launcher_filters_apps_already_in_dock(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	char temporary[ORANGE_DOCK_MAX][128] = {{0}};
	int temporary_count = 0;
	assert(orange_dock_temporary_add(temporary, &temporary_count,
		"org.gnome.Calculator.desktop", &config));

	struct orange_desktop_entry entries[3] = {
		{
			.id = "org.gnome.Maps.desktop",
			.name = "Maps",
			.icon = "org.gnome.Maps",
			.exec = "gnome-maps",
			.categories = "Utility;",
		},
		{
			.id = "org.gnome.Calculator.desktop",
			.name = "Calculator",
			.icon = "accessories-calculator",
			.exec = "gnome-calculator",
			.categories = "Utility;",
		},
		{
			.id = "org.example.Editor.desktop",
			.name = "Example Editor",
			.icon = "accessories-text-editor",
			.exec = "example-editor",
			.categories = "Utility;",
		},
	};
	int indices[3];
	int count = orange_launcher_filter_available(entries, 3, "",
		"Utilities", &config, temporary, temporary_count, indices, 3);
	assert(count == 1);
	assert(filter_contains_id(entries, indices, count,
		"org.example.Editor.desktop"));

	count = orange_launcher_filter_available(entries, 3, "map",
		"Utilities", &config, temporary, temporary_count, indices, 3);
	assert(count == 0);
	count = orange_launcher_filter_available(entries, 3, "calc",
		"Utilities", &config, temporary, temporary_count, indices, 3);
	assert(count == 0);

	orange_config_set_defaults(&config);
	snprintf(config.dock_apps[2], sizeof(config.dock_apps[2]), "%s",
		"sample_sample");
	struct orange_desktop_entry replacement_entries[1] = {
		{
			.id = "sample",
			.name = "Sample",
			.icon = "sample",
			.exec = "sample",
			.categories = "Utility;",
		},
	};
	count = orange_launcher_filter_available(replacement_entries, 1, "",
		"Utilities", &config, NULL, 0, indices, 3);
	assert(count == 0);
}

static void test_small_width_dock_fits(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(900,700,false,&config,2,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1440,900,false,&config,2,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &base);
	orange_shell_layout_compute(3840,2160,false,&config,2,0, NULL, 0, &large);

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
	orange_shell_layout_compute(1440,900,true,&config,0,2, NULL, 0, &small);
	orange_shell_layout_compute(2880,1800,true,&config,0,2, NULL, 0, &large);

	assert_roughly_double(large.menu_bar.height, small.menu_bar.height);
	assert_roughly_double(large.system_menu_button.width,
		small.system_menu_button.width);
	assert(large.system_menu_panel.width > small.system_menu_panel.width);
	assert(abs(large.system_menu_panel.width -
		small.system_menu_panel.width * 2) <= 16);
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
	assert(large.context_menu_panel.width > small.context_menu_panel.width);
	assert(abs(large.context_menu_panel.width -
		small.context_menu_panel.width * 2) <= 16);
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

static void test_menu_surfaces_scale_with_ui_scale(void) {
	const double ui_scales[] = {1.0, 1.25, 1.50, 1.75, 2.00};
	int previous_system_h = 0;
	int previous_desktop_context_h = 0;
	int previous_app_dropdown_h = 0;
	int previous_dock_menu_h = 0;
	int previous_dock_submenu_h = 0;

	for (size_t i = 0; i < sizeof(ui_scales) / sizeof(ui_scales[0]); i++) {
		struct orange_config config;
		orange_config_set_defaults(&config);
		orange_config_apply_ui_scale(&config, ui_scales[i]);

		struct orange_shell_layout layout;
		orange_shell_layout_compute(1440, 900, true, &config,
			2, 0, NULL, 0, &layout);
		double expected_menu_scale = 0.5 * ui_scales[i];
		assert(fabs(layout.menu_scale - expected_menu_scale) < 0.001);
		assert(layout.system_menu_item_count > 0);
		int system_h = layout.system_menu_items[0].height;

		orange_shell_layout_set_context_menu(&layout,
			ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450, NULL);
		assert(layout.context_menu_item_count > 0);
		int desktop_context_h = layout.context_menu_items[0].height;

		orange_shell_layout_set_context_menu(&layout,
			ORANGE_CONTEXT_MENU_APP_FILE, -1, 0, 0, NULL);
		assert(layout.context_menu_item_count > 0);
		int app_dropdown_h = layout.context_menu_items[0].height;

		orange_shell_layout_set_context_menu(&layout,
			ORANGE_CONTEXT_MENU_DOCK, 0, 0, 0, NULL);
		assert(layout.context_menu_item_count > 0);
		int dock_menu_h = layout.context_menu_items[0].height;

		layout.dock_minimize_submenu_open = true;
		orange_shell_layout_set_context_menu(&layout,
			ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
		assert(layout.context_submenu_item_count > 0);
		int dock_submenu_h = layout.context_submenu_items[0].height;

		if (i > 0) {
			assert(system_h > previous_system_h);
			assert(desktop_context_h > previous_desktop_context_h);
			assert(app_dropdown_h > previous_app_dropdown_h);
			assert(dock_menu_h > previous_dock_menu_h);
			assert(dock_submenu_h > previous_dock_submenu_h);
		}

		previous_system_h = system_h;
		previous_desktop_context_h = desktop_context_h;
		previous_app_dropdown_h = app_dropdown_h;
		previous_dock_menu_h = dock_menu_h;
		previous_dock_submenu_h = dock_submenu_h;
	}
}

static void test_desktop_background_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(2048,1153,false,&config,2,0, NULL, 0, &layout);
	/* click on empty desktop area below menu bar */
	struct orange_shell_hit hit = orange_shell_hit_test(&layout, 500, 500);
	assert(hit.kind == ORANGE_HIT_DESKTOP);
	assert(hit.index == -1);
}

static void test_desktop_background_context_menu_matches_golden_gate(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP, -1, 720, 450, NULL);

	assert(layout.context_menu_item_count == 9);
	assert(!orange_menubar_context_menu_uses_icons(ORANGE_CONTEXT_MENU_DESKTOP));
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 0), "New Folder") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 1), "Paste Item") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 2), "Get Info") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 3), "Change Wallpaper...") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 4), "Edit Widgets...") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 5), "Use Stacks") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 6), "Sort By") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 7), "Clean Up By") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 8), "Show View Options") == 0);
	assert(layout.context_menu_separator[2]);
	assert(layout.context_menu_separator[5]);
	assert(!layout.context_menu_separator[3]);
	assert(!layout.context_menu_separator[4]);
	assert(!layout.context_menu_separator[6]);
	assert(!layout.context_menu_separator[7]);
	assert(!layout.context_menu_separator[8]);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 0), "Shift+Ctrl+N") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 1), "Ctrl+V") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 2), "Ctrl+I") == 0);
	assert(strcmp(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 8), "Ctrl+J") == 0);
	assert(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 5) == NULL);
	assert(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 6) == NULL);
	assert(orange_menubar_context_menu_shortcut_label(
		ORANGE_CONTEXT_MENU_DESKTOP, 7) == NULL);
}

static void test_chrome_background_blocks_desktop_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);

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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	hit = orange_shell_hit_test(&layout,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + 4);
	assert(hit.kind == ORANGE_HIT_DOCK);

	config.dock_position = ORANGE_DOCK_POSITION_RIGHT;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	hit = orange_shell_hit_test(&layout,
		layout.dock.x + layout.dock.width / 2,
		layout.dock.y + 4);
	assert(hit.kind == ORANGE_HIT_DOCK);
}

static void test_context_menu_hit(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 4);
	assert(!layout.context_menu_separator[1]);
	assert(!layout.context_menu_separator[2]);
	assert(layout.context_menu_separator[3]);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	assert(layout.dock.y -
		(layout.context_menu_panel.y + layout.context_menu_panel.height) >= 16);
	int dock_anchor_center = layout.dock_items[0].x +
		layout.dock_items[0].width / 2;
	assert(dock_anchor_center > layout.context_menu_panel.x);
	assert(dock_anchor_center < layout.context_menu_panel.x +
		layout.context_menu_panel.width / 3);
	assert(layout.context_menu_panel.width >= 190);
	struct orange_rect item = layout.context_menu_items[0];
	struct orange_shell_hit hit = orange_shell_hit_test(
		&layout,
		item.x + item.width / 2,
		item.y + item.height / 2);
	assert(hit.kind == ORANGE_HIT_CONTEXT_MENU_ITEM);
	assert(hit.index == 0);
	assert(orange_menubar_context_menu_label(ORANGE_CONTEXT_MENU_DOCK, hit.index) != NULL);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK, 0), "Open") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK, 1), "Options") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK, 2), "Show Recents") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK, 3), "Remove from Dock") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 4);
	assert(!layout.context_menu_separator[1]);
	assert(!layout.context_menu_separator[2]);
	assert(layout.context_menu_separator[3]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 0),
		"Show All Windows") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 1), "Hide Others") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 2), "Options") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 3), "Force Quit") == 0);
	assert(strcmp(orange_menubar_context_menu_icon_name(
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 3),
		"process-stop") == 0);

	layout.dock_options_submenu_open = true;
	layout.context_submenu_item_count = 3;
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_OPTIONS, 0), "Keep in Dock") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_OPTIONS, 1), "Show in Files") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_OPTIONS, 2), "Open at Login") == 0);
	layout.dock_options_submenu_open = false;
	layout.context_submenu_item_count = 0;

	layout.context_menu_alt_pressed = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_RUNNING, 0, 0, 0, NULL);
	assert(layout.context_menu_alt_pressed);
	layout.context_menu_alt_pressed = false;
	layout.context_menu_dock_temporary = false;

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_LAUNCHER, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 2);
	assert(layout.context_menu_separator[1]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_LAUNCHER, 0), "Open Launchpad") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_LAUNCHER, 1), "Dock Settings...") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_TRASH, layout.dock_item_count - 1,
		0, 0, NULL);
	assert(layout.context_menu_item_count == 3);
	assert(!layout.context_menu_separator[1]);
	assert(layout.context_menu_separator[2]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_TRASH, 1), "Empty Trash...") == 0);

	orange_shell_layout_set_context_menu(&layout, ORANGE_CONTEXT_MENU_DOCK,
		layout.dock_item_count - 1, layout.width, layout.height, NULL);
	assert(layout.context_menu_panel.x >= 0);
	assert(layout.context_menu_panel.x + layout.context_menu_panel.width <=
		layout.width);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	assert(layout.dock.y -
		(layout.context_menu_panel.y + layout.context_menu_panel.height) >= 16);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DOCK_SEPARATOR);
	assert(layout.context_menu_item_count == 5);
	assert(layout.context_menu_panel.y + layout.context_menu_panel.height <=
		layout.dock.y);
	assert(layout.dock.y -
		(layout.context_menu_panel.y + layout.context_menu_panel.height) >= 16);
	assert(!layout.context_menu_separator[1]);
	assert(!layout.context_menu_separator[2]);
	assert(!layout.context_menu_separator[3]);
	assert(layout.context_menu_separator[4]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 0),
		"Turn Hiding On/Off") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 1),
		"Turn Magnification On/Off") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 2),
		"Minimize Using") == 0);
	assert(orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 2));
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 3),
		"Position on Screen") == 0);
	assert(orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 3));
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, 4),
		"Dock Settings...") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING, 0),
		"Genie Effect") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_MINIMIZE_USING, 1),
		"Scale Effect") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_POSITION, 0), "Left") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_POSITION, 1), "Bottom") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DOCK_POSITION, 2), "Right") == 0);

	layout.dock_minimize_submenu_open = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_submenu_item_count == 2);
	assert(layout.context_submenu_panel.width > 0);
	assert(layout.context_submenu_panel.x >=
		layout.context_menu_panel.x + layout.context_menu_panel.width);
	struct orange_rect minimize_parent = layout.context_menu_items[2];
	assert(layout.context_submenu_panel.y <= minimize_parent.y);
	struct orange_shell_hit minimize_hit = orange_shell_hit_test(
		&layout,
		layout.context_submenu_items[1].x +
			layout.context_submenu_items[1].width / 2,
		layout.context_submenu_items[1].y +
			layout.context_submenu_items[1].height / 2);
	assert(minimize_hit.kind == ORANGE_HIT_CONTEXT_SUBMENU_ITEM);
	assert(minimize_hit.index == 1);

	layout.dock_minimize_submenu_open = false;
	layout.dock_position_submenu_open = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DOCK_SEPARATOR, -1, 0, 0, NULL);
	assert(layout.context_submenu_item_count == 3);
	struct orange_rect position_parent = layout.context_menu_items[3];
	assert(layout.context_submenu_panel.y <= position_parent.y);
	struct orange_shell_hit position_hit = orange_shell_hit_test(
		&layout,
		layout.context_submenu_items[2].x +
			layout.context_submenu_items[2].width / 2,
		layout.context_submenu_items[2].y +
			layout.context_submenu_items[2].height / 2);
	assert(position_hit.kind == ORANGE_HIT_CONTEXT_SUBMENU_ITEM);
	assert(position_hit.index == 2);
	layout.dock_position_submenu_open = false;

	struct orange_file_info files[1] = {
		{.name = "Project Notes.txt", .path = "/tmp/Project Notes.txt"},
	};
	orange_shell_layout_compute(1920,1080,false,&config,0,0, files, 1, &layout);
	assert(layout.desktop_item_count == 1);
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0, 0, 0, NULL);
	assert(layout.context_menu_item_count == 10);
	assert(layout.context_menu_separator[3]);
	assert(layout.context_menu_separator[7]);
	assert(layout.context_menu_separator[9]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 1), "Open With...") == 0);
	assert(orange_menubar_context_menu_has_submenu(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 1));
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 3), "Copy") == 0);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 9), "Move to Trash") == 0);
	assert(layout.context_submenu_item_count == 0);

	layout.open_with_app_count = ORANGE_OPEN_WITH_APP_MAX;
	layout.open_with_submenu_open = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0, 0, 0, NULL);
	assert(layout.context_menu_kind == ORANGE_CONTEXT_MENU_DESKTOP_FILE);
	assert(layout.context_menu_item_count == 10);
	assert(layout.context_submenu_item_count == ORANGE_OPEN_WITH_APP_MAX);
	assert(layout.context_submenu_panel.width > 0);
	assert(layout.context_submenu_items[0].height <
		layout.context_menu_items[0].height);
	assert(layout.context_submenu_panel.x >=
		layout.context_menu_panel.x + layout.context_menu_panel.width);
	struct orange_rect open_with_item = layout.context_menu_items[1];
	assert(layout.context_submenu_panel.y <= open_with_item.y);
	struct orange_rect submenu_item =
		layout.context_submenu_items[ORANGE_OPEN_WITH_APP_MAX - 1];
	struct orange_shell_hit submenu_hit = orange_shell_hit_test(
		&layout,
		submenu_item.x + submenu_item.width / 2,
		submenu_item.y + submenu_item.height / 2);
	assert(submenu_hit.kind == ORANGE_HIT_CONTEXT_SUBMENU_ITEM);
	assert(submenu_hit.index == ORANGE_OPEN_WITH_APP_MAX - 1);

	layout.open_with_app_count = 0;
	layout.open_with_submenu_open = true;
	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE, 0, 360, 320, NULL);
	assert(layout.context_menu_item_count == 10);
	assert(layout.context_submenu_item_count == 1);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE_OPEN_WITH, 0),
		"No Applications Available") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION, 0, 360, 320, NULL);
	assert(layout.context_menu_item_count == 7);
	assert(layout.context_menu_separator[2]);
	assert(layout.context_menu_separator[4]);
	assert(layout.context_menu_separator[6]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_FILE_SELECTION, 6),
		"Move to Trash") == 0);

	orange_shell_layout_set_context_menu(&layout,
		ORANGE_CONTEXT_MENU_DESKTOP_SELECTION, 0, 360, 320, NULL);
	assert(layout.context_menu_item_count == 4);
	assert(layout.context_menu_separator[1]);
	assert(strcmp(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_SELECTION, 3), "Share") == 0);
	assert(orange_menubar_context_menu_label(
		ORANGE_CONTEXT_MENU_DESKTOP_SELECTION, 4) == NULL);
}

static void test_widget_hit_and_context_menu(void) {
	struct orange_config config;
	orange_config_set_defaults(&config);
	struct orange_shell_layout layout;
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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
	orange_shell_layout_compute(1920,1080,false,&config,2,0, NULL, 0, &layout);
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

static void test_dock_auto_hide_animation_progress(void) {
	double show_start = orange_dock_auto_hide_progress(0.0, true, 0);
	assert(show_start == 0.0);
	double show_mid = orange_dock_auto_hide_progress(0.0, true,
		ORANGE_DOCK_AUTO_HIDE_DURATION_MS / 2);
	assert(show_mid > 0.0 && show_mid < 1.0);
	double show_end = orange_dock_auto_hide_progress(0.0, true,
		ORANGE_DOCK_AUTO_HIDE_DURATION_MS);
	assert(show_end == 1.0);

	double hide_mid = orange_dock_auto_hide_progress(1.0, false,
		ORANGE_DOCK_AUTO_HIDE_DURATION_MS / 2);
	assert(hide_mid > 0.0 && hide_mid < 1.0);
	double hide_end = orange_dock_auto_hide_progress(1.0, false,
		ORANGE_DOCK_AUTO_HIDE_DURATION_MS);
	assert(hide_end == 0.0);
}

int main(void) {
	test_dock_hit();
	test_dock_auto_hide_layout();
	test_reordered_dock_hit_resolves_launcher();
	test_dock_position_layout();
	test_dock_visual_dirty_bounds_include_hover_labels();
	test_dock_separator_drag_scale_math();
	test_minimize_animation_rect_math();
	test_workspace_animation_offset_math();
	test_dock_scale_resizes_separator_layout();
	test_dock_separator_menu_fits_minimize_detail();
	test_default_dock_apps_have_fallback_commands();
	test_firefox_dock_matches_snap_desktop_entry();
	test_dock_lookup_skips_stale_package_desktop_entry();
	test_default_dock_lookup_does_not_confuse_calculator_with_maps();
	test_dock_trash_spacing_is_balanced();
	test_dock_remove_visible_compacts_aliases();
	test_dock_insert_from_launcher_clamps_before_trash();
	test_temporary_dock_apps_appear_before_trash();
	test_minimized_dock_screenshots_appear_left_of_trash();
	test_minimized_dock_thumbnails_magnify_and_use_window_title();
	test_temporary_dock_apps_are_session_scoped();
	test_dock_reorder_visible_keeps_permanent_items_fixed();
	test_desktop_hit();
	test_desktop_requires_volumes();
	test_desktop_custom_position_snaps_to_grid();
	test_desktop_default_items_are_slightly_larger();
	test_desktop_label_rect_targets_rename_area();
	test_desktop_image_highlight_area_hits_item();
	test_desktop_image_hit_wins_over_widget_overlap();
	test_desktop_folder_hit_wins_over_neighbor_padding();
	test_desktop_folder_hit_after_context_menu_clear();
	test_desktop_folder_fresh_config_hit_and_menu();
	test_desktop_custom_position_preserves_dragged_coordinates();
	test_desktop_free_positions_separate_overlapping_highlights();
	test_desktop_duplicate_saved_positions_do_not_overlap();
	test_desktop_grid_keeps_visible_spacing();
	test_desktop_grid_reserves_side_dock_space();
	test_desktop_grid_uses_available_horizontal_space();
	test_desktop_grid_reserves_magnified_bottom_dock_space();
	test_desktop_grid_moves_for_larger_dock_scale();
	test_desktop_grid_reserves_magnified_side_dock_space();
	test_desktop_custom_position_clamps_to_visible_area();
	test_desktop_sort_modes_order_items();
	test_system_menu_hit();
	test_app_menu_layout_and_labels();
	test_status_area_hit_and_menu();
	test_status_notifier_items_layout_and_hit();
	test_modern_menu_affordance_metadata();
	test_native_app_menu_separators_are_preserved();
	test_notification_center_layout_and_hit();
	test_launcher_full_layout_scrolls_all_apps();
	test_launcher_search_mode_transforms_to_overlay();
	test_launcher_spotlight_results();
	test_launcher_category_filter();
	test_launcher_hides_desktop_entries_gnome_would_hide();
	test_launcher_filters_apps_already_in_dock();
	test_small_width_dock_fits();
	test_invalid_visible_dock_order_has_no_blank_slot();
	test_resolution_scaling();
	test_major_surfaces_scale_by_resolution();
	test_menu_surfaces_scale_with_ui_scale();
	test_context_menu_hit();
	test_widget_hit_and_context_menu();
	test_hidden_widget_not_hit();
	test_desktop_background_hit();
	test_desktop_background_context_menu_matches_golden_gate();
	test_render_smoke();
	test_dark_render_smoke();
	test_widget_layer_exists();
	test_dock_bounce_offset();
	test_dock_auto_hide_animation_progress();
	test_chrome_background_blocks_desktop_hit();
	puts("shell layout tests passed");
	return 0;
}
