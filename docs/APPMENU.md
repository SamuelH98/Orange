# Orange App Menu Integration

Orange exposes `com.canonical.AppMenu.Registrar` on the session bus and imports
registered DBusMenu trees into the menu bar. That gives real native app menus
for apps that export DBusMenu or GMenu.

On Wayland, the appmenu GTK module may load and find Orange's registrar without
calling `RegisterWindow`, because the old AppMenu registrar path is centered on
window IDs. Orange therefore does not rely on the module as the primary path.
It probes GMenu and GAction exporters owned by the focused client process. This
is the primary path for GTK/GApplication apps on a wlroots session.

## Native GTK And GTK4 Apps

The distro package should launch Orange through the session wrapper so the
session bus, Wayland display, desktop identity, theme, and cursor settings are
exported. For app menus, Orange tries these providers in order:

- AppMenu/DBusMenu registrar entries.
- GMenu models owned by the focused client's PID.
- GAction groups owned by the focused client's PID, exposed as an Actions menu.
- AT-SPI exposed actions.
- App-specific fallback profiles for common app families such as browsers.

The GTK appmenu module is optional compatibility for older GTK3 menu-shell apps.
Orange appends `appmenu-gtk-module` for clients it launches directly when it
finds an installed GTK appmenu module, but a working module is not required for
GMenu/GAction based apps.

For GTK apps that export `org.gtk.Menus`/`org.gtk.Actions`, Orange matches the
focused Wayland client PID to well-known names on the session bus and imports
the app's GMenu or useful GActions directly.

Optional appmenu GTK module package names vary:

- Debian/Ubuntu: `appmenu-gtk-module-common`, `appmenu-gtk2-module`,
  `appmenu-gtk3-module`
- Void: `appmenu-gtk3-module`

GTK4/libadwaita apps and apps that use headerbars or custom chrome may not
create a traditional GTK menu shell. For those apps the GTK appmenu module can
load successfully and still export no menu.

## Browsers

Firefox, Chromium, Chrome, and Brave do not expose their visible browser chrome
menus through the GTK appmenu module in the same way a normal GTK3 app does.
Orange therefore shows a browser fallback menu profile when no native menu
exporter is found. Real imported menus still win when an exporter is present.

The fallback exists to make the menu bar usable, not to claim that every browser
menu item is natively exported. Complete browser integration would require a
browser-specific extension/native host or another browser-specific menu export
bridge.

## Flatpak And Snap

Flatpak and Snap runtimes do not normally see host GTK modules or host GSettings
schemas. The optional sandbox bridge script copies the appmenu GTK3 module, its
small helper libraries, and the `org.appmenu.gtk-module` schema into a
user-readable bundle:

```sh
scripts/orange-appmenu-sandbox-setup.sh
```

For Flatpak, the script installs user overrides for:

- `GTK_MODULES=appmenu-gtk-module`
- `GTK_PATH` pointing at the copied GTK3 module tree
- `LD_LIBRARY_PATH` pointing at the copied helper libraries
- `GSETTINGS_SCHEMA_DIR` pointing at the compiled schema bundle
- DBus talk access to `com.canonical.AppMenu.Registrar`

For Firefox Snap, the script creates a user wrapper and desktop override because
the Snap launcher rewrites library paths after startup. The wrapper enters the
Snap runtime first, then prepends the appmenu bundle paths before launching
Firefox.

The sandbox bridge is best-effort compatibility for apps that still need the
GTK3 appmenu module. It is not required for apps that expose GMenu/GAction on
the session bus, and it cannot make custom-toolkit or headerbar-only apps export
a traditional menu.
