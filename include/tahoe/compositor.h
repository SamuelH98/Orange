#ifndef TAHOE_COMPOSITOR_H
#define TAHOE_COMPOSITOR_H

#include <stdbool.h>

struct tahoe_options {
	bool headless;
	bool once;
	int width;
	int height;
	const char *asset_root;
	const char *config_path;
	const char *desktop_entry_dir;
	const char *theme_root;
};

int tahoe_compositor_run(const struct tahoe_options *options);

#endif
