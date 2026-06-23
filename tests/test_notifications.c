#include "orange/notifications.h"

#include <assert.h>
#include <string.h>

static void test_add_and_strip_notification_text(void) {
	struct orange_notification_store store;
	orange_notification_store_init(&store);

	uint32_t evicted = 99;
	uint32_t id = orange_notification_store_upsert(&store, 0,
		"Mail", "mail-message-new", "<b>Hello</b>",
		"Ready &amp; done\nnext", -1, 1000, &evicted);

	assert(id != 0);
	assert(evicted == 0);
	assert(store.count == 1);
	assert(strcmp(store.items[0].app_name, "Mail") == 0);
	assert(strcmp(store.items[0].summary, "Hello") == 0);
	assert(strcmp(store.items[0].body, "Ready & done next") == 0);
}

static void test_replace_keeps_id_and_count(void) {
	struct orange_notification_store store;
	orange_notification_store_init(&store);

	uint32_t id = orange_notification_store_upsert(&store, 0,
		"Build", "dialog-information", "Started", "",
		-1, 1000, NULL);
	uint32_t replaced = orange_notification_store_upsert(&store, id,
		"Build", "dialog-information", "Finished", "All tests passed",
		-1, 1010, NULL);

	assert(replaced == id);
	assert(store.count == 1);
	assert(strcmp(store.items[0].summary, "Finished") == 0);
	assert(strcmp(store.items[0].body, "All tests passed") == 0);
}

static void test_capacity_evicts_oldest(void) {
	struct orange_notification_store store;
	orange_notification_store_init(&store);

	uint32_t first = 0;
	for (int i = 0; i < ORANGE_NOTIFICATION_MAX; i++) {
		uint32_t id = orange_notification_store_upsert(&store, 0,
			"App", "dialog-information", "Item", "",
			-1, 1000 + i, NULL);
		if (i == 0) {
			first = id;
		}
	}

	uint32_t evicted = 0;
	orange_notification_store_upsert(&store, 0,
		"App", "dialog-information", "Newest", "",
		-1, 2000, &evicted);

	assert(store.count == ORANGE_NOTIFICATION_MAX);
	assert(evicted == first);
	assert(strcmp(store.items[0].summary, "Newest") == 0);
	for (int i = 0; i < store.count; i++) {
		assert(store.items[i].id != first);
	}
}

static void test_remove_notification(void) {
	struct orange_notification_store store;
	orange_notification_store_init(&store);

	uint32_t id = orange_notification_store_upsert(&store, 0,
		"App", "", "Summary", "", -1, 1000, NULL);
	assert(orange_notification_store_remove(&store, id));
	assert(store.count == 0);
	assert(!orange_notification_store_remove(&store, id));
}

int main(void) {
	test_add_and_strip_notification_text();
	test_replace_keeps_id_and_count();
	test_capacity_evicts_oldest();
	test_remove_notification();
	return 0;
}
