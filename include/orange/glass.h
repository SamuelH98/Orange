#ifndef ORANGE_GLASS_H
#define ORANGE_GLASS_H

#include <cairo/cairo.h>
#include <stdbool.h>

struct orange_rect;

void orange_glass_rounded_rect(cairo_t *cr,
	double x,
	double y,
	double width,
	double height,
	double radius);

/* Translucent, blurred glass material. Samples the backdrop behind rect,
 * blurs it, and composites a light/dark shading gradient on top. Every
 * glass surface (dock, menu bar, launcher, widgets, notification cards)
 * funnels through here, so blur strength and edge quality are tuned once. */
void orange_glass_draw(cairo_t *cr,
	const struct orange_rect *rect,
	double radius,
	bool dark);

void orange_glass_draw_path(cairo_t *cr,
	const struct orange_rect *bounds,
	const cairo_path_t *path,
	double radius,
	bool dark);

/* Raised panel material: drop shadow + orange_glass_draw + a brighter vertical
 * shade + outer/inner highlight hairlines. Use when a stronger floating panel
 * surface is intentional. */
void orange_glass_draw_panel(cairo_t *cr,
	const struct orange_rect *rect,
	double radius,
	bool dark);



#endif
