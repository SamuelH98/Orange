#include "orange/session_services.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_display_config_mode_property_types(void) {
	GVariant *props = g_variant_ref_sink(
		orange_session_services_display_config_mode_properties());

	const char *refresh_rate_mode = NULL;
	assert(g_variant_lookup(props, "refresh-rate-mode", "&s",
		&refresh_rate_mode));
	assert(strcmp(refresh_rate_mode, "fixed") == 0);

	gboolean is_extra = true;
	assert(g_variant_lookup(props, "is-extra", "b", &is_extra));
	assert(!is_extra);

	double min_refresh_rate = -1.0;
	assert(g_variant_lookup(props, "min-refresh-rate", "d",
		&min_refresh_rate));
	assert(min_refresh_rate == 0.0);
	(void)min_refresh_rate;

	g_variant_unref(props);
}

static void test_display_config_monitor_property_types(void) {
	GVariant *props = g_variant_ref_sink(
		orange_session_services_display_config_monitor_properties(
			"HEADLESS-1", "Orange", "Display"));

	gboolean enabled = true;
	gboolean locked = true;
	assert(g_variant_lookup(props, "privacy-screen-state", "(bb)",
		&enabled, &locked));
	assert(!enabled);
	assert(!locked);

	gboolean is_extra = true;
	assert(g_variant_lookup(props, "is-extra", "b", &is_extra));
	assert(!is_extra);

	gboolean supports_orientation = false;
	assert(g_variant_lookup(props, "orientation-requested-supported", "b",
		&supports_orientation));
	assert(supports_orientation);

	gint32 orientation = -1;
	assert(g_variant_lookup(props, "orientation-requested-default", "i",
		&orientation));
	assert(orientation == 0);
	assert(g_variant_lookup(props, "orientation-requested", "i",
		&orientation));
	assert(orientation == 0);
	(void)orientation;

	g_variant_unref(props);
}

static void test_display_config_state_properties_enable_fractional(void) {
	GVariant *props = g_variant_ref_sink(
		orange_session_services_display_config_state_properties());

	guint32 layout_mode = 0;
	assert(g_variant_lookup(props, "layout-mode", "u", &layout_mode));
	assert(layout_mode == 1);

	gboolean supports_layout_mode = false;
	assert(g_variant_lookup(props, "supports-changing-layout-mode", "b",
		&supports_layout_mode));
	assert(supports_layout_mode);

	gboolean global_scale_required = true;
	assert(g_variant_lookup(props, "global-scale-required", "b",
		&global_scale_required));
	assert(!global_scale_required);

	gint32 legacy_ui_scale = 0;
	assert(g_variant_lookup(props, "legacy-ui-scaling-factor", "i",
		&legacy_ui_scale));
	assert(legacy_ui_scale == 1);
	(void)legacy_ui_scale;

	g_variant_unref(props);
}

static void test_status_notifier_copy_filters_visible_items(void) {
	struct orange_session_services services;
	orange_session_services_init(&services);
	services.status_notifier_item_count = 4;

	snprintf(services.status_notifier_items[0].id,
		sizeof(services.status_notifier_items[0].id),
		"%s", ":1.10/StatusNotifierItem");
	snprintf(services.status_notifier_items[0].status,
		sizeof(services.status_notifier_items[0].status), "%s", "Active");
	snprintf(services.status_notifier_items[0].icon_name,
		sizeof(services.status_notifier_items[0].icon_name),
		"%s", "network-vpn");

	snprintf(services.status_notifier_items[1].id,
		sizeof(services.status_notifier_items[1].id),
		"%s", ":1.11/StatusNotifierItem");
	snprintf(services.status_notifier_items[1].status,
		sizeof(services.status_notifier_items[1].status), "%s", "Passive");
	snprintf(services.status_notifier_items[1].icon_name,
		sizeof(services.status_notifier_items[1].icon_name),
		"%s", "hidden");

	snprintf(services.status_notifier_items[2].id,
		sizeof(services.status_notifier_items[2].id),
		"%s", ":1.12/StatusNotifierItem");
	snprintf(services.status_notifier_items[2].status,
		sizeof(services.status_notifier_items[2].status),
		"%s", "NeedsAttention");
	snprintf(services.status_notifier_items[2].attention_icon_name,
		sizeof(services.status_notifier_items[2].attention_icon_name),
		"%s", "dialog-warning");

	snprintf(services.status_notifier_items[3].id,
		sizeof(services.status_notifier_items[3].id),
		"%s", ":1.13/StatusNotifierItem");
	snprintf(services.status_notifier_items[3].status,
		sizeof(services.status_notifier_items[3].status), "%s", "Active");

	struct orange_status_notifier_item items[4];
	int count = orange_session_services_copy_status_notifier_items(
		&services, items, 4);
	assert(count == 2);
	assert(strcmp(items[0].id, ":1.10/StatusNotifierItem") == 0);
	assert(strcmp(items[1].id, ":1.12/StatusNotifierItem") == 0);

	memset(items, 0, sizeof(items));
	count = orange_session_services_copy_status_notifier_items(
		&services, items, 1);
	assert(count == 1);
	assert(strcmp(items[0].icon_name, "network-vpn") == 0);
	(void)count;
}

int main(void) {
	test_display_config_mode_property_types();
	test_display_config_monitor_property_types();
	test_display_config_state_properties_enable_fractional();
	test_status_notifier_copy_filters_visible_items();
	return 0;
}
