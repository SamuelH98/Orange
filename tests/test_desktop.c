#include "orange/desktop.h"

#include <assert.h>
#include <string.h>

static void type_ascii(
		struct orange_desktop_rename_state *state,
		const char *text) {
	for (const char *p = text; *p != '\0'; p++) {
		assert(orange_desktop_rename_insert_codepoint(
			state, (unsigned char)*p));
	}
}

static void test_file_rename_replaces_stem_and_keeps_extension(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		4, 2, "Report.txt", "/tmp/Report.txt", false);

	assert(rename.active);
	assert(rename.layout_index == 4);
	assert(rename.file_index == 2);
	assert(rename.selection_start_byte == 0);
	assert(rename.selection_end_byte == 6);
	assert(rename.cursor_byte == 6);

	type_ascii(&rename, "Q1");
	assert(strcmp(rename.text, "Q1.txt") == 0);
	assert(rename.cursor_byte == 2);
	assert(rename.selection_start_byte == rename.cursor_byte);
	assert(rename.selection_end_byte == rename.cursor_byte);
}

static void test_directory_rename_selects_entire_name(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		0, 0, "Projects", "/tmp/Projects", true);

	type_ascii(&rename, "Archive");
	assert(strcmp(rename.text, "Archive") == 0);
}

static void test_hidden_file_rename_selects_entire_name(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		0, 0, ".profile", "/tmp/.profile", false);

	type_ascii(&rename, "profile");
	assert(strcmp(rename.text, "profile") == 0);
}

static void test_backspace_and_delete_remove_initial_selection(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		0, 0, "Report.txt", "/tmp/Report.txt", false);
	assert(orange_desktop_rename_backspace(&rename));
	assert(strcmp(rename.text, ".txt") == 0);
	assert(rename.cursor_byte == 0);

	orange_desktop_rename_begin(&rename,
		0, 0, "Report.txt", "/tmp/Report.txt", false);
	assert(orange_desktop_rename_delete_forward(&rename));
	assert(strcmp(rename.text, ".txt") == 0);
	assert(rename.cursor_byte == 0);
}

static void test_utf8_backspace_removes_one_codepoint(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		0, 0, "Report.txt", "/tmp/Report.txt", false);
	assert(orange_desktop_rename_insert_codepoint(&rename, 0x00e9));
	assert(strcmp(rename.text, "\xc3\xa9.txt") == 0);
	assert(rename.cursor_byte == 2);
	assert(orange_desktop_rename_backspace(&rename));
	assert(strcmp(rename.text, ".txt") == 0);
	assert(rename.cursor_byte == 0);
}

static void test_invalid_slash_keeps_selection_unchanged(void) {
	struct orange_desktop_rename_state rename;
	orange_desktop_rename_begin(&rename,
		0, 0, "Report.txt", "/tmp/Report.txt", false);

	assert(!orange_desktop_rename_insert_codepoint(&rename, '/'));
	assert(strcmp(rename.text, "Report.txt") == 0);
	assert(rename.selection_start_byte == 0);
	assert(rename.selection_end_byte == 6);
}

int main(void) {
	test_file_rename_replaces_stem_and_keeps_extension();
	test_directory_rename_selects_entire_name();
	test_hidden_file_rename_selects_entire_name();
	test_backspace_and_delete_remove_initial_selection();
	test_utf8_backspace_removes_one_codepoint();
	test_invalid_slash_keeps_selection_unchanged();
	return 0;
}
