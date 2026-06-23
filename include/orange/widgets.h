#ifndef ORANGE_WIDGETS_H
#define ORANGE_WIDGETS_H

#include <cairo/cairo.h>

#include "orange/config.h"
#include "orange/shell.h"

void orange_widgets_draw_desktop_layer(cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

void orange_widgets_draw_notification_card(cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config,
	enum orange_notification_center_card_kind kind,
	struct orange_rect rect);

#endif
