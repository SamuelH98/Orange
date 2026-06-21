#include "orange/desktop_entry.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static bool has_desktop_suffix(const char *name) {
	size_t len = strlen(name);
	return len > 8 && strcmp(name + len - 8, ".desktop") == 0;
}

static void strip_desktop_suffix(char *name) {
	size_t len = strlen(name);
	if (len > 8 && strcmp(name + len - 8, ".desktop") == 0) {
		name[len - 8] = '\0';
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
	if (id_len > 8 && strcmp(id + id_len - 8, ".desktop") == 0 &&
			strncmp(entry_id, id, id_len - 8) == 0 &&
			entry_id[id_len - 8] == '\0') {
		return true;
	}
	size_t entry_len = strlen(entry_id);
	if (entry_len + 8 == id_len &&
		strncmp(entry_id, id, entry_len) == 0 &&
			strcmp(id + entry_len, ".desktop") == 0) {
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
		} else if (strcmp(key, "Categories") == 0) {
			copy_value(entry->categories, sizeof(entry->categories), value);
		} else if (strcmp(key, "Terminal") == 0) {
			entry->terminal = strcmp(value, "true") == 0 ||
				strcmp(value, "1") == 0;
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
