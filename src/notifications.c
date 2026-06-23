#include "orange/notifications.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool notification_id_in_use(
		const struct orange_notification_store *store,
		uint32_t id) {
	if (store == NULL || id == 0) {
		return false;
	}
	for (int i = 0; i < store->count; i++) {
		if (store->items[i].id == id) {
			return true;
		}
	}
	return false;
}

static uint32_t notification_next_id(
		struct orange_notification_store *store) {
	if (store->next_id == 0) {
		store->next_id = 1;
	}
	uint32_t id = store->next_id;
	for (uint32_t attempts = 0; attempts < UINT32_MAX; attempts++) {
		if (id == 0) {
			id = 1;
		}
		if (!notification_id_in_use(store, id)) {
			store->next_id = id + 1;
			if (store->next_id == 0) {
				store->next_id = 1;
			}
			return id;
		}
		id++;
	}
	return 0;
}

static int notification_find_index(
		const struct orange_notification_store *store,
		uint32_t id) {
	if (store == NULL || id == 0) {
		return -1;
	}
	for (int i = 0; i < store->count; i++) {
		if (store->items[i].id == id) {
			return i;
		}
	}
	return -1;
}

static bool copy_entity(char *dest, size_t dest_size, size_t *out,
		const char **src) {
	static const struct {
		const char *entity;
		char value;
	} entities[] = {
		{"&amp;", '&'},
		{"&lt;", '<'},
		{"&gt;", '>'},
		{"&quot;", '"'},
		{"&apos;", '\''},
	};
	for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); i++) {
		size_t len = strlen(entities[i].entity);
		if (strncmp(*src, entities[i].entity, len) == 0) {
			if (*out + 1 < dest_size) {
				dest[(*out)++] = entities[i].value;
			}
			*src += len;
			return true;
		}
	}
	return false;
}

static void copy_plain_text(char *dest, size_t dest_size, const char *src) {
	if (dest == NULL || dest_size == 0) {
		return;
	}
	dest[0] = '\0';
	if (src == NULL) {
		return;
	}

	bool in_tag = false;
	bool previous_space = false;
	size_t out = 0;
	for (const char *p = src; *p != '\0' && out + 1 < dest_size;) {
		unsigned char ch = (unsigned char)*p;
		if (ch == '<') {
			in_tag = true;
			p++;
			continue;
		}
		if (in_tag) {
			if (ch == '>') {
				in_tag = false;
			}
			p++;
			continue;
		}
		if (ch == '&' && copy_entity(dest, dest_size, &out, &p)) {
			previous_space = false;
			continue;
		}
		if (ch == '\n' || ch == '\r' || ch == '\t' ||
				(ch < 0x20 && ch != '\0')) {
			if (!previous_space && out + 1 < dest_size) {
				dest[out++] = ' ';
				previous_space = true;
			}
			p++;
			continue;
		}
		if (isspace(ch)) {
			if (!previous_space && out + 1 < dest_size) {
				dest[out++] = ' ';
				previous_space = true;
			}
			p++;
			continue;
		}
		dest[out++] = (char)ch;
		previous_space = false;
		p++;
	}
	while (out > 0 && dest[out - 1] == ' ') {
		out--;
	}
	dest[out] = '\0';
}

void orange_notification_store_init(
		struct orange_notification_store *store) {
	if (store == NULL) {
		return;
	}
	memset(store, 0, sizeof(*store));
	store->next_id = 1;
}

uint32_t orange_notification_store_upsert(
		struct orange_notification_store *store,
		uint32_t replaces_id,
		const char *app_name,
		const char *app_icon,
		const char *summary,
		const char *body,
		int expire_timeout,
		time_t created_at,
		uint32_t *evicted_id) {
	if (evicted_id != NULL) {
		*evicted_id = 0;
	}
	if (store == NULL) {
		return 0;
	}
	if (created_at == 0) {
		created_at = time(NULL);
	}

	int index = notification_find_index(store, replaces_id);
	if (index < 0) {
		if (store->count >= ORANGE_NOTIFICATION_MAX) {
			if (evicted_id != NULL) {
				*evicted_id =
					store->items[ORANGE_NOTIFICATION_MAX - 1].id;
			}
			store->count = ORANGE_NOTIFICATION_MAX - 1;
		}
		if (store->count > 0) {
			memmove(&store->items[1], &store->items[0],
				(size_t)store->count * sizeof(store->items[0]));
		}
		index = 0;
		store->count++;
		store->items[index].id = replaces_id != 0 ?
			replaces_id : notification_next_id(store);
		if (store->items[index].id == 0) {
			store->items[index].id = 1;
		}
		if (replaces_id >= store->next_id) {
			store->next_id = replaces_id + 1;
			if (store->next_id == 0) {
				store->next_id = 1;
			}
		}
	}

	struct orange_notification *item = &store->items[index];
	item->created_at = created_at;
	item->expire_timeout = expire_timeout;
	copy_plain_text(item->app_name, sizeof(item->app_name), app_name);
	copy_plain_text(item->app_icon, sizeof(item->app_icon), app_icon);
	copy_plain_text(item->summary, sizeof(item->summary), summary);
	copy_plain_text(item->body, sizeof(item->body), body);
	if (item->summary[0] == '\0') {
		if (item->app_name[0] != '\0') {
			char app_name[sizeof(item->app_name)];
			snprintf(app_name, sizeof(app_name), "%s", item->app_name);
			snprintf(item->summary, sizeof(item->summary),
				"%s", app_name);
		} else {
			snprintf(item->summary, sizeof(item->summary),
				"%s", "Notification");
		}
	}
	return item->id;
}

bool orange_notification_store_remove(
		struct orange_notification_store *store,
		uint32_t id) {
	int index = notification_find_index(store, id);
	if (index < 0) {
		return false;
	}
	for (int i = index; i + 1 < store->count; i++) {
		store->items[i] = store->items[i + 1];
	}
	store->count--;
	if (store->count < 0) {
		store->count = 0;
	}
	if (store->count < ORANGE_NOTIFICATION_MAX) {
		memset(&store->items[store->count], 0,
			sizeof(store->items[store->count]));
	}
	return true;
}
