#ifndef ORANGE_DESKTOP_ENTRY_H
#define ORANGE_DESKTOP_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#define ORANGE_DESKTOP_ENTRY_MAX_TEXT 256

struct orange_desktop_entry {
	char name[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char icon[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char exec[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	bool terminal;
};

bool orange_desktop_entry_load(
	const char *path,
	struct orange_desktop_entry *entry);

bool orange_desktop_entry_load_all(
	const char *directory,
	struct orange_desktop_entry *entries,
	size_t capacity,
	size_t *count);

#endif
