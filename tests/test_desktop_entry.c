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

static void write_desktop_entry(
		const char *path,
		const char *name,
		const char *icon,
		const char *exec,
		const char *categories) {
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
	fclose(file);
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
	snprintf(viewer_path, sizeof(viewer_path),
		"%s/org.example.Viewer.desktop", home_apps);
	snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.desktop", home_apps);
	snprintf(beta_path, sizeof(beta_path), "%s/beta.desktop", data_apps);
	write_desktop_entry(viewer_path, "Example Viewer", "org.example.Viewer",
		"example-viewer --open %U --name %c %i", "Graphics;Photography;");
	write_desktop_entry(alpha_path, "Alpha", "folder", "alpha-app %f",
		"Utility;");
	write_desktop_entry(beta_path, "Beta", "text-editor", "beta-app %u",
		"Office;Finance;");

	struct orange_desktop_entry entry;
	assert(orange_desktop_entry_load(viewer_path, &entry));
	assert(entry.id[0] != '\0');
	assert(strcmp(entry.id, "org.example.Viewer") == 0);
	assert(strcmp(entry.name, "Example Viewer") == 0);
	assert(strcmp(entry.icon, "org.example.Viewer") == 0);
	assert(strcmp(entry.exec, "example-viewer --open %U --name %c %i") == 0);
	assert(strcmp(entry.categories, "Graphics;Photography;") == 0);
	assert(orange_desktop_entry_id_matches(entry.id, "org.example.Viewer.desktop"));
	assert(orange_desktop_entry_id_matches("firefox_firefox", "firefox.desktop"));
	assert(orange_desktop_entry_id_matches("org.mozilla.firefox", "firefox.desktop"));
	char command[1024];
	assert(orange_desktop_entry_expand_exec(&entry, command, sizeof(command)));
	assert(strchr(command, '%') == NULL);
	assert(strstr(command, "'Example Viewer'") != NULL);
	assert(strstr(command, "--icon 'org.example.Viewer'") != NULL);

	struct orange_desktop_entry entries[8];
	size_t count = 0;
	assert(orange_desktop_entry_load_all(home_apps, entries, 8, &count));
	assert(count == 2);
	assert(contains_entry(entries, count, "alpha"));
	assert(contains_entry(entries, count, "org.example.Viewer"));

	char xdg_home[1024];
	char xdg_data_dirs[1024];
	snprintf(xdg_home, sizeof(xdg_home), "%s/home", root);
	snprintf(xdg_data_dirs, sizeof(xdg_data_dirs), "%s/data", root);
	assert(setenv("XDG_DATA_HOME", xdg_home, 1) == 0);
	assert(setenv("XDG_DATA_DIRS", xdg_data_dirs, 1) == 0);
	size_t xdg_count = orange_desktop_entry_load_all_xdg(entries, 8);
	assert(xdg_count >= 3);
	assert(contains_entry(entries, xdg_count, "alpha"));
	assert(contains_entry(entries, xdg_count, "beta"));
	assert(contains_entry(entries, xdg_count, "org.example.Viewer"));

	puts("desktop entry tests passed");
	return 0;
}
