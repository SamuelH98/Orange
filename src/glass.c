#include "orange/glass.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "orange/shell.h"
#include "orange/util.h"

/* Blur tuning.
 *
 * GLASS_BLUR_DOWNSCALE shrinks the backdrop before blurring; smaller =
 * stronger blur (and cheaper). The small surface is then blurred with a few
 * separable box passes (a cheap gaussian approximation) and upscaled into a
 * device-resolution intermediate. Decoupling the upscale from the clip keeps
 * both the clip mask and the content at full device resolution, so the
 * rounded corners antialias cleanly instead of staircasing. */

#define GLASS_BLUR_DOWNSCALE 0.24
#define GLASS_BLUR_PASSES    3

void orange_glass_rounded_rect(cairo_t *cr,
		double x,
		double y,
		double width,
		double height,
		double radius) {
	double r = clamp(radius, 0.0, fmin(width, height) / 2.0);
	if (r <= 0.0) {
		cairo_rectangle(cr, x, y, width, height);
		return;
	}
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + width - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_arc(cr, x + width - r, y + height - r, r, 0.0, M_PI / 2.0);
	cairo_arc(cr, x + r, y + height - r, r, M_PI / 2.0, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static int glass_surface_width(cairo_surface_t *surface) {
	if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) {
		return 0;
	}
	return cairo_image_surface_get_width(surface);
}

static int glass_surface_height(cairo_surface_t *surface) {
	if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) {
		return 0;
	}
	return cairo_image_surface_get_height(surface);
}

/* Separable box blur on an ARGB32 image surface. Uses a sliding window so the
 * cost per row/column is linear in the long axis. The source surface is read
 * back into a working buffer; we never modify it in place. */
static void box_blur_pass(uint32_t *dst,
		const uint32_t *src,
		int count,
		int radius) {
	if (count <= 0 || radius <= 0) {
		if (count > 0 && dst != src) {
			memcpy(dst, src, (size_t)count * sizeof(uint32_t));
		}
		return;
	}
	int window = 2 * radius + 1;
	int sa = 0, sr = 0, sg = 0, sb = 0;
	for (int j = -radius; j <= radius; j++) {
		int k = j < 0 ? 0 : (j >= count ? count - 1 : j);
		uint32_t p = src[k];
		sa += (p >> 24) & 0xff;
		sr += (p >> 16) & 0xff;
		sg += (p >> 8) & 0xff;
		sb += p & 0xff;
	}
	for (int i = 0; i < count; i++) {
		uint32_t a = (uint32_t)(sa / window);
		uint32_t r = (uint32_t)(sr / window);
		uint32_t g = (uint32_t)(sg / window);
		uint32_t b = (uint32_t)(sb / window);
		dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
		int out = i - radius;
		int in = i + radius + 1;
		if (out < 0) {
			out = 0;
		} else if (out >= count) {
			out = count - 1;
		}
		if (in < 0) {
			in = 0;
		} else if (in >= count) {
			in = count - 1;
		}
		uint32_t old = src[out];
		uint32_t add = src[in];
		sa += (int)((add >> 24) & 0xff) - (int)((old >> 24) & 0xff);
		sr += (int)((add >> 16) & 0xff) - (int)((old >> 16) & 0xff);
		sg += (int)((add >> 8) & 0xff) - (int)((old >> 8) & 0xff);
		sb += (int)(add & 0xff) - (int)(old & 0xff);
	}
}

static void blur_argb32_surface(cairo_surface_t *surface, int radius) {
	if (surface == NULL || cairo_surface_get_type(surface) !=
			CAIRO_SURFACE_TYPE_IMAGE) {
		return;
	}
	int w = cairo_image_surface_get_width(surface);
	int h = cairo_image_surface_get_height(surface);
	if (w <= 0 || h <= 0 || radius <= 0) {
		return;
	}

	cairo_surface_flush(surface);
	uint32_t *src = (uint32_t *)cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	if (src == NULL || stride < w * 4) {
		return;
	}

	uint32_t *row_tmp = malloc((size_t)w * sizeof(uint32_t));
	uint32_t *col_src = malloc((size_t)h * sizeof(uint32_t));
	uint32_t *col_dst = malloc((size_t)h * sizeof(uint32_t));
	if (row_tmp == NULL || col_src == NULL || col_dst == NULL) {
		free(row_tmp);
		free(col_src);
		free(col_dst);
		return;
	}

	for (int pass = 0; pass < GLASS_BLUR_PASSES; pass++) {
		/* horizontal: blur each row in place via a temp buffer. */
		for (int y = 0; y < h; y++) {
			uint32_t *row = (uint32_t *)((unsigned char *)src +
				(size_t)y * (size_t)stride);
			box_blur_pass(row_tmp, row, w, radius);
			memcpy(row, row_tmp, (size_t)w * sizeof(uint32_t));
		}
		/* vertical: blur each column in place. */
		for (int x = 0; x < w; x++) {
			for (int y = 0; y < h; y++) {
				col_src[y] = *(uint32_t *)((unsigned char *)src +
					(size_t)y * (size_t)stride + (size_t)x * sizeof(uint32_t));
			}
			box_blur_pass(col_dst, col_src, h, radius);
			for (int y = 0; y < h; y++) {
				*(uint32_t *)((unsigned char *)src +
					(size_t)y * (size_t)stride +
					(size_t)x * sizeof(uint32_t)) = col_dst[y];
			}
		}
	}

	free(row_tmp);
	free(col_src);
	free(col_dst);
	cairo_surface_mark_dirty(surface);
}

static void paint_blurred_backdrop(cairo_t *cr,
		const struct orange_rect *rect,
		double radius) {
	cairo_surface_t *main = cairo_get_target(cr);
	cairo_surface_flush(main);

	int surface_w = glass_surface_width(main);
	int surface_h = glass_surface_height(main);

	/* Sample a padded region around the panel so the blur has backdrop to
	 * pull from at the edges; clamp the padding to what's actually on the
	 * main surface. */
	int pad = (int)ceil(clamp(radius * 0.55, 14.0, 40.0));
	int pad_left = rect->x < pad ? rect->x : pad;
	int pad_top = rect->y < pad ? rect->y : pad;
	int pad_right = pad;
	int pad_bottom = pad;
	if (surface_w > 0 && rect->x + rect->width + pad_right > surface_w) {
		pad_right = surface_w - rect->x - rect->width;
	}
	if (surface_h > 0 && rect->y + rect->height + pad_bottom > surface_h) {
		pad_bottom = surface_h - rect->y - rect->height;
	}
	if (pad_left < 0) {
		pad_left = 0;
	}
	if (pad_top < 0) {
		pad_top = 0;
	}
	if (pad_right < 0) {
		pad_right = 0;
	}
	if (pad_bottom < 0) {
		pad_bottom = 0;
	}

	int sample_x = rect->x - pad_left;
	int sample_y = rect->y - pad_top;
	int sample_w = rect->width + pad_left + pad_right;
	int sample_h = rect->height + pad_top + pad_bottom;
	if (sample_w <= 0 || sample_h <= 0) {
		return;
	}

	/* 1. downsample the padded backdrop into a small surface. */
	int small_w = (int)ceil((double)sample_w * GLASS_BLUR_DOWNSCALE);
	int small_h = (int)ceil((double)sample_h * GLASS_BLUR_DOWNSCALE);
	if (small_w < 1) {
		small_w = 1;
	}
	if (small_h < 1) {
		small_h = 1;
	}
	double scale_x = (double)small_w / (double)sample_w;
	double scale_y = (double)small_h / (double)sample_h;

	cairo_surface_t *small = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, small_w, small_h);
	cairo_t *small_cr = cairo_create(small);
	cairo_scale(small_cr, scale_x, scale_y);
	cairo_set_source_surface(small_cr, main, -sample_x, -sample_y);
	cairo_pattern_set_filter(cairo_get_source(small_cr), CAIRO_FILTER_BEST);
	cairo_paint(small_cr);
	cairo_destroy(small_cr);

	/* 2. blur the small surface. a radius proportional to its size keeps the
	 *   blur perceptually consistent across panel sizes. */
	int blur_radius = (int)clamp(
		fmax((double)small_w, (double)small_h) * 0.10, 2.0, 16.0);
	blur_argb32_surface(small, blur_radius);

	/* 3. upscale the blurred small surface into a device-resolution
	 *   intermediate the size of the padded sample region. This is the key
	 *   step for clean edges: the clip below is applied to a full-resolution
	 *   surface, so the rounded corners antialias at device resolution. */
	cairo_surface_t *full = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, sample_w, sample_h);
	cairo_t *full_cr = cairo_create(full);
	cairo_scale(full_cr, 1.0 / scale_x, 1.0 / scale_y);
	cairo_set_source_surface(full_cr, small, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(full_cr), CAIRO_FILTER_BEST);
	cairo_paint(full_cr);
	cairo_destroy(full_cr);
	cairo_surface_destroy(small);

	/* 4. paint the device-res backdrop through the AA rounded-rect clip. */
	cairo_save(cr);
	orange_glass_rounded_rect(cr, rect->x, rect->y,
		rect->width, rect->height, radius);
	cairo_clip(cr);
	cairo_set_source_surface(cr, full, sample_x, sample_y);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_surface_destroy(full);
}

void orange_glass_draw(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		bool dark) {
	if (cr == NULL || rect == NULL || rect->width <= 0 || rect->height <= 0) {
		return;
	}

	paint_blurred_backdrop(cr, rect, radius);

	cairo_save(cr);
	orange_glass_rounded_rect(cr, rect->x, rect->y,
		rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.12, 0.14, 0.18, 0.35);
		cairo_pattern_add_color_stop_rgba(shade, 0.50, 0.10, 0.12, 0.15, 0.30);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.04, 0.05, 0.07, 0.25);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.50, 0.50, 0.50, 0.22);
		cairo_pattern_add_color_stop_rgba(shade, 0.50, 0.45, 0.45, 0.45, 0.18);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.40, 0.40, 0.40, 0.14);
	}
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);
}

void orange_glass_draw_panel(cairo_t *cr,
		const struct orange_rect *rect,
		double radius,
		bool dark) {
	if (cr == NULL || rect == NULL || rect->width <= 0 || rect->height <= 0) {
		return;
	}

	/* drop shadow, offset down so the panel lifts off the desktop. */
	orange_glass_rounded_rect(cr, rect->x, rect->y + 3.0,
		rect->width, rect->height, radius);
	set_source_rgba255(cr, 0, 0, 0, dark ? 0.20 : 0.12);
	cairo_fill(cr);

	/* the blurred glass material itself. */
	orange_glass_draw(cr, rect, radius, dark);

	/* brighter vertical shade, clipped to the rounded panel. */
	cairo_save(cr);
	orange_glass_rounded_rect(cr, rect->x, rect->y,
		rect->width, rect->height, radius);
	cairo_clip(cr);

	cairo_pattern_t *shade = cairo_pattern_create_linear(
		rect->x, rect->y, rect->x, rect->y + rect->height);
	if (dark) {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 0.17, 0.18, 0.20, 0.65);
		cairo_pattern_add_color_stop_rgba(shade, 0.55, 0.12, 0.13, 0.15, 0.62);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.07, 0.08, 0.10, 0.58);
	} else {
		cairo_pattern_add_color_stop_rgba(shade, 0.0, 1.0, 1.0, 1.0, 0.70);
		cairo_pattern_add_color_stop_rgba(shade, 0.55, 0.96, 0.97, 0.98, 0.66);
		cairo_pattern_add_color_stop_rgba(shade, 1.0, 0.80, 0.84, 0.88, 0.60);
	}
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source(cr, shade);
	cairo_fill(cr);
	cairo_pattern_destroy(shade);

	cairo_restore(cr);

	/* outer + inner highlight hairlines for the crisp Liquid Glass edge. */
	orange_glass_rounded_rect(cr, rect->x, rect->y,
		rect->width, rect->height, radius);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.20 : 0.48);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	orange_glass_rounded_rect(cr, rect->x + 1.5, rect->y + 1.5,
		rect->width - 3.0, rect->height - 3.0, radius - 1.5);
	set_source_rgba255(cr, 255, 255, 255, dark ? 0.07 : 0.18);
	cairo_stroke(cr);
}
