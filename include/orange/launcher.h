#ifndef ORANGE_LAUNCHER_H
#define ORANGE_LAUNCHER_H

#include "orange/shell.h"

void orange_launcher_draw(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);
void orange_launcher_draw_app_drag_overlay(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

int orange_launcher_filter(
	const struct orange_desktop_entry *entries,
	int entry_count,
	const char *query,
	const char *category_filter,
	int *out_indices,
	int out_capacity);

const char *orange_launcher_app_label(
	const struct orange_desktop_entry *entries,
	int entry_count,
	const int *indices,
	int index_count,
	int visible_index);

const char *orange_launcher_app_icon(
	const struct orange_desktop_entry *entries,
	int entry_count,
	const int *indices,
	int index_count,
	int visible_index);

#endif
