#include "orange/widget_data.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static time_t utc_time(int year,
		int month,
		int day,
		int hour,
		int minute,
		int second) {
	struct tm tm = {
		.tm_year = year - 1900,
		.tm_mon = month - 1,
		.tm_mday = day,
		.tm_hour = hour,
		.tm_min = minute,
		.tm_sec = second,
		.tm_isdst = -1,
	};
	return mktime(&tm);
}

static void write_temp_file(char path[64], const char *contents) {
	snprintf(path, 64, "/tmp/orange-widget-data-XXXXXX");
	int fd = mkstemp(path);
	assert(fd >= 0);
	size_t len = strlen(contents);
	assert(write(fd, contents, len) == (ssize_t)len);
	(void)len;
	close(fd);
}

static void test_calendar_file_loads_upcoming_events(void) {
	setenv("TZ", "UTC", 1);
	tzset();
	time_t now = utc_time(2026, 6, 21, 12, 0, 0);
	char path[64];
	write_temp_file(path,
		"BEGIN:VCALENDAR\r\n"
		"BEGIN:VEVENT\r\n"
		"DTSTART;VALUE=DATE:20260621\r\n"
		"SUMMARY:All-day plan\r\n"
		"END:VEVENT\r\n"
		"BEGIN:VEVENT\r\n"
		"DTSTART:20260621T133000Z\r\n"
		"DTEND:20260621T143000Z\r\n"
		"SUMMARY:Standup\\, demo\r\n"
		"END:VEVENT\r\n"
		"BEGIN:VEVENT\r\n"
		"DTSTART:20260621T150000Z\r\n"
		"SUMMARY:Cancelled\r\n"
		"STATUS:CANCELLED\r\n"
		"END:VEVENT\r\n"
		"END:VCALENDAR\r\n");

	struct orange_widget_data data;
	orange_widget_data_init(&data, now, now - 3600);
	assert(orange_widget_data_load_calendar_file(&data, path, now));
	assert(data.calendar.available);
	assert(data.calendar.event_count == 2);
	assert(data.calendar.events[0].all_day);
	assert(strcmp(data.calendar.events[0].time_label, "All day") == 0);
	assert(strcmp(data.calendar.events[0].title, "All-day plan") == 0);
	assert(!data.calendar.events[1].all_day);
	assert(strcmp(data.calendar.events[1].time_label, "1:30 PM") == 0);
	assert(strcmp(data.calendar.events[1].title, "Standup, demo") == 0);
	unlink(path);
}

static void test_weather_file_loads_local_values(void) {
	char path[64];
	write_temp_file(path,
		"location=Memphis\n"
		"temperature=79F\n"
		"condition=Clear\n"
		"detail=Updated by GNOME Weather cache\n"
		"icon=weather-clear\n");

	struct orange_widget_data data;
	orange_widget_data_init(&data, time(NULL), time(NULL) - 3600);
	assert(orange_widget_data_load_weather_file(&data, path));
	assert(data.weather.available);
	assert(strcmp(data.weather.location, "Memphis") == 0);
	assert(strcmp(data.weather.temperature, "79F") == 0);
	assert(strcmp(data.weather.condition, "Clear") == 0);
	assert(strcmp(data.weather.icon_name, "weather-clear") == 0);
	unlink(path);
}

static void test_duration_formatting(void) {
	char label[32];
	orange_widget_data_format_duration(3660, label, sizeof(label));
	assert(strcmp(label, "1h 1m") == 0);
	orange_widget_data_format_duration(59, label, sizeof(label));
	assert(strcmp(label, "0m") == 0);
}

int main(void) {
	test_calendar_file_loads_upcoming_events();
	test_weather_file_loads_local_values();
	test_duration_formatting();
	return 0;
}
