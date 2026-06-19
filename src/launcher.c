#include "orange/launcher.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orange/assets.h"
#include "orange/config.h"
#include "orange/desktop_entry.h"
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

static void draw_search_glass(cairo_t *cr,
		double x, double y, double w, double h, bool focused) {
	cairo_save(cr);
	rounded_rect(cr, x, y, w, h, h / 2.0);
	if (focused) {
		cairo_set_source_rgba(cr, 0.12, 0.14, 0.18, 0.40);
	} else {
		cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.35);
	}
	cairo_fill(cr);
	rounded_rect(cr, x, y, w, h, h / 2.0);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
	cairo_set_line_width(cr, 0.8);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_mode_button(cairo_t *cr,
		double x, double y, double size, int mode,
		bool is_current, double s, cairo_surface_t *icon) {
	cairo_save(cr);
	double r = size / 2.0;
	cairo_arc(cr, x + r, y + r, r, 0, 2.0 * M_PI);
	if (is_current) {
		cairo_set_source_rgba(cr, 0.12, 0.14, 0.18, 0.40);
	} else {
		cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.30);
	}
	cairo_fill(cr);
	cairo_arc(cr, x + r, y + r, r, 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);
	if (icon != NULL) {
		int sw = cairo_image_surface_get_width(icon);
		int sh = cairo_image_surface_get_height(icon);
		if (sw > 0 && sh > 0) {
			double icon_size = size * 0.55;
			double scale = icon_size / (double)fmax(sw, sh);
			double ix = x + (size - sw * scale) / 2.0;
			double iy = y + (size - sh * scale) / 2.0;
			cairo_translate(cr, ix, iy);
			cairo_scale(cr, scale, scale);
			cairo_set_source_surface(cr, icon, 0, 0);
			cairo_paint(cr);
		}
	} else {
		/* Draw mode-specific icon glyphs */
		double cx = x + r;
		double cy = y + r;
		double lr = size * 0.30;
		cairo_set_line_width(cr, scaled_i(1.5, s));
		set_source_rgba255(cr, 200, 215, 235, 0.85);
		if (mode == 0) {
			/* Compass / Apps icon */
			cairo_arc(cr, cx, cy, lr, 0, 2.0 * M_PI);
			cairo_stroke(cr);
			cairo_move_to(cr, cx, cy - lr * 0.65);
			cairo_line_to(cr, cx + lr * 0.55, cy + lr * 0.2);
			cairo_line_to(cr, cx, cy + lr * 0.65);
			cairo_line_to(cr, cx - lr * 0.55, cy - lr * 0.2);
			cairo_close_path(cr);
			cairo_stroke(cr);
		} else if (mode == 1) {
			/* Folder / Files icon */
			double fw = lr * 1.4;
			double fh = lr * 1.1;
			double fx = cx - fw / 2;
			double fy = cy - fh / 2 + lr * 0.12;
			cairo_move_to(cr, fx, fy + lr * 0.25);
			cairo_line_to(cr, fx + fw * 0.35, fy + lr * 0.25);
			cairo_line_to(cr, fx + fw * 0.45, fy);
			cairo_line_to(cr, fx + fw, fy);
			cairo_line_to(cr, fx + fw, fy + fh);
			cairo_line_to(cr, fx, fy + fh);
			cairo_close_path(cr);
			cairo_stroke(cr);
		} else if (mode == 2) {
			/* Tools / Actions icon */
			cairo_arc(cr, cx - lr * 0.25, cy + lr * 0.15, lr * 0.45,
				-M_PI * 0.8, M_PI * 0.1);
			cairo_stroke(cr);
			cairo_move_to(cr, cx + lr * 0.15, cy - lr * 0.35);
			cairo_line_to(cr, cx + lr * 0.55, cy - lr * 0.15);
			cairo_line_to(cr, cx + lr * 0.35, cy + lr * 0.05);
			cairo_line_to(cr, cx - lr * 0.05, cy - lr * 0.15);
			cairo_close_path(cr);
			cairo_stroke(cr);
		} else {
			/* Clipboard icon */
			double cw = lr * 1.1;
			double ch = lr * 1.4;
			rounded_rect(cr, cx - cw / 2, cy - ch / 2, cw, ch,
				lr * 0.2);
			cairo_stroke(cr);
			cairo_move_to(cr, cx - lr * 0.25, cy - ch / 2);
			cairo_line_to(cr, cx + lr * 0.25, cy - ch / 2);
			cairo_stroke(cr);
		}
	}
	cairo_restore(cr);
}

void orange_launcher_draw(cairo_t *cr,
		const struct orange_shell_layout *layout,
		const struct orange_shell_state *state,
		const struct orange_config *config) {
	(void)config;
	if (!layout->launcher_visible) {
		return;
	}
	double s = layout_scale(layout);

	/* Search field (floating, no panel behind it) */
	struct orange_rect field = layout->launcher_search_field;
	bool focused = state->launcher_search_focus;
	draw_search_glass(cr, field.x, field.y, field.width, field.height, focused);

	/* Magnifier glyph at the left of the field */
	double icon_cx = field.x + scaled_i(16, s);
	double icon_cy = field.y + field.height / 2.0;
	double lens_r = scaled_i(5, s);
	cairo_save(cr);
	set_source_rgba255(cr, 180, 200, 225, 0.75);
	cairo_set_line_width(cr, scaled_i(1.5, s));
	cairo_arc(cr, icon_cx, icon_cy - scaled_i(0.5, s), lens_r, 0, 2.0 * M_PI);
	cairo_stroke(cr);
	cairo_set_line_width(cr, scaled_i(1.8, s));
	cairo_move_to(cr, icon_cx + lens_r * 0.7,
		icon_cy - scaled_i(0.5, s) + lens_r * 0.7);
	cairo_line_to(cr, icon_cx + lens_r * 1.6,
		icon_cy - scaled_i(0.5, s) + lens_r * 1.6);
	cairo_stroke(cr);
	cairo_restore(cr);

	/* Query text or placeholder */
	double font_size = 17.0 * s;
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	double text_x = field.x + scaled_i(34, s);
	double text_baseline = field.y +
		(field.height - font_size) * 0.5 + font_size * 0.78;
	const char *query = state->launcher_query;
	if (query != NULL && query[0] != '\0') {
		draw_text(cr, query, text_x, text_baseline,
			font_size, 220, 230, 245, 0.95, false);
		if (focused) {
			cairo_text_extents_t qext;
			cairo_text_extents(cr, query, &qext);
			double caret_x = text_x + qext.width + scaled_i(2, s);
			set_source_rgba255(cr, 180, 210, 240, 0.9);
			cairo_rectangle(cr, caret_x,
				text_baseline - font_size * 0.78,
				fmax(1.0, scaled_i(1.5, s)), font_size * 0.78);
			cairo_fill(cr);
		}
	} else {
		draw_text(cr, "Spotlight Search", text_x, text_baseline,
			font_size, 160, 180, 200, 0.45, false);
	}

	/* Mode buttons to the right of the search field */
	int current_mode = state->launcher_current_mode;
	for (int i = 0; i < ORANGE_LAUNCHER_MODE_COUNT; i++) {
		struct orange_rect btn = layout->launcher_mode_buttons[i];
		draw_mode_button(cr, btn.x, btn.y, btn.width, i,
			i == current_mode, s, NULL);
	}

	/* Category tabs (below search row, only in Apps mode with no query) */
	if (state->launcher_current_mode == ORANGE_LAUNCHER_MODE_APPS &&
			(query == NULL || query[0] == '\0') &&
			layout->launcher_category_count > 0) {
		int active = layout->launcher_category_active;
		for (int i = 0; i < layout->launcher_category_count; i++) {
			struct orange_rect tab = layout->launcher_category_tabs[i];
			bool is_active = (i == active);
			cairo_save(cr);
			rounded_rect(cr, tab.x, tab.y, tab.width, tab.height,
				tab.height / 2.0);
			if (is_active) {
				cairo_set_source_rgba(cr, 0.12, 0.14, 0.18, 0.40);
			} else {
				cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.25);
			}
			cairo_fill(cr);
			rounded_rect(cr, tab.x, tab.y, tab.width, tab.height,
				tab.height / 2.0);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
			cairo_set_line_width(cr, 0.5);
			cairo_stroke(cr);
			cairo_restore(cr);

			if (i < state->launcher_category_count) {
				double tab_font = 11.0 * s;
				cairo_select_font_face(cr, "Sans",
					CAIRO_FONT_SLANT_NORMAL,
					is_active ? CAIRO_FONT_WEIGHT_BOLD :
					CAIRO_FONT_WEIGHT_NORMAL);
				cairo_set_font_size(cr, tab_font);
				cairo_text_extents_t te;
				cairo_text_extents(cr,
					state->launcher_categories[i], &te);
				set_source_rgba255(cr, 200, 215, 235,
					is_active ? 230 : 150);
				cairo_move_to(cr,
					tab.x + (tab.width - te.width) / 2.0 - te.x_bearing,
					tab.y + (tab.height - te.height) / 2.0 - te.y_bearing);
				cairo_show_text(cr, state->launcher_categories[i]);
			}
		}
	}

	/* App icons for the current mode */
	int mode = state->launcher_current_mode;
	int per_page = layout->launcher_grid_cell_count;

	if (mode == ORANGE_LAUNCHER_MODE_APPS && per_page > 0) {
		cairo_save(cr);
		cairo_rectangle(cr, layout->launcher_viewport.x,
			layout->launcher_viewport.y,
			layout->launcher_viewport.width,
			layout->launcher_viewport.height);
		cairo_clip(cr);

		int label_max_w = layout->launcher_viewport.width - scaled_i(20, s);
		for (int i = 0; i < per_page; i++) {
			int app_index = i;
			if (app_index >= state->launcher_app_count) {
				break;
			}
			struct orange_rect cell = layout->launcher_grid_cells[i];

			const char *icon_name = orange_launcher_app_icon(
				state->desktop_entries, state->desktop_entry_count,
				state->launcher_app_indices, state->launcher_app_count,
				app_index);
			int variant = ORANGE_ASSET_ICON_LIGHT;
			cairo_surface_t *icon = NULL;
			if (state->assets != NULL && icon_name != NULL) {
				icon = orange_assets_icon(state->assets, variant, icon_name);
			}

			double icon_size = scaled_i(78, s);
			double icon_x = cell.x + (cell.width - icon_size) / 2.0;
			double icon_y = cell.y;
			struct orange_rect icon_rect = {
				(int)icon_x, (int)icon_y, (int)icon_size, (int)icon_size};
			double corner = icon_size * 0.22;

			/* Hot selection highlight */
			if (i == state->launcher_hot_app) {
				cairo_save(cr);
				rounded_rect(cr,
					icon_rect.x - scaled_i(4, s),
					icon_rect.y - scaled_i(4, s),
					icon_rect.width + scaled_i(8, s),
					icon_rect.height + scaled_i(8, s),
					scaled_i(18, s));
				cairo_set_source_rgba(cr, 0.20, 0.35, 0.55, 0.35);
				cairo_fill(cr);
				cairo_restore(cr);
			}

			/* Icon shadow */
			cairo_save(cr);
			rounded_rect(cr, icon_rect.x + scaled_i(1, s),
				icon_rect.y + scaled_i(2, s),
				icon_rect.width, icon_rect.height, corner);
			cairo_set_source_rgba(cr, 0, 0, 0, 0.18);
			cairo_fill(cr);
			cairo_restore(cr);

			if (icon != NULL) {
				draw_image_cover(cr, icon, icon_rect);
			} else {
				/* Fallback rounded tile */
				cairo_save(cr);
				rounded_rect(cr, icon_rect.x, icon_rect.y,
					icon_rect.width, icon_rect.height, corner);
				cairo_set_source_rgba(cr, 0.18, 0.22, 0.28, 0.70);
				cairo_fill(cr);
				cairo_restore(cr);
			}

			/* App label */
			const char *label = orange_launcher_app_label(
				state->desktop_entries, state->desktop_entry_count,
				state->launcher_app_indices, state->launcher_app_count,
				app_index);
			if (label != NULL) {
				cairo_select_font_face(cr, "Sans",
					CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
				cairo_set_font_size(cr, 12.0 * s);
				char line1[128];
				char line2[128];
				int lines = wrap_label_two_lines(cr, label,
					(double)label_max_w, line1, sizeof(line1),
					line2, sizeof(line2));
				double label_font = 12.0 * s;
				double line_gap = label_font * 1.15;
				double label_y = icon_rect.y + icon_rect.height +
					scaled_i(6, s) + label_font * 0.78;
				if (lines >= 1 && line1[0] != '\0') {
					cairo_text_extents_t e1;
					cairo_text_extents(cr, line1, &e1);
					draw_text(cr, line1,
						cell.x + (cell.width - e1.width) / 2.0,
						label_y, label_font, 220, 230, 245, 0.90, false);
				}
				if (lines == 2 && line2[0] != '\0') {
					cairo_text_extents_t e2;
					cairo_text_extents(cr, line2, &e2);
					draw_text(cr, line2,
						cell.x + (cell.width - e2.width) / 2.0,
						label_y + line_gap, label_font,
						220, 230, 245, 0.90, false);
				}
			}
		}
		cairo_restore(cr);
	} else if (mode == ORANGE_LAUNCHER_MODE_FILES) {
		double fx = layout->launcher_viewport.x + scaled_i(16, s);
		double fy = layout->launcher_viewport.y + scaled_i(16, s);
		draw_text(cr, "Recent Files", fx, fy + 16.0 * s * 0.78,
			16.0 * s, 200, 215, 235, 0.55, false);
		draw_text(cr, "Search for files to see results",
			fx, fy + 40.0 * s, 13.0 * s, 200, 215, 235, 0.30, false);
	} else if (mode == ORANGE_LAUNCHER_MODE_ACTIONS) {
		double fx = layout->launcher_viewport.x + scaled_i(16, s);
		double fy = layout->launcher_viewport.y + scaled_i(16, s);
		draw_text(cr, "Actions", fx, fy + 16.0 * s * 0.78,
			16.0 * s, 200, 215, 235, 0.55, false);
		draw_text(cr, "Quick Keys and Shortcuts",
			fx, fy + 40.0 * s, 13.0 * s, 200, 215, 235, 0.30, false);
	} else if (mode == ORANGE_LAUNCHER_MODE_CLIPBOARD) {
		double fx = layout->launcher_viewport.x + scaled_i(16, s);
		double fy = layout->launcher_viewport.y + scaled_i(16, s);
		draw_text(cr, "Clipboard History", fx, fy + 16.0 * s * 0.78,
			16.0 * s, 200, 215, 235, 0.55, false);
		draw_text(cr, "Recently copied items will appear here",
			fx, fy + 40.0 * s, 13.0 * s, 200, 215, 235, 0.30, false);
	}
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

int orange_launcher_filter(
		const struct orange_desktop_entry *entries,
		int entry_count,
		const char *query,
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
