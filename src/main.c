#include "tahoe/compositor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
	fprintf(stderr,
		"usage: %s [--headless] [--once] [--width N] [--height N] [--assets PATH]\n",
		argv0);
}

int main(int argc, char **argv) {
	struct tahoe_options options = {
		.headless = false,
		.once = false,
		.width = 1920,
		.height = 1080,
		.asset_root = "assets/apple",
	};

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--headless") == 0) {
			options.headless = true;
		} else if (strcmp(argv[i], "--once") == 0) {
			options.once = true;
		} else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
			options.width = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
			options.height = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
			options.asset_root = argv[++i];
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	if (options.width <= 0 || options.height <= 0) {
		fprintf(stderr, "width and height must be positive\n");
		return 2;
	}

	return tahoe_compositor_run(&options);
}
