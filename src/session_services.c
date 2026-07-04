#include "orange/session_services.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ORANGE_DBUS_NAME_FLAG_DO_NOT_QUEUE 4u
#define ORANGE_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1u
#define ORANGE_DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER 4u

static GDBusConnection *session_services_connection(void) {
	static GDBusConnection *connection = NULL;
	if (connection != NULL && !g_dbus_connection_is_closed(connection)) {
		return connection;
	}
	if (connection != NULL) {
		g_object_unref(connection);
		connection = NULL;
	}

	GError *error = NULL;
	connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error_free(error);
	}
	return connection;
}

static guint session_services_request_name(const char *name) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return 0;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"org.freedesktop.DBus",
		"RequestName",
		g_variant_new("(su)", name, ORANGE_DBUS_NAME_FLAG_DO_NOT_QUEUE),
		G_VARIANT_TYPE("(u)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error);
	if (reply == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return 0;
	}

	guint32 result = 0;
	g_variant_get(reply, "(u)", &result);
	g_variant_unref(reply);
	return result == ORANGE_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
			result == ORANGE_DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
}

static void session_services_release_name(guint *owned, const char *name) {
	if (owned == NULL || *owned == 0) {
		return;
	}
	*owned = 0;

	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"org.freedesktop.DBus",
		"ReleaseName",
		g_variant_new("(s)", name),
		G_VARIANT_TYPE("(u)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error);
	if (reply != NULL) {
		g_variant_unref(reply);
	} else if (error != NULL) {
		g_error_free(error);
	}
}

static void status_notifier_notify_changed(
		struct orange_session_services *services) {
	if (services != NULL && services->status_notifier_changed != NULL) {
		services->status_notifier_changed(services->status_notifier_data);
	}
}

static void status_notifier_emit_item_signal(
		const char *signal_name,
		const char *id) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL || signal_name == NULL || id == NULL) {
		return;
	}
	g_dbus_connection_emit_signal(
		connection,
		NULL,
		"/StatusNotifierWatcher",
		"org.kde.StatusNotifierWatcher",
		signal_name,
		g_variant_new("(s)", id),
		NULL);
}

static void status_notifier_emit_host_registered(void) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return;
	}
	g_dbus_connection_emit_signal(
		connection,
		NULL,
		"/StatusNotifierWatcher",
		"org.kde.StatusNotifierWatcher",
		"StatusNotifierHostRegistered",
		NULL,
		NULL);
}

static int status_notifier_find_index(
		const struct orange_session_services *services,
		const char *id) {
	if (services == NULL || id == NULL) {
		return -1;
	}
	for (int i = 0; i < services->status_notifier_item_count; i++) {
		if (strcmp(services->status_notifier_items[i].id, id) == 0) {
			return i;
		}
	}
	return -1;
}

static bool status_notifier_normalize_item(
		const char *sender,
		const char *service,
		char *out_service,
		size_t out_service_size,
		char *out_path,
		size_t out_path_size,
		char *out_id,
		size_t out_id_size) {
	if (sender == NULL || sender[0] == '\0' ||
			service == NULL || service[0] == '\0' ||
			out_service == NULL || out_path == NULL || out_id == NULL ||
			out_service_size == 0 || out_path_size == 0 || out_id_size == 0) {
		return false;
	}

	if (service[0] == '/') {
		snprintf(out_service, out_service_size, "%s", sender);
		snprintf(out_path, out_path_size, "%s", service);
	} else {
		const char *slash = strchr(service, '/');
		if (slash != NULL) {
			size_t bus_len = (size_t)(slash - service);
			if (bus_len == 0 || bus_len >= out_service_size) {
				return false;
			}
			memcpy(out_service, service, bus_len);
			out_service[bus_len] = '\0';
			snprintf(out_path, out_path_size, "%s", slash);
		} else {
			snprintf(out_service, out_service_size, "%s", service);
			snprintf(out_path, out_path_size, "%s", "/StatusNotifierItem");
		}
	}
	if (out_service[0] == '\0' || out_path[0] != '/') {
		return false;
	}
	snprintf(out_id, out_id_size, "%s%s", out_service, out_path);
	return true;
}

static bool status_notifier_item_visible(
		const struct orange_status_notifier_item *item) {
	return item != NULL &&
		strcmp(item->status, "Passive") != 0 &&
		(item->icon_name[0] != '\0' ||
		 item->attention_icon_name[0] != '\0');
}

static bool status_notifier_fetch_item_properties(
		struct orange_status_notifier_item *item) {
	if (item == NULL || item->service[0] == '\0' || item->path[0] == '\0') {
		return false;
	}

	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return false;
	}

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		item->service,
		item->path,
		"org.freedesktop.DBus.Properties",
		"GetAll",
		g_variant_new("(s)", "org.kde.StatusNotifierItem"),
		G_VARIANT_TYPE("(a{sv})"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		160,
		NULL,
		&error);
	if (reply == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return false;
	}

	GVariant *props = NULL;
	g_variant_get(reply, "(@a{sv})", &props);
	g_variant_unref(reply);
	if (props == NULL) {
		return false;
	}

	const char *text = NULL;
	item->title[0] = '\0';
	item->icon_name[0] = '\0';
	item->attention_icon_name[0] = '\0';
	item->status[0] = '\0';
	item->menu_path[0] = '\0';
	item->has_menu = false;
	item->item_is_menu = false;
	if (g_variant_lookup(props, "Title", "&s", &text) &&
			text != NULL) {
		snprintf(item->title, sizeof(item->title), "%s", text);
	}
	if (g_variant_lookup(props, "IconName", "&s", &text) &&
			text != NULL) {
		snprintf(item->icon_name, sizeof(item->icon_name), "%s", text);
	}
	if (g_variant_lookup(props, "AttentionIconName", "&s", &text) &&
			text != NULL) {
		snprintf(item->attention_icon_name,
			sizeof(item->attention_icon_name), "%s", text);
	}
	if (g_variant_lookup(props, "Status", "&s", &text) &&
			text != NULL) {
		snprintf(item->status, sizeof(item->status), "%s", text);
	}
	if (item->status[0] == '\0') {
		snprintf(item->status, sizeof(item->status), "%s", "Active");
	}
	if (g_variant_lookup(props, "Menu", "&o", &text) &&
			text != NULL && text[0] != '\0') {
		snprintf(item->menu_path, sizeof(item->menu_path), "%s", text);
		item->has_menu = true;
	}
	gboolean item_is_menu = FALSE;
	if (g_variant_lookup(props, "ItemIsMenu", "b", &item_is_menu)) {
		item->item_is_menu = item_is_menu;
	}
	g_variant_unref(props);
	return true;
}

static bool status_notifier_add_or_update_item(
		struct orange_session_services *services,
		const char *sender,
		const char *service) {
	char normalized_service[ORANGE_STATUS_NOTIFIER_SERVICE_MAX];
	char path[ORANGE_STATUS_NOTIFIER_PATH_MAX];
	char id[ORANGE_STATUS_NOTIFIER_ID_MAX];
	if (!status_notifier_normalize_item(sender, service,
			normalized_service, sizeof(normalized_service),
			path, sizeof(path),
			id, sizeof(id))) {
		return false;
	}

	int index = status_notifier_find_index(services, id);
	bool added = false;
	if (index < 0) {
		if (services->status_notifier_item_count >=
				ORANGE_STATUS_NOTIFIER_ITEM_MAX) {
			return false;
		}
		index = services->status_notifier_item_count++;
		memset(&services->status_notifier_items[index], 0,
			sizeof(services->status_notifier_items[index]));
		added = true;
	}

	struct orange_status_notifier_item *item =
		&services->status_notifier_items[index];
	struct orange_status_notifier_item before = *item;
	snprintf(item->id, sizeof(item->id), "%s", id);
	snprintf(item->service, sizeof(item->service), "%s", normalized_service);
	snprintf(item->path, sizeof(item->path), "%s", path);
	if (!status_notifier_fetch_item_properties(item) &&
			item->icon_name[0] == '\0') {
		snprintf(item->icon_name, sizeof(item->icon_name),
			"%s", "application-x-executable");
	}

	if (added) {
		status_notifier_emit_item_signal(
			"StatusNotifierItemRegistered", item->id);
		status_notifier_notify_changed(services);
		return true;
	}
	if (memcmp(&before, item, sizeof(*item)) != 0) {
		status_notifier_notify_changed(services);
		return true;
	}
	return true;
}

static bool status_notifier_remove_at(
		struct orange_session_services *services,
		int index) {
	if (services == NULL || index < 0 ||
			index >= services->status_notifier_item_count) {
		return false;
	}
	char id[ORANGE_STATUS_NOTIFIER_ID_MAX];
	snprintf(id, sizeof(id), "%s",
		services->status_notifier_items[index].id);
	for (int i = index; i + 1 < services->status_notifier_item_count; i++) {
		services->status_notifier_items[i] =
			services->status_notifier_items[i + 1];
	}
	services->status_notifier_item_count--;
	if (services->status_notifier_item_count >= 0 &&
			services->status_notifier_item_count <
			ORANGE_STATUS_NOTIFIER_ITEM_MAX) {
		memset(&services->status_notifier_items[
			services->status_notifier_item_count], 0,
			sizeof(services->status_notifier_items[0]));
	}
	status_notifier_emit_item_signal(
		"StatusNotifierItemUnregistered", id);
	status_notifier_notify_changed(services);
	return true;
}

static const char status_notifier_watcher_introspection_xml[] =
	"<node>"
	"  <interface name='org.kde.StatusNotifierWatcher'>"
	"    <method name='RegisterStatusNotifierItem'>"
	"      <arg type='s' name='service' direction='in'/>"
	"    </method>"
	"    <method name='RegisterStatusNotifierHost'>"
	"      <arg type='s' name='service' direction='in'/>"
	"    </method>"
	"    <property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
	"    <property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
	"    <property name='ProtocolVersion' type='i' access='read'/>"
	"    <signal name='StatusNotifierItemRegistered'>"
	"      <arg type='s' name='service'/>"
	"    </signal>"
	"    <signal name='StatusNotifierItemUnregistered'>"
	"      <arg type='s' name='service'/>"
	"    </signal>"
	"    <signal name='StatusNotifierHostRegistered'/>"
	"    <signal name='StatusNotifierHostUnregistered'/>"
	"  </interface>"
	"</node>";

static void handle_status_notifier_watcher_method_call(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)object_path;
	(void)interface_name;

	if (services == NULL) {
		g_dbus_method_invocation_return_error(invocation,
			G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Orange StatusNotifierWatcher is unavailable");
		return;
	}
	if (strcmp(method_name, "RegisterStatusNotifierItem") == 0) {
		const char *service = NULL;
		g_variant_get(parameters, "(&s)", &service);
		if (!status_notifier_add_or_update_item(
				services, sender, service)) {
			g_dbus_method_invocation_return_error(invocation,
				G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
				"Invalid or unsupported StatusNotifierItem");
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (strcmp(method_name, "RegisterStatusNotifierHost") == 0) {
		(void)parameters;
		status_notifier_emit_host_registered();
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else {
		g_dbus_method_invocation_return_error(invocation,
			G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
			"Unknown method: %s", method_name);
	}
}

static GVariant *handle_status_notifier_watcher_get_property(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *property_name,
		GError **error,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;
	(void)error;

	if (strcmp(property_name, "RegisteredStatusNotifierItems") == 0) {
		GVariantBuilder builder;
		g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
		if (services != NULL) {
			for (int i = 0; i < services->status_notifier_item_count; i++) {
				g_variant_builder_add(&builder, "s",
					services->status_notifier_items[i].id);
			}
		}
		return g_variant_builder_end(&builder);
	}
	if (strcmp(property_name, "IsStatusNotifierHostRegistered") == 0) {
		return g_variant_new_boolean(true);
	}
	if (strcmp(property_name, "ProtocolVersion") == 0) {
		return g_variant_new_int32(0);
	}
	return NULL;
}

static void handle_status_notifier_name_owner_changed(
		GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)sender_name;
	(void)object_path;
	(void)interface_name;
	(void)signal_name;

	const char *name = NULL;
	const char *old_owner = NULL;
	const char *new_owner = NULL;
	g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
	if (services == NULL || name == NULL || new_owner == NULL ||
			new_owner[0] != '\0') {
		return;
	}
	for (int i = services->status_notifier_item_count - 1; i >= 0; i--) {
		struct orange_status_notifier_item *item =
			&services->status_notifier_items[i];
		if (strcmp(item->service, name) == 0 ||
				(old_owner != NULL && old_owner[0] != '\0' &&
				 strcmp(item->service, old_owner) == 0)) {
			status_notifier_remove_at(services, i);
		}
	}
}

static void handle_status_notifier_item_signal(
		GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)interface_name;
	(void)signal_name;
	(void)parameters;

	if (services == NULL || object_path == NULL) {
		return;
	}
	bool changed = false;
	for (int i = 0; i < services->status_notifier_item_count; i++) {
		struct orange_status_notifier_item *item =
			&services->status_notifier_items[i];
		if (strcmp(item->path, object_path) != 0) {
			continue;
		}
		if (sender_name != NULL && sender_name[0] != '\0' &&
				item->service[0] == ':' &&
				strcmp(item->service, sender_name) != 0) {
			continue;
		}
		struct orange_status_notifier_item before = *item;
		if (!status_notifier_fetch_item_properties(item) &&
				item->icon_name[0] == '\0') {
			snprintf(item->icon_name, sizeof(item->icon_name),
				"%s", "application-x-executable");
		}
		if (memcmp(&before, item, sizeof(*item)) != 0) {
			changed = true;
		}
	}
	if (changed) {
		status_notifier_notify_changed(services);
	}
}

static void session_services_setup_status_notifier(
		struct orange_session_services *services) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return;
	}

	GError *error = NULL;
	GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(
		status_notifier_watcher_introspection_xml, &error);
	if (introspection_data == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return;
	}

	static const GDBusInterfaceVTable interface_vtable = {
		.method_call = handle_status_notifier_watcher_method_call,
		.get_property = handle_status_notifier_watcher_get_property,
	};
	services->status_notifier_watcher_registration_id =
		g_dbus_connection_register_object(
			connection,
			"/StatusNotifierWatcher",
			introspection_data->interfaces[0],
			&interface_vtable,
			services,
			NULL,
			&error);
	if (services->status_notifier_watcher_registration_id == 0) {
		if (error != NULL) {
			g_error_free(error);
		}
		g_dbus_node_info_unref(introspection_data);
		return;
	}
	g_dbus_node_info_unref(introspection_data);

	services->status_notifier_name_owner_subscription_id =
		g_dbus_connection_signal_subscribe(
			connection,
			"org.freedesktop.DBus",
			"org.freedesktop.DBus",
			"NameOwnerChanged",
			"/org/freedesktop/DBus",
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			handle_status_notifier_name_owner_changed,
			services,
			NULL);
	services->status_notifier_item_signal_subscription_id =
		g_dbus_connection_signal_subscribe(
			connection,
			NULL,
			"org.kde.StatusNotifierItem",
			NULL,
			NULL,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			handle_status_notifier_item_signal,
			services,
			NULL);
	services->status_notifier_watcher_name_id =
		session_services_request_name("org.kde.StatusNotifierWatcher");
}

static const char portal_settings_introspection_xml[] =
	"<node>"
	"  <interface name='org.freedesktop.impl.portal.Settings'>"
	"    <method name='Read'>"
	"      <arg type='s' name='namespace' direction='in'/>"
	"      <arg type='s' name='key' direction='in'/>"
	"      <arg type='v' name='value' direction='out'/>"
	"    </method>"
	"    <method name='ReadAll'>"
	"      <arg type='as' name='namespaces' direction='in'/>"
	"      <arg type='a{sa{sv}}' name='value' direction='out'/>"
	"    </method>"
	"    <signal name='SettingChanged'>"
	"      <arg type='s' name='namespace'/>"
	"      <arg type='s' name='key'/>"
	"      <arg type='v' name='value'/>"
	"    </signal>"
	"  </interface>"
	"  <interface name='org.freedesktop.portal.Settings'>"
	"    <method name='ReadAll'>"
	"      <arg type='as' name='namespaces' direction='in'/>"
	"      <arg type='a{sa{sv}}' name='value' direction='out'/>"
	"    </method>"
	"    <method name='Read'>"
	"      <arg type='s' name='namespace' direction='in'/>"
	"      <arg type='s' name='key' direction='in'/>"
	"      <arg type='v' name='value' direction='out'/>"
	"    </method>"
	"    <method name='ReadOne'>"
	"      <arg type='s' name='namespace' direction='in'/>"
	"      <arg type='s' name='key' direction='in'/>"
	"      <arg type='v' name='value' direction='out'/>"
	"    </method>"
	"    <property name='version' type='u' access='read'/>"
	"    <signal name='SettingChanged'>"
	"      <arg type='s' name='namespace'/>"
	"      <arg type='s' name='key'/>"
	"      <arg type='v' name='value'/>"
	"    </signal>"
	"  </interface>"
	"</node>";

static uint32_t portal_appearance_to_color_scheme(
		enum orange_appearance appearance) {
	return appearance == ORANGE_APPEARANCE_DARK ? 1 : 2;
}

static bool portal_namespace_requested(char **namespaces, const char *needle) {
	if (namespaces == NULL || namespaces[0] == NULL) {
		return true;
	}
	for (int i = 0; namespaces[i] != NULL; i++) {
		if (strcmp(namespaces[i], needle) == 0) {
			return true;
		}
	}
	return false;
}

static void portal_settings_return_color_scheme(
		GDBusMethodInvocation *invocation,
		enum orange_appearance appearance,
		bool deprecated_frontend_read) {
	uint32_t cs = portal_appearance_to_color_scheme(appearance);
	if (deprecated_frontend_read) {
		g_dbus_method_invocation_return_value(invocation,
			g_variant_new("(v)",
				g_variant_new_variant(g_variant_new_uint32(cs))));
	} else {
		g_dbus_method_invocation_return_value(invocation,
			g_variant_new("(v)", g_variant_new_uint32(cs)));
	}
}

static void portal_settings_return_read_all(
		GDBusMethodInvocation *invocation,
		enum orange_appearance appearance,
		char **namespaces) {
	GVariantBuilder outer;
	g_variant_builder_init(&outer, G_VARIANT_TYPE("a{sa{sv}}"));
	if (portal_namespace_requested(namespaces, "org.freedesktop.appearance")) {
		GVariantBuilder inner;
		g_variant_builder_init(&inner, G_VARIANT_TYPE("a{sv}"));
		uint32_t cs = portal_appearance_to_color_scheme(appearance);
		g_variant_builder_add(&inner, "{sv}", "color-scheme",
			g_variant_new_uint32(cs));
		g_variant_builder_add(&outer, "{sa{sv}}",
			"org.freedesktop.appearance", &inner);
	}
	g_dbus_method_invocation_return_value(invocation,
		g_variant_new("(a{sa{sv}})", &outer));
}

static void handle_portal_settings_method_call(GDBusConnection *connection,
		const gchar *sender, const gchar *object_path,
		const gchar *interface_name, const gchar *method_name,
		GVariant *parameters, GDBusMethodInvocation *invocation,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)sender;
	(void)object_path;

	if (strcmp(method_name, "Read") == 0 ||
			strcmp(method_name, "ReadOne") == 0) {
		const char *ns;
		const char *key;
		g_variant_get(parameters, "(&s&s)", &ns, &key);
		if (strcmp(ns, "org.freedesktop.appearance") == 0 &&
				strcmp(key, "color-scheme") == 0) {
			bool deprecated_frontend_read =
				strcmp(interface_name, "org.freedesktop.portal.Settings") == 0 &&
				strcmp(method_name, "Read") == 0;
			portal_settings_return_color_scheme(invocation,
				services->appearance,
				deprecated_frontend_read);
		} else {
			g_dbus_method_invocation_return_dbus_error(invocation,
				"org.freedesktop.impl.portal.Settings.Error.NotFound",
				"Setting not found");
		}
	} else if (strcmp(method_name, "ReadAll") == 0) {
		char **namespaces = NULL;
		g_variant_get(parameters, "(^as)", &namespaces);
		portal_settings_return_read_all(invocation,
			services->appearance,
			namespaces);
		g_strfreev(namespaces);
	} else {
		g_dbus_method_invocation_return_error(invocation,
			G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
			"Unknown method: %s", method_name);
	}
}

static GVariant *handle_portal_settings_get_property(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *property_name,
		GError **error,
		gpointer user_data) {
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;
	(void)error;
	(void)user_data;
	if (strcmp(property_name, "version") == 0) {
		return g_variant_new_uint32(2);
	}
	return NULL;
}

static void session_services_setup_portal(
		struct orange_session_services *services) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return;
	}

	GError *error = NULL;
	GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(
		portal_settings_introspection_xml, &error);
	if (introspection_data == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return;
	}

	static const GDBusInterfaceVTable interface_vtable = {
		.method_call = handle_portal_settings_method_call,
		.get_property = handle_portal_settings_get_property,
	};

	services->portal_backend_registration_id =
		g_dbus_connection_register_object(
			connection,
			"/org/freedesktop/portal/desktop",
			introspection_data->interfaces[0],
			&interface_vtable,
			services,
			NULL,
			&error);
	if (services->portal_backend_registration_id == 0) {
		if (error != NULL) {
			g_error_free(error);
		}
		g_dbus_node_info_unref(introspection_data);
		return;
	}

	services->portal_frontend_registration_id =
		g_dbus_connection_register_object(
			connection,
			"/org/freedesktop/portal/desktop",
			introspection_data->interfaces[1],
			&interface_vtable,
			services,
			NULL,
			&error);
	if (services->portal_frontend_registration_id == 0 && error != NULL) {
		g_error_free(error);
		error = NULL;
	}

	g_dbus_node_info_unref(introspection_data);

	services->portal_backend_name_id = session_services_request_name(
		"org.freedesktop.impl.portal.desktop.orange");
	services->portal_frontend_name_id = session_services_request_name(
		"org.freedesktop.portal.Desktop");
}

GVariant *orange_session_services_display_config_mode_properties(void) {
	GVariantBuilder props;
	g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&props, "{sv}", "is-current",
		g_variant_new_boolean(true));
	g_variant_builder_add(&props, "{sv}", "is-preferred",
		g_variant_new_boolean(true));
	g_variant_builder_add(&props, "{sv}", "is-interlaced",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "is-underscanning",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "is-extra",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "min-refresh-rate",
		g_variant_new_double(0.0));
	g_variant_builder_add(&props, "{sv}", "refresh-rate-mode",
		g_variant_new_string("fixed"));
	return g_variant_builder_end(&props);
}

GVariant *orange_session_services_display_config_monitor_properties(
		const char *connector,
		const char *vendor,
		const char *product) {
	GVariantBuilder props;
	g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
	char display_name[192];
	snprintf(display_name, sizeof(display_name), "%s %s",
		vendor, product);
	g_variant_builder_add(&props, "{sv}", "display-name",
		g_variant_new_string(display_name));
	g_variant_builder_add(&props, "{sv}", "connector-type",
		g_variant_new_string(connector));
	g_variant_builder_add(&props, "{sv}", "is-builtin",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "is-extra",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "supports-underscanning",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "is-underscanning",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "orientation-requested-supported",
		g_variant_new_boolean(true));
	g_variant_builder_add(&props, "{sv}", "orientation-requested-default",
		g_variant_new_int32(0));
	g_variant_builder_add(&props, "{sv}", "orientation-requested",
		g_variant_new_int32(0));
	g_variant_builder_add(&props, "{sv}", "privacy-screen-state",
		g_variant_new("(bb)", false, false));
	return g_variant_builder_end(&props);
}

GVariant *orange_session_services_display_config_state_properties(void) {
	GVariantBuilder props;
	g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&props, "{sv}", "layout-mode",
		g_variant_new_uint32(1));
	g_variant_builder_add(&props, "{sv}", "supports-changing-layout-mode",
		g_variant_new_boolean(true));
	g_variant_builder_add(&props, "{sv}", "global-scale-required",
		g_variant_new_boolean(false));
	g_variant_builder_add(&props, "{sv}", "legacy-ui-scaling-factor",
		g_variant_new_int32(1));
	g_variant_builder_add(&props, "{sv}", "supports-mirroring",
		g_variant_new_boolean(false));
	return g_variant_builder_end(&props);
}

static const char *display_config_introspection_xml =
	"<node>"
	"  <interface name='org.gnome.Mutter.DisplayConfig'>"
	"    <method name='GetCurrentState'>"
	"      <arg type='u' name='serial' direction='out'/>"
	"      <arg type='a((ssss)a(siiddada{sv})a{sv})' name='monitors' direction='out'/>"
	"      <arg type='a(iiduba(ssss)a{sv})' name='logical_monitors' direction='out'/>"
	"      <arg type='a{sv}' name='properties' direction='out'/>"
	"    </method>"
	"    <method name='ApplyMonitorsConfig'>"
	"      <arg type='u' name='serial' direction='in'/>"
	"      <arg type='u' name='method' direction='in'/>"
	"      <arg type='a(iiduba(ssa{sv}))' name='logical_monitors' direction='in'/>"
	"      <arg type='a{sv}' name='properties' direction='in'/>"
	"    </method>"
	"    <signal name='MonitorsChanged'/>"
	"    <property name='ApplyMonitorsConfigAllowed' type='b' access='read'/>"
	"    <property name='NightLightSupported' type='b' access='read'/>"
	"    <property name='PanelOrientationManaged' type='b' access='read'/>"
	"    <property name='PowerSaveMode' type='i' access='readwrite'/>"
	"  </interface>"
	"</node>";

void orange_session_services_emit_monitors_changed(
		struct orange_session_services *services) {
	GDBusConnection *connection = session_services_connection();
	if (services == NULL || connection == NULL) {
		return;
	}
	g_dbus_connection_emit_signal(
		connection,
		NULL,
		"/org/gnome/Mutter/DisplayConfig",
		"org.gnome.Mutter.DisplayConfig",
		"MonitorsChanged",
		NULL,
		NULL);
}

static void handle_display_config_method_call(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data) {
	struct orange_session_services *services = user_data;
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;

	if (services == NULL) {
		g_dbus_method_invocation_return_error(invocation,
			G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
			"Orange DisplayConfig is unavailable");
		return;
	}

	if (strcmp(method_name, "GetCurrentState") == 0) {
		if (services->display_config_state == NULL) {
			g_dbus_method_invocation_return_error(invocation,
				G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"Orange DisplayConfig has no output provider");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
			services->display_config_state(services->display_config_data,
				services->display_config_serial != 0 ?
					services->display_config_serial : 1));
	} else if (strcmp(method_name, "ApplyMonitorsConfig") == 0) {
		guint32 serial = 0;
		guint32 method = 0;
		GVariant *logical_monitors = NULL;
		GVariant *properties = NULL;
		GError *apply_error = NULL;
		g_variant_get(parameters, "(uu@a(iiduba(ssa{sv}))@a{sv})",
			&serial, &method, &logical_monitors, &properties);
		if (serial != 0 && services->display_config_serial != 0 &&
				serial != services->display_config_serial) {
			g_dbus_method_invocation_return_error(invocation,
				G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
				"Display configuration serial is stale");
		} else if (method > 2) {
			g_dbus_method_invocation_return_error(invocation,
				G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
				"Invalid DisplayConfig apply method");
		} else if (services->display_config_apply != NULL &&
				!services->display_config_apply(
					services->display_config_data,
					method,
					logical_monitors,
					properties,
					&apply_error)) {
			if (apply_error != NULL) {
				g_dbus_method_invocation_return_gerror(
					invocation, apply_error);
				g_error_free(apply_error);
			} else {
				g_dbus_method_invocation_return_error(invocation,
					G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
					"Display configuration apply failed");
			}
		} else {
			g_dbus_method_invocation_return_value(invocation, NULL);
			if (method != 0) {
				services->display_config_serial++;
				if (services->display_config_serial == 0) {
					services->display_config_serial = 1;
				}
				orange_session_services_emit_monitors_changed(services);
			}
		}
		if (logical_monitors != NULL) {
			g_variant_unref(logical_monitors);
		}
		if (properties != NULL) {
			g_variant_unref(properties);
		}
		(void)method;
	} else {
		g_dbus_method_invocation_return_error(invocation,
			G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
			"Unknown method: %s", method_name);
	}
}

static GVariant *handle_display_config_get_property(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *property_name,
		GError **error,
		gpointer user_data) {
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;
	(void)error;
	(void)user_data;

	if (strcmp(property_name, "ApplyMonitorsConfigAllowed") == 0) {
		return g_variant_new_boolean(true);
	}
	if (strcmp(property_name, "NightLightSupported") == 0 ||
			strcmp(property_name, "PanelOrientationManaged") == 0) {
		return g_variant_new_boolean(false);
	}
	if (strcmp(property_name, "PowerSaveMode") == 0) {
		return g_variant_new_int32(0);
	}
	return NULL;
}

static gboolean handle_display_config_set_property(
		GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *property_name,
		GVariant *value,
		GError **error,
		gpointer user_data) {
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;
	(void)value;
	(void)error;
	(void)user_data;
	return strcmp(property_name, "PowerSaveMode") == 0;
}

static void session_services_setup_display_config(
		struct orange_session_services *services) {
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return;
	}

	GError *error = NULL;
	GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(
		display_config_introspection_xml, &error);
	if (introspection_data == NULL) {
		if (error != NULL) {
			g_error_free(error);
		}
		return;
	}

	static const GDBusInterfaceVTable interface_vtable = {
		.method_call = handle_display_config_method_call,
		.get_property = handle_display_config_get_property,
		.set_property = handle_display_config_set_property,
	};
	services->display_config_registration_id =
		g_dbus_connection_register_object(
			connection,
			"/org/gnome/Mutter/DisplayConfig",
			introspection_data->interfaces[0],
			&interface_vtable,
			services,
			NULL,
			&error);
	if (services->display_config_registration_id == 0) {
		if (error != NULL) {
			g_error_free(error);
		}
		g_dbus_node_info_unref(introspection_data);
		return;
	}

	g_dbus_node_info_unref(introspection_data);
	services->display_config_serial = 1;
	services->display_config_name_id = session_services_request_name(
		"org.gnome.Mutter.DisplayConfig");
}

void orange_session_services_init(struct orange_session_services *services) {
	if (services != NULL) {
		memset(services, 0, sizeof(*services));
		services->appearance = ORANGE_APPEARANCE_LIGHT;
	}
}

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
		void *display_config_data) {
	if (services == NULL) {
		return;
	}
	services->appearance = appearance;
	services->display_config_state = display_config_state;
	services->display_config_apply = display_config_apply;
	services->display_config_data = display_config_data;
	session_services_setup_status_notifier(services);
	session_services_setup_display_config(services);
	session_services_setup_portal(services);
}

void orange_session_services_set_status_notifier_changed_handler(
		struct orange_session_services *services,
		void (*changed)(void *data),
		void *data) {
	if (services == NULL) {
		return;
	}
	services->status_notifier_changed = changed;
	services->status_notifier_data = data;
}

void orange_session_services_finish(struct orange_session_services *services) {
	if (services == NULL) {
		return;
	}
	session_services_release_name(&services->status_notifier_watcher_name_id,
		"org.kde.StatusNotifierWatcher");
	session_services_release_name(&services->portal_backend_name_id,
		"org.freedesktop.impl.portal.desktop.orange");
	session_services_release_name(&services->portal_frontend_name_id,
		"org.freedesktop.portal.Desktop");
	session_services_release_name(&services->display_config_name_id,
		"org.gnome.Mutter.DisplayConfig");

	GDBusConnection *connection = session_services_connection();
	if (connection != NULL) {
		if (services->status_notifier_name_owner_subscription_id != 0) {
			g_dbus_connection_signal_unsubscribe(connection,
				services->status_notifier_name_owner_subscription_id);
			services->status_notifier_name_owner_subscription_id = 0;
		}
		if (services->status_notifier_item_signal_subscription_id != 0) {
			g_dbus_connection_signal_unsubscribe(connection,
				services->status_notifier_item_signal_subscription_id);
			services->status_notifier_item_signal_subscription_id = 0;
		}
		if (services->status_notifier_watcher_registration_id != 0) {
			g_dbus_connection_unregister_object(connection,
				services->status_notifier_watcher_registration_id);
			services->status_notifier_watcher_registration_id = 0;
		}
		if (services->portal_backend_registration_id != 0) {
			g_dbus_connection_unregister_object(connection,
				services->portal_backend_registration_id);
			services->portal_backend_registration_id = 0;
		}
		if (services->portal_frontend_registration_id != 0) {
			g_dbus_connection_unregister_object(connection,
				services->portal_frontend_registration_id);
			services->portal_frontend_registration_id = 0;
		}
		if (services->display_config_registration_id != 0) {
			g_dbus_connection_unregister_object(connection,
				services->display_config_registration_id);
			services->display_config_registration_id = 0;
		}
	}
}

void orange_session_services_set_appearance(
		struct orange_session_services *services,
		enum orange_appearance appearance) {
	if (services != NULL) {
		services->appearance = appearance;
	}
}

void orange_session_services_emit_setting_changed(
		struct orange_session_services *services) {
	GDBusConnection *connection = session_services_connection();
	if (services == NULL || connection == NULL) {
		return;
	}
	uint32_t cs = portal_appearance_to_color_scheme(services->appearance);
	g_dbus_connection_emit_signal(
		connection,
		NULL,
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.impl.portal.Settings",
		"SettingChanged",
		g_variant_new("(ssv)", "org.freedesktop.appearance",
			"color-scheme", g_variant_new_uint32(cs)),
		NULL);
	g_dbus_connection_emit_signal(
		connection,
		NULL,
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Settings",
		"SettingChanged",
		g_variant_new("(ssv)", "org.freedesktop.appearance",
			"color-scheme", g_variant_new_uint32(cs)),
		NULL);
}

int orange_session_services_copy_status_notifier_items(
		struct orange_session_services *services,
		struct orange_status_notifier_item *items,
		int capacity) {
	if (services == NULL || items == NULL || capacity <= 0) {
		return 0;
	}
	int count = 0;
	for (int i = 0; i < services->status_notifier_item_count &&
			count < capacity; i++) {
		if (!status_notifier_item_visible(
				&services->status_notifier_items[i])) {
			continue;
		}
		items[count++] = services->status_notifier_items[i];
	}
	return count;
}

bool orange_session_services_refresh_status_notifier_items(
		struct orange_session_services *services) {
	if (services == NULL) {
		return false;
	}
	bool changed = false;
	for (int i = services->status_notifier_item_count - 1; i >= 0; i--) {
		struct orange_status_notifier_item before =
			services->status_notifier_items[i];
		if (!status_notifier_fetch_item_properties(
				&services->status_notifier_items[i])) {
			if (services->status_notifier_items[i].icon_name[0] == '\0') {
				snprintf(services->status_notifier_items[i].icon_name,
					sizeof(services->status_notifier_items[i].icon_name),
					"%s", "application-x-executable");
			}
		}
		if (memcmp(&before, &services->status_notifier_items[i],
				sizeof(before)) != 0) {
			changed = true;
		}
	}
	if (changed) {
		status_notifier_notify_changed(services);
	}
	return changed;
}

static bool status_notifier_call_item_method(
		struct orange_session_services *services,
		const char *id,
		const char *method,
		int x,
		int y) {
	int index = status_notifier_find_index(services, id);
	if (index < 0 || method == NULL) {
		return false;
	}
	struct orange_status_notifier_item *item =
		&services->status_notifier_items[index];
	GDBusConnection *connection = session_services_connection();
	if (connection == NULL) {
		return false;
	}
	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(
		connection,
		item->service,
		item->path,
		"org.kde.StatusNotifierItem",
		method,
		g_variant_new("(ii)", x, y),
		NULL,
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		160,
		NULL,
		&error);
	if (reply != NULL) {
		g_variant_unref(reply);
		return true;
	}
	if (error != NULL) {
		g_error_free(error);
	}
	return false;
}

bool orange_session_services_activate_status_notifier_item(
		struct orange_session_services *services,
		const char *id,
		int x,
		int y) {
	int index = status_notifier_find_index(services, id);
	if (index >= 0 && (services->status_notifier_items[index].item_is_menu ||
			services->status_notifier_items[index].has_menu)) {
		if (status_notifier_call_item_method(services, id,
				"ContextMenu", x, y)) {
			return true;
		}
	}
	if (status_notifier_call_item_method(services, id, "Activate", x, y)) {
		return true;
	}
	if (index >= 0) {
		return status_notifier_call_item_method(services, id,
			"ContextMenu", x, y);
	}
	return false;
}

bool orange_session_services_context_menu_status_notifier_item(
		struct orange_session_services *services,
		const char *id,
		int x,
		int y) {
	if (status_notifier_call_item_method(services, id, "ContextMenu", x, y)) {
		return true;
	}
	return status_notifier_call_item_method(services, id, "Activate", x, y);
}
