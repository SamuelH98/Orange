#ifndef ORANGE_BUFFER_H
#define ORANGE_BUFFER_H

#include <stdint.h>

#include <wlr/types/wlr_buffer.h>

struct orange_buffer {
	struct wlr_buffer base;
	uint32_t *pixels;
	int stride;
};

struct orange_buffer *orange_buffer_create(int width, int height);
struct orange_buffer *orange_buffer_from_wlr_buffer(struct wlr_buffer *buffer);

#endif
