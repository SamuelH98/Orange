#include "orange/desktop.h"

#include <stdio.h>
#include <string.h>

void orange_desktop_rename_reset(struct orange_desktop_rename_state *rename) {
	if (rename == NULL) {
		return;
	}
	rename->active = false;
	rename->layout_index = -1;
	rename->file_index = -1;
	rename->cursor_byte = 0;
	rename->selection_start_byte = 0;
	rename->selection_end_byte = 0;
	rename->text[0] = '\0';
	rename->path[0] = '\0';
}

static int initial_selection_end(const char *name, bool is_directory) {
	if (name == NULL || name[0] == '\0') {
		return 0;
	}
	size_t len = strlen(name);
	if (is_directory) {
		return (int)len;
	}
	const char *dot = strrchr(name, '.');
	if (dot == NULL || dot == name) {
		return (int)len;
	}
	return (int)(dot - name);
}

void orange_desktop_rename_begin(
		struct orange_desktop_rename_state *rename,
		int layout_index,
		int file_index,
		const char *name,
		const char *path,
		bool is_directory) {
	if (rename == NULL) {
		return;
	}
	orange_desktop_rename_reset(rename);
	rename->active = true;
	rename->layout_index = layout_index;
	rename->file_index = file_index;
	snprintf(rename->text, sizeof(rename->text), "%s", name != NULL ? name : "");
	snprintf(rename->path, sizeof(rename->path), "%s", path != NULL ? path : "");
	rename->selection_start_byte = 0;
	rename->selection_end_byte = initial_selection_end(
		rename->text, is_directory);
	rename->cursor_byte = rename->selection_end_byte;
}

static size_t utf8_previous_offset(const char *text, size_t offset) {
	if (text == NULL || offset == 0) {
		return 0;
	}
	size_t len = strlen(text);
	if (offset > len) {
		offset = len;
	}
	if (offset == 0) {
		return 0;
	}
	offset--;
	while (offset > 0 && (((unsigned char)text[offset] & 0xC0) == 0x80)) {
		offset--;
	}
	return offset;
}

static size_t utf8_next_offset(const char *text, size_t offset) {
	if (text == NULL) {
		return 0;
	}
	size_t len = strlen(text);
	if (offset >= len) {
		return len;
	}
	offset++;
	while (offset < len && (((unsigned char)text[offset] & 0xC0) == 0x80)) {
		offset++;
	}
	return offset;
}

static void remove_utf8_byte_range(char *text, size_t start, size_t end) {
	if (text == NULL || start >= end) {
		return;
	}
	size_t len = strlen(text);
	if (start > len) {
		start = len;
	}
	if (end > len) {
		end = len;
	}
	if (start >= end) {
		return;
	}
	memmove(text + start, text + end, len - end + 1);
}

static bool has_selection(const struct orange_desktop_rename_state *rename) {
	return rename != NULL &&
		rename->selection_start_byte < rename->selection_end_byte;
}

static void clear_selection(struct orange_desktop_rename_state *rename) {
	if (rename == NULL) {
		return;
	}
	rename->selection_start_byte = rename->cursor_byte;
	rename->selection_end_byte = rename->cursor_byte;
}

static bool delete_selection(struct orange_desktop_rename_state *rename) {
	if (!has_selection(rename)) {
		return false;
	}
	size_t len = strlen(rename->text);
	size_t start = (size_t)rename->selection_start_byte;
	size_t end = (size_t)rename->selection_end_byte;
	if (start > len) {
		start = len;
	}
	if (end > len) {
		end = len;
	}
	remove_utf8_byte_range(rename->text, start, end);
	rename->cursor_byte = (int)start;
	clear_selection(rename);
	return true;
}

static bool insert_utf8_codepoint(
		char *text,
		size_t text_size,
		int *cursor_byte,
		uint32_t cp) {
	if (text == NULL || cursor_byte == NULL || text_size == 0 ||
			cp < 0x20 || cp == 0x7f || cp == '/') {
		return false;
	}
	int len = (int)strlen(text);
	int cursor = *cursor_byte;
	if (cursor < 0) {
		cursor = 0;
	}
	if (cursor > len) {
		cursor = len;
	}
	char buf[5];
	int n = 0;
	if (cp < 0x80) {
		buf[n++] = (char)cp;
	} else if (cp < 0x800) {
		buf[n++] = (char)(0xC0 | (cp >> 6));
		buf[n++] = (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		buf[n++] = (char)(0xE0 | (cp >> 12));
		buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		buf[n++] = (char)(0x80 | (cp & 0x3F));
	} else {
		buf[n++] = (char)(0xF0 | (cp >> 18));
		buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
		buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		buf[n++] = (char)(0x80 | (cp & 0x3F));
	}
	if ((size_t)len + (size_t)n >= text_size) {
		return false;
	}
	memmove(text + cursor + n, text + cursor, (size_t)(len - cursor) + 1);
	memcpy(text + cursor, buf, (size_t)n);
	*cursor_byte = cursor + n;
	return true;
}

bool orange_desktop_rename_insert_codepoint(
		struct orange_desktop_rename_state *rename,
		uint32_t cp) {
	if (rename == NULL || cp < 0x20 || cp == 0x7f || cp == '/') {
		return false;
	}
	delete_selection(rename);
	bool inserted = insert_utf8_codepoint(
		rename->text,
		sizeof(rename->text),
		&rename->cursor_byte,
		cp);
	if (inserted) {
		clear_selection(rename);
	}
	return inserted;
}

bool orange_desktop_rename_backspace(
		struct orange_desktop_rename_state *rename) {
	if (rename == NULL) {
		return false;
	}
	if (delete_selection(rename)) {
		return true;
	}
	size_t cursor = (size_t)rename->cursor_byte;
	if (cursor == 0) {
		return false;
	}
	size_t previous = utf8_previous_offset(rename->text, cursor);
	remove_utf8_byte_range(rename->text, previous, cursor);
	rename->cursor_byte = (int)previous;
	clear_selection(rename);
	return true;
}

bool orange_desktop_rename_delete_forward(
		struct orange_desktop_rename_state *rename) {
	if (rename == NULL) {
		return false;
	}
	if (delete_selection(rename)) {
		return true;
	}
	size_t cursor = (size_t)rename->cursor_byte;
	size_t next = utf8_next_offset(rename->text, cursor);
	if (next == cursor) {
		return false;
	}
	remove_utf8_byte_range(rename->text, cursor, next);
	clear_selection(rename);
	return true;
}
