#include "orange/desktop_entry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void mkdir_p(const char *path) {
	char partial[1024];
	snprintf(partial, sizeof(partial), "%s", path);
	for (char *p = partial + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(partial, 0700);
			*p = '/';
		}
	}
	mkdir(partial, 0700);
}

static void write_desktop_entry_with_extra(
		const char *path,
		const char *name,
		const char *icon,
		const char *exec,
		const char *categories,
		const char *extra) {
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	fprintf(file,
		"[Desktop Entry]\n"
		"Type=Application\n"
		"Name=%s\n"
		"Icon=%s\n"
		"Exec=%s\n"
		"Categories=%s\n"
		"Terminal=false\n",
		name,
		icon,
		exec,
		categories);
	if (extra != NULL) {
		fputs(extra, file);
	}
	fclose(file);
}

static void write_desktop_entry(
		const char *path,
		const char *name,
		const char *icon,
		const char *exec,
		const char *categories) {
	write_desktop_entry_with_extra(path, name, icon, exec, categories, NULL);
}

static bool contains_entry(
		const struct orange_desktop_entry *entries,
		size_t count,
		const char *id) {
	for (size_t i = 0; i < count; i++) {
		if (strcmp(entries[i].id, id) == 0) {
			return true;
		}
	}
	return false;
}

int main(void) {
	char root[256];
	snprintf(root, sizeof(root), "/tmp/orange-desktop-entry-test-%ld",
		(long)getpid());
	char home_apps[1024];
	char data_apps[1024];
	snprintf(home_apps, sizeof(home_apps), "%s/home/applications", root);
	snprintf(data_apps, sizeof(data_apps), "%s/data/applications", root);
	mkdir_p(home_apps);
	mkdir_p(data_apps);

	char viewer_path[2048];
	char alpha_path[2048];
	char beta_path[2048];
	char hidden_path[2048];
	char nodisplay_path[2048];
	char only_path[2048];
	char not_path[2048];
	char try_path[2048];
	char try_missing_path[2048];
	char stale_snap_path[2048];
	char stale_flatpak_path[2048];
	char game_path[2048];
	char helper_path[2048];
	char missing_helper_path[2048];
	snprintf(viewer_path, sizeof(viewer_path),
		"%s/org.example.Viewer.desktop", home_apps);
	snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.desktop", home_apps);
	snprintf(beta_path, sizeof(beta_path), "%s/beta.desktop", data_apps);
	snprintf(hidden_path, sizeof(hidden_path), "%s/hidden.desktop", home_apps);
	snprintf(nodisplay_path, sizeof(nodisplay_path),
		"%s/nodisplay.desktop", home_apps);
	snprintf(only_path, sizeof(only_path), "%s/only.desktop", home_apps);
	snprintf(not_path, sizeof(not_path), "%s/not.desktop", home_apps);
	snprintf(try_path, sizeof(try_path), "%s/try.desktop", home_apps);
	snprintf(try_missing_path, sizeof(try_missing_path),
		"%s/try-missing.desktop", home_apps);
	snprintf(stale_snap_path, sizeof(stale_snap_path),
		"%s/stale-snap.desktop", home_apps);
	snprintf(stale_flatpak_path, sizeof(stale_flatpak_path),
		"%s/stale-flatpak.desktop", home_apps);
	snprintf(game_path, sizeof(game_path),
		"%s/example-game.desktop", home_apps);
	snprintf(helper_path, sizeof(helper_path), "%s/helper", root);
	snprintf(missing_helper_path, sizeof(missing_helper_path),
		"%s/missing-helper", root);
	write_desktop_entry(viewer_path, "Example Viewer", "org.example.Viewer",
		"example-viewer --open %U --name %c %i", "Graphics;Photography;");
	write_desktop_entry(alpha_path, "Alpha", "folder", "alpha-app %f",
		"Utility;");
	write_desktop_entry(beta_path, "Beta", "text-editor", "beta-app %u",
		"Office;Finance;");
	write_desktop_entry_with_extra(hidden_path, "Hidden Helper", "helper",
		"hidden-helper", "Utility;", "Hidden=true\n");
	write_desktop_entry_with_extra(nodisplay_path, "MIME Helper", "helper",
		"mime-helper", "Utility;", "NoDisplay=true\n");
	write_desktop_entry_with_extra(only_path, "GNOME Only", "helper",
		"gnome-only", "Utility;", "OnlyShowIn=GNOME;Unity;\n");
	write_desktop_entry_with_extra(not_path, "Not GNOME", "helper",
		"not-gnome", "Utility;", "NotShowIn=GNOME;Unity;\n");
	FILE *helper = fopen(helper_path, "w");
	assert(helper != NULL);
	fputs("#!/bin/sh\n", helper);
	fclose(helper);
	assert(chmod(helper_path, 0700) == 0);
	char try_extra[4096];
	snprintf(try_extra, sizeof(try_extra), "TryExec=%s\n", helper_path);
	write_desktop_entry_with_extra(try_path, "Try Present", "helper",
		"try-present", "Utility;", try_extra);
	char try_missing_extra[4096];
	snprintf(try_missing_extra, sizeof(try_missing_extra),
		"TryExec=%s\n", missing_helper_path);
	write_desktop_entry_with_extra(try_missing_path, "Try Missing", "helper",
		"try-missing", "Utility;", try_missing_extra);
	write_desktop_entry_with_extra(stale_snap_path, "Stale Snap", "helper",
		"stale-snap", "Utility;",
		"X-SnapInstanceName=orange-missing-snap\n"
		"X-SnapAppName=orange-missing-snap\n"
		"StartupWMClass=orange-missing-snap\n");
	write_desktop_entry_with_extra(stale_flatpak_path, "Stale Flatpak",
		"helper", "stale-flatpak", "Utility;",
		"X-Flatpak=dev.orange.MissingFlatpak\n"
		"StartupWMClass=dev.orange.MissingFlatpak\n");
	write_desktop_entry(game_path, "Example Game", "game-icon",
		"example-game", "Game;ActionGame;");

	struct orange_desktop_entry entry;
	assert(orange_desktop_entry_load(viewer_path, &entry));
	assert(entry.id[0] != '\0');
	assert(strcmp(entry.id, "org.example.Viewer") == 0);
	assert(strcmp(entry.name, "Example Viewer") == 0);
	assert(strcmp(entry.icon, "org.example.Viewer") == 0);
	assert(strcmp(entry.exec, "example-viewer --open %U --name %c %i") == 0);
	assert(strcmp(entry.categories, "Graphics;Photography;") == 0);
	assert(orange_desktop_entry_has_category(&entry, "Graphics"));
	assert(!orange_desktop_entry_is_game(&entry));
	assert(orange_desktop_entry_id_matches(entry.id, "org.example.Viewer.desktop"));
	assert(orange_desktop_entry_id_matches("firefox_firefox", "firefox.desktop"));
	assert(orange_desktop_entry_id_matches("org.mozilla.firefox", "firefox.desktop"));
	char command_buf[1024];
	assert(orange_desktop_entry_expand_exec(&entry, command_buf, sizeof(command_buf)));
	assert(strchr(command_buf, '%') == NULL);
	assert(strstr(command_buf, "'Example Viewer'") != NULL);
	assert(strstr(command_buf, "--icon 'org.example.Viewer'") != NULL);

	assert(orange_desktop_entry_load(hidden_path, &entry));
	assert(entry.hidden);
	assert(!orange_desktop_entry_should_show(&entry, "GNOME"));
	assert(orange_desktop_entry_load(nodisplay_path, &entry));
	assert(entry.no_display);
	assert(!orange_desktop_entry_should_show(&entry, "GNOME"));
	assert(orange_desktop_entry_load(only_path, &entry));
	assert(orange_desktop_entry_should_show(&entry, "GNOME:orange"));
	assert(!orange_desktop_entry_should_show(&entry, "KDE"));
	assert(orange_desktop_entry_load(not_path, &entry));
	assert(!orange_desktop_entry_should_show(&entry, "GNOME:orange"));
	assert(orange_desktop_entry_should_show(&entry, "KDE"));
	assert(orange_desktop_entry_load(try_path, &entry));
	assert(orange_desktop_entry_should_show(&entry, "orange"));
	assert(orange_desktop_entry_load(try_missing_path, &entry));
	assert(!orange_desktop_entry_should_show(&entry, "orange"));
	assert(orange_desktop_entry_load(stale_snap_path, &entry));
	assert(strcmp(entry.snap_instance, "orange-missing-snap") == 0);
	assert(strcmp(entry.startup_wm_class, "orange-missing-snap") == 0);
	assert(!orange_desktop_entry_is_available(&entry));
	assert(!orange_desktop_entry_should_show(&entry, "orange"));
	assert(orange_desktop_entry_load(stale_flatpak_path, &entry));
	assert(strcmp(entry.flatpak_id, "dev.orange.MissingFlatpak") == 0);
	assert(!orange_desktop_entry_is_available(&entry));
	assert(!orange_desktop_entry_should_show(&entry, "orange"));
	assert(orange_desktop_entry_load(game_path, &entry));
	assert(orange_desktop_entry_has_category(&entry, "ActionGame"));
	assert(orange_desktop_entry_is_game(&entry));

	struct orange_desktop_entry entries[16];
	size_t count = 0;
	assert(orange_desktop_entry_load_all(home_apps, entries, 16, &count));
	assert(count == 11);
	assert(contains_entry(entries, count, "alpha"));
	assert(contains_entry(entries, count, "example-game"));
	assert(contains_entry(entries, count, "org.example.Viewer"));

	char xdg_home[1024];
	char xdg_data_dirs[1024];
	snprintf(xdg_home, sizeof(xdg_home), "%s/home", root);
	snprintf(xdg_data_dirs, sizeof(xdg_data_dirs), "%s/data", root);
	assert(setenv("XDG_DATA_HOME", xdg_home, 1) == 0);
	assert(setenv("XDG_DATA_DIRS", xdg_data_dirs, 1) == 0);
	size_t xdg_count = orange_desktop_entry_load_all_xdg(entries, 16);
	(void)xdg_count;
	assert(contains_entry(entries, xdg_count, "alpha"));
	assert(contains_entry(entries, xdg_count, "beta"));
	assert(contains_entry(entries, xdg_count, "org.example.Viewer"));

	puts("desktop entry tests passed");
	return 0;
}
