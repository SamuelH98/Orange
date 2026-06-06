#include "tahoe/buffer.h"

#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/interfaces/wlr_buffer.h>

static const struct wlr_buffer_impl tahoe_buffer_impl;

static void tahoe_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct tahoe_buffer *buffer = tahoe_buffer_from_wlr_buffer(wlr_buffer);
	free(buffer->pixels);
	free(buffer);
}

static bool tahoe_buffer_get_dmabuf(
		struct wlr_buffer *wlr_buffer,
		struct wlr_dmabuf_attributes *attribs) {
	(void)wlr_buffer;
	(void)attribs;
	return false;
}

static bool tahoe_buffer_get_shm(
		struct wlr_buffer *wlr_buffer,
		struct wlr_shm_attributes *attribs) {
	(void)wlr_buffer;
	(void)attribs;
	return false;
}

static bool tahoe_buffer_begin_data_ptr_access(
		struct wlr_buffer *wlr_buffer,
		uint32_t flags,
		void **data,
		uint32_t *format,
		size_t *stride) {
	(void)flags;
	struct tahoe_buffer *buffer = tahoe_buffer_from_wlr_buffer(wlr_buffer);
	*data = buffer->pixels;
	*format = DRM_FORMAT_ARGB8888;
	*stride = (size_t)buffer->stride;
	return true;
}

static void tahoe_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	(void)wlr_buffer;
}

static const struct wlr_buffer_impl tahoe_buffer_impl = {
	.destroy = tahoe_buffer_destroy,
	.get_dmabuf = tahoe_buffer_get_dmabuf,
	.get_shm = tahoe_buffer_get_shm,
	.begin_data_ptr_access = tahoe_buffer_begin_data_ptr_access,
	.end_data_ptr_access = tahoe_buffer_end_data_ptr_access,
};

struct tahoe_buffer *tahoe_buffer_create(int width, int height) {
	if (width <= 0 || height <= 0) {
		return NULL;
	}

	struct tahoe_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}

	buffer->stride = width * 4;
	size_t size = (size_t)buffer->stride * (size_t)height;
	buffer->pixels = malloc(size);
	if (buffer->pixels == NULL) {
		free(buffer);
		return NULL;
	}
	memset(buffer->pixels, 0, size);

	wlr_buffer_init(&buffer->base, &tahoe_buffer_impl, width, height);
	return buffer;
}

struct tahoe_buffer *tahoe_buffer_from_wlr_buffer(struct wlr_buffer *buffer) {
	if (buffer == NULL || buffer->impl != &tahoe_buffer_impl) {
		return NULL;
	}
	return (struct tahoe_buffer *)buffer;
}
