# Architecture

## Components

### Compositor Runtime

Owns Wayland display setup, wlroots backend creation, renderer/allocator setup,
output lifecycle, and process options.

Responsibilities:

- initialize wlroots,
- create core Wayland globals,
- create xdg-shell support,
- initialize new outputs,
- render frames,
- shut down cleanly in `--once` mode.

### Shell Renderer And Interaction Model

Produces a complete desktop shell image into an ARGB software buffer using
Cairo. The buffer is uploaded to a wlroots texture and composited to the output.

Responsibilities:

- draw wallpaper,
- draw a transparent menu bar, widgets, desktop items, dock, and icons,
- apply persistent appearance, desktop, and Dock configuration,
- load wallpapers from GNOME GSettings and ask the asset layer for themed icons,
- place desktop icons from persisted coordinates when the user drags them,
  using a Dock-aware grid that spans the usable desktop work area, snaps both
  axes, reserves scaled edge/Dock clearance, and resolves duplicate saved cells
  to the nearest free slot,
- render locally decoded image previews for image desktop files, with themed
  icon fallback when decoding fails, and draw Finder-like selection marquees
  plus selected-item highlighting independently from the icon-move gesture,
- draw shell context menus for Dock items, the Dock separator, widgets,
  desktop items, multi-selected desktop groups, and empty desktop background,
- render regular command menus through shared text-first menu metadata:
  labels, separators, right-aligned shortcut/detail text, checked-state
  markers, enabled state for imported native app items, and a per-menu
  decision about whether leading icons are appropriate,
- draw the top-left active app menu tabs as shell chrome anchored under the
  active app title and File/Edit/View/Go/Window/Help tab rectangles, using
  focused-client state for the visible app label,
- draw a right-edge Notification Center overlay as transient shell chrome,
- expose deterministic layout and hit-test math for tests,
- maintain transient UI state such as the system menu popover.

The live compositor keeps a base shell buffer for wallpaper, menu bar, widgets,
desktop items, and Dock chrome, plus a separate transparent overlay buffer for
transient surfaces such as context menus, system menus, Notification Center,
and the launcher. Closing the last transient surface clears and damages the
overlay buffer before the next scene commit, preventing stale overlay pixels
from remaining above the desktop or client windows.
Window minimize and restore effects also use the overlay buffer. Before a view
is hidden, the compositor captures the view's current output rectangle from a
scene-output readback into a transient `orange_buffer`, stores that screenshot
on the view while it is minimized, and drives a short compositor-owned
animation between the view rectangle and a minimized-window Dock thumbnail.
Those thumbnail slots are part of Dock layout, sit immediately left of Trash,
and draw the retained screenshot after the minimize animation lands. The Scale
effect draws the fitted screenshot rectangle directly; the Genie effect draws
the same screenshot clipped through a tapered curved path toward the configured
Dock edge. The real scene node stays disabled while minimized and is re-enabled
after a restore animation finishes. Clicking a minimized thumbnail restores the
corresponding view instead of entering Dock app reorder logic.
Live Dock and launcher-app drag feedback also belongs to the overlay buffer:
the overlay draws the moving icon ghost and Remove affordance over the
old/current damage union, while the base shell redraws only when the insertion
slot changes so Dock icons slide aside instead of showing a static marker.
Persistent `dock_apps` mutations are centralized in the Dock module so remove,
reorder, context-menu removal, and launcher-to-Dock insertion all compact
aliases, reset visible order, avoid duplicates, and keep permanent shell
affordances available.

Dock layout carries a config-derived screen position (`bottom`, `left`, or
`right`) and a first-class separator rectangle. Shell hit testing treats the
separator as its own target so right-click opens a Dock-wide command menu,
while Dock item menus remain app-specific. Bottom Dock layout uses horizontal item
geometry; side Dock layout uses vertical item geometry, rotated separator
drawing, side-aware drag insertion, and maximize bounds that reserve the side
Dock strip.
The Dock glass rectangle and menu-bar glass rectangle are also first-class
chrome hit targets. Child controls such as Dock items, the Dock separator,
menu tabs, and status items win first; otherwise the chrome background consumes
the pointer hit so right-click dispatch cannot fall through to the desktop
background menu.

Orange's session services also own `org.kde.StatusNotifierWatcher` on the
session bus for AppIndicator/KStatusNotifierItem clients. Registered
StatusNotifierItem objects are normalized to a stable service/path ID, their
standard properties are cached, and active or attention-needed items are copied
into shell state as immutable draw snapshots. Layout reserves tray slots to the
left of the built-in Wi-Fi/sound/battery/search/clock items, hit testing returns
`ORANGE_HIT_TRAY_ITEM`, and the compositor routes left-clicks to
`Activate` or menu-first `ContextMenu` for AppIndicator-style items while
right-clicks always request `ContextMenu`.

Dock item menus are selected by compositor state after hit testing. The layout
module exposes static non-running, running, launcher, Trash, and Dock separator
menu shapes. The compositor chooses launcher/Trash variants for built-in
permanent affordances, otherwise chooses the running variant when
`dock_open[launcher_idx]` is true. Running Dock menu actions reuse the same
view-to-launcher matching as Dock indicators: Show All Windows unminimizes,
raises, and focuses matching mapped toplevels; Hide minimizes matching mapped
toplevels; Quit sends xdg-toplevel close requests to matching mapped toplevels.

Dock bounce remains a Dock renderer concern. The scalar bounce waveform is
kept as a reusable helper, and a position-aware displacement helper maps it to
upward movement for the bottom Dock, rightward movement for the left Dock, and
leftward movement for the right Dock.

### Widget Layer

Root-window widget registry for desktop cards. Calendar and Weather are the
initial built-in widgets. Widget painting lives in `src/widgets.c` so
`src/shell.c` owns layout, hit testing, and composition orchestration rather
than individual widget visuals.

Responsibilities:

- compute widget rectangles from output size and config,
- keep stable widget IDs and types,
- apply widget visibility and size settings,
- expose widget hit targets and shortcut-menu actions,
- draw widgets beneath application windows,
- expose deterministic layout for tests.
- render Notification Center widget cards for Calendar, Screen Time, and
  Weather from shared widget data.

### Widget Data Provider

`src/widget_data.c` collects local data for widget rendering without making
the renderer depend on GNOME libraries at compile time.

Responsibilities:

- read upcoming events from local iCalendar files used by Evolution Data
  Server/GNOME Calendar,
- report Screen Time from the systemd-logind session start when available,
  falling back to the Orange compositor start time,
- read local weather key/value data from `ORANGE_WEATHER_FILE` or
  `$XDG_CACHE_HOME/orange/weather`,
- read GNOME Weather's configured location from `org.gnome.GWeather4`
  settings when that schema is installed,
- provide empty/unavailable states instead of demo values when live data is
  missing.

### Asset Loader

Loads wallpapers from GNOME GSettings, accepts runtime GNOME
Background settings from the compositor, and resolves PNG/SVG icons from
freedesktop-style icon theme directories. The loader maps specific Dock,
desktop, weather, and status icon names so shell code asks for semantic icon
names while the asset layer handles configured themes, inherited themes,
`hicolor` fallback, unthemed pixmaps, and aliases to freedesktop standard icon
names.

Responsibilities:

- keep tracked assets wallpaper-only,
- lazily decode bundled and GNOME-selected wallpapers and cache output-sized
  wallpaper surfaces,
- compose GNOME Background picture options (`none`, `wallpaper`, `centered`,
  `scaled`, `stretched`, `zoom`, and `spanned`), opacity, solid color, and
  gradient colors before shell drawing,
- lazily resolve and cache only requested icon names,
- resolve desktop shortcut icons from each parsed `.desktop` file's `Icon=`
  name,
- resolve status and Dock icon names from the configured icon theme, inherited
  themes, then hicolor/system fallbacks,
- validate loaded image dimensions,
- avoid failing startup when optional files are absent.

### Configuration Store

Small line-oriented config model used by both the compositor and Settings app.

Responsibilities:

- read and write `appearance`, desktop icon, widget, Dock, and window-effect
  preferences,
- read and write cursor theme/size, GTK theme names, icon theme name, and
  dragged desktop icon positions,
- provide defaults when config is missing,
- expose one struct consumed by shell layout and rendering.

### GNOME Settings Bridge

The compositor mirrors the subset of GNOME desktop settings that GNOME Control
Center and GTK/libadwaita clients expect in an Orange session.

Responsibilities:

- write `org.gnome.desktop.interface` appearance, GTK theme, icon theme,
  cursor theme, and cursor size when Orange appearance settings change,
- read `org.gnome.desktop.interface color-scheme` while running so GNOME
  Settings' Style selector can change Orange appearance between light/default
  and dark,
- expose `org.freedesktop.appearance.color-scheme` through the Settings portal
  backend and local frontend interfaces,
- keep Settings portal variant shapes compatible with both the backend API and
  the frontend API: `ReadOne`, backend `Read`, `ReadAll`, and
  `SettingChanged` use the normal value variant, while deprecated frontend
  `Read` returns the historical extra variant layer expected by older
  GTK/libadwaita callers,
- read `org.gnome.desktop.background` at startup and poll it periodically so
  GNOME Control Center wallpaper changes redraw the shell,
- keep GNOME-specific failures non-fatal when GSettings schemas, dconf, or the
  session bus are unavailable.
- pump the default GLib context from the compositor timer so GSettings/DBus
  notifications are serviced without adding a second event loop.

### GTK Utility Apps

GTK application sources that are conditionally built when GTK development
libraries are present.

Settings app responsibilities include appearance, desktop icon, widget, Dock,
installed GTK/icon/cursor theme dropdowns, and cursor size controls, writing
config changes without a compositor restart.

About app responsibilities include showing the Orange version/build
affordance, model, real chip and memory values, and theme-style window
controls, launched from the system menu. Both GTK utility apps set themed
window icon names and use GTK CSD classes so the active GTK theme controls the
outer window radius.

### GTK Themes

Launched GTK clients receive `GTK_THEME` from `gtk_theme_light` or
`gtk_theme_dark` in `orange.conf`, depending on the current appearance.
MacTahoe is the current test default, but any installed GTK theme name can be
used. Settings discovers GTK4 themes from standard user and system theme
directories and includes the current saved name even when it is missing.

### Icon Theme

`icon_theme` in `orange.conf` selects the freedesktop icon theme used by the
shell and launched GTK clients. The asset loader checks standard user icon
directories, `$XDG_DATA_DIRS/icons`, inherited themes, `hicolor`, and
`/usr/share/pixmaps`. It caches both successful lookups and missing icon names
so normal redraws do not scan or render whole icon themes.
Settings discovers icon and cursor theme directories from the same XDG icon
roots, plus `XCURSOR_PATH` for cursor themes.

### View Management And Input

xdg-shell support for ordinary clients. New toplevels are centered and rendered
above the shell layer with wlroots scene helpers.

Responsibilities:

- track mapped/unmapped xdg toplevels,
- send basic configure events,
- render client textures,
- focus and raise windows,
- collect mapped toplevel app IDs/titles for Dock active indicators,
- move/resize windows from compositor grabs,
- respond to maximize/fullscreen/close requests,
- dispatch pointer and keyboard input to the focused client.

Spec responsibilities:

- xdg-shell: create desktop-style toplevels/popups through wlroots, configure
  map/unmap/destroy, focus, raise, move, resize, maximize, fullscreen, and
  close flows without inventing non-protocol client state.
- xdg-decoration/KDE server decoration: prefer client-side decorations and do
  not force proprietary server decorations.
- AppMenu/DBusMenu: own `com.canonical.AppMenu.Registrar` on the session bus,
  collect `RegisterWindow(windowId, menuObjectPath)` calls from clients that
  export DBusMenu themselves, resolve the DBus sender PID through
  `org.freedesktop.DBus`, and match registered menu exporters to the focused
  Wayland client PID. Imported DBusMenu trees populate the active app tabs and
  dispatch item clicks through `com.canonical.dbusmenu.Event`. Orange does not
  install or inject the legacy GTK AppMenu exporter module.
- AT-SPI fallback: when no DBusMenu/GMenu exporter is found, connect to the
  accessibility bus through `org.a11y.Bus`, match the focused Wayland client PID
  to an AT-SPI application bus name, scan a bounded accessible subtree, and
  expose named actionable controls as an `Actions` app-menu tab. Item clicks
  dispatch `org.a11y.atspi.Action.DoAction`.
- freedesktop desktop integration: launch `.desktop` entries using spec-aware
  `Exec` field-code removal for no-file launches, resolve `Icon=` through the
  Icon Theme inheritance and `hicolor` fallback path, and use standard Icon
  Naming aliases for shell semantic icons.

### Launch Services Shim

Small local process launcher for Dock, desktop, and shortcut commands.

Responsibilities:

- launch commands through `/bin/sh -c` in a child process,
- prepare freedesktop `.desktop` `Exec` commands for no-file launches by
  removing file/URL field codes and expanding safe name/icon field codes,
- keep the full parsed desktop-entry table for Dock, desktop, and app-ID
  resolution, but apply GNOME-style `should_show` filtering before app launcher
  search/category display so `Hidden`, `NoDisplay`, incompatible
  `OnlyShowIn`/`NotShowIn`, and missing `TryExec` entries do not become
  top-level launcher shortcuts,
- parse package and window-matching metadata (`X-SnapInstanceName`,
  `X-SnapAppName`, `X-Flatpak`, and `StartupWMClass`) so Dock/app matching can
  skip stale package-specific desktop files and choose the best available
  equivalent entry,
- prefer user-configured environment variables for terminal/app picker,
- export `GTK_THEME`, `GTK_ICON_THEME`, and `ORANGE_APPEARANCE` for launched
  GTK clients,
- force Wayland toolkit backends and Chromium/Electron Ozone hints for
  non-game launches, while letting `Categories=Game` desktop entries avoid
  forced toolkit Wayland variables and keep an inherited nested-session
  `DISPLAY` and session-bus state until Orange has its own Xwayland host,
- clear inherited GTK module injection from client launch environments,
- wrap `gnome-control-center` launches with `XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu`
  and GNOME session identity variables, unsetting forced GTK theme variables
  so GNOME Settings and panel desktop entries use system appearance settings,
- avoid blocking the compositor event loop.

## Data Flow

1. wlroots emits an output frame event.
2. Runtime updates each output's Cairo-backed base shell buffer when persistent
   shell state changes.
3. Runtime updates or clears the transparent overlay buffer when transient
   launcher/menu/notification state changes, and also marks the overlay dirty
   when an app commits new pixels under a backdrop-dependent overlay. Before
   drawing glass overlays, it builds an offscreen backdrop from the shell buffer
   plus visible client scene buffers below the overlay when the renderer can
   import them, falling back to the shell buffer if readback/import is
   unavailable. Overlay unblending uses a separate transparent alpha mask so a
   light glass panel over a light app still keeps its material opacity instead
   of collapsing to a transparent source layer.
4. The shell and overlay buffers are exposed as wlroots scene buffers below and
   above mapped client surfaces respectively.
5. wlroots scene nodes render shell buffers and mapped client surfaces.
6. Runtime commits the scene output and sends frame callbacks.
7. Pointer hit testing first updates Dock hiding state by checking whether a
   mapped, non-minimized client rectangle overlaps the Dock's normal rectangle.
   Edge-hover reveal is only active while that obstruction-driven hidden state
   is true, so the Dock remains visible unless an app gets in the way. Normal
   client hit-testing then runs; when a hidden Dock is revealed, Dock hover and
   click dispatch are handled as overlay shell input above client surfaces.
   Otherwise, shell hit testing handles Dock, desktop, and menu clicks only
   when no client surface is under the pointer.
8. A compositor timer refreshes widget data and dirties the shell/overlay when
   Calendar, Screen Time, or Weather values change.
8. Clicking or right-clicking the active app title or one of the app menu tabs
   opens a shell-rendered app menu anchored under that tab. The compositor fills
   the menu bar label from the focused view's Dock entry, app ID, or title. If
   the focused Wayland client PID matches an AppMenu registrar entry, the
   compositor imports that DBusMenu tree and uses the exported native menu
   labels/items. If no menu exporter is present, Orange can fall back to
   AT-SPI-exposed buttons/actions under an `Actions` tab. Otherwise, common
   commands are dispatched to the focused keyboard client as standard
   accelerators such as Ctrl+O, Ctrl+S, Ctrl+Z, Ctrl+C, Ctrl+V, Ctrl+F, Ctrl+P,
   F11, and F1; window-level commands use compositor state where available.
9. Shell click handlers launch commands from Dock definitions or parsed XDG
   `.desktop` entries. Dock item presses become click-or-drag gestures: a
   click launches, horizontal/vertical movement starts overlay-only drag
   feedback, dropping in the Dock reorders, and dropping clearly off the Dock
   removes the alias when the item is removable. When Dock hiding is enabled
   and a window obstructs the Dock, the base shell buffer omits the Dock and
   the overlay buffer draws it while revealed so it sits above client windows
   without changing window work areas during reveal. Launcher app presses use
   the same click-or-drag split: a click launches, while dropping a
   non-duplicate app on the Dock inserts a new alias before Trash.
   The Dock separator is a separate hover and drag target: hover sets a
   divider-axis resize cursor, left-drag updates `dock_scale` live using
   Dock-position-aware drag math, and release persists the new scale. Bottom
   Docks resize from vertical pointer movement above/below the Dock, while side
   Docks resize from vertical movement along the horizontal divider. Hover
   magnification, hover labels, and bounce frames mark a conservative Dock dirty
   rectangle so the base shell redraw clips to the Dock area instead of
   repainting and damaging the whole output.
10. Desktop drag state updates the in-memory config while dragging and saves
   `orange.conf` on release. The shell layout recomputes desktop grid metrics
   after Dock geometry is known so dragged icons cannot be placed under or too
   close to the Dock. Desktop item click/open and context-menu actions use the
   layout's file/volume metadata so sorting does not break file or volume
   targeting.
11. Right-click hit testing opens a shell context menu above a Dock item, near a
   widget, at a desktop item cursor, or at the empty desktop cursor location.
   Empty Dock and menu-bar chrome hits clear/consume the shell interaction
   without opening the desktop menu.
12. Left-clicking status items dispatches by item: Wi-Fi, Sound, Battery,
    Search, Control Center, and Clock each have their own hit rectangle and
    action. Wi-Fi, Sound, and Battery open item-specific status menus; Search
    opens a centered compact glass search pill with four adjacent circular mode
    buttons. Typing expands the pill into the smaller launcher panel anchored
    to the same search/header position, and the result model switches from the
    app grid to Spotlight-style rows that can represent apps, desktop files,
    built-in actions, and web search. The mode buttons narrow the same surface
    to Applications, Files, Actions, or Clipboard-style actions. The Dock
    launcher and Super+Space open the centered app launcher panel directly.
    Both launcher surfaces can be dragged by their search/header area. Control
    Center opens quick controls; Clock toggles Notification Center.
13. Layout computes a right-edge Notification Center overlay after the base
    shell geometry, hit testing treats the overlay and Edit Widgets button as
    top-level shell targets, and the compositor maps Edit Widgets to Orange
    Settings.
14. The compositor owns `org.freedesktop.Notifications` on the session DBus
    when available, stores recent notifications in memory, updates existing
    notifications by `replaces_id`, emits `NotificationClosed` for explicit
    closes and evictions, and passes the current notification list into shell
    overlay rendering.

## Failure Modes

- Renderer creation failure: exit with an error.
- Output render initialization failure: disable that output and continue.
- Asset load failure: use procedural background fallback only.
- Missing bundled wallpaper assets: use the procedural background fallback.
- Missing or undecodable GNOME Background picture URI: render the configured
  GNOME background color/gradient and keep the compositor running.
- Missing icon theme or icon names: cache the miss and leave that icon slot
  empty while continuing to render.
- Missing cursor theme: wlroots falls back according to xcursor lookup rules.
- Headless `--once` no output: create an explicit headless output.
- Vulkan unavailable: wlroots renderer creation may fail; the log identifies
  the selected renderer when it succeeds.
- Launcher command missing: child exits; compositor remains running.
- GNOME settings launcher unavailable: fall back to Orange Settings or
  desktop-neutral tools. When `gnome-control-center` is installed, launch it
  with a GNOME/Unity-compatible desktop identity so its built-in session guard
  and individual panel desktop entries work inside Orange.
- Input device absent in headless mode: shell still renders and exits in
  `--once` validation.
- GTK missing at build time: Settings app target is skipped, compositor and
  tests remain buildable.
