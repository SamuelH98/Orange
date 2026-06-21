#include "orange/launcher.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
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

static void draw_more_icon(cairo_t *cr, cairo_surface_t *icon,
		struct orange_rect r, double s, bool dark) {
	if (icon != NULL) {
		draw_image_fit(cr, icon, r, dark ? 0.78 : 0.72);
		return;
	}
	cairo_save(cr);
	double dot_r = fmax(1.5, 2.0 * s);
	double cx = r.x + r.width / 2.0;
	double cy = r.y + r.height / 2.0;
	double gap = fmax(5.0, 7.0 * s);
	set_source_rgba255(cr,
		dark ? 208 : 82,
		dark ? 216 : 92,
		dark ? 236 : 116,
		dark ? 0.82 : 0.72);
	for (int i = -1; i <= 1; i++) {
		cairo_arc(cr, cx + i * gap, cy, dot_r, 0, 2.0 * M_PI);
		cairo_fill(cr);
	}
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
		return "Shortcuts";
	case ORANGE_LAUNCHER_MODE_CLIPBOARD:
		return "Clipboard";
	case ORANGE_LAUNCHER_MODE_APPS:
	default:
		return "Applications";
	}
}

static const char *launcher_mode_icon_name(int mode) {
	switch (mode) {
	case ORANGE_LAUNCHER_MODE_FILES:
		return "system-file-manager";
	case ORANGE_LAUNCHER_MODE_ACTIONS:
		return "preferences-system";
	case ORANGE_LAUNCHER_MODE_CLIPBOARD:
		return "edit-paste";
	case ORANGE_LAUNCHER_MODE_APPS:
	default:
		return "view-app-grid";
	}
}

static const char *launcher_compact_mode_icon_name(int mode) {
	switch (mode) {
	case ORANGE_LAUNCHER_MODE_FILES:
		return "folder";
	case ORANGE_LAUNCHER_MODE_ACTIONS:
		return "view-grid-symbolic";
	case ORANGE_LAUNCHER_MODE_CLIPBOARD:
		return "edit-copy";
	case ORANGE_LAUNCHER_MODE_APPS:
	default:
		return "system-software-install";
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
		double radius = fmax(16.0, 30.0 * s);
		orange_glass_draw(cr, &panel, radius, dark);
		rounded_rect(cr, panel.x + 0.5, panel.y + 0.5,
			panel.width - 1.0, panel.height - 1.0, radius);
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, dark ? 0.15 : 0.18);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);

		struct orange_rect field = layout->launcher_search_field;
		int header_icon_size = (int)clamp(36.0 * s, 26.0,
			(double)field.height - 10.0);
		struct orange_rect icon_rect = {
			field.x,
			field.y + (field.height - header_icon_size) / 2,
			header_icon_size,
			header_icon_size,
		};
		cairo_surface_t *title_icon = state->assets != NULL ?
			orange_assets_icon(state->assets,
				dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
				launcher_mode_icon_name(state->launcher_current_mode)) : NULL;
		if (title_icon != NULL) {
			draw_image_fit(cr, title_icon, icon_rect, dark ? 0.88 : 0.82);
		}
		double divider_x = icon_rect.x + icon_rect.width +
			fmax(10.0, 18.0 * s);
		cairo_save(cr);
		set_source_rgba255(cr, 255, 255, 255, dark ? 0.18 : 0.44);
		cairo_set_line_width(cr, fmax(0.6, 1.0 * s));
		cairo_move_to(cr, divider_x, field.y + scaled_i(10, s));
		cairo_line_to(cr, divider_x, field.y + field.height - scaled_i(10, s));
		cairo_stroke(cr);
		cairo_restore(cr);

		const char *query = state->launcher_query;
		const char *title = query != NULL && query[0] != '\0' ?
			query : launcher_mode_title(state->launcher_current_mode);
		double title_size = clamp(36.0 * s, 20.0,
			(double)field.height * 0.56);
		cairo_select_font_face(cr, "Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, title_size);
		double title_x = divider_x + fmax(12.0, 22.0 * s);
		double title_y = field.y + (field.height - title_size) * 0.5 +
			title_size * 0.76;
		int title_rgb = 255;
		draw_text(cr, title, title_x, title_y, title_size,
			title_rgb, title_rgb, title_rgb,
			query != NULL && query[0] != '\0' ? 0.96 : 0.78, false);
		if (state->launcher_search_focus) {
			cairo_text_extents_t ext;
			cairo_text_extents(cr, title, &ext);
			set_source_rgba255(cr, dark ? 200 : 30,
				dark ? 218 : 80, dark ? 255 : 180, 0.85);
			cairo_rectangle(cr,
				title_x + ext.width + scaled_i(3, s),
				title_y - title_size * 0.82,
				fmax(1.0, 2.0 * s),
				title_size * 0.88);
			cairo_fill(cr);
		}

		struct orange_rect option = layout->launcher_mode_buttons[0];
		if (option.width > 0) {
			cairo_surface_t *more_icon = state->assets != NULL ?
				orange_assets_icon(state->assets,
					dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
					"view-more-horizontal-symbolic") : NULL;
			if (more_icon == NULL && state->assets != NULL) {
				more_icon = orange_assets_icon(state->assets,
					dark ? ORANGE_ASSET_ICON_DARK : ORANGE_ASSET_ICON_LIGHT,
					"open-menu-symbolic");
			}
			draw_more_icon(cr, more_icon, option, s, dark);
		}

		int header_divider_y = field.y + field.height + scaled_i(2, s);
		cairo_save(cr);
		set_source_rgba255(cr, 255, 255, 255, dark ? 0.13 : 0.34);
		cairo_set_line_width(cr, fmax(0.7, 1.1 * s));
		cairo_move_to(cr, panel.x + scaled_i(8, s), header_divider_y);
		cairo_line_to(cr, panel.x + panel.width - scaled_i(8, s),
			header_divider_y);
		cairo_stroke(cr);
		cairo_restore(cr);

		int mode = state->launcher_current_mode;
		if (mode != ORANGE_LAUNCHER_MODE_APPS) {
			double x = layout->launcher_viewport.x + scaled_i(8, s);
			double y = layout->launcher_viewport.y + scaled_i(26, s);
			draw_text(cr, launcher_mode_title(mode), x, y + 30.0 * s,
				fmax(18.0, 34.0 * s), 255, 255, 255, 0.86, true);
			const char *body = mode == ORANGE_LAUNCHER_MODE_FILES ?
				"Recent files appear here as Spotlight results." :
				mode == ORANGE_LAUNCHER_MODE_ACTIONS ?
				"Quick actions and shortcuts appear here." :
				"Clipboard history appears here after it is enabled.";
			draw_text(cr, body, x, y + 74.0 * s,
				fmax(12.0, 22.0 * s), 255, 255, 255, 0.62, false);
			return;
		}

		for (int section = 0; section < layout->launcher_section_count; section++) {
			struct orange_rect header = layout->launcher_section_headers[section];
			if (header.width <= 0) {
				continue;
			}
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
			cairo_text_extents_t te;
			cairo_text_extents(cr, label, &te);
			draw_text(cr, label,
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
		int search_icon_left = (int)fmax(10.0, 14.0 * s);
		struct orange_rect search_icon_rect = {
			field.x + search_icon_left,
			field.y + (field.height - search_icon_size) / 2,
			search_icon_size,
			search_icon_size,
		};
		draw_tinted_image_fit(cr, search_icon, search_icon_rect,
			0.90,
			255,
			255,
			255);
	}

	/* Query text or placeholder */
	double font_size = clamp(30.0 * s, 17.0,
		(double)field.height * 0.52);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	double text_x = field.x + fmax(38.0, 52.0 * s);
	double text_baseline = field.y +
		(field.height - font_size) * 0.5 + font_size * 0.78;
	const char *query = state->launcher_query;
	if (query != NULL && query[0] != '\0') {
		draw_text(cr, query, text_x, text_baseline,
			font_size,
			255,
			255,
			255,
			0.98, false);
		if (focused) {
			cairo_text_extents_t qext;
			cairo_text_extents(cr, query, &qext);
			double caret_x = text_x + qext.width + scaled_i(2, s);
			set_source_rgba255(cr,
				255,
				255,
				255,
				0.9);
			cairo_rectangle(cr, caret_x,
				text_baseline - font_size * 0.78,
				fmax(1.0, scaled_i(1.5, s)), font_size * 0.78);
			cairo_fill(cr);
		}
	} else {
		draw_text(cr, "Spotlight Search", text_x, text_baseline,
			font_size,
			255,
			255,
			255,
			0.82, false);
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
	if (strcasecmp(filter, "Social") == 0) {
		static const char *const tokens[] = {
			"Network", "Chat", "Email", "InstantMessaging",
		};
		return category_matches_any(categories, tokens, 4);
	}
	if (strcasecmp(filter, "Entertainment") == 0 ||
			strcasecmp(filter, "Media") == 0) {
		static const char *const tokens[] = {
			"AudioVideo", "Audio", "Video", "Game", "Player",
			"Graphics", "Photography",
		};
		return category_matches_any(categories, tokens, 7);
	}
	if (strcasecmp(filter, "Photo & Video") == 0 ||
			strcasecmp(filter, "Photos") == 0) {
		static const char *const tokens[] = {
			"Graphics", "Photography", "Video", "AudioVideo", "Viewer",
		};
		return category_matches_any(categories, tokens, 5);
	}
	if (strcasecmp(filter, "Information") == 0 ||
			strcasecmp(filter, "Info") == 0) {
		static const char *const tokens[] = {
			"Education", "Science", "Reference", "Documentation", "News",
		};
		return category_matches_any(categories, tokens, 5);
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
