#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

#ifndef TAHOE_PROJECT_VERSION
#define TAHOE_PROJECT_VERSION "0.1.0"
#endif

struct about_app {
	GtkApplication *app;
	GtkWidget *version_button;
	bool showing_build;
};

static void apply_css(void) {
	GtkCssProvider *provider = gtk_css_provider_new();
	const char *css =
		"window { background: transparent; }"
		".about-root {"
		"  background: linear-gradient(145deg, rgba(248,252,255,0.98), rgba(216,232,246,0.95));"
		"  color: #17202a;"
		"  padding: 30px;"
		"}"
		".chrome-card {"
		"  background: rgba(255,255,255,0.72);"
		"  border: 1px solid rgba(255,255,255,0.92);"
		"  border-radius: 18px;"
		"  box-shadow: 0 24px 70px rgba(30,50,80,0.22);"
		"  padding: 28px;"
		"}"
		".badge {"
		"  min-width: 126px;"
		"  min-height: 126px;"
		"  border-radius: 32px;"
		"  background: linear-gradient(145deg, #fafdff, #a7d5ff 48%, #5f8ff0);"
		"  color: white;"
		"  font-size: 72px;"
		"  font-weight: 800;"
		"  text-shadow: 0 2px 12px rgba(30,70,140,0.42);"
		"}"
		".model { font-size: 30px; font-weight: 800; letter-spacing: 0; }"
		".subtitle { color: rgba(22,31,42,0.68); font-size: 14px; }"
		".macos { font-size: 24px; font-weight: 760; }"
		".version-button {"
		"  background: rgba(255,255,255,0.46);"
		"  border: 1px solid rgba(100,120,150,0.18);"
		"  border-radius: 999px;"
		"  color: rgba(22,31,42,0.72);"
		"  padding: 7px 14px;"
		"}"
		".spec-card {"
		"  background: rgba(255,255,255,0.48);"
		"  border: 1px solid rgba(110,130,150,0.14);"
		"  border-radius: 12px;"
		"  padding: 14px;"
		"}"
		".spec-key { color: rgba(22,31,42,0.55); font-size: 13px; }"
		".spec-value { font-size: 14px; font-weight: 650; }"
		".action-button { border-radius: 999px; padding: 8px 16px; }";
	gtk_css_provider_load_from_string(provider, css);
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);
}

static GtkWidget *label_new(const char *text, const char *css_class) {
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_add_css_class(label, css_class);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	return label;
}

static char *memory_text(void) {
	struct sysinfo info;
	if (sysinfo(&info) != 0 || info.mem_unit == 0) {
		return g_strdup("Unknown");
	}
	double gib = (double)info.totalram * (double)info.mem_unit /
		(1024.0 * 1024.0 * 1024.0);
	return g_strdup_printf("%.0f GB unified memory", gib);
}

static char *kernel_text(void) {
	struct utsname uts;
	if (uname(&uts) != 0) {
		return g_strdup("Linux");
	}
	return g_strdup_printf("%s %s", uts.sysname, uts.release);
}

static char *machine_text(void) {
	struct utsname uts;
	if (uname(&uts) != 0) {
		return g_strdup("Wayland workstation");
	}
	return g_strdup_printf("%s Wayland workstation", uts.machine);
}

static void append_spec(GtkWidget *grid, int row, const char *key, const char *value) {
	GtkWidget *key_label = label_new(key, "spec-key");
	GtkWidget *value_label = label_new(value, "spec-value");
	gtk_grid_attach(GTK_GRID(grid), key_label, 0, row, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), value_label, 1, row, 1, 1);
}

static void on_version_clicked(GtkButton *button, gpointer data) {
	(void)button;
	struct about_app *about = data;
	about->showing_build = !about->showing_build;
	gtk_button_set_label(GTK_BUTTON(about->version_button),
		about->showing_build ?
			"Build Tahoe " TAHOE_PROJECT_VERSION :
			"Version 26.5.1");
}

static void on_more_info(GtkButton *button, gpointer data) {
	(void)button;
	(void)data;
	g_spawn_command_line_async(
		"sh -c 'command -v hardinfo2 >/dev/null && hardinfo2 || "
		"command -v hardinfo >/dev/null && hardinfo || true'",
		NULL);
}

static void on_software_update(GtkButton *button, gpointer data) {
	(void)button;
	(void)data;
	g_spawn_command_line_async(
		"sh -c 'GSK_RENDERER=cairo build/tahoe-settings tahoe.conf'",
		NULL);
}

static void activate(GtkApplication *app, gpointer data) {
	struct about_app *about = data;
	apply_css();

	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "About This Mac");
	gtk_window_set_default_size(GTK_WINDOW(window), 620, 500);
	gtk_window_set_resizable(GTK_WINDOW(window), false);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(root, "about-root");
	gtk_window_set_child(GTK_WINDOW(window), root);

	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
	gtk_widget_add_css_class(card, "chrome-card");
	gtk_widget_set_hexpand(card, true);
	gtk_widget_set_vexpand(card, true);
	gtk_box_append(GTK_BOX(root), card);

	GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 26);
	gtk_box_append(GTK_BOX(card), hero);

	GtkWidget *badge = gtk_label_new("T");
	gtk_widget_add_css_class(badge, "badge");
	gtk_widget_set_halign(badge, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(badge, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hero), badge);

	GtkWidget *summary = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_hexpand(summary, true);
	gtk_box_append(GTK_BOX(hero), summary);

	gtk_box_append(GTK_BOX(summary), label_new("Tahoe", "model"));
	gtk_box_append(GTK_BOX(summary), label_new("Liquid Glass desktop prototype", "subtitle"));
	gtk_box_append(GTK_BOX(summary), label_new("macOS Tahoe", "macos"));

	about->version_button = gtk_button_new_with_label("Version 26.5.1");
	gtk_widget_add_css_class(about->version_button, "version-button");
	gtk_widget_set_halign(about->version_button, GTK_ALIGN_START);
	g_signal_connect(about->version_button, "clicked",
		G_CALLBACK(on_version_clicked), about);
	gtk_box_append(GTK_BOX(summary), about->version_button);

	GtkWidget *spec_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(spec_card, "spec-card");
	gtk_box_append(GTK_BOX(card), spec_card);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 28);
	gtk_box_append(GTK_BOX(spec_card), grid);

	char *machine = machine_text();
	char *memory = memory_text();
	char *kernel = kernel_text();
	append_spec(grid, 0, "Model", machine);
	append_spec(grid, 1, "Chip", "wlroots compositor stack");
	append_spec(grid, 2, "Memory", memory);
	append_spec(grid, 3, "Graphics", "Cairo shell with Pixman/GLES2/Vulkan support");
	append_spec(grid, 4, "Serial number", "TAHOE-0001");
	append_spec(grid, 5, "Kernel", kernel);
	g_free(machine);
	g_free(memory);
	g_free(kernel);

	GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_halign(actions, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(card), actions);

	GtkWidget *more_info = gtk_button_new_with_label("More Info...");
	gtk_widget_add_css_class(more_info, "action-button");
	g_signal_connect(more_info, "clicked", G_CALLBACK(on_more_info), NULL);
	gtk_box_append(GTK_BOX(actions), more_info);

	GtkWidget *software_update = gtk_button_new_with_label("Software Update...");
	gtk_widget_add_css_class(software_update, "action-button");
	g_signal_connect(software_update, "clicked",
		G_CALLBACK(on_software_update), NULL);
	gtk_box_append(GTK_BOX(actions), software_update);

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	struct about_app about = {0};
	about.app = gtk_application_new("dev.tahoe.About",
		G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(about.app, "activate", G_CALLBACK(activate), &about);
	int status = g_application_run(G_APPLICATION(about.app), argc, argv);
	g_object_unref(about.app);
	return status;
}
