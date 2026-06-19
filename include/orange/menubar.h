#ifndef ORANGE_MENUBAR_H
#define ORANGE_MENUBAR_H

#include "orange/shell.h"

void orange_menubar_draw(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

void orange_menubar_draw_system_menu(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

void orange_menubar_draw_context_menu(
	cairo_t *cr,
	const struct orange_shell_layout *layout,
	const struct orange_shell_state *state,
	const struct orange_config *config);

const char *orange_menubar_active_app_label(const struct orange_shell_state *state);
const char *orange_menubar_menu_label(int index);
const char *orange_menubar_context_menu_label(
	enum orange_context_menu_kind kind,
	int index);
const char *orange_menubar_context_menu_icon_name(
	enum orange_context_menu_kind kind,
	int index);
const char *orange_menubar_app_menu_tab_label(
	const struct orange_app_menu_model *app_menu,
	int tab,
	const char *active_app_label);
int orange_menubar_app_menu_item_count(
	enum orange_context_menu_kind kind,
	const struct orange_app_menu_model *app_menu);
enum orange_app_menu_tab orange_menubar_tab_for_context_kind(
	enum orange_context_menu_kind kind);

#endif
