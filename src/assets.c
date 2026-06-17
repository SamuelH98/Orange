#include "orange/assets.h"

#include <dirent.h>
#include <librsvg/rsvg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ICON_LOOKUP_SIZE 128
#define ICON_LOOKUP_SCALE 1
#define ICON_THEME_BASE_MAX 32
#define ICON_THEME_DIR_MAX 1024
#define ICON_THEME_INHERITS_MAX 512
#define ICON_THEME_DEPTH_MAX 12
#define ICON_NAME_VARIANT_MAX 64

enum icon_dir_type {
	ICON_DIR_FIXED,
	ICON_DIR_SCALABLE,
	ICON_DIR_THRESHOLD,
};

struct icon_dir_info {
	char name[256];
	int size;
	int scale;
	int min_size;
	int max_size;
	int threshold;
	enum icon_dir_type type;
};

struct icon_theme_info {
	bool found;
	char theme_path[4096];
	char inherits[ICON_THEME_INHERITS_MAX];
	struct icon_dir_info dirs[ICON_THEME_DIR_MAX];
	int dir_count;
};

static bool has_suffix(const char *name, const char *suffix) {
	size_t name_len = strlen(name);
	size_t suffix_len = strlen(suffix);
	return name_len > suffix_len &&
		strcmp(name + name_len - suffix_len, suffix) == 0;
}

static bool has_png_suffix(const char *name) {
	return has_suffix(name, ".png");
}

static bool has_svg_suffix(const char *name) {
	return has_suffix(name, ".svg") || has_suffix(name, ".svgz");
}

static cairo_surface_t *load_png(const char *path) {
	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

static cairo_surface_t *load_root_png(const char *root, const char *relative) {
	char path[4096];
	snprintf(path, sizeof(path), "%s/%s", root, relative);
	return load_png(path);
}

static cairo_surface_t *scale_wallpaper(cairo_surface_t *source,
		int width,
		int height) {
	if (source == NULL || width <= 0 || height <= 0) {
		return NULL;
	}
	int source_width = cairo_image_surface_get_width(source);
	int source_height = cairo_image_surface_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		return NULL;
	}

	cairo_surface_t *scaled = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(scaled) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(scaled);
		return NULL;
	}
	cairo_t *cr = cairo_create(scaled);
	double scale_x = (double)width / (double)source_width;
	double scale_y = (double)height / (double)source_height;
	double scale = scale_x > scale_y ? scale_x : scale_y;
	double scaled_width = source_width * scale;
	double scaled_height = source_height * scale;
	double offset_x = (width - scaled_width) * 0.5;
	double offset_y = (height - scaled_height) * 0.5;

	cairo_save(cr);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);
	cairo_translate(cr, offset_x, offset_y);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, source, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_destroy(cr);

	if (cairo_surface_status(scaled) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(scaled);
		return NULL;
	}
	return scaled;
}

static cairo_surface_t *ensure_wallpaper_loaded(
		struct orange_assets *assets,
		bool dark) {
	if (assets == NULL || assets->asset_root[0] == '\0') {
		return NULL;
	}
	if (dark) {
		if (!assets->wallpaper_dark_checked) {
			assets->wallpaper_dark = load_root_png(
				assets->asset_root, "wallpaper-dark.png");
			assets->wallpaper_dark_checked = true;
		}
		if (assets->wallpaper_dark != NULL) {
			return assets->wallpaper_dark;
		}
	}
	if (!assets->wallpaper_checked) {
		assets->wallpaper = load_root_png(assets->asset_root, "wallpaper.png");
		assets->wallpaper_checked = true;
	}
	return assets->wallpaper;
}

static cairo_surface_t *load_svg(const char *path, int size) {
	GError *error = NULL;
	RsvgHandle *handle = rsvg_handle_new_from_file(path, &error);
	if (handle == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return NULL;
	}

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(surface);
	RsvgRectangle viewport = {0, 0, (double)size, (double)size};
	gboolean rendered = rsvg_handle_render_document(handle, cr, &viewport, &error);
	cairo_destroy(cr);
	g_object_unref(handle);

	if (!rendered || error != NULL ||
			cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		if (error != NULL) {
			g_error_free(error);
		}
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

static cairo_surface_t *load_icon_file(const char *path) {
	if (has_png_suffix(path)) {
		return load_png(path);
	}
	if (has_svg_suffix(path)) {
		return load_svg(path, ICON_LOOKUP_SIZE);
	}
	return NULL;
}

static bool file_exists(const char *path) {
	return access(path, R_OK) == 0;
}

static char *trim(char *value) {
	while (*value == ' ' || *value == '\t' ||
			*value == '\n' || *value == '\r') {
		value++;
	}
	if (*value == '\0') {
		return value;
	}
	char *end = value + strlen(value) - 1;
	while (end > value && (*end == ' ' || *end == '\t' ||
			*end == '\n' || *end == '\r')) {
		*end = '\0';
		end--;
	}
	return value;
}

static int parse_leading_int(const char *text, int fallback) {
	char *end = NULL;
	long parsed = strtol(text, &end, 10);
	if (end == text || parsed <= 0 || parsed > 4096) {
		return fallback;
	}
	return (int)parsed;
}

static int parse_scale_from_name(const char *name) {
	const char *at = strchr(name, '@');
	if (at == NULL) {
		return 1;
	}
	return parse_leading_int(at + 1, 1);
}

static struct icon_dir_info *find_theme_dir(
		struct icon_theme_info *info,
		const char *name) {
	for (int i = 0; i < info->dir_count; i++) {
		if (strcmp(info->dirs[i].name, name) == 0) {
			return &info->dirs[i];
		}
	}
	return NULL;
}

static void add_theme_dir(struct icon_theme_info *info, const char *name) {
	if (name == NULL || name[0] == '\0' ||
			info->dir_count >= ICON_THEME_DIR_MAX ||
			find_theme_dir(info, name) != NULL) {
		return;
	}
	struct icon_dir_info *dir = &info->dirs[info->dir_count++];
	memset(dir, 0, sizeof(*dir));
	snprintf(dir->name, sizeof(dir->name), "%s", name);
	dir->size = parse_leading_int(name, 48);
	dir->scale = parse_scale_from_name(name);
	dir->min_size = dir->size;
	dir->max_size = dir->size;
	dir->threshold = 2;
	dir->type = strstr(name, "scalable") != NULL ?
		ICON_DIR_SCALABLE : ICON_DIR_THRESHOLD;
	if (dir->type == ICON_DIR_SCALABLE) {
		dir->min_size = 1;
		dir->max_size = 512;
	}
}

static void add_theme_dir_list(struct icon_theme_info *info, char *value) {
	char *save = NULL;
	char *token = strtok_r(value, ",", &save);
	while (token != NULL) {
		token = trim(token);
		if (token[0] != '\0') {
			add_theme_dir(info, token);
		}
		token = strtok_r(NULL, ",", &save);
	}
}

static void finalize_theme_dirs(struct icon_theme_info *info) {
	for (int i = 0; i < info->dir_count; i++) {
		struct icon_dir_info *dir = &info->dirs[i];
		if (dir->scale <= 0) {
			dir->scale = 1;
		}
		if (dir->size <= 0) {
			dir->size = parse_leading_int(dir->name, 48);
		}
		if (dir->threshold <= 0) {
			dir->threshold = 2;
		}
		if (dir->min_size <= 0) {
			dir->min_size = dir->size;
		}
		if (dir->max_size <= 0) {
			dir->max_size = dir->size;
		}
	}
}

static void parse_theme_index(FILE *file, struct icon_theme_info *info) {
	char *line = NULL;
	size_t line_cap = 0;
	char section[256] = "";
	while (getline(&line, &line_cap, file) >= 0) {
		char *start = trim(line);
		if (start[0] == '\0' || start[0] == '#' || start[0] == ';') {
			continue;
		}
		if (start[0] == '[') {
			char *end = strchr(start, ']');
			if (end == NULL) {
				continue;
			}
			*end = '\0';
			snprintf(section, sizeof(section), "%s", start + 1);
			continue;
		}

		char *equals = strchr(start, '=');
		if (equals == NULL) {
			continue;
		}
		*equals = '\0';
		char *key = trim(start);
		char *value = trim(equals + 1);

		if (strcmp(section, "Icon Theme") == 0) {
			if (strcmp(key, "Inherits") == 0) {
				snprintf(info->inherits, sizeof(info->inherits), "%s", value);
			} else if (strcmp(key, "Directories") == 0 ||
					strcmp(key, "ScaledDirectories") == 0) {
				char *list = strdup(value);
				if (list != NULL) {
					add_theme_dir_list(info, list);
					free(list);
				}
			}
			continue;
		}

		struct icon_dir_info *dir = find_theme_dir(info, section);
		if (dir == NULL) {
			continue;
		}
		if (strcmp(key, "Size") == 0) {
			dir->size = parse_leading_int(value, dir->size);
		} else if (strcmp(key, "Scale") == 0) {
			dir->scale = parse_leading_int(value, dir->scale);
		} else if (strcmp(key, "MinSize") == 0) {
			dir->min_size = parse_leading_int(value, dir->min_size);
		} else if (strcmp(key, "MaxSize") == 0) {
			dir->max_size = parse_leading_int(value, dir->max_size);
		} else if (strcmp(key, "Threshold") == 0) {
			dir->threshold = parse_leading_int(value, dir->threshold);
		} else if (strcmp(key, "Type") == 0) {
			if (strcmp(value, "Fixed") == 0) {
				dir->type = ICON_DIR_FIXED;
			} else if (strcmp(value, "Scalable") == 0) {
				dir->type = ICON_DIR_SCALABLE;
			} else {
				dir->type = ICON_DIR_THRESHOLD;
			}
		}
	}
	free(line);
	finalize_theme_dirs(info);
}

static bool add_icon_base(char bases[ICON_THEME_BASE_MAX][4096],
		int *count,
		const char *base) {
	if (base == NULL || base[0] == '\0' || *count >= ICON_THEME_BASE_MAX) {
		return false;
	}
	for (int i = 0; i < *count; i++) {
		if (strcmp(bases[i], base) == 0) {
			return false;
		}
	}
	snprintf(bases[*count], 4096, "%s", base);
	(*count)++;
	return true;
}

static void collect_icon_bases(const struct orange_assets *assets,
		char bases[ICON_THEME_BASE_MAX][4096],
		int *count) {
	*count = 0;
	char path[4096];
	(void)assets;

	const char *home = getenv("HOME");
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home != NULL && xdg_data_home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/icons", xdg_data_home);
		add_icon_base(bases, count, path);
	} else if (home != NULL && home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/.local/share/icons", home);
		add_icon_base(bases, count, path);
	}
	if (home != NULL && home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/.icons", home);
		add_icon_base(bases, count, path);
	}

	const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL || xdg_data_dirs[0] == '\0') {
		xdg_data_dirs = "/usr/local/share:/usr/share";
	}
	char dirs[4096];
	snprintf(dirs, sizeof(dirs), "%s", xdg_data_dirs);
	char *save = NULL;
	char *token = strtok_r(dirs, ":", &save);
	while (token != NULL) {
		if (token[0] != '\0') {
			snprintf(path, sizeof(path), "%s/icons", token);
			add_icon_base(bases, count, path);
		}
		token = strtok_r(NULL, ":", &save);
	}
	add_icon_base(bases, count, "/var/lib/snapd/desktop/icons");
	add_icon_base(bases, count, "/var/lib/flatpak/exports/share/icons");
	if (home != NULL && home[0] != '\0') {
		snprintf(path, sizeof(path),
			"%s/.local/share/flatpak/exports/share/icons", home);
		add_icon_base(bases, count, path);
	}
	add_icon_base(bases, count, "/usr/share/pixmaps");
}

static bool read_theme_info(const char *theme_name,
		char bases[ICON_THEME_BASE_MAX][4096],
		int base_count,
		struct icon_theme_info *info) {
	memset(info, 0, sizeof(*info));
	if (theme_name == NULL || theme_name[0] == '\0') {
		return false;
	}

	for (int i = 0; i < base_count; i++) {
		char index_path[4096];
		snprintf(index_path, sizeof(index_path), "%s/%s/index.theme",
			bases[i], theme_name);
		FILE *file = fopen(index_path, "r");
		if (file == NULL) {
			continue;
		}
		snprintf(info->theme_path, sizeof(info->theme_path),
			"%s/%s", bases[i], theme_name);
		info->found = true;
		parse_theme_index(file, info);
		fclose(file);
		return true;
	}

	return false;
}

static bool directory_matches_size(const struct icon_dir_info *dir,
		int icon_size,
		int icon_scale) {
	if (dir->scale != icon_scale) {
		return false;
	}
	switch (dir->type) {
	case ICON_DIR_FIXED:
		return dir->size == icon_size;
	case ICON_DIR_SCALABLE:
		return dir->min_size <= icon_size && icon_size <= dir->max_size;
	case ICON_DIR_THRESHOLD:
	default:
		return dir->size - dir->threshold <= icon_size &&
			icon_size <= dir->size + dir->threshold;
	}
}

static int directory_size_distance(const struct icon_dir_info *dir,
		int icon_size,
		int icon_scale) {
	int requested = icon_size * icon_scale;
	int scale = dir->scale > 0 ? dir->scale : 1;
	switch (dir->type) {
	case ICON_DIR_FIXED:
		return abs(dir->size * scale - requested);
	case ICON_DIR_SCALABLE:
		if (requested < dir->min_size * scale) {
			return dir->min_size * scale - requested;
		}
		if (requested > dir->max_size * scale) {
			return requested - dir->max_size * scale;
		}
		return 0;
	case ICON_DIR_THRESHOLD:
	default:
		if (requested < (dir->size - dir->threshold) * scale) {
			return (dir->size - dir->threshold) * scale - requested;
		}
		if (requested > (dir->size + dir->threshold) * scale) {
			return requested - (dir->size + dir->threshold) * scale;
		}
		return 0;
	}
}

static bool try_icon_path(char *out,
		size_t out_size,
		const char *base,
		const char *theme_name,
		const char *subdir,
		const char *icon_name) {
	const char *extensions[] = {"svg", "svgz", "png"};
	for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
		char path[4096];
		snprintf(path, sizeof(path), "%s/%s/%s/%s.%s",
			base, theme_name, subdir, icon_name, extensions[i]);
		if (file_exists(path)) {
			snprintf(out, out_size, "%s", path);
			return true;
		}
	}
	return false;
}

static bool has_symbolic_suffix(const char *name) {
	return has_suffix(name, "-symbolic");
}

static int generate_themed_icon_names(const char *name,
		char names[][ORANGE_ASSET_ICON_NAME_MAX],
		int max) {
	if (name == NULL || name[0] == '\0' || max < 1) {
		return 0;
	}

	bool is_symbolic = has_symbolic_suffix(name);
	char base[ORANGE_ASSET_ICON_NAME_MAX];
	if (is_symbolic) {
		size_t len = strlen(name);
		snprintf(base, sizeof(base), "%.*s",
			(int)(len - 9), name);
	} else {
		snprintf(base, sizeof(base), "%s", name);
	}

	char splits[ICON_NAME_VARIANT_MAX][ORANGE_ASSET_ICON_NAME_MAX];
	int split_count = 0;
	snprintf(splits[split_count++], sizeof(splits[0]), "%s", base);

	char buf[ORANGE_ASSET_ICON_NAME_MAX];
	snprintf(buf, sizeof(buf), "%s", base);
	char *p = buf + strlen(buf);

	while (split_count < ICON_NAME_VARIANT_MAX) {
		while (p > buf && *p != '-') {
			p--;
		}
		if (p <= buf) {
			break;
		}
		*p = '\0';
		bool dup = false;
		for (int i = 0; i < split_count; i++) {
			if (strcmp(splits[i], buf) == 0) {
				dup = true;
				break;
			}
		}
		if (!dup) {
			snprintf(splits[split_count++], sizeof(splits[0]), "%s", buf);
		}
	}

	int count = 0;
	for (int i = 0; i < split_count && count < max; i++) {
		if (is_symbolic) {
			snprintf(names[count], ORANGE_ASSET_ICON_NAME_MAX,
				"%s-symbolic", splits[i]);
		} else {
			snprintf(names[count], ORANGE_ASSET_ICON_NAME_MAX,
				"%s", splits[i]);
		}
		count++;
	}
	for (int i = 0; i < split_count && count < max; i++) {
		if (is_symbolic) {
			snprintf(names[count], ORANGE_ASSET_ICON_NAME_MAX,
				"%s", splits[i]);
		} else {
			snprintf(names[count], ORANGE_ASSET_ICON_NAME_MAX,
				"%s-symbolic", splits[i]);
		}
		count++;
	}
	return count;
}

static bool lookup_icon_in_theme(char *out,
		size_t out_size,
		const char *icon_name,
		const char *theme_name,
		const struct icon_theme_info *info,
		char bases[ICON_THEME_BASE_MAX][4096],
		int base_count,
		int icon_size,
		int icon_scale) {
	for (int i = 0; i < info->dir_count; i++) {
		const struct icon_dir_info *dir = &info->dirs[i];
		if (!directory_matches_size(dir, icon_size, icon_scale)) {
			continue;
		}
		for (int b = 0; b < base_count; b++) {
			if (try_icon_path(out, out_size, bases[b], theme_name,
					dir->name, icon_name)) {
				return true;
			}
		}
	}

	int best_distance = INT_MAX;
	int best_dir_size = 0;
	enum icon_dir_type best_type = ICON_DIR_FIXED;
	char best_path[4096] = "";
	for (int i = 0; i < info->dir_count; i++) {
		const struct icon_dir_info *dir = &info->dirs[i];
		int distance = directory_size_distance(dir, icon_size, icon_scale);
		for (int b = 0; b < base_count; b++) {
			char candidate[4096];
			if (!try_icon_path(candidate, sizeof(candidate), bases[b],
					theme_name, dir->name, icon_name)) {
				continue;
			}
			bool better = false;
			if (best_path[0] == '\0') {
				better = true;
			} else if (dir->type == ICON_DIR_SCALABLE &&
					best_type != ICON_DIR_SCALABLE) {
				better = true;
			} else if (dir->type != ICON_DIR_SCALABLE &&
					best_type == ICON_DIR_SCALABLE) {
				better = false;
			} else if (distance < best_distance) {
				better = true;
			} else if (distance == best_distance &&
					dir->size > best_dir_size) {
				better = true;
			}
			if (better) {
				best_distance = distance;
				best_dir_size = dir->size;
				best_type = dir->type;
				snprintf(best_path, sizeof(best_path), "%s", candidate);
			}
			break;
		}
	}
	if (best_path[0] != '\0') {
		snprintf(out, out_size, "%s", best_path);
		return true;
	}
	return false;
}

static bool lookup_fallback_icon(char *out,
		size_t out_size,
		const char *icon_name,
		char bases[ICON_THEME_BASE_MAX][4096],
		int base_count) {
	const char *extensions[] = {"svg", "svgz", "png"};
	for (int b = 0; b < base_count; b++) {
		for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
			char path[4096];
			snprintf(path, sizeof(path), "%s/%s.%s",
				bases[b], icon_name, extensions[i]);
			if (file_exists(path)) {
				snprintf(out, out_size, "%s", path);
				return true;
			}
		}
	}
	return false;
}

static bool find_icon_helper(char *out,
		size_t out_size,
		const char *icon_name,
		const char *theme_name,
		const struct orange_assets *assets,
		char bases[ICON_THEME_BASE_MAX][4096],
		int base_count,
		int icon_size,
		int icon_scale,
		int depth) {
	if (theme_name == NULL || theme_name[0] == '\0' ||
			depth > ICON_THEME_DEPTH_MAX) {
		return false;
	}

	struct icon_theme_info info;
	if (!read_theme_info(theme_name, bases, base_count, &info)) {
		return false;
	}
	if (lookup_icon_in_theme(out, out_size, icon_name, theme_name, &info,
			bases, base_count, icon_size, icon_scale)) {
		return true;
	}

	char inherits[ICON_THEME_INHERITS_MAX];
	snprintf(inherits, sizeof(inherits), "%s", info.inherits);
	char *save = NULL;
	char *token = strtok_r(inherits, ",", &save);
	while (token != NULL) {
		token = trim(token);
		if (token[0] != '\0' &&
				find_icon_helper(out, out_size, icon_name, token, assets,
					bases, base_count, icon_size, icon_scale, depth + 1)) {
			return true;
		}
		token = strtok_r(NULL, ",", &save);
	}
	(void)assets;
	return false;
}

static bool normalize_icon_name(const char *name,
		char normalized[ORANGE_ASSET_ICON_NAME_MAX]) {
	if (name == NULL || name[0] == '\0') {
		return false;
	}
	snprintf(normalized, ORANGE_ASSET_ICON_NAME_MAX, "%s", name);
	if (has_png_suffix(normalized)) {
		normalized[strlen(normalized) - 4] = '\0';
	} else if (has_suffix(normalized, ".svg")) {
		normalized[strlen(normalized) - 4] = '\0';
	} else if (has_suffix(normalized, ".svgz")) {
		normalized[strlen(normalized) - 5] = '\0';
	}
	return normalized[0] != '\0';
}

static bool find_icon_file(char *out,
		size_t out_size,
		const char *icon_name,
		const struct orange_assets *assets) {
	if (icon_name == NULL || icon_name[0] == '\0') {
		return false;
	}
	if (icon_name[0] == '/' && file_exists(icon_name)) {
		snprintf(out, out_size, "%s", icon_name);
		return true;
	}

	char normalized[ORANGE_ASSET_ICON_NAME_MAX];
	if (!normalize_icon_name(icon_name, normalized)) {
		return false;
	}

	char bases[ICON_THEME_BASE_MAX][4096];
	int base_count = 0;
	collect_icon_bases(assets, bases, &base_count);
	if (base_count == 0) {
		return false;
	}

	const char *theme = assets->icon_theme[0] != '\0' ?
		assets->icon_theme : "hicolor";

	if (find_icon_helper(out, out_size, normalized, theme, assets,
			bases, base_count, ICON_LOOKUP_SIZE, ICON_LOOKUP_SCALE, 0)) {
		return true;
	}
	if (strcmp(theme, "hicolor") != 0 &&
			find_icon_helper(out, out_size, normalized, "hicolor", assets,
				bases, base_count, ICON_LOOKUP_SIZE, ICON_LOOKUP_SCALE, 0)) {
		return true;
	}
	return lookup_fallback_icon(out, out_size, normalized, bases, base_count);
}

static bool find_icon_file_with_fallbacks(char *out,
		size_t out_size,
		const char *icon_name,
		const struct orange_assets *assets) {
	if (icon_name == NULL || icon_name[0] == '\0') {
		return false;
	}

	char names[ICON_NAME_VARIANT_MAX * 2][ORANGE_ASSET_ICON_NAME_MAX];
	int name_count = generate_themed_icon_names(icon_name,
		names, ICON_NAME_VARIANT_MAX * 2);

	for (int i = 0; i < name_count; i++) {
		if (find_icon_file(out, out_size, names[i], assets)) {
			return true;
		}
	}
	return false;
}

static const char *const aliases_network_wireless[] = {
	"network-wireless", "network-wireless-signal-excellent",
	"network-wireless-signal-excellent-symbolic", "wifi", NULL,
};
static const char *const aliases_network_offline[] = {
	"network-offline", "network-error", "network-wireless", NULL,
};
static const char *const aliases_battery[] = {
	"battery", "battery-full", "battery-full-charged",
	"battery-level-100", "battery.100", NULL,
};
static const char *const aliases_search[] = {
	"edit-find", "system-search", "search", "magnifyingglass", NULL,
};
static const char *const aliases_settings[] = {
	"preferences-system", "org.gnome.Settings", "settings",
	"control-center", NULL,
};
static const char *const aliases_control_center[] = {
	"control-center", "preferences-system", "settings",
	"view-grid-symbolic", NULL,
};
static const char *const aliases_bluetooth[] = {
	"network-bluetooth", "bluetooth-active", "bluetooth",
	"preferences-system-bluetooth", NULL,
};
static const char *const aliases_airdrop[] = {
	"folder-publicshare", "folder-remote", "network-workgroup",
	"emblem-shared", NULL,
};
static const char *const aliases_focus[] = {
	"preferences-system-notifications", "preferences-desktop-notification",
	"notification-active", "notification-disabled", NULL,
};
static const char *const aliases_sound[] = {
	"audio-volume-high", "audio-volume-high-symbolic",
	"multimedia-volume-control", "audio-card", NULL,
};
static const char *const aliases_display[] = {
	"preferences-desktop-display", "display",
	"screen", NULL,
};
static const char *const aliases_showtime[] = {
	"org.gnome.Showtime", "Showtime",
	"video-player", "multimedia-video-player",
	"applications-multimedia", "video",
	"multimedia-player", "application-x-executable", NULL,
};
static const char *const aliases_video_player[] = {
	"video-player", "multimedia-video-player",
	"org.gnome.Showtime", "Showtime",
	"applications-multimedia", "video",
	"multimedia-player", "application-x-executable", NULL,
};
static const char *const aliases_keyboard[] = {
	"input-keyboard", "keyboard-brightness", "preferences-desktop-keyboard",
	"keyboard", NULL,
};
static const char *const aliases_generic_app[] = {
	"application-x-executable", "application-default-icon",
	"application", "exec", NULL,
};
static const char *const aliases_weather[] = {
	"weather-clear", "weather-clear-symbolic", "weather-few-clouds",
	"weather", NULL,
};
static const char *const aliases_system_menu[] = {
	"start-here", "start-here-symbolic",
	"view-app-grid", "view-app-grid-symbolic",
	"application-menu", "application-menu-symbolic",
	"orange-menu", NULL,
};
static const char *const aliases_app_grid[] = {
	"view-app-grid", "view-app-grid-symbolic",
	"shell-focus-app-grid-symbolic", "application-menu",
	"start-here", "application-x-executable", "launcher", NULL,
};
static const char *const aliases_trash[] = {
	"user-trash", "user-trash-full", "trash", NULL,
};
static const char *const aliases_files[] = {
	"system-file-manager", "org.gnome.Nautilus", "nautilus",
	"folder", "files", NULL,
};
static const char *const aliases_browser[] = {
	"firefox", "firefox-esr", "web-browser", "browser", NULL,
};
static const char *const aliases_calculator[] = {
	"org.gnome.Calculator", "accessories-calculator", "calculator", NULL,
};
static const char *const aliases_notes_app[] = {
	"org.gnome.Notes", "Gnome-notes", "Gnome-Notes",
	"notes", "accessories-notes", "stock_notes",
	"utilities-notes", "application-x-executable", NULL,
};
static const char *const aliases_text_editor[] = {
	"org.gnome.TextEditor", "gnome-text-editor",
	"text-editor", "accessories-text-editor",
	"utilities-text-editor", NULL,
};
static const char *const aliases_software[] = {
	"org.gnome.Software", "system-software-install",
	"software", "application-x-executable", NULL,
};
static const char *const aliases_terminal[] = {
	"org.gnome.Terminal", "utilities-terminal", "terminal",
	"application-x-executable", NULL,
};
static const char *const aliases_folder[] = {
	"folder", "inode-directory", "folder-new", "directory", NULL,
};
static const char *const aliases_folder_new[] = {
	"folder-new", "folder", NULL,
};
static const char *const aliases_maps[] = {
	"maps-app", "org.gnome.Maps", "maps", "gnome-maps", NULL,
};
static const char *const aliases_loupe[] = {
	"org.gnome.Loupe", "image-viewer",
	"accessories-image-viewer", "graphics-image-viewer",
	"org.gnome.eog", "eog", "image-x-generic",
	"applications-graphics", "photo", NULL,
};
static const char *const aliases_image_ext[] = {
	"image-viewer", "accessories-image-viewer",
	"org.gnome.Loupe", "graphics-image-viewer",
	"image-x-generic", "image", "photo",
	"applications-graphics", NULL,
};
static const char *const aliases_address_book[] = {
	"x-office-address-book", "org.gnome.Contacts", "contacts",
	"address-book", "x-office-contact",
	"application-x-executable", NULL,
};
static const char *const aliases_audio_generic[] = {
	"multimedia-audio-player", "audio-player",
	"org.gnome.Decibels", "applications-multimedia",
	"audio-x-generic", "audio", "music", NULL,
};
static const char *const aliases_calendar[] = {
	"x-office-calendar", "org.gnome.Calendar", "calendar",
	"application-x-executable", NULL,
};

struct icon_alias_set {
	const char *name;
	const char *const *aliases;
};

static const struct icon_alias_set icon_alias_sets[] = {
	{"orange-menu", aliases_system_menu},
	{"network-wireless", aliases_network_wireless},
	{"network-offline", aliases_network_offline},
	{"network-error", aliases_network_offline},
	{"battery", aliases_battery},
	{"battery-full", aliases_battery},
	{"edit-find", aliases_search},
	{"system-search", aliases_search},
	{"preferences-system", aliases_settings},
	{"org.gnome.Settings", aliases_settings},
	{"control-center", aliases_control_center},
	{"network-bluetooth", aliases_bluetooth},
	{"folder-publicshare", aliases_airdrop},
	{"preferences-system-notifications", aliases_focus},
	{"audio-volume-high", aliases_sound},
	{"preferences-desktop-display", aliases_display},
	{"video-display", aliases_video_player},
	{"org.gnome.Showtime", aliases_showtime},
	{"video-player", aliases_video_player},
	{"multimedia-video-player", aliases_video_player},
	{"input-keyboard", aliases_keyboard},
	{"application-x-executable", aliases_generic_app},
	{"application-default-icon", aliases_generic_app},
	{"weather-clear", aliases_weather},
	{"org.gnome.Weather", aliases_weather},
	{"view-app-grid", aliases_app_grid},
	{"start-here", aliases_system_menu},
	{"application-menu", aliases_system_menu},
	{"user-trash", aliases_trash},
	{"user-trash-full", aliases_trash},
	{"system-file-manager", aliases_files},
	{"org.gnome.Nautilus", aliases_files},
	{"nautilus", aliases_files},
	{"firefox", aliases_browser},
	{"web-browser", aliases_browser},
	{"org.gnome.Calculator", aliases_calculator},
	{"accessories-calculator", aliases_calculator},
	{"org.gnome.TextEditor", aliases_text_editor},
	{"accessories-text-editor", aliases_text_editor},
	{"text-editor", aliases_text_editor},
	{"org.gnome.Notes", aliases_notes_app},
	{"accessories-notes", aliases_notes_app},
	{"notes", aliases_notes_app},
	{"org.gnome.Software", aliases_software},
	{"system-software-install", aliases_software},
	{"org.gnome.Terminal", aliases_terminal},
	{"utilities-terminal", aliases_terminal},
	{"folder", aliases_folder},
	{"inode-directory", aliases_folder},
	{"directory", aliases_folder},
	{"folder-new", aliases_folder_new},
	{"maps-app", aliases_maps},
	{"image-x-generic", aliases_image_ext},
	{"org.gnome.Loupe", aliases_loupe},
	{"image-viewer", aliases_image_ext},
	{"accessories-image-viewer", aliases_image_ext},
	{"x-office-address-book", aliases_address_book},
	{"x-office-calendar", aliases_calendar},
	{"audio-x-generic", aliases_audio_generic},
};

static const char *const *aliases_for_icon(const char *name) {
	for (size_t i = 0; i < sizeof(icon_alias_sets) / sizeof(icon_alias_sets[0]); i++) {
		if (strcmp(icon_alias_sets[i].name, name) == 0) {
			return icon_alias_sets[i].aliases;
		}
	}
	return NULL;
}

static cairo_surface_t *resolve_icon_surface(
		const struct orange_assets *assets,
		const char *name) {
	if (assets == NULL || name == NULL || name[0] == '\0') {
		return NULL;
	}

	char path[4096];
	if (find_icon_file_with_fallbacks(path, sizeof(path), name, assets)) {
		return load_icon_file(path);
	}

	const char *const *aliases = aliases_for_icon(name);
	if (aliases != NULL) {
		for (int i = 0; aliases[i] != NULL; i++) {
			if (find_icon_file(path, sizeof(path), aliases[i], assets)) {
				return load_icon_file(path);
			}
		}
	}

	if (strcmp(name, "application-x-executable") != 0 &&
			find_icon_file_with_fallbacks(path, sizeof(path),
				"application-x-executable", assets)) {
		return load_icon_file(path);
	}

	if (find_icon_file_with_fallbacks(path, sizeof(path),
			"image-missing", assets)) {
		return load_icon_file(path);
	}

	return NULL;
}

static struct orange_named_icon *find_cached_icon(
		struct orange_named_icon *icons,
		int *count,
		int capacity,
		const char *name) {
	for (int i = 0; i < *count; i++) {
		if (strcmp(icons[i].name, name) == 0) {
			return &icons[i];
		}
	}
	if (*count >= capacity) {
		return NULL;
	}
	struct orange_named_icon *icon = &icons[*count];
	memset(icon, 0, sizeof(*icon));
	snprintf(icon->name, sizeof(icon->name), "%s", name);
	(*count)++;
	return icon;
}

void orange_assets_init(struct orange_assets *assets) {
	memset(assets, 0, sizeof(*assets));
}

bool orange_assets_load(
		struct orange_assets *assets,
		const char *asset_root,
		const char *icon_theme) {
	if (assets == NULL || asset_root == NULL || asset_root[0] == '\0') {
		return false;
	}

	snprintf(assets->asset_root, sizeof(assets->asset_root), "%s", asset_root);
	snprintf(assets->icon_theme, sizeof(assets->icon_theme), "%s",
		(icon_theme != NULL && icon_theme[0] != '\0') ? icon_theme : "hicolor");

	return true;
}

void orange_assets_finish(struct orange_assets *assets) {
	if (assets == NULL) {
		return;
	}
	if (assets->wallpaper != NULL) {
		cairo_surface_destroy(assets->wallpaper);
		assets->wallpaper = NULL;
	}
	if (assets->wallpaper_dark != NULL) {
		cairo_surface_destroy(assets->wallpaper_dark);
		assets->wallpaper_dark = NULL;
	}
	if (assets->wallpaper_scaled != NULL) {
		cairo_surface_destroy(assets->wallpaper_scaled);
		assets->wallpaper_scaled = NULL;
	}
	if (assets->wallpaper_dark_scaled != NULL) {
		cairo_surface_destroy(assets->wallpaper_dark_scaled);
		assets->wallpaper_dark_scaled = NULL;
	}
	for (int i = 0; i < assets->icon_count; i++) {
		for (int variant = 0; variant < ORANGE_ASSET_ICON_VARIANTS; variant++) {
			if (assets->icons[i].surface[variant] != NULL) {
				cairo_surface_destroy(assets->icons[i].surface[variant]);
				assets->icons[i].surface[variant] = NULL;
			}
			assets->icons[i].resolved[variant] = false;
		}
	}
	assets->icon_count = 0;
	assets->asset_root[0] = '\0';
	assets->icon_theme[0] = '\0';
	assets->wallpaper_checked = false;
	assets->wallpaper_dark_checked = false;
	assets->wallpaper_scaled_width = 0;
	assets->wallpaper_scaled_height = 0;
	assets->wallpaper_dark_scaled_width = 0;
	assets->wallpaper_dark_scaled_height = 0;
}

cairo_surface_t *orange_assets_wallpaper(
		struct orange_assets *assets,
		bool dark,
		int width,
		int height) {
	if (assets == NULL || width <= 0 || height <= 0) {
		return NULL;
	}

	cairo_surface_t *source = ensure_wallpaper_loaded(assets, dark);
	if (source == NULL) {
		return NULL;
	}

	cairo_surface_t **scaled = dark && assets->wallpaper_dark != NULL ?
		&assets->wallpaper_dark_scaled : &assets->wallpaper_scaled;
	int *scaled_width = dark && assets->wallpaper_dark != NULL ?
		&assets->wallpaper_dark_scaled_width : &assets->wallpaper_scaled_width;
	int *scaled_height = dark && assets->wallpaper_dark != NULL ?
		&assets->wallpaper_dark_scaled_height : &assets->wallpaper_scaled_height;
	if (*scaled != NULL && *scaled_width == width && *scaled_height == height) {
		return *scaled;
	}
	if (*scaled != NULL) {
		cairo_surface_destroy(*scaled);
		*scaled = NULL;
	}
	*scaled = scale_wallpaper(source, width, height);
	*scaled_width = *scaled != NULL ? width : 0;
	*scaled_height = *scaled != NULL ? height : 0;
	return *scaled;
}

cairo_surface_t *orange_assets_icon(
		struct orange_assets *assets,
		enum orange_asset_icon_variant variant,
		const char *name) {
	if (assets == NULL || name == NULL || name[0] == '\0' ||
			variant < 0 || variant >= ORANGE_ASSET_ICON_VARIANTS) {
		return NULL;
	}

	struct orange_named_icon *icon = find_cached_icon(assets->icons,
		&assets->icon_count, ORANGE_ASSET_ICON_MAX, name);
	if (icon == NULL) {
		return NULL;
	}

	if (icon->surface[variant] != NULL) {
		return icon->surface[variant];
	}
	enum orange_asset_icon_variant fallback =
		variant == ORANGE_ASSET_ICON_DARK ?
		ORANGE_ASSET_ICON_LIGHT : ORANGE_ASSET_ICON_DARK;
	if (icon->surface[fallback] != NULL) {
		return icon->surface[fallback];
	}
	if (!icon->resolved[variant]) {
		icon->surface[variant] = resolve_icon_surface(assets, name);
		icon->resolved[variant] = true;
	}
	if (icon->surface[variant] != NULL) {
		return icon->surface[variant];
	}
	return icon->surface[fallback];
}
