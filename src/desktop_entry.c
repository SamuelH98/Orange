#include "orange/desktop_entry.h"

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static char *trim(char *value) {
	while (isspace((unsigned char)*value)) {
		value++;
	}
	if (*value == '\0') {
		return value;
	}
	char *end = value + strlen(value) - 1;
	while (end > value && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}
	return value;
}

static void copy_value(char *dest, size_t dest_size, const char *value) {
	if (dest_size == 0) {
		return;
	}
	snprintf(dest, dest_size, "%s", value != NULL ? value : "");
}

static bool parse_bool_value(const char *value) {
	return value != NULL &&
		(strcasecmp(value, "true") == 0 ||
		 strcmp(value, "1") == 0);
}

#define DESKTOP_SUFFIX ".desktop"
#define DESKTOP_SUFFIX_LEN 8

static bool has_desktop_suffix(const char *name) {
	size_t len = strlen(name);
	return len > DESKTOP_SUFFIX_LEN && strcmp(name + len - DESKTOP_SUFFIX_LEN, DESKTOP_SUFFIX) == 0;
}

static void strip_desktop_suffix(char *name) {
	size_t len = strlen(name);
	if (len > DESKTOP_SUFFIX_LEN && strcmp(name + len - DESKTOP_SUFFIX_LEN, DESKTOP_SUFFIX) == 0) {
		name[len - DESKTOP_SUFFIX_LEN] = '\0';
	}
}

static const char *desktop_id_tail(const char *id) {
	const char *dot = strrchr(id, '.');
	const char *underscore = strrchr(id, '_');
	const char *separator = dot > underscore ? dot : underscore;
	return separator != NULL ? separator + 1 : id;
}

bool orange_desktop_entry_id_matches(const char *entry_id, const char *id) {
	if (entry_id == NULL || id == NULL || entry_id[0] == '\0' ||
			id[0] == '\0') {
		return false;
	}
	if (strcmp(entry_id, id) == 0) {
		return true;
	}
	size_t id_len = strlen(id);
	if (id_len > DESKTOP_SUFFIX_LEN &&
			strcmp(id + id_len - DESKTOP_SUFFIX_LEN, DESKTOP_SUFFIX) == 0 &&
			strncmp(entry_id, id, id_len - DESKTOP_SUFFIX_LEN) == 0 &&
			entry_id[id_len - DESKTOP_SUFFIX_LEN] == '\0') {
		return true;
	}
	size_t entry_len = strlen(entry_id);
	if (entry_len + DESKTOP_SUFFIX_LEN == id_len &&
		strncmp(entry_id, id, entry_len) == 0 &&
			strcmp(id + entry_len, DESKTOP_SUFFIX) == 0) {
		return true;
	}

	char entry_normal[256];
	char id_normal[256];
	snprintf(entry_normal, sizeof(entry_normal), "%s", entry_id);
	snprintf(id_normal, sizeof(id_normal), "%s", id);
	strip_desktop_suffix(entry_normal);
	strip_desktop_suffix(id_normal);
	const char *entry_tail = desktop_id_tail(entry_normal);
	const char *id_tail = desktop_id_tail(id_normal);
	/* Require at least the entry to have had a separator (dot or underscore),
	 * so "firefox_firefox" -> tail "firefox" matches query "firefox".
	 * The query may itself be a plain short name with no separator. */
	return entry_tail != entry_normal &&
		strcmp(entry_tail, id_tail) == 0;
}

static bool desktop_list_contains_token(const char *list,
		const char *token, size_t token_len) {
	if (list == NULL || token == NULL || token_len == 0) {
		return false;
	}
	const char *p = list;
	while (*p != '\0') {
		const char *start = p;
		while (*p != '\0' && *p != ';') {
			p++;
		}
		if ((size_t)(p - start) == token_len &&
				strncasecmp(start, token, token_len) == 0) {
			return true;
		}
		if (*p == ';') {
			p++;
		}
	}
	return false;
}

static bool desktop_list_has_values(const char *list) {
	if (list == NULL) {
		return false;
	}
	for (const char *p = list; *p != '\0'; p++) {
		if (*p != ';' && !isspace((unsigned char)*p)) {
			return true;
		}
	}
	return false;
}

static bool entry_show_in_desktop(
		const struct orange_desktop_entry *entry,
		const char *desktop_env) {
	bool has_only = desktop_list_has_values(entry->only_show_in);
	bool show = !has_only;
	if (desktop_env == NULL || desktop_env[0] == '\0') {
		return show;
	}
	const char *p = desktop_env;
	while (*p != '\0') {
		const char *start = p;
		while (*p != '\0' && *p != ':') {
			p++;
		}
		size_t len = (size_t)(p - start);
		if (len > 0) {
			if (desktop_list_contains_token(entry->only_show_in,
					start, len)) {
				return true;
			}
			if (desktop_list_contains_token(entry->not_show_in,
					start, len)) {
				return false;
			}
		}
		if (*p == ':') {
			p++;
		}
	}
	return show;
}

static bool try_exec_available(const char *try_exec) {
	if (try_exec == NULL || try_exec[0] == '\0') {
		return true;
	}
	if (strchr(try_exec, '/') != NULL) {
		return access(try_exec, X_OK) == 0;
	}
	const char *path_env = getenv("PATH");
	if (path_env == NULL || path_env[0] == '\0') {
		return false;
	}
	char paths[4096];
	snprintf(paths, sizeof(paths), "%s", path_env);
	char *save;
	char *token = strtok_r(paths, ":", &save);
	while (token != NULL) {
		const char *dir = token[0] != '\0' ? token : ".";
		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", dir, try_exec);
		if (access(path, X_OK) == 0) {
			return true;
		}
		token = strtok_r(NULL, ":", &save);
	}
	return false;
}

static bool path_exists(const char *path) {
	return path != NULL && path[0] != '\0' && access(path, F_OK) == 0;
}

static bool package_id_is_safe_path_component(const char *id) {
	if (id == NULL || id[0] == '\0' || strstr(id, "..") != NULL) {
		return false;
	}
	for (const char *p = id; *p != '\0'; p++) {
		if (*p == '/') {
			return false;
		}
	}
	return true;
}

static bool snap_instance_available(const char *instance) {
	if (!package_id_is_safe_path_component(instance)) {
		return false;
	}
	char path[4096];
	snprintf(path, sizeof(path), "/snap/%s/current", instance);
	if (path_exists(path)) {
		return true;
	}
	snprintf(path, sizeof(path), "/var/lib/snapd/snap/%s/current",
		instance);
	return path_exists(path);
}

static bool flatpak_id_available(const char *flatpak_id) {
	if (!package_id_is_safe_path_component(flatpak_id)) {
		return false;
	}
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		char user_path[4096];
		snprintf(user_path, sizeof(user_path),
			"%s/.local/share/flatpak/app/%s/current", home, flatpak_id);
		if (path_exists(user_path)) {
			return true;
		}
	}
	char system_path[4096];
	snprintf(system_path, sizeof(system_path),
		"/var/lib/flatpak/app/%s/current", flatpak_id);
	return path_exists(system_path);
}

static bool package_metadata_available(
		const struct orange_desktop_entry *entry) {
	const char *snap = entry->snap_instance[0] != '\0' ?
		entry->snap_instance : entry->snap_app;
	if (snap[0] != '\0' && !snap_instance_available(snap)) {
		return false;
	}
	if (entry->flatpak_id[0] != '\0' &&
			!flatpak_id_available(entry->flatpak_id)) {
		return false;
	}
	return true;
}

bool orange_desktop_entry_is_available(
		const struct orange_desktop_entry *entry) {
	if (entry == NULL || entry->name[0] == '\0' ||
			entry->exec[0] == '\0') {
		return false;
	}
	if (entry->hidden) {
		return false;
	}
	if (!try_exec_available(entry->try_exec)) {
		return false;
	}
	return package_metadata_available(entry);
}

bool orange_desktop_entry_should_show(
		const struct orange_desktop_entry *entry,
		const char *desktop_env) {
	if (!orange_desktop_entry_is_available(entry) || entry->no_display) {
		return false;
	}
	return entry_show_in_desktop(entry, desktop_env);
}

int orange_desktop_entry_match_score(
		const struct orange_desktop_entry *entry,
		const char *id) {
	if (entry == NULL || id == NULL || id[0] == '\0') {
		return 0;
	}
	if (strcmp(entry->id, id) == 0) {
		return 100;
	}
	if (entry->flatpak_id[0] != '\0' &&
			(strcmp(entry->flatpak_id, id) == 0 ||
			 orange_desktop_entry_id_matches(entry->flatpak_id, id))) {
		return 98;
	}
	if (entry->snap_instance[0] != '\0' &&
			(strcmp(entry->snap_instance, id) == 0 ||
			 orange_desktop_entry_id_matches(entry->snap_instance, id))) {
		return 96;
	}
	if (entry->snap_app[0] != '\0' &&
			(strcmp(entry->snap_app, id) == 0 ||
			 orange_desktop_entry_id_matches(entry->snap_app, id))) {
		return 94;
	}
	if (orange_desktop_entry_id_matches(entry->id, id)) {
		return 90;
	}
	if (orange_desktop_entry_id_matches(id, entry->id)) {
		return 80;
	}
	if (entry->startup_wm_class[0] != '\0' &&
			(strcasecmp(entry->startup_wm_class, id) == 0 ||
			 orange_desktop_entry_id_matches(
				entry->startup_wm_class, id) ||
			 orange_desktop_entry_id_matches(
				id, entry->startup_wm_class))) {
		return 70;
	}
	return 0;
}

static bool append_char(char *out, size_t out_size, size_t *len, char c) {
	if (*len + 1 >= out_size) {
		return false;
	}
	out[(*len)++] = c;
	out[*len] = '\0';
	return true;
}

static bool append_text(char *out, size_t out_size, size_t *len,
		const char *text) {
	if (text == NULL) {
		return true;
	}
	while (*text != '\0') {
		if (!append_char(out, out_size, len, *text++)) {
			return false;
		}
	}
	return true;
}

static bool append_shell_quoted(char *out, size_t out_size, size_t *len,
		const char *text) {
	if (!append_char(out, out_size, len, '\'')) {
		return false;
	}
	for (const char *p = text != NULL ? text : ""; *p != '\0'; p++) {
		if (*p == '\'') {
			if (!append_text(out, out_size, len, "'\\''")) {
				return false;
			}
		} else if (!append_char(out, out_size, len, *p)) {
			return false;
		}
	}
	return append_char(out, out_size, len, '\'');
}

static void trim_trailing_space(char *out, size_t *len) {
	while (*len > 0 && isspace((unsigned char)out[*len - 1])) {
		(*len)--;
	}
	out[*len] = '\0';
}

bool orange_desktop_entry_expand_exec(
		const struct orange_desktop_entry *entry,
		char *command,
		size_t command_size) {
	if (entry == NULL || command == NULL || command_size == 0 ||
			entry->exec[0] == '\0') {
		return false;
	}

	size_t len = 0;
	command[0] = '\0';
	for (const char *p = entry->exec; *p != '\0'; p++) {
		if (*p != '%') {
			if (!append_char(command, command_size, &len, *p)) {
				return false;
			}
			continue;
		}

		p++;
		if (*p == '\0') {
			return false;
		}
		switch (*p) {
		case '%':
			if (!append_char(command, command_size, &len, '%')) {
				return false;
			}
			break;
		case 'f':
		case 'F':
		case 'u':
		case 'U':
		case 'd':
		case 'D':
		case 'n':
		case 'N':
		case 'v':
		case 'm':
			/* No file/URL target is being launched from the shell. */
			break;
		case 'i':
			if (entry->icon[0] != '\0' &&
					(!append_text(command, command_size, &len, "--icon ") ||
					!append_shell_quoted(command, command_size, &len, entry->icon))) {
				return false;
			}
			break;
		case 'c':
			if (!append_shell_quoted(command, command_size, &len, entry->name)) {
				return false;
			}
			break;
		case 'k':
			break;
		default:
			return false;
		}
	}

	trim_trailing_space(command, &len);
	return command[0] != '\0';
}

bool orange_desktop_entry_load(
		const char *path,
		struct orange_desktop_entry *entry) {
	memset(entry, 0, sizeof(*entry));

	/* Extract ID from filename: path/to/Foo.desktop -> Foo */
	const char *slash = strrchr(path, '/');
	const char *base = slash != NULL ? slash + 1 : path;
	char id_buf[256];
	snprintf(id_buf, sizeof(id_buf), "%s", base);
	strip_desktop_suffix(id_buf);
	snprintf(entry->id, sizeof(entry->id), "%s", id_buf);

	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	bool in_desktop_entry = false;
	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		char *start = trim(line);
		if (start[0] == '\0' || start[0] == '#') {
			continue;
		}
		if (start[0] == '[') {
			in_desktop_entry = strcmp(start, "[Desktop Entry]") == 0;
			continue;
		}
		if (!in_desktop_entry) {
			continue;
		}
		char *equals = strchr(start, '=');
		if (equals == NULL) {
			continue;
		}
		*equals = '\0';
		char *key = trim(start);
		char *value = trim(equals + 1);
		if (strcmp(key, "Name") == 0) {
			copy_value(entry->name, sizeof(entry->name), value);
		} else if (strcmp(key, "Icon") == 0) {
			copy_value(entry->icon, sizeof(entry->icon), value);
		} else if (strcmp(key, "Exec") == 0) {
			copy_value(entry->exec, sizeof(entry->exec), value);
		} else if (strcmp(key, "TryExec") == 0) {
			copy_value(entry->try_exec, sizeof(entry->try_exec), value);
		} else if (strcmp(key, "Categories") == 0) {
			copy_value(entry->categories, sizeof(entry->categories), value);
		} else if (strcmp(key, "StartupWMClass") == 0) {
			copy_value(entry->startup_wm_class,
				sizeof(entry->startup_wm_class), value);
		} else if (strcmp(key, "OnlyShowIn") == 0) {
			copy_value(entry->only_show_in, sizeof(entry->only_show_in), value);
		} else if (strcmp(key, "NotShowIn") == 0) {
			copy_value(entry->not_show_in, sizeof(entry->not_show_in), value);
		} else if (strcmp(key, "X-SnapInstanceName") == 0) {
			copy_value(entry->snap_instance,
				sizeof(entry->snap_instance), value);
		} else if (strcmp(key, "X-SnapAppName") == 0) {
			copy_value(entry->snap_app, sizeof(entry->snap_app), value);
		} else if (strcmp(key, "X-Flatpak") == 0) {
			copy_value(entry->flatpak_id, sizeof(entry->flatpak_id), value);
		} else if (strcmp(key, "Terminal") == 0) {
			entry->terminal = parse_bool_value(value);
		} else if (strcmp(key, "Hidden") == 0) {
			entry->hidden = parse_bool_value(value);
		} else if (strcmp(key, "NoDisplay") == 0) {
			entry->no_display = parse_bool_value(value);
		}
	}

	fclose(file);
	return entry->name[0] != '\0' && entry->exec[0] != '\0';
}

bool orange_desktop_entry_load_all(
		const char *directory,
		struct orange_desktop_entry *entries,
		size_t capacity,
		size_t *count) {
	*count = 0;
	if (capacity == 0) {
		return true;
	}
	DIR *dir = opendir(directory);
	if (dir == NULL) {
		return false;
	}

	char (*names)[256] = calloc(capacity, sizeof(*names));
	if (names == NULL) {
		closedir(dir);
		return false;
	}

	struct dirent *dent;
	size_t name_count = 0;
	while ((dent = readdir(dir)) != NULL && name_count < capacity) {
		if (!has_desktop_suffix(dent->d_name)) {
			continue;
		}
		snprintf(names[name_count], 256, "%s", dent->d_name);
		name_count++;
	}
	closedir(dir);

	for (size_t i = 1; i < name_count; i++) {
		char current[256];
		snprintf(current, sizeof(current), "%s", names[i]);
		size_t j = i;
		while (j > 0 && strcmp(names[j - 1], current) > 0) {
			memcpy(names[j], names[j - 1], sizeof(names[j]));
			j--;
		}
		memcpy(names[j], current, sizeof(names[j]));
	}

	for (size_t i = 0; i < name_count && *count < capacity; i++) {
		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
		if (orange_desktop_entry_load(path, &entries[*count])) {
			(*count)++;
		}
	}

	free(names);
	return true;
}

static void load_applications_dir(
		const char *path,
		struct orange_desktop_entry *entries,
		size_t capacity,
		size_t *count) {
	if (path == NULL || path[0] == '\0' || *count >= capacity) {
		return;
	}
	size_t dir_count = 0;
	orange_desktop_entry_load_all(path, entries + *count,
		capacity - *count, &dir_count);
	*count += dir_count;
}

static void load_applications_base(
		const char *base,
		struct orange_desktop_entry *entries,
		size_t capacity,
		size_t *count) {
	if (base == NULL || base[0] == '\0') {
		return;
	}
	char path[4096];
	snprintf(path, sizeof(path), "%s/applications", base);
	load_applications_dir(path, entries, capacity, count);
}

size_t orange_desktop_entry_load_all_xdg(
		struct orange_desktop_entry *entries,
		size_t capacity) {
	size_t count = 0;

	/* XDG data home: $XDG_DATA_HOME/applications/ */
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	if (xdg_data_home != NULL && xdg_data_home[0] != '\0') {
		load_applications_base(xdg_data_home, entries, capacity, &count);
	} else if (home != NULL && home[0] != '\0') {
		char path[4096];
		snprintf(path, sizeof(path), "%s/.local/share/applications", home);
		load_applications_dir(path, entries, capacity, &count);
	}

	/* XDG data dirs: $XDG_DATA_DIRS/applications/ */
	const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL || xdg_data_dirs[0] == '\0') {
		xdg_data_dirs = "/usr/local/share:/usr/share";
	}

	char dirs_copy[4096];
	snprintf(dirs_copy, sizeof(dirs_copy), "%s", xdg_data_dirs);
	char *save;
	char *token = strtok_r(dirs_copy, ":", &save);
	while (token != NULL && count < capacity) {
		load_applications_base(token, entries, capacity, &count);
		token = strtok_r(NULL, ":", &save);
	}

	load_applications_base("/var/lib/snapd/desktop",
		entries, capacity, &count);
	load_applications_base("/var/lib/flatpak/exports/share",
		entries, capacity, &count);
	if (home != NULL && home[0] != '\0') {
		char path[4096];
		snprintf(path, sizeof(path),
			"%s/.local/share/flatpak/exports/share", home);
		load_applications_base(path, entries, capacity, &count);
	}

	return count;
}
