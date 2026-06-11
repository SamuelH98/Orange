#include "orange/desktop_entry.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool orange_desktop_entry_load(
		const char *path,
		struct orange_desktop_entry *entry) {
	memset(entry, 0, sizeof(*entry));
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
