#ifndef ORANGE_UTIL_H
#define ORANGE_UTIL_H

#include <cairo/cairo.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "orange/config.h"

#define ORANGE_REFERENCE_WIDTH 2880.0
#define ORANGE_REFERENCE_HEIGHT 1800.0
#define ORANGE_MIN_UI_SCALE 0.50
#define ORANGE_MAX_UI_SCALE 1.60

static inline double clamp(double value, double min_value, double max_value) {
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

static inline int scaled_i(double value, double scale) {
	return (int)lrint(value * scale);
}

static inline void set_source_rgba255(
		cairo_t *cr, int r, int g, int b, double a) {
	cairo_set_source_rgba(cr, r / 255.0, g / 255.0, b / 255.0, a);
}

static inline void draw_text(cairo_t *cr, const char *text,
		double x, double y,
		double size, int r, int g, int b, double alpha, bool bold) {
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, size);
	set_source_rgba255(cr, r, g, b, alpha);
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, text);
}

static inline double ui_scale_for_size(int width, int height) {
	double scale = fmin((double)width / ORANGE_REFERENCE_WIDTH,
		(double)height / ORANGE_REFERENCE_HEIGHT);
	return clamp(scale, ORANGE_MIN_UI_SCALE, ORANGE_MAX_UI_SCALE);
}

static inline bool is_dark_config(const struct orange_config *config) {
	return config != NULL && config->appearance == ORANGE_APPEARANCE_DARK;
}

#endif
