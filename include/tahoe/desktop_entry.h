#ifndef TAHOE_DESKTOP_ENTRY_H
#define TAHOE_DESKTOP_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#define TAHOE_DESKTOP_ENTRY_MAX_TEXT 256

struct tahoe_desktop_entry {
	char name[TAHOE_DESKTOP_ENTRY_MAX_TEXT];
	char icon[TAHOE_DESKTOP_ENTRY_MAX_TEXT];
	char exec[TAHOE_DESKTOP_ENTRY_MAX_TEXT];
	bool terminal;
};

bool tahoe_desktop_entry_load(
	const char *path,
	struct tahoe_desktop_entry *entry);

bool tahoe_desktop_entry_load_all(
	const char *directory,
	struct tahoe_desktop_entry *entries,
	size_t capacity,
	size_t *count);

#endif
