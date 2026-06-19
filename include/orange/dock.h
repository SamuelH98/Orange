#ifndef ORANGE_DOCK_H
#define ORANGE_DOCK_H

#include "orange/shell.h"

void orange_dock_draw(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

int orange_dock_count(const struct orange_config *config);
int orange_dock_launcher_index(
	const struct orange_shell_layout *layout,
	int visible_index);
const char *orange_dock_label(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_dock_command(int index,
	const struct orange_desktop_entry *entries, int entry_count,
	const struct orange_config *config);
const char *orange_dock_builtin_command(const char *app_id);
#endif
