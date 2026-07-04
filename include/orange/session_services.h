#ifndef ORANGE_SESSION_SERVICES_H
#define ORANGE_SESSION_SERVICES_H

#include <gio/gio.h>
#include <stdbool.h>
#include <stdint.h>

#include "orange/config.h"
#include "orange/status_notifier.h"

struct orange_session_services {
	guint portal_backend_name_id;
	guint portal_frontend_name_id;
	guint portal_backend_registration_id;
	guint portal_frontend_registration_id;
	guint display_config_name_id;
	guint display_config_registration_id;
	guint status_notifier_watcher_name_id;
	guint status_notifier_watcher_registration_id;
	guint status_notifier_name_owner_subscription_id;
	guint status_notifier_item_signal_subscription_id;
	uint32_t display_config_serial;
	enum orange_appearance appearance;
	struct orange_status_notifier_item status_notifier_items[
		ORANGE_STATUS_NOTIFIER_ITEM_MAX];
	int status_notifier_item_count;
	void (*status_notifier_changed)(void *data);
	void *status_notifier_data;
	GVariant *(*display_config_state)(void *data, uint32_t serial);
	bool (*display_config_apply)(
		void *data,
		uint32_t method,
		GVariant *logical_monitors,
		GVariant *properties,
		GError **error);
	void *display_config_data;
};

void orange_session_services_init(struct orange_session_services *services);
void orange_session_services_setup(
	struct orange_session_services *services,
	enum orange_appearance appearance,
	GVariant *(*display_config_state)(void *data, uint32_t serial),
	bool (*display_config_apply)(
		void *data,
		uint32_t method,
		GVariant *logical_monitors,
		GVariant *properties,
		GError **error),
	void *display_config_data);
void orange_session_services_set_status_notifier_changed_handler(
	struct orange_session_services *services,
	void (*changed)(void *data),
	void *data);
void orange_session_services_finish(struct orange_session_services *services);
void orange_session_services_set_appearance(
	struct orange_session_services *services,
	enum orange_appearance appearance);
void orange_session_services_emit_setting_changed(
	struct orange_session_services *services);
void orange_session_services_emit_monitors_changed(
	struct orange_session_services *services);
GVariant *orange_session_services_display_config_mode_properties(void);
GVariant *orange_session_services_display_config_monitor_properties(
	const char *connector,
	const char *vendor,
	const char *product);
GVariant *orange_session_services_display_config_state_properties(void);
int orange_session_services_copy_status_notifier_items(
	struct orange_session_services *services,
	struct orange_status_notifier_item *items,
	int capacity);
bool orange_session_services_refresh_status_notifier_items(
	struct orange_session_services *services);
bool orange_session_services_activate_status_notifier_item(
	struct orange_session_services *services,
	const char *id,
	int x,
	int y);
bool orange_session_services_context_menu_status_notifier_item(
	struct orange_session_services *services,
	const char *id,
	int x,
	int y);

#endif
