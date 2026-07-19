#ifndef ORANGE_COMPOSITOR_H
#define ORANGE_COMPOSITOR_H

#include <stdbool.h>

struct orange_options {
	bool headless;
	bool once;
	int width;
	int height;
	int num_outputs;
	const char *config_path;
};

int orange_compositor_run(const struct orange_options *options);

#endif
