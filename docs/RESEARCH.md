# Research Summary

## Local Environment

- `wlroots`: 0.20.1 through the versioned `wlroots-0.20` pkg-config
  dependency in the repo-local `.local/wlroots-0.20` prefix
- `wayland-server`: 1.22.0
- `vulkan`: 1.3.275
- `cairo`: 1.18.0
- `pixman`: 0.42.2
- `meson`: 1.3.2
- `ninja`: 1.11.1
- `gcc`: 13.3.0
- `gtk4`: 4.14.5
- `gtk+-3.0`: not installed in this environment
- `convert` / ImageMagick: installed and used to generate repo-safe Orange
  wallpaper PNG assets

## wlroots Notes

Orange targets wlroots 0.20 directly. Builds should use
`dependency('wlroots-0.20')`; the older unversioned `wlroots` pkg-config package
is intentionally unsupported.

The compositor uses the wlroots 0.20 backend, renderer, allocator, output,
scene, xdg-shell, and input APIs without a compatibility wrapper.

The Vulkan renderer can be selected through wlroots with
`WLR_RENDERER=vulkan`. The project checks `wlr_renderer_is_vk()` at runtime
when the Vulkan renderer headers are available. On this machine, the headless
backend cannot create a Vulkan renderer because wlroots has no DRM file
descriptor in that mode; headless validation uses Pixman.

wlroots version changes should not be treated as the fix for Orange's
context-menu and menu-bar dropdown latency by themselves. The measured local
issue was Orange's own full-output overlay path: tiny menus caused full-screen
backdrop copies, full-screen unblend scans, and full-screen scene-buffer
damage. The fix is therefore to compute conservative overlay bounds,
copy/unblend only those bounds, and submit damage for the previous/current
overlay union.

If context menu/dropdown lag disappears after the same menu has been used a few
times, the remaining cause is cold icon/theme cache work rather than scene
damage. Menu drawing resolves themed icons lazily, and missing icon names can
force expensive fallback directory walks. Orange now warms the small shell
chrome icon set after asset loading, caches preload misses, and resolves icons
inside the discovered theme path first. It then checks the same theme name in
secondary XDG icon bases so split theme installs still work, without returning
to the old every-base/every-directory cold-cache scan.

## Wayland And Freedesktop Spec Notes

- Desktop Entry Specification 1.5 is the current referenced launch contract.
  Orange removes no-file field codes such as `%f`, `%F`, `%u`, and `%U` for
  Dock/desktop launches and treats `Icon=` as either an absolute path or a
  themed icon name.
- GNOME-style launcher filtering is a GIO/GDesktopAppInfo concern, not a
  Mutter compositor concern. `g_app_info_get_all()` may include desktop files
  hidden from menus by `NoDisplay`, `OnlyShowIn`, or `NotShowIn`, while
  `g_app_info_should_show()` is the menu/app-grid gate. `Hidden=true` means
  deleted at the user's level, `NoDisplay=true` means valid for MIME/default
  app handling but not menus, `OnlyShowIn`/`NotShowIn` are evaluated against
  `XDG_CURRENT_DESKTOP`, and `TryExec` can be used to ignore entries whose
  executable is not installed.
- Mutter exposes sandbox identity for Flatpak/Snap windows through
  `meta_window_get_sandboxed_app_id()`. GNOME Shell consumes that in
  `ShellWindowTracker`, looking up `<sandbox-id>.desktop`, and otherwise falls
  back through `_GTK_APPLICATION_ID`, `StartupWMClass`, and WM_CLASS
  heuristics. GNOME Shell's `ShellAppSystem` also gives priority to visible
  desktop files when multiple entries publish the same `StartupWMClass`.
  Orange mirrors the practical behavior by ranking available desktop entries
  and skipping stale package-specific entries before falling back to an
  equivalent non-sandboxed entry.
- Icon Theme Specification 0.13 defines lookup through `$HOME/.icons`,
  `$XDG_DATA_DIRS/icons`, theme inheritance, mandatory `hicolor` fallback, and
  `/usr/share/pixmaps`. Orange follows that practical lookup model with lazy
  positive and negative caches.
- Icon Naming Specification 0.8.90 defines the standard semantic icon names
  and naming constraints used by Orange's shell aliases.
- Stable xdg-shell version 7 is the Wayland protocol surface for desktop-style
  toplevels and popups. Orange delegates protocol mechanics to wlroots and
  implements toplevel map/unmap/focus/move/resize/maximize/fullscreen/close
  behavior in compositor state.
- wlroots documentation keeps `wlr_renderer_autocreate()` as the renderer
  selection path; Vulkan is requested through the environment/backend renderer
  path and checked with `wlr_renderer_is_vk()`. A raw Vulkan compositor port is
  therefore a separate renderer-backend project, not a prerequisite for this
  bug-fix pass.

## Visual Reference Notes

The visual reference uses Apple's Liquid Glass-era design language:
translucent UI materials that adapt to surrounding content and include
real-time highlights. In Orange, that is approximated with:

- blurred/translucent glass panels,
- rounded widgets and dock,
- bright specular edge highlights,
- softly tinted icon backgrounds,
- wallpaper-colored transparency.

The same material treatment is used for context menus and the system menu
popover. Apple's current HIG positions Liquid Glass as the system material for
foreground controls and navigation surfaces, and its menu/action components are
part of that foreground UI layer. The shell therefore renders dock, system menu,
and context menu panels through glass material helpers instead of opaque menu
panels.

Apple's 2025 Liquid Glass announcement describes controls and navigation as a
distinct translucent layer above app content, with sidebars refracting content
behind and around them and the macOS Tahoe menu bar becoming transparent. For
Orange's transient menus, this means separation should come from the glass
material itself without an added bright outside stroke or extra shadow; a white
rim reads as a halo around the menu instead of a macOS-like floating surface.

Apple's Mac widget guide describes desktop widget customization from the
desktop and widget shortcut menus: the desktop shortcut menu exposes
`Edit Widgets`, while a widget shortcut menu exposes widget editing, size
selection, and removal. The shell models that surface with widget hit targets,
widget context menus, persistent widget visibility, and small/medium/large
size settings.

Pixel validation renders at the native `2880x1800` shell size and can ignore
wallpaper/background differences for manual inspection. Automated shell render
coverage keeps light/dark smoke tests plus a foreground-only desktop context
menu check for scaled geometry and translucent glass, without comparing against
a checked-in visual reference image.

Apple documents About This Mac as an Apple-menu window that always includes the
system name and version, with build details revealed from the version text. The
Mac user guide says the Mac model appears at the top, hardware details such as
chip and serial number sit immediately below, and version information appears
below that. The bundled Orange About app follows that structure while showing
the local Orange build string instead of claiming to be an Apple OS release.

Apple's Desktop & Dock settings expose `Minimize windows using` as a Dock
preference with the named Genie and Scale effects, and also expose Dock screen
position and the option to minimize windows into the application icon. Apple's
Dock user guide presents minimized windows as Dock-resident items that can be
restored from the Dock. For Orange, the practical model is therefore: minimize
and restore should animate a visual representation of the actual window toward
the Dock item that represents the app, falling back to the temporary running
app slot or the Dock area when no pinned item is available.

Local wlroots 0.20 research showed that Orange can implement this without a
new protocol. The existing scene-output readback path can build a one-frame
output state, hide the overlay buffer during capture, read pixels through
`wlr_texture_read_pixels`, and copy the window rectangle into an
`orange_buffer`. The compositor can then keep that screenshot buffer while the
view is minimized and draw it on the existing transparent overlay buffer for
both minimize and restore animations. This keeps the real toplevel scene node
hidden during minimize and restore until the animation completes.

The upstream `vinceliuice/MacTahoe-gtk-theme` project describes itself as a
macOS Tahoe-like GTK theme for Linux desktops, based on WhiteSur. Its README
documents source installation, `./install.sh`, destination selection with
`-d/--dest`, and naming with `-n/--name`. The companion
`vinceliuice/MacTahoe-icon-theme` README documents the same install-script
shape for icons, defaulting icon installation to `$HOME/.local/share/icons`.
Orange keeps MacTahoe as config-selected test values when those themes are
installed locally; theme payloads live outside this compositor repository.

Freedesktop's Desktop Entry Specification defines `Exec` as the command line
used by launchers and reserves field codes such as `%f`, `%F`, `%u`, and `%U`
for file or URL arguments. For a no-file Dock or desktop launch, those field
codes must be removed rather than passed literally to a shell. This avoids real
desktop entries failing when they include `%U` or similar launch placeholders.
It also defines the `Icon` key as either an absolute path or a themed icon name
resolved through the Icon Theme Specification.

Freedesktop's Icon Theme Specification defines icon lookup as a mapping from an
icon name and size to theme files, searched through base directories, theme
inheritance, and finally `hicolor`. Orange keeps a compact in-memory cache but
should still honor the practical directory shapes used by XDG themes:
`$XDG_DATA_DIRS/icons/<theme>/...`, `$HOME/.icons/<theme>/...`,
and `/usr/share/pixmaps`. The same named theme may have files spread across
multiple XDG bases, so lookup cannot stop at the first base containing
`index.theme`.
The Freedesktop Icon Naming Specification provides standard icon names for the
Linux-facing shell contract, including `start-here`, `system-search`,
`system-shutdown`, `system-lock-screen`, `network-wireless`, `network-offline`,
`audio-volume-high`, `battery`, `user-trash`, and `weather-clear`.

Apple's Mac User Guide describes the menu bar as a top-screen strip with the
Apple/system menu and app menus on the left, and status menu icons plus date
and time on the right. Status icons are interactive: clicking a status item
opens details and quick controls such as Wi-Fi options. Orange should preserve
that user-facing shape while resolving images and launch targets through
freedesktop desktop entries, icon themes, and standard Linux control commands.
This requires separate hit targets for Wi-Fi, sound, battery, search, Control
Center, and date/time rather than a single generic status-area button.
The closest Linux-facing behavior is: Wi-Fi opens a network status menu with
network settings actions; Sound opens a sound status menu with output/settings
actions; Battery opens a power status menu; Spotlight maps to Orange's app
picker; Control Center maps to Orange's quick-control menu; date/time maps to
Notification Center.
The same guide describes Control Center as a menu-bar entry that exposes common
settings such as Wi-Fi, AirDrop, Focus, Sound, Screen Mirroring, Display, and
related customization. Orange models the status-area menu as this
Control-Center-style grouping while launching Linux settings panes and tools.

Apple's Notification Centre guide says the surface opens from the menu-bar
date/time area, closes when the user clicks the desktop or the date/time area
again, and combines missed notifications with widgets. It also describes
grouped notification stacks, notification actions, clear actions, and app
notification settings. Orange's shell keeps the same right-edge overlay model
and now sources missed notification cards from the freedesktop desktop
notifications protocol instead of local placeholder text.

The freedesktop Desktop Notifications Specification defines
`org.freedesktop.Notifications` on the session bus with `GetCapabilities`,
`Notify`, `CloseNotification`, and `GetServerInformation` methods plus
`NotificationClosed`, `ActionInvoked`, and `ActivationToken` signals. Orange
implements the persistent, body, and static-icon subset required for real app
notifications and keeps unsupported notification actions as ignored client
input rather than presenting fake actions.

Apple's widget guide says Notification Centre widgets are added from the Edit
Widgets button at the bottom of Notification Centre, can be rearranged within
Notification Centre, and can be resized or removed from widget shortcut menus.
Orange already has desktop widget shortcut menus for resize/remove and maps
Edit Widgets to the local Settings app until a dedicated widget gallery exists.

Local Linux widget data findings:

- GNOME Calendar is backed by Evolution Data Server on this test system, with
  a local iCalendar file at
  `$XDG_DATA_HOME/evolution/calendar/system/calendar.ics`. The Orange widget
  data provider scans Evolution calendar `.ics` files directly so calendar
  widgets can show real local events without requiring EDS development headers.
- `pkg-config` exposes `gio-2.0`, `geoclue-2.0`, `json-c`, and `libsystemd`,
  but not Evolution Data Server, libical, libgweather, libsoup, or libcurl.
  The implementation therefore avoids compile-time GNOME calendar/weather
  dependencies and keeps parsing small and local.
- `org.gnome.GWeather4` settings are installed and expose a
  `default-location` key. That provides a real GNOME Weather location, but not
  a stable public current-conditions API in this environment. Orange reads a
  local weather file when present and otherwise renders the configured
  location with an unavailable current-weather state instead of fake weather.
- systemd-logind exposes session start time through `libsystemd`, so Screen
  Time can display the real login-session duration when available and fall
  back to Orange's compositor start time elsewhere.

`gnome-control-center` is guarded by desktop identity: on this system
`XDG_CURRENT_DESKTOP=Orange gnome-control-center --list` exits with "Running
gnome-control-center is only supported under GNOME and Unity", while
`XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu` lists panels including
`applications`, `background`, `bluetooth`, `display`, `keyboard`, `network`,
`wifi`, `notifications`, `power`, `privacy`, `sound`, `system`, `ubuntu`,
`universal-access`, and more. Orange therefore launches GNOME Control Center
through a wrapper that advertises a GNOME/Unity-compatible desktop identity and
unsets forced GTK theme variables so GNOME Settings follows the system
appearance keys.

The installed GNOME Background schema is `org.gnome.desktop.background`.
`gsettings describe` reports `picture-uri` and `picture-uri-dark` as local
`file://` URI settings. It also exposes `picture-options` values `none`,
`wallpaper`, `centered`, `scaled`, `stretched`, `zoom`, and `spanned`,
`picture-opacity`, `primary-color`, `secondary-color`, and
`color-shading-type` values `solid`, `vertical`, and `horizontal`. Orange maps
these keys into the wallpaper asset cache so the GNOME Background panel changes
the compositor wallpaper.

The xdg-desktop-portal Settings documentation standardizes
`org.freedesktop.appearance color-scheme` as an unsigned integer with values
0 for no preference, 1 for prefer dark, and 2 for prefer light. It defines
`Read`, `ReadOne`, `ReadAll`, and `SettingChanged` on the frontend portal and
similar backend methods/signals for desktop implementations. Orange implements
the color-scheme subset for GTK/libadwaita and portal-aware clients.
The frontend `Read` method is deprecated and keeps a historical double-variant
return shape; newer `ReadOne`, backend `Read`, `ReadAll`, and
`SettingChanged` use the normal single value variant. Returning the single
variant from frontend `Read` can make Nautilus/GTK callers attempt to unpack a
raw unsigned integer as a variant and crash.

Apple's current appearance documentation says light/dark appearance applies to
the menu bar, Dock, windows, and built-in apps. For Orange this means context
menus and the system menu must use the dark material palette when
`appearance=dark`, not only the Dock/widgets.

Apple's Apple-menu documentation lists About, System Settings, App Store,
Recent Items, Force Quit, Sleep, Restart, Shut Down, Lock Screen, and Log Out.
Orange keeps those visible menu labels but maps actions to local Orange apps,
standard Linux power commands, and installed software-center tools.

GTK's `GtkIconTheme` documentation describes a named icon theme as a shared
database and notes that direct lookup takes an icon name, size, and scale.
The shell renderer cannot depend on a GTK display object inside the wlroots
compositor path, so Orange implements the same practical model locally: parse
`index.theme`, probe only the requested icon name through exact/closest size
lookup, cache successful surfaces, and cache misses. This avoids rendering
hundreds of theme icons during startup or every shell redraw.

GTK4 utility apps can still use GTK's own theme machinery. The local GTK 4.14
headers expose `gtk_window_set_icon_name`, `gtk_window_set_default_icon_name`,
and `gtk-icon-theme-name` through `GtkSettings`. Installed MacTahoe GTK4 CSS
sets the outer CSD window radius on `window.csd`, so Orange Settings and About
mark their undecorated custom windows with `background` and `csd` classes and
let their root widgets inherit that radius instead of hardcoding a separate
corner value.

Freedesktop's StatusNotifierItem family is the DBus tray/status-area protocol:
StatusNotifierItem instances register with `org.kde.StatusNotifierWatcher`, and
hosts render those items in a status area. Orange now owns the watcher name on
the session bus, tracks registered AppIndicator/KStatusNotifierItem objects,
renders active items in the menu-bar tray, and forwards pointer actions through
the standard `Activate` and `ContextMenu` methods. Orange does not render
remote DBusMenu trees itself; AppIndicator-style menu items are asked to open
their own menu through `ContextMenu`.

Orange's custom About app controls use 16px geometry, a 23px button step,
active red/gray/gray colors, a hover/pressed glyph only on the red close
button, and gray inactive/backdrop colors, drawing the states directly without
depending on external SVG assets at runtime. The About dark palette follows
the installed MacTahoe dark GTK named colors used during testing:
`window_bg_color #333333`, `window_fg_color #dadada`, and `view_bg_color #242424`.

Apple's Desktop & Dock settings documentation describes Dock size as
slider-controlled, Dock magnification as icon magnification when the pointer
moves over icons, Dock screen position as left/bottom/right, minimized-window
animation as a selectable effect, minimizing windows into application icons,
automatic Dock hiding/showing, opening-animation control, and open-app
indicators. Apple's Dock usage documentation also says the Dock separator line
can be Control-clicked to open a shortcut menu and dragged to adjust Dock size.
That interaction also implies the separator is a first-class pointer target:
hover should present a resize cursor and drag should change size directly,
without requiring the shortcut menu.
Dock behavior references describe dynamic resizing to fit and labels appearing
on pointer hover. The shell Dock therefore avoids a circular hover halo, keeps
the small running indicator dot, uses icon magnification plus a compact label
bubble for hover feedback, and exposes Dock-wide controls from the separator
context menu. For Orange's requested hide behavior, the closer Linux precedent
is an "intellihide"/"dodge windows" model such as Dash to Dock: the Dock stays
visible unless a mapped application window overlaps its normal rectangle, then
reveals from the configured edge and is drawn in the overlay layer above
clients.

Apple's System Settings guide shows the current System Settings interaction
shape: launchable from the Dock or Apple menu, a searchable sidebar, and grouped
setting panes such as Appearance, Menu Bar, General, Accessibility, Desktop &
Dock, Displays, and Spotlight. Orange's GTK Settings app follows that structure
with a local config-backed Appearance panel instead of delegating to GNOME
Control Center.

The public GNOME Settings source is useful as a Linux settings-panel taxonomy
reference, but it is GPL-2.0 and uses GNOME-specific panel/back-end code.
Orange therefore keeps the implementation original and GTK4-only while using
GNOME's broad categories as inspiration for sidebar grouping.

macOS Tahoe 26 changes Spotlight from a narrow search launcher into a broader
command and browsing surface. Apple's current Tahoe Mac User Guide describes
Spotlight results from apps, files, actions, the internet, and Clipboard, plus
browse modes for Applications, Files, Actions, and Clipboard. Orange models
that with a centered glass Search pill that expands into the launcher when the
user types, four browse-mode buttons, and unified result rows for apps,
Desktop files, built-in actions, and web search. Dock launcher and Super+Space
can still open the centered Apps overlay directly.

Apple's Tahoe Mac User Guide keeps the long-standing Dock customization model:
apps are added by dragging them into the app side of the Dock, removing a Dock
item removes only the Dock alias once the Remove affordance is shown, and items
can be rearranged by dragging. The Tahoe app browser is now part of Spotlight:
the Dock Apps icon or menu-bar Spotlight can open the apps browsing view, and
Apple documents dragging an app from that apps browsing window to the Dock.
Orange should therefore make Dock removal a drag-off alias operation, keep
Trash and the launcher available as permanent shell affordances, and let app
icons from the launcher become Dock aliases when dropped on the Dock.

Fildem's global menu has two relevant discovery paths. Its Python service owns
`com.canonical.AppMenu.Registrar` at `/com/canonical/AppMenu/Registrar`.
Exporter modules call `RegisterWindow(windowId, menuObjectPath)`, Fildem stores
the DBus sender and object path, and then asks `GetMenuForWindow(xid)` before
reading the `com.canonical.dbusmenu` tree with `GetLayout`. Fildem also has a
GTK-specific fallback that reads `_GTK_UNIQUE_BUS_NAME`,
`_GTK_MENUBAR_OBJECT_PATH`, and related X11 window properties before querying
`org.gtk.Menus`/`org.gtk.Actions`. Orange cannot depend on those X11
properties or legacy GTK exporter modules for native Wayland clients, so the
compositor-owned registrar uses DBus sender PID plus Wayland client PID
matching as the Wayland equivalent.

The AT-SPI DBus interfaces provide a no-appmenu-module fallback for controls
that applications expose to accessibility tools: `org.a11y.Bus.GetAddress`
returns the accessibility bus address, `org.a11y.atspi.Accessible.GetChildren`
walks the accessible tree, `Accessible.Name`/`GetRoleName` identify controls,
and `org.a11y.atspi.Action.DoAction` invokes an exposed action. This can find
buttons, menu items, tabs, links, and toggles in many GTK/libadwaita apps, but
it is not a true native global-menu protocol. Completeness depends on the app's
accessibility tree and exposed action names.

Mac Dock shortcut-menu references confirm that running applications are
represented distinctly in the Dock and expose app lifecycle actions from their
Dock menu. TechRadar describes open apps as marked by a small dot and says an
open app can be closed from the Dock shortcut menu with Quit; Lifewire also
documents quitting a running Mac app from the Dock with right-click > Quit.
The macOS Dock behavior reference describes extended Dock menus with common
actions such as Quit, Keep in Dock, and Remove from Dock, and Mission
Control/App Expose references describe Show All Windows from an app's Dock
icon. Orange maps that shape to Linux/wlroots by adding a running Dock menu
variant with Show All Windows, Hide, and Quit, while preserving its existing
Show in Files, Remove from Dock, Open at Login, and Dock Settings rows. Trash
and Launchpad/Apps are not normal app aliases on macOS, so Orange treats its
built-in launcher and Trash entries as permanent Dock affordances with special
shortcut menus: launcher opens Launchpad or Dock settings, and Trash opens or
empties Trash.

Current Tahoe-era Apple references keep the menu bar and app command menus as
text-first command surfaces: the menu bar groups the Apple/system menu and app
menus on the left, status menus and the clock on the right, and app menus
present command labels with keyboard-shortcut symbols or text on the trailing
edge. Status menu items, Control Center, and related quick controls remain
icon-led because they represent device/control state rather than plain command
lists. Orange therefore now suppresses leading icons for regular system, app,
Dock, widget, desktop background, desktop file/volume, and desktop app-icon
menus, while keeping icons in status/control menus. Because common Linux Sans
font stacks can render macOS shortcut symbols as missing-glyph boxes in Cairo,
Orange presents legible ASCII shortcut labels such as `Ctrl+O` and
`Ctrl+Alt+Esc` instead of relying on proprietary or unavailable symbol fonts.

The same references and Mac desktop behavior indicate that desktop shortcut
menus should expose useful local state rather than inert rows. Orange maps
desktop Use Stacks to the saved `desktop_use_stacks` preference, cycles Sort By
through the currently implemented sortable data (`Name`, `Kind`, and `None`),
and maps Clean Up By to a snap-back-to-grid operation that clears saved manual
positions. Finder desktop item behavior also implies that files on the desktop
open when clicked, can be repositioned by dragging, and expose real file
operations from the shortcut menu. Orange therefore maps desktop files to
Open, Open With, Show in Files, Copy, Get Info, Rename/select, Duplicate,
Quick Look/open, Share, and Move to Trash actions. Open With behaves as a
highlighted side submenu rather than replacing the parent menu, is populated
from the freedesktop MIME associations exposed through GIO's GAppInfo
recommended and all-app lookup APIs, shows up to 16 associated app choices, and
launches the selected desktop application ID with the file URI. Orange also
stores dragged desktop positions for the full visible desktop item capacity.
Date/size desktop sorts remain out of scope until Orange threads filesystem
metadata through shell layout.

Apple's desktop organization guide describes desktop item layout as an icon
view surface with configurable icon size, grid spacing, text size, label
position, stacking, and sorting/cleanup commands. The current implementation
therefore keeps unsaved desktop items arranged from the right edge, but lets
manual dragged positions snap across the full usable desktop grid. That grid
must be computed after Dock layout so bottom/left/right Dock positions reserve
real clearance before cells are chosen. Apple's file preview guidance and
Finder/desktop behavior also support image thumbnails, drag-selection marquees,
and selected-item highlighting. Orange limits previews to image files that
decode locally, uses the marquee to select multiple desktop icons, and does not
add blue highlighting to the icon-move gesture itself.
For multiple highlighted desktop items, the menu should describe the active
selection rather than a single icon. Orange uses the full file command set for
file-only selections, but switches to a smaller mixed-selection command set when
volumes are included so volume rows are not offered file-only actions such as
Move to Trash.

Because Dock and menu-bar surfaces are foreground shell chrome, right-clicks on
their empty glass/background regions should be consumed by shell hit testing.
They should not be treated as empty desktop background hits merely because the
pointer is not over a child icon or text item.

## Risks

- wlroots APIs are explicitly unstable; code targets wlroots 0.20.1 through the
  `wlroots-0.20` pkg-config dependency.
- Vulkan renderer availability depends on GPU/driver/runtime environment.
- wlroots' Vulkan renderer needs a DRM render-node path from the backend. WSLg
  can expose GPU acceleration to Linux GUI clients without exposing the DRM FD
  needed by `WLR_BACKENDS=wayland WLR_RENDERER=vulkan`; in that case wlroots
  fails during renderer creation with `no DRM FD available`, and Pixman is the
  expected WSLg fallback.
- Running a real compositor outside headless mode may require a nested Wayland
  session or a TTY with seat permissions. Orange does not currently host
  Xwayland; `Game` desktop entries are merely exempted from forced toolkit
  Wayland variables and may keep inherited nested-session display and session
  bus state so native game launchers can choose their supported path.
- Exact proprietary vendor assets cannot be redistributed in this repository.
- The native GTK Settings and About apps are implemented conditionally so the
  compositor can still build on systems without GTK development headers.
- Client-side traffic-light controls depend on GTK client support for CSD and
  theme CSS. The compositor can force client-side preference, but non-GTK
  clients may not honor GTK CSS.

## References

- Apple newsroom: Liquid Glass software design announcement.
- Apple macOS design/product pages for visual layout reference.
- Apple Human Interface Guidelines: Liquid Glass material guidance and
  menu/action component guidance.
- Apple Support, "Add and customize widgets on Mac":
  https://support.apple.com/guide/mac-help/add-and-customize-widgets-mchl52be5da5/mac
- Apple Support, "Use Notification Centre on Mac":
  https://support.apple.com/guide/mac-help/mchl2fb1258f/mac
- Apple Support, "What’s in the menu bar on Mac?":
  https://support.apple.com/guide/mac-help/whats-in-the-menu-bar-mchlp1446/mac
- Apple Support, "What’s in the Apple menu on Mac?":
  https://support.apple.com/guide/mac-help/whats-in-the-apple-menu-mchlp1130/mac
- Apple Support, "Use Control Center on Mac":
  https://support.apple.com/guide/mac-help/quickly-change-settings-mchl50f94f8f/mac
- Apple Support, "Search for anything with Spotlight on Mac":
  https://support.apple.com/guide/mac-help/search-with-spotlight-mchlp1008/mac
- Apple Support, "Use the Dock on Mac":
  https://support.apple.com/guide/mac-help/open-apps-from-the-dock-mh35859/mac
- Apple Support, "View and open apps on Mac":
  https://support.apple.com/guide/mac-help/open-apps-in-spotlight-mh35840/26/mac/26
- Apple Support, "Use a light or dark appearance on your Mac":
  https://support.apple.com/guide/mac-help/use-a-light-or-dark-appearance-mchl52e1c2d2/mac
- Apple Support, "Find out which macOS your Mac is using":
  https://support.apple.com/109033
- Apple Support, macOS update notes:
  https://support.apple.com/en-au/122868
- Apple Support, "Find the Getting Started guide for your Mac":
  https://support.apple.com/guide/mac-help/mchl30bc42cb/mac
- MacTahoe GTK Theme:
  https://github.com/vinceliuice/MacTahoe-gtk-theme
- MacTahoe Icon Theme:
  https://github.com/vinceliuice/MacTahoe-icon-theme
- GTK4 `GtkIconTheme`:
  https://docs.gtk.org/gtk4/class.IconTheme.html
- Freedesktop Icon Theme Specification:
  https://specifications.freedesktop.org/icon-theme-spec/latest/
- Freedesktop Icon Naming Specification:
  https://specifications.freedesktop.org/icon-naming-spec/latest/
- Freedesktop Desktop Entry Specification:
  https://specifications.freedesktop.org/desktop-entry-spec/latest/
- Wayland xdg-shell protocol:
  https://wayland.app/protocols/xdg-shell
- wlroots renderer documentation:
  https://wlroots.pages.freedesktop.org/wlroots/wlr/render/wlr_renderer.h.html
- wlroots Vulkan renderer documentation:
  https://wlroots.pages.freedesktop.org/wlroots/wlr/render/vulkan.h.html
- Freedesktop StatusNotifierItem:
  https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
- Apple Support, "Change Desktop & Dock settings on Mac":
  https://support.apple.com/guide/mac-help/change-desktop-dock-settings-mchlp1119/mac
- Apple Support, "Use the Dock on Mac":
  https://support.apple.com/guide/mac-help/open-apps-from-the-dock-mh35859/mac
- Apple Support, "Organize files on your desktop":
  https://support.apple.com/guide/mac-help/organize-files-on-your-desktop-mh35846/mac
- Apple Support, "Preview a file":
  https://support.apple.com/guide/mac-help/preview-a-file-mh14119/mac
- Apple Support, "Choose an app to open a file on Mac":
  https://support.apple.com/guide/mac-help/choose-an-app-to-open-a-file-on-mac-mh35597/mac
- GIO `GAppInfo`:
  https://docs.gtk.org/gio/iface.AppInfo.html
- GIO `g_app_info_get_all`:
  https://docs.gtk.org/gio/type_func.AppInfo.get_all.html
- GIO `g_app_info_should_show`:
  https://docs.gtk.org/gio/method.AppInfo.should_show.html
- GIO Unix `GDesktopAppInfo`:
  https://docs.gtk.org/gio-unix/class.DesktopAppInfo.html
- GNOME Shell `ShellAppSystem` source:
  https://raw.githubusercontent.com/GNOME/gnome-shell/main/src/shell-app-system.c
- GNOME Shell `ShellWindowTracker` source:
  https://raw.githubusercontent.com/GNOME/gnome-shell/main/src/shell-window-tracker.c
- GNOME Mutter `MetaWindow` sandboxed app ID source:
  https://raw.githubusercontent.com/GNOME/mutter/main/src/core/window.c
- Freedesktop MIME Applications Associations Specification:
  https://specifications.freedesktop.org/mime-apps-spec/latest/
- Apple Support, "System Settings on your Mac":
  https://support.apple.com/guide/imac/system-settings-apda966cb8af/mac
- GNOME Settings source mirror:
  https://github.com/GNOME/gnome-control-center
- Dash to Dock source and intellihide behavior:
  https://github.com/micheleg/dash-to-dock
- xdg-desktop-portal Settings frontend interface:
  https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html
- xdg-desktop-portal Settings backend interface:
  https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Settings.html
- Dock (macOS) behavior reference:
  https://en.wikipedia.org/wiki/Dock_(macOS)
- TechRadar, "What is the macOS Dock?":
  https://www.techradar.com/computing/mac-os/what-is-the-macos-dock-heres-how-to-master-apples-taskbar-rival-on-your-brand-new-macbook-neo
- Lifewire, "How to Close Applications on Mac":
  https://www.lifewire.com/close-applications-on-mac-5184824
- Mission Control (macOS) App Expose reference:
  https://en.wikipedia.org/wiki/Mission_Control_(macOS)
- Apple Human Interface Guidelines, "Liquid Glass":
  https://developer.apple.com/design/human-interface-guidelines/liquid-glass
- Apple Human Interface Guidelines, "Menus":
  https://developer.apple.com/design/human-interface-guidelines/menus
- Apple Newsroom, "Apple introduces a delightful and elegant new software
  design":
  https://www.apple.com/newsroom/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/
- Apple Newsroom, "macOS Tahoe 26 makes the Mac more capable, productive, and
  intelligent than ever":
  https://www.apple.com/newsroom/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/
- The Verge, "Hands on with macOS Tahoe 26: Liquid Glass, new theme options,
  and Spotlight":
  https://www.theverge.com/apple/685052/apple-macos-tahoe-26-beta-hands-on-liquid-glass-themes-spotlight
- TechRadar, "How to use Spotlight in macOS Tahoe":
  https://www.techradar.com/computing/mac-os/how-to-use-spotlight-in-macos-tahoe
- wlroots 0.20 local headers and online renderer documentation.
- Khronos Vulkan documentation for Vulkan instance/rendering concepts.
