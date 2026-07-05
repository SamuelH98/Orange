#include "orange/launcher.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
#include "orange/dock.h"
#include "orange/glass.h"
#include "orange/shell.h"
#include "orange/util.h"

static double layout_scale(const struct orange_shell_layout *layout) {
	return clamp(ui_scale_for_size(layout->width, layout->height),
		ORANGE_MIN_UI_SCALE, ORANGE_MAX_UI_SCALE);
}

static void rounded_rect(cairo_t *cr, double x, double y,
		double width, double height, double radius) {
	double r = clamp(radius, 0.0, fmin(width, height) / 2.0);
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + width - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_arc(cr, x + width - r, y + height - r, r, 0.0, M_PI / 2.0);
	cairo_arc(cr, x + r, y + height - r, r, M_PI / 2.0, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static void draw_image_cover(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmax((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	rounded_rect(cr, r.x, r.y, r.width, r.height, r.width * 0.22);
	cairo_clip(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
	cairo_restore(cr);
}

static void draw_image_fit(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, double opacity) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmin((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint_with_alpha(cr, opacity);
	cairo_restore(cr);
}

static void draw_tinted_image_fit(cairo_t *cr, cairo_surface_t *surface,
		struct orange_rect r, double opacity, int red, int green, int blue) {
	int sw = cairo_image_surface_get_width(surface);
	int sh = cairo_image_surface_get_height(surface);
	if (sw <= 0 || sh <= 0) {
		return;
	}
	double scale = fmin((double)r.width / (double)sw,
		(double)r.height / (double)sh);
	double tx = r.x + ((double)r.width - sw * scale) * 0.5;
	double ty = r.y + ((double)r.height - sh * scale) * 0.5;
	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);
	cairo_set_source_rgba(cr,
		red / 255.0,
		green / 255.0,
		blue / 255.0,
		opacity);
	cairo_mask_surface(cr, surface, 0, 0);
	cairo_restore(cr);
}

static void draw_round_mode_button(cairo_t *cr,
		struct orange_rect r,
		double s,
		bool dark,
		cairo_surface_t *icon,
		const char *fallback_label) {
	orange_glass_draw(cr, &r, r.height / 2.0, dark);
	cairo_save(cr);
	cairo_arc(cr, r.x + r.width / 2.0, r.y + r.height / 2.0,
		r.width / 2.0 - fmax(0.5, 0.8 * s), 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.18 : 0.46);
	cairo_set_line_width(cr, fmax(0.6, 1.0 * s));
	cairo_stroke(cr);
	cairo_restore(cr);

	int icon_pad = scaled_i(10, s);
	if (icon_pad * 2 > r.width - 8) {
		icon_pad = (r.width - 8) / 2;
	}
	struct orange_rect icon_rect = {
		r.x + icon_pad,
		r.y + icon_pad,
		r.width - icon_pad * 2,
		r.height - icon_pad * 2,
	};
	if (icon != NULL) {
		draw_image_fit(cr, icon, icon_rect, dark ? 0.84 : 0.76);
		return;
	}
	if (fallback_label != NULL && fallback_label[0] != '\0') {
		double font_size = fmax(12.0, 18.0 * s);
		cairo_text_extents_t te;
		cairo_select_font_face(cr, "Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, font_size);
		cairo_text_extents(cr, fallback_label, &te);
		draw_text(cr, fallback_label,
			r.x + (r.width - te.width) / 2.0 - te.x_bearing,
			r.y + (r.height - te.height) / 2.0 - te.y_bearing,
			font_size,
			dark ? 210 : 64,
			dark ? 224 : 74,
			dark ? 245 : 90,
			0.78, true);
	}
}

static int wrap_label_two_lines(cairo_t *cr, const char *text,
		double max_width, char *out1, size_t out1_sz,
		char *out2, size_t out2_sz) {
	if (out1 == NULL || out1_sz == 0 || out2 == NULL || out2_sz == 0) {
		return 0;
	}
	out1[0] = '\0';
	out2[0] = '\0';
	if (text == NULL || text[0] == '\0') {
		return 0;
	}
	cairo_text_extents_t extents;
	cairo_text_extents(cr, text, &extents);
	if (extents.width <= max_width) {
		snprintf(out1, out1_sz, "%s", text);
		return 1;
	}
	int len = (int)strlen(text);
	int best = -1;
	for (int i = len - 1; i > 0; i--) {
		if (text[i] != ' ') {
			continue;
		}
		char trial[256];
		snprintf(trial, sizeof(trial), "%.*s", i, text);
		cairo_text_extents(cr, trial, &extents);
		if (extents.width <= max_width) {
			best = i;
			break;
		}
	}
	if (best > 0) {
		snprintf(out1, out1_sz, "%.*s", best, text);
		snprintf(out2, out2_sz, "%s", text + best + 1);
	} else {
		snprintf(out1, out1_sz, "%.10s\u2026", text);
	}
	return out2[0] != '\0' ? 2 : 1;
}

static void ellipsize_to_width(cairo_t *cr, const char *text,
		double max_width, char *out, size_t out_sz) {
	if (out_sz == 0) {
		return;
	}
	snprintf(out, out_sz, "%s", text != NULL ? text : "");
	cairo_text_extents_t ext;
	cairo_text_extents(cr, out, &ext);
	if (ext.width <= max_width) {
		return;
	}
	const char *suffix = "...";
	size_t suffix_len = strlen(suffix);
	size_t len = strlen(out);
	while (len > 0) {
		if (len + suffix_len + 1 <= out_sz) {
			memcpy(out + len, suffix, suffix_len + 1);
		}
		cairo_text_extents(cr, out, &ext);
		if (ext.width <= max_width) {
			return;
		}
		len--;
		out[len] = '\0';
	}
	snprintf(out, out_sz, "%s", suffix);
}

static void draw_search_glass(cairo_t *cr,
		double x, double y, double w, double h, bool focused, double s,
		bool dark) {
	(void)focused;
	struct orange_rect rect = {
		(int)lrint(x),
		(int)lrint(y),
		(int)lrint(w),
		(int)lrint(h),
	};
	orange_glass_draw(cr, &rect, h / 2.0, dark);
	cairo_save(cr);
	rounded_rect(cr, x, y, w, h, h / 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.16 : 0.52);
	cairo_set_line_width(cr, fmax(0.5, 0.8 * s));
	cairo_stroke(cr);
	cairo_restore(cr);
}

static const char *launcher_mode_title(int mode) {
	switch (mode) {
	case ORANGE_LAUNCHER_MODE_FILES:
		return "Files";
	case ORANGE_LAUNCHER_MODE_ACTIONS:
		return "Actions";
	case ORANGE_LAUNCHER_MODE_CLIPBOARD:
		return "Clipboard";
	case ORANGE_LAUNCHER_MODE_APPS:
	default:
		return "Applications";
	}
}

static const char *launcher_compact_mode_icon_name(int mode) {
	switch (mode) {
	case ORANGE_LAUNCHER_MODE_FILES:
		return "folder";
	case ORANGE_LAUNCHER_MODE_ACTIONS:
		return "system-run";
	case ORANGE_LAUNCHER_MODE_CLIPBOARD:
		return "edit-copy";
	case ORANGE_LAUNCHER_MODE_APPS:
	default:
		return "system-software-install";
	}
}

static void draw_mode_chip(cairo_t *cr,
		struct orange_rect r,
		double s,
		bool dark,
		bool active,
		cairo_surface_t *icon,
		const char *label) {
	if (r.width <= 0 || r.height <= 0 || label == NULL) {
		return;
	}
	cairo_save(cr);
	rounded_rect(cr, r.x, r.y, r.width, r.height, r.height / 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
		active ? (dark ? 0.22 : 0.42) : (dark ? 0.08 : 0.18));
	cairo_fill(cr);
	rounded_rect(cr, r.x + 0.5, r.y + 0.5,
		r.width - 1.0, r.height - 1.0, r.height / 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
		active ? 0.34 : 0.14);
	cairo_set_line_width(cr, fmax(0.6, 1.0 * s));
	cairo_stroke(cr);
	cairo_restore(cr);

	int icon_size = (int)clamp((double)r.height * 0.54, 14.0, 22.0);
	int icon_x = r.x + (int)fmax(10.0, 14.0 * s);
	int icon_y = r.y + (r.height - icon_size) / 2;
	if (icon != NULL) {
		draw_tinted_image_fit(cr, icon,
			(struct orange_rect){icon_x, icon_y, icon_size, icon_size},
			active ? 0.96 : 0.72, 255, 255, 255);
	}

	double font_size = fmax(11.0, 18.0 * s);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		active ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	double text_x = icon != NULL ?
		icon_x + icon_size + fmax(6.0, 8.0 * s) :
		r.x + fmax(12.0, 16.0 * s);
	double text_max = r.x + r.width - text_x - fmax(10.0, 14.0 * s);
	char fitted[64];
	ellipsize_to_width(cr, label, text_max, fitted, sizeof(fitted));
	cairo_text_extents_t ext;
	cairo_text_extents(cr, fitted, &ext);
	draw_text(cr, fitted, text_x,
		r.y + (r.height - ext.height) / 2.0 - ext.y_bearing,
		font_size, 255, 255, 255,
		active ? 0.96 : 0.76, active);
}

static void draw_launcher_result_row(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		struct orange_rect row,
		int result_index,
		bool dark,
		double s) {
	if (state == NULL || result_index < 0 ||
			result_index >= state->launcher_result_count ||
			result_index >= ORANGE_LAUNCHER_RESULT_MAX) {
		return;
	}
	const struct orange_launcher_result *result =
		&state->launcher_results[result_index];
	bool hot = result_index == state->launcher_hot_app;
	int inset_x = (int)fmax(4.0, 8.0 * s);
	struct orange_rect bg = {
		row.x + inset_x,
		row.y + (int)fmax(2.0, 4.0 * s),
		row.width - inset_x * 2,
		row.height - (int)fmax(4.0, 8.0 * s),
	};
	if (hot) {
		cairo_save(cr);
		rounded_rect(cr, bg.x, bg.y, bg.width, bg.height,
			fmax(12.0, 18.0 * s));
		cairo_set_source_rgba(cr, dark ? 0.26 : 0.68,
			dark ? 0.38 : 0.82, dark ? 0.68 : 1.00,
			dark ? 0.30 : 0.34);
		cairo_fill(cr);
		cairo_restore(cr);
	}

	int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	cairo_surface_t *icon = NULL;
	if (state->assets != NULL && result->icon_name[0] != '\0') {
		icon = orange_assets_icon(state->assets, variant, result->icon_name);
	}
	if (icon == NULL && state->assets != NULL) {
		icon = orange_assets_icon(state->assets, variant,
			result->kind == ORANGE_LAUNCHER_RESULT_FILE ?
				"text-x-generic" : "application-x-executable");
	}
	int icon_size = layout->launcher_grid_icon_size > 0 ?
		layout->launcher_grid_icon_size : (int)clamp(42.0 * s, 34.0, 48.0);
	struct orange_rect icon_rect = {
		bg.x + (int)fmax(12.0, 16.0 * s),
		bg.y + (bg.height - icon_size) / 2,
		icon_size,
		icon_size,
	};
	if (icon != NULL) {
		draw_image_fit(cr, icon, icon_rect, 0.92);
	}

	double title_size = fmax(13.0, 21.0 * s);
	double subtitle_size = fmax(10.0, 15.0 * s);
	double text_x = icon_rect.x + icon_rect.width + fmax(12.0, 18.0 * s);
	double text_w = bg.x + bg.width - text_x - fmax(12.0, 18.0 * s);
	char title[ORANGE_LAUNCHER_RESULT_LABEL_MAX];
	char subtitle[ORANGE_LAUNCHER_RESULT_SUBTITLE_MAX];
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, title_size);
	ellipsize_to_width(cr, result->label, text_w, title, sizeof(title));
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, subtitle_size);
	ellipsize_to_width(cr, result->subtitle, text_w,
		subtitle, sizeof(subtitle));
	double title_y = bg.y + bg.height * 0.43;
	double subtitle_y = bg.y + bg.height * 0.70;
	draw_text(cr, title, text_x, title_y, title_size,
		255, 255, 255, hot ? 0.98 : 0.92, true);
	if (subtitle[0] != '\0') {
		draw_text(cr, subtitle, text_x, subtitle_y, subtitle_size,
			255, 255, 255, hot ? 0.72 : 0.56, false);
	}
}

void orange_launcher_draw(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (!layout->launcher_visible) {
		return;
	}
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	bool is_panel = layout->launcher_display_mode ==
		ORANGE_LAUNCHER_DISPLAY_FULL;

	if (is_panel) {
		struct orange_rect panel = layout->launcher_panel;
		struct orange_rect field = layout->launcher_search_field;
		bool focused = state->launcher_search_focus;
		draw_search_glass(cr, field.x, field.y, field.width, field.height,
			focused, s, dark);

		cairo_surface_t *search_icon = state->assets != NULL ?
			orange_assets_icon(state->assets,
				dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
				"edit-find") : NULL;
		if (search_icon != NULL) {
			int search_icon_size = (int)clamp(34.0 * s, 20.0,
				(double)field.height - 12.0);
			int search_icon_left = (int)fmax(12.0, 18.0 * s);
			struct orange_rect search_icon_rect = {
				field.x + search_icon_left,
				field.y + (field.height - search_icon_size) / 2,
				search_icon_size,
				search_icon_size,
			};
			draw_tinted_image_fit(cr, search_icon, search_icon_rect,
				0.90, 255, 255, 255);
		}

		double font_size = clamp(30.0 * s, 17.0,
			(double)field.height * 0.50);
		cairo_select_font_face(cr, "Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, font_size);
		double text_x = field.x + fmax(42.0, 58.0 * s);
		double text_baseline = field.y +
			(field.height - font_size) * 0.5 + font_size * 0.78;
		const char *query = state->launcher_query;
		const char *placeholder = state->launcher_current_mode ==
			ORANGE_LAUNCHER_MODE_APPS ?
			"Search apps, files, actions, and web" :
			launcher_mode_title(state->launcher_current_mode);
		const char *text = query != NULL && query[0] != '\0' ?
			query : placeholder;
		double text_max_w = (double)(field.x + field.width) - text_x -
			fmax(54.0, 72.0 * s);
		if (text_max_w < 24.0) {
			text_max_w = 24.0;
		}
		char fitted_text[128];
		ellipsize_to_width(cr, text, text_max_w,
			fitted_text, sizeof(fitted_text));
		draw_text(cr, fitted_text, text_x, text_baseline,
			font_size, 255, 255, 255,
			query != NULL && query[0] != '\0' ? 0.98 : 0.82, false);
		if (focused && query != NULL && query[0] != '\0') {
			cairo_text_extents_t qext;
			cairo_text_extents(cr, fitted_text, &qext);
			double caret_x = text_x + qext.width + scaled_i(2, s);
			set_source_rgba255(cr, 255, 255, 255, 0.9);
			cairo_rectangle(cr, caret_x,
				text_baseline - font_size * 0.78,
				fmax(1.0, scaled_i(1.5, s)), font_size * 0.78);
			cairo_fill(cr);
		}

		double radius = fmax(18.0, 34.0 * s);
		orange_glass_draw(cr, &panel, radius, dark);
		rounded_rect(cr, panel.x + 0.5, panel.y + 0.5,
			panel.width - 1.0, panel.height - 1.0, radius);
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.15 : 0.18);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);

		for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
			struct orange_rect chip = layout->launcher_mode_buttons[i];
			if (chip.width <= 0) {
				continue;
			}
			cairo_surface_t *mode_icon = state->assets != NULL ?
				orange_assets_icon(state->assets,
					dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
					launcher_compact_mode_icon_name(i)) : NULL;
			draw_mode_chip(cr, chip, s, dark,
				i == state->launcher_current_mode,
				mode_icon, launcher_mode_title(i));
		}

		for (int i = 0; i < layout->launcher_category_count; i++) {
			struct orange_rect tab = layout->launcher_category_tabs[i];
			if (tab.width <= 0 || i >= state->launcher_category_count) {
				continue;
			}
			bool active = i == layout->launcher_category_active;
			cairo_save(cr);
			rounded_rect(cr, tab.x, tab.y, tab.width, tab.height,
				tab.height / 2.0);
			if (active) {
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
					dark ? 0.20 : 0.36);
			} else {
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
					dark ? 0.08 : 0.18);
			}
			cairo_fill(cr);
			rounded_rect(cr, tab.x + 0.5, tab.y + 0.5,
				tab.width - 1.0, tab.height - 1.0, tab.height / 2.0);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
				active ? 0.30 : 0.16);
			cairo_set_line_width(cr, fmax(0.6, 1.0 * s));
			cairo_stroke(cr);
			cairo_restore(cr);

			const char *label = state->launcher_categories[i];
			double tab_font = fmax(11.0, 20.0 * s);
			cairo_select_font_face(cr, "Sans",
				CAIRO_FONT_SLANT_NORMAL,
				active ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, tab_font);
			char fitted_label[128];
			ellipsize_to_width(cr, label,
				(double)tab.width - fmax(12.0, 20.0 * s),
				fitted_label, sizeof(fitted_label));
			cairo_text_extents_t te;
			cairo_text_extents(cr, fitted_label, &te);
			draw_text(cr, fitted_label,
				tab.x + (tab.width - te.width) / 2.0 - te.x_bearing,
				tab.y + (tab.height - te.height) / 2.0 - te.y_bearing,
				tab_font, 255, 255, 255, active ? 0.98 : 0.78, active);
		}

		int divider_y = layout->launcher_viewport.y - scaled_i(9, s);
		cairo_save(cr);
		set_source_rgba255(cr, 255, 255, 255, dark ? 0.13 : 0.34);
		cairo_set_line_width(cr, fmax(0.7, 1.1 * s));
		cairo_move_to(cr, panel.x + scaled_i(8, s), divider_y);
		cairo_line_to(cr, panel.x + panel.width - scaled_i(8, s), divider_y);
		cairo_stroke(cr);
		cairo_restore(cr);

		cairo_save(cr);
		cairo_rectangle(cr, layout->launcher_viewport.x,
			layout->launcher_viewport.y,
			layout->launcher_viewport.width,
			layout->launcher_viewport.height);
		cairo_clip(cr);

		if (state->launcher_app_count == 0) {
			double empty_size = fmax(13.0, 22.0 * s);
			draw_text(cr, "No results",
				layout->launcher_viewport.x + scaled_i(12, s),
				layout->launcher_viewport.y + scaled_i(34, s),
				empty_size, 255, 255, 255, 0.58, false);
		}

		if (layout->launcher_list_results) {
			for (int i = 0; i < layout->launcher_grid_cell_count; i++) {
				int result_index = layout->launcher_grid_indices[i];
				if (result_index < 0 ||
						result_index >= state->launcher_result_count) {
					continue;
				}
				draw_launcher_result_row(cr, layout, state,
					layout->launcher_grid_cells[i], result_index,
					dark, s);
			}
			cairo_restore(cr);
			if (layout->launcher_scroll_track.width > 0 &&
					layout->launcher_scroll_thumb.width > 0) {
				struct orange_rect track = layout->launcher_scroll_track;
				struct orange_rect thumb = layout->launcher_scroll_thumb;
				double visual_w = fmax(3.0, 5.0 * s);
				double track_x = track.x + (track.width - visual_w) / 2.0;
				double thumb_x = thumb.x + (thumb.width - visual_w) / 2.0;
				cairo_save(cr);
				rounded_rect(cr, track_x, track.y, visual_w, track.height,
					visual_w / 2.0);
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
					dark ? 0.07 : 0.16);
				cairo_fill(cr);
				rounded_rect(cr, thumb_x, thumb.y, visual_w, thumb.height,
					visual_w / 2.0);
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
					dark ? 0.42 : 0.64);
				cairo_fill(cr);
				cairo_restore(cr);
			}
			return;
		}

		for (int i = 0; i < layout->launcher_grid_cell_count; i++) {
			int app_index = layout->launcher_grid_indices[i];
			if (app_index < 0 || app_index >= state->launcher_app_count) {
				continue;
			}
			struct orange_rect cell = layout->launcher_grid_cells[i];
			const char *label = orange_launcher_app_label(
				state->desktop_entries, state->desktop_entry_count,
				state->launcher_app_indices, state->launcher_app_count,
				app_index);
			const char *icon_name = orange_launcher_app_icon(
				state->desktop_entries, state->desktop_entry_count,
				state->launcher_app_indices, state->launcher_app_count,
				app_index);
			int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
			cairo_surface_t *icon = NULL;
			if (state->assets != NULL && icon_name != NULL) {
				icon = orange_assets_icon(state->assets, variant, icon_name);
			}
			if (icon == NULL && state->assets != NULL) {
				icon = orange_assets_icon(state->assets, variant,
					"application-x-executable");
			}

			double icon_size = (double)layout->launcher_grid_icon_size;
			int icon_top_pad = (int)fmax(4.0, 8.0 * s);
			struct orange_rect icon_rect = {
				cell.x + (cell.width - (int)icon_size) / 2,
				cell.y + icon_top_pad,
				(int)icon_size,
				(int)icon_size,
			};
			if (app_index == state->launcher_hot_app) {
				cairo_save(cr);
				rounded_rect(cr,
					icon_rect.x - scaled_i(8, s),
					icon_rect.y - scaled_i(8, s),
					icon_rect.width + scaled_i(16, s),
					icon_rect.height + scaled_i(16, s),
					icon_rect.width * 0.26);
				cairo_set_source_rgba(cr, dark ? 0.20 : 0.72,
					dark ? 0.34 : 0.84, dark ? 0.62 : 1.00,
					dark ? 0.28 : 0.34);
				cairo_fill(cr);
				cairo_restore(cr);
			}
			if (icon != NULL) {
				draw_image_cover(cr, icon, icon_rect);
			}

			if (label != NULL) {
				double label_font = fmax(11.0, 20.0 * s);
				cairo_select_font_face(cr, "Sans",
					CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
				cairo_set_font_size(cr, label_font);
				char line1[128];
				char line2[128];
				int lines = wrap_label_two_lines(cr, label,
					(double)cell.width - scaled_i(8, s),
					line1, sizeof(line1), line2, sizeof(line2));
				double icon_label_gap = fmax(8.0, 14.0 * s);
				double label_y = icon_rect.y + icon_rect.height +
					icon_label_gap + label_font * 0.74;
				const char *draw_lines[2] = {line1, line2};
				for (int line = 0; line < lines && line < 2; line++) {
					char fitted[128];
					ellipsize_to_width(cr, draw_lines[line],
						(double)cell.width - scaled_i(8, s),
						fitted, sizeof(fitted));
					cairo_text_extents_t le;
					cairo_text_extents(cr, fitted, &le);
					draw_text(cr, fitted,
						cell.x + (cell.width - le.width) / 2.0 - le.x_bearing,
						label_y + line * label_font * 1.12,
						label_font,
						255, 255, 255,
						0.96, true);
				}
			}
		}
		cairo_restore(cr);

		if (layout->launcher_scroll_track.width > 0 &&
				layout->launcher_scroll_thumb.width > 0) {
			struct orange_rect track = layout->launcher_scroll_track;
			struct orange_rect thumb = layout->launcher_scroll_thumb;
			double visual_w = fmax(3.0, 5.0 * s);
			double track_x = track.x + (track.width - visual_w) / 2.0;
			double thumb_x = thumb.x + (thumb.width - visual_w) / 2.0;
			cairo_save(cr);
			rounded_rect(cr, track_x, track.y, visual_w, track.height,
				visual_w / 2.0);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
				dark ? 0.07 : 0.16);
			cairo_fill(cr);
			rounded_rect(cr, thumb_x, thumb.y, visual_w, thumb.height,
				visual_w / 2.0);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0,
				dark ? 0.42 : 0.64);
			cairo_fill(cr);
			cairo_restore(cr);
		}
		return;
	}

	/* Search field */
	struct orange_rect field = layout->launcher_search_field;
	bool focused = state->launcher_search_focus;
	draw_search_glass(cr, field.x, field.y, field.width, field.height,
		focused, s, dark);

	cairo_surface_t *search_icon = state->assets != NULL ?
		orange_assets_icon(state->assets,
			dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
			"edit-find") : NULL;
	if (search_icon != NULL) {
		int search_icon_size = (int)clamp(34.0 * s, 20.0,
			(double)field.height - 12.0);
		int search_icon_left = (int)fmax(12.0, 18.0 * s);
		struct orange_rect search_icon_rect = {
			field.x + search_icon_left,
			field.y + (field.height - search_icon_size) / 2,
			search_icon_size,
			search_icon_size,
		};
		draw_tinted_image_fit(cr, search_icon, search_icon_rect,
			0.90, 255, 255, 255);
	}

	double font_size = clamp(30.0 * s, 17.0,
		(double)field.height * 0.50);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	double text_x = field.x + fmax(42.0, 58.0 * s);
	double text_baseline = field.y +
		(field.height - font_size) * 0.5 + font_size * 0.78;
	const char *query = state->launcher_query;
	const char *placeholder = state->launcher_current_mode ==
		ORANGE_LAUNCHER_MODE_APPS ?
		"Search apps, files, actions, and web" :
		launcher_mode_title(state->launcher_current_mode);
	const char *text = query != NULL && query[0] != '\0' ?
		query : placeholder;
	double text_max_w = (double)(field.x + field.width) - text_x -
		fmax(54.0, 72.0 * s);
	if (text_max_w < 24.0) {
		text_max_w = 24.0;
	}
	char fitted_text[128];
	ellipsize_to_width(cr, text, text_max_w,
		fitted_text, sizeof(fitted_text));
	draw_text(cr, fitted_text, text_x, text_baseline,
		font_size, 255, 255, 255,
		query != NULL && query[0] != '\0' ? 0.98 : 0.82, false);
	if (focused && query != NULL && query[0] != '\0') {
		cairo_text_extents_t qext;
		cairo_text_extents(cr, fitted_text, &qext);
		double caret_x = text_x + qext.width + scaled_i(2, s);
		set_source_rgba255(cr, 255, 255, 255, 0.9);
		cairo_rectangle(cr, caret_x,
			text_baseline - font_size * 0.78,
			fmax(1.0, scaled_i(1.5, s)), font_size * 0.78);
		cairo_fill(cr);
	}

	static const char *const fallback_labels[] = {"A", "F", "S", "C"};
	for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
		struct orange_rect btn = layout->launcher_mode_buttons[i];
		if (btn.width <= 0) {
			continue;
		}
		cairo_surface_t *mode_icon = state->assets != NULL ?
			orange_assets_icon(state->assets,
				dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
				launcher_compact_mode_icon_name(i)) : NULL;
		draw_round_mode_button(cr, btn, s, dark, mode_icon,
			i < (int)(sizeof(fallback_labels) / sizeof(fallback_labels[0])) ?
				fallback_labels[i] : "?");
	}
	return;
}

void orange_launcher_draw_app_drag_overlay(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	if (state == NULL || !state->launcher_app_drag_active ||
			state->launcher_app_drag_entry_index < 0 ||
			state->launcher_app_drag_entry_index >= state->desktop_entry_count) {
		return;
	}
	double s = layout_scale(layout);
	bool dark = is_dark_config(config);
	int icon_size = layout->launcher_grid_icon_size > 0 ?
		layout->launcher_grid_icon_size : scaled_i(92, s);
	icon_size = (int)clamp((double)icon_size, 42.0, 128.0 * s);
	struct orange_rect ghost = {
		state->launcher_app_drag_x - icon_size / 2,
		state->launcher_app_drag_y - icon_size / 2,
		icon_size,
		icon_size,
	};

	const struct orange_desktop_entry *entry =
		&state->desktop_entries[state->launcher_app_drag_entry_index];
	const char *icon_name = entry->icon[0] != '\0' ?
		entry->icon : "application-x-executable";
	int variant = dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT;
	cairo_surface_t *icon = state->assets != NULL ?
		orange_assets_icon(state->assets, variant, icon_name) : NULL;
	if (icon == NULL && state->assets != NULL) {
		icon = orange_assets_icon(state->assets, variant,
			"application-x-executable");
	}
	if (icon == NULL) {
		return;
	}

	cairo_push_group(cr);
	draw_image_cover(cr, icon, ghost);
	cairo_pop_group_to_source(cr);
	cairo_paint_with_alpha(cr, 0.82);
}

struct launcher_sort_ctx {
	const struct orange_desktop_entry *entries;
	int entry_count;
};

static int launcher_sort_compare(const void *pa, const void *pb, void *ctx) {
	struct launcher_sort_ctx *c = (struct launcher_sort_ctx *)ctx;
	int ia = *(const int *)pa;
	int ib = *(const int *)pb;
	const char *na = (ia >= 0 && ia < c->entry_count) ?
		c->entries[ia].name : "";
	const char *nb = (ib >= 0 && ib < c->entry_count) ?
		c->entries[ib].name : "";
	return strcasecmp(na, nb);
}

static bool category_has_token(const char *categories, const char *token) {
	if (categories == NULL || token == NULL || token[0] == '\0') {
		return false;
	}
	const char *p = categories;
	size_t token_len = strlen(token);
	while (*p != '\0') {
		while (*p == ';' || *p == ' ' || *p == '\t') {
			p++;
		}
		const char *start = p;
		while (*p != '\0' && *p != ';') {
			p++;
		}
		size_t len = (size_t)(p - start);
		while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
			len--;
		}
		if (len == token_len && strncasecmp(start, token, len) == 0) {
			return true;
		}
	}
	return false;
}

static bool category_matches_any(
		const char *categories,
		const char *const *tokens,
		int token_count) {
	for (int i = 0; i < token_count; i++) {
		if (category_has_token(categories, tokens[i])) {
			return true;
		}
	}
	return false;
}

static bool entry_matches_category_filter(
		const struct orange_desktop_entry *entry,
		const char *filter) {
	if (filter == NULL || filter[0] == '\0' ||
			strcasecmp(filter, "All") == 0) {
		return true;
	}
	const char *categories = entry != NULL ? entry->categories : NULL;
	if (categories == NULL || categories[0] == '\0') {
		return false;
	}
	if (strcasecmp(filter, "Utilities") == 0) {
		static const char *const tokens[] = {
			"Utility", "System", "Settings",
		};
		return category_matches_any(categories, tokens, 3);
	}
	if (strcasecmp(filter, "Productivity & Finance") == 0 ||
			strcasecmp(filter, "Productivity") == 0) {
		static const char *const tokens[] = {
			"Office", "Finance", "Calendar", "ContactManagement",
		};
		return category_matches_any(categories, tokens, 4);
	}
	if (strcasecmp(filter, "Creativity") == 0) {
		static const char *const tokens[] = {
			"Graphics", "2DGraphics", "VectorGraphics",
			"RasterGraphics", "Art", "Design", "Development",
		};
		return category_matches_any(categories, tokens, 7);
	}
	if (strcasecmp(filter, "Social") == 0) {
		static const char *const tokens[] = {
			"Network", "Chat", "Email", "InstantMessaging",
		};
		return category_matches_any(categories, tokens, 4);
	}
	if (strcasecmp(filter, "Entertainment") == 0 ||
			strcasecmp(filter, "Media") == 0) {
		static const char *const tokens[] = {
			"AudioVideo", "Audio", "Video", "Game", "Player", "Music",
		};
		return category_matches_any(categories, tokens, 6);
	}
	if (strcasecmp(filter, "Photo & Video") == 0 ||
			strcasecmp(filter, "Photos") == 0) {
		static const char *const tokens[] = {
			"Photography", "Video", "AudioVideo", "Viewer",
			"RasterGraphics",
		};
		return category_matches_any(categories, tokens, 5);
	}
	if (strcasecmp(filter, "Information & Reading") == 0 ||
			strcasecmp(filter, "Information") == 0 ||
			strcasecmp(filter, "Info") == 0) {
		static const char *const tokens[] = {
			"Education", "Science", "Reference", "Documentation",
			"News", "Literature",
		};
		return category_matches_any(categories, tokens, 6);
	}
	return category_has_token(categories, filter);
}

int orange_launcher_filter(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const char *query,
		const char *category_filter,
		int *out_indices,
		int out_capacity) {
	if (out_indices == NULL || out_capacity <= 0) {
		return 0;
	}
	int count = 0;
	uint32_t seen_hashes[out_capacity];
	bool has_query = query != NULL && query[0] != '\0';
	for (int i = 0; i < entry_count && count < out_capacity; i++) {
		const struct orange_desktop_entry *e = &entries[i];
		if (e->name[0] == '\0' || e->exec[0] == '\0') {
			continue;
		}
		if (!orange_desktop_entry_should_show(e,
				getenv("XDG_CURRENT_DESKTOP"))) {
			continue;
		}
		if (!entry_matches_category_filter(e, category_filter)) {
			continue;
		}
		bool match = true;
		if (has_query) {
			match = strcasestr(e->name, query) != NULL ||
				strcasestr(e->id, query) != NULL;
		}
		if (!match) {
			continue;
		}
		bool dup = false;
		unsigned h = 2166136261u;
		for (const char *p = e->name; *p != '\0'; p++) {
			h ^= (unsigned)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
			h *= 16777619u;
		}
		for (int j = 0; j < count; j++) {
			if (seen_hashes[j] == h &&
					strcasecmp(entries[out_indices[j]].name, e->name) == 0) {
				dup = true;
				break;
			}
		}
		if (dup) {
			continue;
		}
		seen_hashes[count] = h;
		out_indices[count++] = i;
	}
	struct launcher_sort_ctx ctx = { entries, entry_count };
	qsort_r(out_indices, (size_t)count, sizeof(int),
		launcher_sort_compare, &ctx);
	return count;
}

static bool launcher_entry_matches_app_id(
		const struct orange_desktop_entry *entry,
		const char *app_id) {
	if (entry == NULL || app_id == NULL || app_id[0] == '\0') {
		return false;
	}
	return orange_desktop_entry_match_score(entry, app_id) > 0 ||
		(entry->name[0] != '\0' &&
		 strcasecmp(entry->name, app_id) == 0) ||
		(entry->icon[0] != '\0' &&
		 orange_desktop_entry_id_matches(entry->icon, app_id));
}

static bool launcher_entry_is_in_dock(
		const struct orange_desktop_entry *entry,
		const struct orange_config *dock_config,
		const char dock_temporary_app_ids[ORANGE_DOCK_MAX][128],
		int dock_temporary_count) {
	if (entry == NULL) {
		return false;
	}
	if (orange_dock_config_contains_app(dock_config, entry->id) ||
			orange_dock_config_contains_app(dock_config, entry->name) ||
			orange_dock_config_contains_app(dock_config, entry->icon)) {
		return true;
	}
	if (orange_dock_temporary_contains(
			dock_temporary_app_ids, dock_temporary_count, entry->id) ||
			orange_dock_temporary_contains(
				dock_temporary_app_ids, dock_temporary_count, entry->name) ||
			orange_dock_temporary_contains(
				dock_temporary_app_ids, dock_temporary_count, entry->icon)) {
		return true;
	}
	if (dock_temporary_app_ids == NULL) {
		return false;
	}
	for (int i = 0; i < dock_temporary_count && i < ORANGE_DOCK_MAX; i++) {
		if (launcher_entry_matches_app_id(entry, dock_temporary_app_ids[i])) {
			return true;
		}
	}
	return false;
}

int orange_launcher_filter_available(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const char *query,
		const char *category_filter,
		const struct orange_config *dock_config,
		const void *dock_temporary_app_ids,
		int dock_temporary_count,
		int *out_indices,
		int out_capacity) {
	const char (*temporary_ids)[128] =
		(const char (*)[128])dock_temporary_app_ids;
	int count = orange_launcher_filter(entries, entry_count, query,
		category_filter, out_indices, out_capacity);
	int out = 0;
	for (int i = 0; i < count; i++) {
		int idx = out_indices[i];
		if (idx < 0 || idx >= entry_count) {
			continue;
		}
		if (launcher_entry_is_in_dock(&entries[idx], dock_config,
				temporary_ids, dock_temporary_count)) {
			continue;
		}
		out_indices[out++] = idx;
	}
	return out;
}

struct launcher_action_def {
	const char *id;
	const char *label;
	const char *subtitle;
	const char *icon_name;
	const char *keywords;
};

static const struct launcher_action_def launcher_actions[] = {
	{
		"open-terminal",
		"Open Terminal",
		"Ghostty terminal",
		"utilities-terminal",
		"terminal ghostty shell command console",
	},
	{
		"open-files",
		"Open Files",
		"Home folder",
		"system-file-manager",
		"files finder file manager home folder",
	},
	{
		"open-documents",
		"Open Documents",
		"Documents folder",
		"folder-documents",
		"documents docs files folder",
	},
	{
		"open-downloads",
		"Open Downloads",
		"Downloads folder",
		"folder-download",
		"downloads downloaded files folder",
	},
	{
		"open-desktop",
		"Open Desktop",
		"Desktop folder",
		"user-desktop",
		"desktop files folder",
	},
	{
		"open-settings",
		"Open Settings",
		"System settings",
		"preferences-system",
		"settings preferences control center system",
	},
	{
		"open-software",
		"Open Software",
		"Install and manage apps",
		"system-software-install",
		"software store install apps packages",
	},
	{
		"recent-files",
		"Recent Files",
		"Open recent items",
		"document-open-recent",
		"recent files documents history",
	},
	{
		"new-folder",
		"New Folder",
		"Create a folder on the Desktop",
		"folder-new",
		"new folder create desktop",
	},
	{
		"paste-desktop",
		"Paste to Desktop",
		"Paste copied files onto the Desktop",
		"edit-paste",
		"paste clipboard desktop files copy",
	},
	{
		"lock-screen",
		"Lock Screen",
		"Secure this session",
		"system-lock-screen",
		"lock screen secure session",
	},
};

static bool text_matches_query(const char *text, const char *query) {
	if (query == NULL || query[0] == '\0') {
		return true;
	}
	return text != NULL && text[0] != '\0' &&
		strcasestr(text, query) != NULL;
}

static bool result_text_matches_query(
		const char *query,
		const char *label,
		const char *subtitle,
		const char *keywords) {
	if (query == NULL || query[0] == '\0') {
		return true;
	}
	return text_matches_query(label, query) ||
		text_matches_query(subtitle, query) ||
		text_matches_query(keywords, query);
}

static bool launcher_add_result(
		struct orange_launcher_result *out_results,
		int out_capacity,
		int *count,
		enum orange_launcher_result_kind kind,
		int source_index,
		const char *label,
		const char *subtitle,
		const char *icon_name,
		const char *action) {
	if (out_results == NULL || count == NULL ||
			*count < 0 || *count >= out_capacity ||
			label == NULL || label[0] == '\0') {
		return false;
	}
	struct orange_launcher_result *result = &out_results[*count];
	memset(result, 0, sizeof(*result));
	result->kind = kind;
	result->source_index = source_index;
	snprintf(result->label, sizeof(result->label), "%s", label);
	snprintf(result->subtitle, sizeof(result->subtitle), "%s",
		subtitle != NULL ? subtitle : "");
	snprintf(result->icon_name, sizeof(result->icon_name), "%s",
		icon_name != NULL && icon_name[0] != '\0' ?
			icon_name : "application-x-executable");
	snprintf(result->action, sizeof(result->action), "%s",
		action != NULL ? action : "");
	(*count)++;
	return true;
}

static int launcher_add_app_results(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const char *query,
		const char *category_filter,
		const struct orange_config *dock_config,
		const void *dock_temporary_app_ids,
		int dock_temporary_count,
		bool only_available,
		struct orange_launcher_result *out_results,
		int out_capacity,
		int count,
		int limit) {
	if (entries == NULL || entry_count <= 0 || count >= out_capacity ||
			limit <= 0) {
		return count;
	}
	int indices[ORANGE_LAUNCHER_APP_MAX];
	int index_count = only_available ?
		orange_launcher_filter_available(entries, entry_count, query,
			category_filter, dock_config, dock_temporary_app_ids,
			dock_temporary_count, indices, ORANGE_LAUNCHER_APP_MAX) :
		orange_launcher_filter(entries, entry_count, query,
			category_filter, indices, ORANGE_LAUNCHER_APP_MAX);
	for (int i = 0; i < index_count && count < out_capacity &&
			i < limit; i++) {
		int idx = indices[i];
		if (idx < 0 || idx >= entry_count) {
			continue;
		}
		const struct orange_desktop_entry *entry = &entries[idx];
		launcher_add_result(out_results, out_capacity, &count,
			ORANGE_LAUNCHER_RESULT_APP, idx, entry->name, "Application",
			entry->icon[0] != '\0' ?
				entry->icon : "application-x-executable",
			"");
	}
	return count;
}

static int launcher_add_file_results(
		const struct orange_file_info *files,
		int file_count,
		const char *query,
		struct orange_launcher_result *out_results,
		int out_capacity,
		int count,
		int limit) {
	if (files == NULL || file_count <= 0 || count >= out_capacity ||
			limit <= 0) {
		return count;
	}
	for (int i = 0; i < file_count && count < out_capacity &&
			limit > 0; i++) {
		const struct orange_file_info *file = &files[i];
		if (file->name[0] == '\0' || file->path[0] == '\0') {
			continue;
		}
		if (!result_text_matches_query(query,
				file->name, file->path,
				file->is_directory ? "folder directory" : "file document")) {
			continue;
		}
		launcher_add_result(out_results, out_capacity, &count,
			ORANGE_LAUNCHER_RESULT_FILE, i, file->name,
			file->is_directory ? "Folder on Desktop" : "File on Desktop",
			file->icon_name[0] != '\0' ? file->icon_name :
				(file->is_directory ? "folder" : "text-x-generic"),
			"");
		limit--;
	}
	return count;
}

static int launcher_add_action_results(
		const char *query,
		struct orange_launcher_result *out_results,
		int out_capacity,
		int count,
		int limit) {
	int action_count = (int)(sizeof(launcher_actions) /
		sizeof(launcher_actions[0]));
	for (int i = 0; i < action_count && count < out_capacity &&
			limit > 0; i++) {
		const struct launcher_action_def *action = &launcher_actions[i];
		if (!result_text_matches_query(query, action->label,
				action->subtitle, action->keywords)) {
			continue;
		}
		launcher_add_result(out_results, out_capacity, &count,
			ORANGE_LAUNCHER_RESULT_ACTION, i, action->label,
			action->subtitle, action->icon_name, action->id);
		limit--;
	}
	return count;
}

static int launcher_add_web_result(
		const char *query,
		struct orange_launcher_result *out_results,
		int out_capacity,
		int count) {
	if (query == NULL || query[0] == '\0' || count >= out_capacity) {
		return count;
	}
	char label[ORANGE_LAUNCHER_RESULT_LABEL_MAX];
	snprintf(label, sizeof(label), "Search Web for \"%s\"", query);
	launcher_add_result(out_results, out_capacity, &count,
		ORANGE_LAUNCHER_RESULT_WEB, -1, label,
		"Internet Search", "web-browser", query);
	return count;
}

int orange_launcher_build_results(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const struct orange_file_info *files,
		int file_count,
		const char *query,
		int mode,
		const char *category_filter,
		const struct orange_config *dock_config,
		const void *dock_temporary_app_ids,
		int dock_temporary_count,
		struct orange_launcher_result *out_results,
		int out_capacity) {
	if (out_results == NULL || out_capacity <= 0) {
		return 0;
	}
	for (int i = 0; i < out_capacity; i++) {
		memset(&out_results[i], 0, sizeof(out_results[i]));
	}
	if (mode < 0 || mode >= ORANGE_LAUNCHER_MODE_COUNT) {
		mode = ORANGE_LAUNCHER_MODE_APPS;
	}
	bool has_query = query != NULL && query[0] != '\0';
	int count = 0;

	if (mode == ORANGE_LAUNCHER_MODE_APPS) {
		count = launcher_add_app_results(entries, entry_count, query,
			has_query ? NULL : category_filter, dock_config,
			dock_temporary_app_ids, dock_temporary_count,
			!has_query, out_results, out_capacity, count,
			has_query ? 6 : out_capacity);
		if (has_query) {
			count = launcher_add_file_results(files, file_count, query,
				out_results, out_capacity, count, 5);
			count = launcher_add_action_results(query, out_results,
				out_capacity, count, 5);
			count = launcher_add_web_result(query, out_results,
				out_capacity, count);
		}
		return count;
	}

	if (mode == ORANGE_LAUNCHER_MODE_FILES) {
		count = launcher_add_file_results(files, file_count, query,
			out_results, out_capacity, count, out_capacity);
		if (has_query) {
			count = launcher_add_web_result(query, out_results,
				out_capacity, count);
		}
		return count;
	}

	if (mode == ORANGE_LAUNCHER_MODE_ACTIONS) {
		count = launcher_add_action_results(query, out_results,
			out_capacity, count, out_capacity);
		if (has_query) {
			count = launcher_add_web_result(query, out_results,
				out_capacity, count);
		}
		return count;
	}

	if (mode == ORANGE_LAUNCHER_MODE_CLIPBOARD) {
		count = launcher_add_action_results(
			has_query ? query : "paste clipboard",
			out_results, out_capacity, count, out_capacity);
		if (has_query) {
			count = launcher_add_web_result(query, out_results,
				out_capacity, count);
		}
		return count;
	}

	return count;
}

const char *orange_launcher_app_label(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const int *indices,
		int index_count,
		int visible_index) {
	(void)entry_count;
	if (indices == NULL || visible_index < 0 ||
			visible_index >= index_count) {
		return NULL;
	}
	int idx = indices[visible_index];
	if (idx < 0 || idx >= entry_count) {
		return NULL;
	}
	return entries[idx].name;
}

const char *orange_launcher_app_icon(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const int *indices,
		int index_count,
		int visible_index) {
	(void)entry_count;
	if (indices == NULL || visible_index < 0 ||
			visible_index >= index_count) {
		return NULL;
	}
	int idx = indices[visible_index];
	if (idx < 0 || idx >= entry_count) {
		return NULL;
	}
	if (entries[idx].icon[0] != '\0') {
		return entries[idx].icon;
	}
	return "application-x-executable";
}
