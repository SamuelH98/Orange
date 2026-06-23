#ifndef ORANGE_WIDGET_DATA_H
#define ORANGE_WIDGET_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define ORANGE_WIDGET_EVENT_MAX 3
#define ORANGE_WIDGET_TEXT_MAX 96
#define ORANGE_WIDGET_LABEL_MAX 32
#define ORANGE_WIDGET_ICON_NAME_MAX 64

struct orange_calendar_event {
	time_t start;
	time_t end;
	bool all_day;
	char title[ORANGE_WIDGET_TEXT_MAX];
	char time_label[ORANGE_WIDGET_LABEL_MAX];
};

struct orange_calendar_data {
	bool available;
	char source[ORANGE_WIDGET_TEXT_MAX];
	int event_count;
	struct orange_calendar_event events[ORANGE_WIDGET_EVENT_MAX];
};

struct orange_screen_time_data {
	bool available;
	uint64_t seconds;
	char duration_label[ORANGE_WIDGET_LABEL_MAX];
	char detail[ORANGE_WIDGET_TEXT_MAX];
	char source[ORANGE_WIDGET_TEXT_MAX];
};

struct orange_weather_data {
	bool available;
	char source[ORANGE_WIDGET_TEXT_MAX];
	char location[ORANGE_WIDGET_TEXT_MAX];
	char temperature[ORANGE_WIDGET_LABEL_MAX];
	char condition[ORANGE_WIDGET_TEXT_MAX];
	char detail[ORANGE_WIDGET_TEXT_MAX];
	char icon_name[ORANGE_WIDGET_ICON_NAME_MAX];
};

struct orange_widget_data {
	struct orange_calendar_data calendar;
	struct orange_screen_time_data screen_time;
	struct orange_weather_data weather;
};

void orange_widget_data_init(struct orange_widget_data *data,
	time_t now,
	time_t fallback_session_start);
bool orange_widget_data_refresh(struct orange_widget_data *data,
	time_t now,
	time_t fallback_session_start);
time_t orange_widget_data_session_start_time(time_t fallback);

bool orange_widget_data_load_calendar_file(struct orange_widget_data *data,
	const char *path,
	time_t now);
bool orange_widget_data_load_weather_file(struct orange_widget_data *data,
	const char *path);
void orange_widget_data_format_duration(uint64_t seconds,
	char *out,
	size_t out_size);

#endif
