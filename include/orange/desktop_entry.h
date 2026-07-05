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
	char try_exec[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char categories[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char startup_wm_class[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char only_show_in[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char not_show_in[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char snap_instance[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char snap_app[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	char flatpak_id[ORANGE_DESKTOP_ENTRY_MAX_TEXT];
	bool terminal;
	bool hidden;
	bool no_display;
};

bool orange_desktop_entry_id_matches(const char *entry_id, const char *id);
bool orange_desktop_entry_is_available(
	const struct orange_desktop_entry *entry);
bool orange_desktop_entry_should_show(
	const struct orange_desktop_entry *entry,
	const char *desktop_env);
bool orange_desktop_entry_has_category(
	const struct orange_desktop_entry *entry,
	const char *category);
bool orange_desktop_entry_is_game(
	const struct orange_desktop_entry *entry);
int orange_desktop_entry_match_score(
	const struct orange_desktop_entry *entry,
	const char *id);
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
