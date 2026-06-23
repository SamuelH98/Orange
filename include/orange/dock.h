#ifndef ORANGE_DOCK_H
#define ORANGE_DOCK_H

#include <stdint.h>

#include "orange/shell.h"

#define ORANGE_DOCK_BOUNCE_DURATION_MS 800

struct orange_dock_bounce {
	bool active;
	int launcher_idx;
	uint32_t start_time;
};

struct orange_dock_bounce_displacement {
	double x;
	double y;
};

double orange_dock_bounce_offset(const struct orange_dock_bounce *bounce,
	uint32_t now_ms, double scale);
struct orange_dock_bounce_displacement orange_dock_bounce_displacement(
	const struct orange_dock_bounce *bounce,
	uint32_t now_ms,
	enum orange_dock_position position,
	double scale);

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
bool orange_dock_app_is_permanent(const char *app_id);
bool orange_dock_config_contains_app(
	const struct orange_config *config,
	const char *app_id);
bool orange_dock_config_remove_visible(
	struct orange_config *config,
	int visible_index);
bool orange_dock_config_reorder_visible(
	struct orange_config *config,
	int from_visible_index,
	int to_visible_index);
bool orange_dock_config_insert_app(
	struct orange_config *config,
	const char *app_id,
	int visible_index);
void orange_dock_draw_drag_overlay(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);
#endif
