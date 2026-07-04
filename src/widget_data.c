#include "orange/widget_data.h"

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#define USEC_PER_SEC 1000000u
#define SECONDS_PER_DAY 86400

#ifdef ORANGE_HAVE_SYSTEMD
#include <systemd/sd-login.h>
#endif

static void copy_text(char *dst, size_t dst_size, const char *src) {
	if (dst == NULL || dst_size == 0) {
		return;
	}
	snprintf(dst, dst_size, "%s", src != NULL ? src : "");
}

static char *trim_ascii(char *text) {
	if (text == NULL) {
		return NULL;
	}
	while (*text != '\0' && isspace((unsigned char)*text)) {
		text++;
	}
	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1])) {
		*--end = '\0';
	}
	return text;
}

void orange_widget_data_format_duration(uint64_t seconds,
		char *out,
		size_t out_size) {
	if (out == NULL || out_size == 0) {
		return;
	}
	uint64_t minutes = seconds / 60;
	uint64_t hours = minutes / 60;
	minutes %= 60;
	if (hours > 0) {
		snprintf(out, out_size, "%lluh %llum",
			(unsigned long long)hours,
			(unsigned long long)minutes);
	} else {
		snprintf(out, out_size, "%llum", (unsigned long long)minutes);
	}
}

time_t orange_widget_data_session_start_time(time_t fallback) {
#if defined(ORANGE_HAVE_SYSTEMD) && defined(ORANGE_HAVE_SD_SESSION_GET_START_TIME)
	char *session = NULL;
	uint64_t usec = 0;
	if (sd_pid_get_session(0, &session) >= 0 &&
			session != NULL &&
			sd_session_get_start_time(session, &usec) >= 0 &&
			usec > 0) {
		free(session);
		return (time_t)(usec / USEC_PER_SEC);
	}
	free(session);
#endif
	return fallback;
}

static void init_screen_time(struct orange_screen_time_data *screen_time,
		time_t now,
		time_t fallback_session_start) {
	memset(screen_time, 0, sizeof(*screen_time));
	time_t session_start = orange_widget_data_session_start_time(
		fallback_session_start);
	if (session_start <= 0 || session_start > now) {
		session_start = now;
	}
	screen_time->available = true;
	screen_time->seconds = (uint64_t)(now - session_start);
	orange_widget_data_format_duration(screen_time->seconds,
		screen_time->duration_label, sizeof(screen_time->duration_label));
	copy_text(screen_time->source, sizeof(screen_time->source),
		session_start == fallback_session_start ?
			"Orange session" : "Login session");
	copy_text(screen_time->detail, sizeof(screen_time->detail),
		session_start == fallback_session_start ?
			"Since Orange started" : "Since login");
}

void orange_widget_data_init(struct orange_widget_data *data,
		time_t now,
		time_t fallback_session_start) {
	if (data == NULL) {
		return;
	}
	memset(data, 0, sizeof(*data));
	copy_text(data->calendar.source, sizeof(data->calendar.source),
		"GNOME Calendar");
	init_screen_time(&data->screen_time, now, fallback_session_start);
	copy_text(data->weather.source, sizeof(data->weather.source),
		"GNOME Weather");
	copy_text(data->weather.location, sizeof(data->weather.location),
		"Weather");
	copy_text(data->weather.temperature, sizeof(data->weather.temperature),
		"--");
	copy_text(data->weather.condition, sizeof(data->weather.condition),
		"Current weather unavailable");
	copy_text(data->weather.detail, sizeof(data->weather.detail),
		"Set a GNOME Weather location or local weather file");
	copy_text(data->weather.icon_name, sizeof(data->weather.icon_name),
		"weather-none-available");
}

static int parse_digits(const char *text, int count) {
	int value = 0;
	for (int i = 0; i < count; i++) {
		if (text[i] < '0' || text[i] > '9') {
			return -1;
		}
		value = value * 10 + (text[i] - '0');
	}
	return value;
}

static bool parse_ics_datetime(const char *value,
		bool *all_day,
		time_t *out) {
	if (value == NULL || strlen(value) < 8 || out == NULL) {
		return false;
	}
	int year = parse_digits(value, 4);
	int month = parse_digits(value + 4, 2);
	int day = parse_digits(value + 6, 2);
	if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31) {
		return false;
	}

	struct tm tm = {
		.tm_year = year - 1900,
		.tm_mon = month - 1,
		.tm_mday = day,
		.tm_isdst = -1,
	};
	bool date_only = value[8] == '\0';
	if (!date_only) {
		if (value[8] != 'T' || strlen(value + 9) < 6) {
			return false;
		}
		tm.tm_hour = parse_digits(value + 9, 2);
		tm.tm_min = parse_digits(value + 11, 2);
		tm.tm_sec = parse_digits(value + 13, 2);
		if (tm.tm_hour < 0 || tm.tm_hour > 23 ||
				tm.tm_min < 0 || tm.tm_min > 59 ||
				tm.tm_sec < 0 || tm.tm_sec > 60) {
			return false;
		}
	}
	if (all_day != NULL) {
		*all_day = date_only;
	}
	size_t len = strlen(value);
	if (!date_only && len > 0 && value[len - 1] == 'Z') {
		*out = timegm(&tm);
	} else {
		*out = mktime(&tm);
	}
	return *out != (time_t)-1;
}

static const char *ics_value(const char *line) {
	const char *colon = strchr(line, ':');
	return colon != NULL ? colon + 1 : NULL;
}

static bool ics_property_is(const char *line, const char *name) {
	size_t n = strlen(name);
	return strncmp(line, name, n) == 0 &&
		(line[n] == ':' || line[n] == ';');
}

static void unescape_ics_text(char *dst, size_t dst_size, const char *src) {
	if (dst == NULL || dst_size == 0) {
		return;
	}
	size_t out = 0;
	for (size_t i = 0; src != NULL && src[i] != '\0' &&
			out + 1 < dst_size; i++) {
		if (src[i] == '\\' && src[i + 1] != '\0') {
			i++;
			switch (src[i]) {
			case 'n':
			case 'N':
				dst[out++] = ' ';
				break;
			case ',':
			case ';':
			case '\\':
				dst[out++] = src[i];
				break;
			default:
				dst[out++] = src[i];
				break;
			}
		} else {
			dst[out++] = src[i];
		}
	}
	dst[out] = '\0';
}

static time_t local_day_start(time_t now) {
	struct tm local;
	localtime_r(&now, &local);
	local.tm_hour = 0;
	local.tm_min = 0;
	local.tm_sec = 0;
	local.tm_isdst = -1;
	return mktime(&local);
}

static void format_event_time(struct orange_calendar_event *event) {
	if (event->all_day) {
		copy_text(event->time_label, sizeof(event->time_label), "All day");
		return;
	}
	struct tm local;
	localtime_r(&event->start, &local);
	char text[32];
	strftime(text, sizeof(text), "%I:%M %p", &local);
	copy_text(event->time_label, sizeof(event->time_label),
		text[0] == '0' ? text + 1 : text);
}

static void calendar_add_event(struct orange_calendar_data *calendar,
		const struct orange_calendar_event *event) {
	if (calendar == NULL || event == NULL) {
		return;
	}
	int count = calendar->event_count;
	if (count < ORANGE_WIDGET_EVENT_MAX) {
		calendar->events[count++] = *event;
	} else if (event->start >= calendar->events[count - 1].start) {
		return;
	} else {
		calendar->events[count - 1] = *event;
	}
	for (int i = count - 1; i > 0 &&
			calendar->events[i].start < calendar->events[i - 1].start; i--) {
		struct orange_calendar_event tmp = calendar->events[i - 1];
		calendar->events[i - 1] = calendar->events[i];
		calendar->events[i] = tmp;
	}
	calendar->event_count = count;
}

static void calendar_source_from_path(struct orange_calendar_data *calendar,
		const char *path) {
	if (calendar->source[0] != '\0') {
		return;
	}
	if (path != NULL && strstr(path, "evolution/calendar") != NULL) {
		copy_text(calendar->source, sizeof(calendar->source),
			"GNOME Calendar");
	} else {
		copy_text(calendar->source, sizeof(calendar->source), "Calendar");
	}
}

static void process_ics_line(struct orange_calendar_data *calendar,
		const char *path,
		time_t now,
		bool *in_event,
		bool *cancelled,
		bool *has_start,
		bool *has_end,
		struct orange_calendar_event *event,
		const char *line) {
	if (strcmp(line, "BEGIN:VEVENT") == 0) {
		memset(event, 0, sizeof(*event));
		*in_event = true;
		*cancelled = false;
		*has_start = false;
		*has_end = false;
		return;
	}
	if (!*in_event) {
		return;
	}
	if (strcmp(line, "END:VEVENT") == 0) {
		*in_event = false;
		if (*cancelled || !*has_start) {
			return;
		}
		if (!*has_end) {
			event->end = event->start + (event->all_day ? SECONDS_PER_DAY : 3600);
		}
		time_t day_start = local_day_start(now);
		time_t day_end = day_start + SECONDS_PER_DAY;
		if (event->start < day_end && event->end >= now) {
			if (event->title[0] == '\0') {
				copy_text(event->title, sizeof(event->title),
					"Calendar event");
			}
			format_event_time(event);
			calendar_add_event(calendar, event);
			calendar->available = true;
			calendar_source_from_path(calendar, path);
		}
		return;
	}
	if (ics_property_is(line, "STATUS")) {
		const char *value = ics_value(line);
		if (value != NULL && strcmp(value, "CANCELLED") == 0) {
			*cancelled = true;
		}
		return;
	}
	if (ics_property_is(line, "SUMMARY")) {
		unescape_ics_text(event->title, sizeof(event->title), ics_value(line));
		return;
	}
	if (ics_property_is(line, "DTSTART")) {
		bool all_day = false;
		if (parse_ics_datetime(ics_value(line), &all_day, &event->start)) {
			event->all_day = all_day ||
				strstr(line, "VALUE=DATE") != NULL;
			*has_start = true;
		}
		return;
	}
	if (ics_property_is(line, "DTEND")) {
		bool all_day = false;
		if (parse_ics_datetime(ics_value(line), &all_day, &event->end)) {
			(void)all_day;
			*has_end = true;
		}
	}
}

bool orange_widget_data_load_calendar_file(struct orange_widget_data *data,
		const char *path,
		time_t now) {
	if (data == NULL || path == NULL || path[0] == '\0') {
		return false;
	}
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	data->calendar.available = true;
	calendar_source_from_path(&data->calendar, path);
	bool in_event = false;
	bool cancelled = false;
	bool has_start = false;
	bool has_end = false;
	struct orange_calendar_event event;
	memset(&event, 0, sizeof(event));

	char *line = NULL;
	size_t cap = 0;
	char logical[4096] = "";
	while (getline(&line, &cap, file) >= 0) {
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}
		if ((line[0] == ' ' || line[0] == '\t') &&
				logical[0] != '\0') {
			strncat(logical, line + 1,
				sizeof(logical) - strlen(logical) - 1);
			continue;
		}
		if (logical[0] != '\0') {
			process_ics_line(&data->calendar, path, now, &in_event,
				&cancelled, &has_start, &has_end, &event, logical);
		}
		snprintf(logical, sizeof(logical), "%s", line);
	}
	if (logical[0] != '\0') {
		process_ics_line(&data->calendar, path, now, &in_event,
			&cancelled, &has_start, &has_end, &event, logical);
	}

	free(line);
	fclose(file);
	return true;
}

static void refresh_calendar_from_list(struct orange_widget_data *data,
		const char *list,
		time_t now) {
	if (list == NULL || list[0] == '\0') {
		return;
	}
	char *copy = strdup(list);
	if (copy == NULL) {
		return;
	}
	for (char *save = NULL, *part = strtok_r(copy, ":", &save);
			part != NULL;
			part = strtok_r(NULL, ":", &save)) {
		orange_widget_data_load_calendar_file(data, part, now);
	}
	free(copy);
}

static void refresh_calendar_from_glob(struct orange_widget_data *data,
		const char *pattern,
		time_t now) {
	if (pattern == NULL || pattern[0] == '\0') {
		return;
	}
	glob_t matches;
	memset(&matches, 0, sizeof(matches));
	if (glob(pattern, 0, NULL, &matches) == 0) {
		for (size_t i = 0; i < matches.gl_pathc; i++) {
			orange_widget_data_load_calendar_file(
				data, matches.gl_pathv[i], now);
		}
	}
	globfree(&matches);
}

static void refresh_calendar(struct orange_widget_data *data, time_t now) {
	const char *env = getenv("ORANGE_CALENDAR_FILE");
	if (env != NULL && env[0] != '\0') {
		refresh_calendar_from_list(data, env, now);
		return;
	}

	const char *xdg = getenv("XDG_DATA_HOME");
	char pattern[1024];
	if (xdg != NULL && xdg[0] != '\0') {
		snprintf(pattern, sizeof(pattern),
			"%s/evolution/calendar/*/calendar.ics", xdg);
		refresh_calendar_from_glob(data, pattern, now);
		return;
	}
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		snprintf(pattern, sizeof(pattern),
			"%s/.local/share/evolution/calendar/*/calendar.ics", home);
		refresh_calendar_from_glob(data, pattern, now);
	}
}

static bool key_value_line(const char *line, char *key, size_t key_size,
		char *value, size_t value_size) {
	const char *eq = strchr(line, '=');
	if (eq == NULL) {
		eq = strchr(line, ':');
	}
	if (eq == NULL) {
		return false;
	}
	size_t klen = (size_t)(eq - line);
	if (klen >= key_size) {
		klen = key_size - 1;
	}
	memcpy(key, line, klen);
	key[klen] = '\0';
	copy_text(value, value_size, eq + 1);
	char *trimmed_key = trim_ascii(key);
	char *trimmed_value = trim_ascii(value);
	if (trimmed_key != key) {
		memmove(key, trimmed_key, strlen(trimmed_key) + 1);
	}
	if (trimmed_value != value) {
		memmove(value, trimmed_value, strlen(trimmed_value) + 1);
	}
	for (char *p = key; *p != '\0'; p++) {
		*p = (char)tolower((unsigned char)*p);
	}
	return key[0] != '\0';
}

bool orange_widget_data_load_weather_file(struct orange_widget_data *data,
		const char *path) {
	if (data == NULL || path == NULL || path[0] == '\0') {
		return false;
	}
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	bool saw_value = false;
	char *line = NULL;
	size_t cap = 0;
	while (getline(&line, &cap, file) >= 0) {
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}
		char key[64];
		char value[ORANGE_WIDGET_TEXT_MAX];
		if (!key_value_line(line, key, sizeof(key), value, sizeof(value))) {
			continue;
		}
		if (strcmp(key, "location") == 0 || strcmp(key, "city") == 0) {
			copy_text(data->weather.location,
				sizeof(data->weather.location), value);
			saw_value = true;
		} else if (strcmp(key, "temperature") == 0 ||
				strcmp(key, "temp") == 0) {
			copy_text(data->weather.temperature,
				sizeof(data->weather.temperature), value);
			saw_value = true;
		} else if (strcmp(key, "condition") == 0 ||
				strcmp(key, "summary") == 0) {
			copy_text(data->weather.condition,
				sizeof(data->weather.condition), value);
			saw_value = true;
		} else if (strcmp(key, "detail") == 0 ||
				strcmp(key, "alert") == 0) {
			copy_text(data->weather.detail,
				sizeof(data->weather.detail), value);
			saw_value = true;
		} else if (strcmp(key, "icon") == 0 ||
				strcmp(key, "icon_name") == 0) {
			copy_text(data->weather.icon_name,
				sizeof(data->weather.icon_name), value);
		}
	}
	free(line);
	fclose(file);

	if (saw_value) {
		data->weather.available = true;
		copy_text(data->weather.source, sizeof(data->weather.source),
			"Local weather");
		if (data->weather.icon_name[0] == '\0') {
			copy_text(data->weather.icon_name,
				sizeof(data->weather.icon_name), "weather-clear");
		}
		if (data->weather.detail[0] == '\0') {
			copy_text(data->weather.detail,
				sizeof(data->weather.detail), "Updated locally");
		}
	}
	return saw_value;
}

static void update_weather_location_from_gsettings(
		struct orange_widget_data *data) {
	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL) {
		return;
	}
	GSettingsSchema *schema = g_settings_schema_source_lookup(
		source, "org.gnome.GWeather4", true);
	if (schema == NULL) {
		return;
	}
	g_settings_schema_unref(schema);

	GSettings *settings = g_settings_new("org.gnome.GWeather4");
	GVariant *location = g_settings_get_value(settings, "default-location");
	if (location != NULL && g_variant_n_children(location) >= 2) {
		const char *name = NULL;
		const char *station = NULL;
		GVariant *child = g_variant_get_child_value(location, 0);
		if (child != NULL) {
			name = g_variant_get_string(child, NULL);
			if (name != NULL && name[0] != '\0') {
				copy_text(data->weather.location,
					sizeof(data->weather.location), name);
			}
			g_variant_unref(child);
		}
		child = g_variant_get_child_value(location, 1);
		if (child != NULL) {
			station = g_variant_get_string(child, NULL);
			if (data->weather.location[0] == '\0' ||
					strcmp(data->weather.location, "Weather") == 0) {
				copy_text(data->weather.location,
					sizeof(data->weather.location),
					station != NULL && station[0] != '\0' ?
						station : "Weather");
			}
			if (!data->weather.available && station != NULL &&
					station[0] != '\0') {
				snprintf(data->weather.detail,
					sizeof(data->weather.detail),
					"GNOME Weather location: %s", station);
			}
			g_variant_unref(child);
		}
		g_variant_unref(location);
	}
	g_object_unref(settings);
}

static void refresh_weather_from_default_files(struct orange_widget_data *data) {
	const char *env = getenv("ORANGE_WEATHER_FILE");
	if (env != NULL && env[0] != '\0' &&
			orange_widget_data_load_weather_file(data, env)) {
		return;
	}

	const char *cache = getenv("XDG_CACHE_HOME");
	char path[1024];
	if (cache != NULL && cache[0] != '\0') {
		snprintf(path, sizeof(path), "%s/orange/weather", cache);
		if (orange_widget_data_load_weather_file(data, path)) {
			return;
		}
		snprintf(path, sizeof(path), "%s/orange/weather.txt", cache);
		if (orange_widget_data_load_weather_file(data, path)) {
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		snprintf(path, sizeof(path), "%s/.cache/orange/weather", home);
		if (orange_widget_data_load_weather_file(data, path)) {
			return;
		}
		snprintf(path, sizeof(path), "%s/.cache/orange/weather.txt", home);
		orange_widget_data_load_weather_file(data, path);
	}
}

bool orange_widget_data_refresh(struct orange_widget_data *data,
		time_t now,
		time_t fallback_session_start) {
	if (data == NULL) {
		return false;
	}
	struct orange_widget_data next;
	orange_widget_data_init(&next, now, fallback_session_start);
	refresh_calendar(&next, now);
	update_weather_location_from_gsettings(&next);
	refresh_weather_from_default_files(&next);

	bool changed = memcmp(data, &next, sizeof(next)) != 0;
	*data = next;
	return changed;
}
