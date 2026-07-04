#ifndef ORANGE_DESKTOP_H
#define ORANGE_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>

#define ORANGE_DESKTOP_RENAME_TEXT_MAX 256
#define ORANGE_DESKTOP_RENAME_PATH_MAX 1024

struct orange_desktop_rename_state {
	bool active;
	int layout_index;
	int file_index;
	int cursor_byte;
	int selection_start_byte;
	int selection_end_byte;
	char text[ORANGE_DESKTOP_RENAME_TEXT_MAX];
	char path[ORANGE_DESKTOP_RENAME_PATH_MAX];
};

void orange_desktop_rename_reset(struct orange_desktop_rename_state *rename);
void orange_desktop_rename_begin(
	struct orange_desktop_rename_state *rename,
	int layout_index,
	int file_index,
	const char *name,
	const char *path,
	bool is_directory);
bool orange_desktop_rename_insert_codepoint(
	struct orange_desktop_rename_state *rename,
	uint32_t cp);
bool orange_desktop_rename_backspace(
	struct orange_desktop_rename_state *rename);
bool orange_desktop_rename_delete_forward(
	struct orange_desktop_rename_state *rename);

#endif
