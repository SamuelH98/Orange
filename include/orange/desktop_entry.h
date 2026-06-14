#ifndef ORANGE_DESKTOP_ENTRY_H
#define ORANGE_DESKTOP_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#define ORANGE_DESKTOP_ENTRY_MAX_TEXT 512

struct orange_desktop_entry {
	char id[256];
	char name[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char icon[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char exec[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	bool terminal;
};

bool orange_desktop_entry_id_matches(const char *entry_id, const char *id);
bool orange_desktop_entry_expand_exec(
	const struct orange_desktop_entry *entry,
	char *command,
	size_t command_size);

bool orange_desktop_entry_load(
	const char *path,
	struct orange_desktop_entry *entry);

bool orange_desktop_entry_load_all(
	const char *directory,
	struct orange_desktop_entry *entries,
	size_t capacity,
	size_t *count);

size_t orange_desktop_entry_load_all_xdg(
	struct orange_desktop_entry *entries,
	size_t capacity);

#endif
