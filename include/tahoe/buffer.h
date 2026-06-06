#ifndef TAHOE_BUFFER_H
#define TAHOE_BUFFER_H

#include <stdint.h>

#include <wlr/types/wlr_buffer.h>

struct tahoe_buffer {
	struct wlr_buffer base;
	uint32_t *pixels;
	int stride;
};

struct tahoe_buffer *tahoe_buffer_create(int width, int height);
struct tahoe_buffer *tahoe_buffer_from_wlr_buffer(struct wlr_buffer *buffer);

#endif
