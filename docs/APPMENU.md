# Orange App Menu Integration

Orange exposes `com.canonical.AppMenu.Registrar` on the session bus and imports
registered DBusMenu trees into the menu bar when an application exports one on
its own. It also probes GMenu, GAction, and AT-SPI providers owned by the
focused Wayland client process.

Orange does not install, launch, or inject the legacy GTK AppMenu exporter
module. The packaged session clears inherited GTK module injection before
starting Orange. The current menu strategy is Wayland first and process/PID
matched:

- AppMenu/DBusMenu registrar entries exported by the focused client.
- GMenu models owned by the focused client's PID.
- GAction groups owned by the focused client's PID, exposed as an Actions menu.
- AT-SPI exposed actions.
- App-specific fallback profiles for common app families such as browsers.

## Native GTK And GTK4 Apps

The distro package should launch Orange through the session wrapper so the
session bus, Wayland display, desktop identity, theme, cursor settings, and
Wayland toolkit variables are exported.

For GTK apps that export `org.gtk.Menus` or `org.gtk.Actions`, Orange matches
the focused Wayland client PID to names on the session bus and imports the
app's GMenu or useful GActions directly. GTK4/libadwaita apps and apps that use
headerbars or custom chrome may expose little or no traditional menu model; for
those apps Orange either shows detected actions or falls back to an app-family
profile.

## Browsers

Firefox, Chromium, Chrome, and Brave do not expose their visible browser chrome
menus like normal desktop app menu models. Orange therefore shows a browser
fallback menu profile when no native menu exporter is found. Real imported
menus still win when an exporter is present.

The fallback exists to make the menu bar usable, not to claim that every browser
menu item is natively exported. Complete browser integration would require a
browser-specific extension/native host or another browser-specific menu export
bridge.

## Sandboxed Apps

Flatpak and Snap runtimes do not normally see host session services unless
their sandbox permissions allow it. Orange does not ship a host module bridge
for sandboxed apps. Sandboxed apps can still participate when they expose
GMenu, GAction, DBusMenu, or AT-SPI actions over a bus they are permitted to
use.
