#ifndef ORANGE_NOTIFICATIONS_H
#define ORANGE_NOTIFICATIONS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "orange/assets.h"

#define ORANGE_NOTIFICATION_MAX 16
#define ORANGE_NOTIFICATION_APP_NAME_MAX 96
#define ORANGE_NOTIFICATION_SUMMARY_MAX 160
#define ORANGE_NOTIFICATION_BODY_MAX 320

struct orange_notification {
	uint32_t id;
	time_t created_at;
	int expire_timeout;
	char app_name[ORANGE_NOTIFICATION_APP_NAME_MAX];
	char app_icon[ORANGE_ASSET_ICON_NAME_MAX];
	char summary[ORANGE_NOTIFICATION_SUMMARY_MAX];
	char body[ORANGE_NOTIFICATION_BODY_MAX];
};

struct orange_notification_store {
	struct orange_notification items[ORANGE_NOTIFICATION_MAX];
	int count;
	uint32_t next_id;
};

void orange_notification_store_init(
	struct orange_notification_store *store);
uint32_t orange_notification_store_upsert(
	struct orange_notification_store *store,
	uint32_t replaces_id,
	const char *app_name,
	const char *app_icon,
	const char *summary,
	const char *body,
	int expire_timeout,
	time_t created_at,
	uint32_t *evicted_id);
bool orange_notification_store_remove(
	struct orange_notification_store *store,
	uint32_t id);

#endif
