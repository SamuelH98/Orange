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
- load wallpapers from `./assets/` and ask the asset layer for themed icons,
- place desktop icons from persisted coordinates when the user drags them,
- draw shell context menus for Dock items, the Dock separator, widgets,
  desktop items, and empty desktop background,
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
separator as its own target so right-click opens a Dock-wide bubble menu, while
Dock item menus remain app-specific. Bottom Dock layout uses horizontal item
geometry; side Dock layout uses vertical item geometry, rotated separator
drawing, side-aware drag insertion, and maximize bounds that reserve the side
Dock strip.

### Widget Layer

Root-window widget registry for desktop cards. Calendar and Weather are the
initial built-in widgets.

Responsibilities:

- compute widget rectangles from output size and config,
- keep stable widget IDs and types,
- apply widget visibility and size settings,
- expose widget hit targets and shortcut-menu actions,
- draw widgets beneath application windows,
- expose deterministic layout for tests.

### Asset Loader

Loads wallpapers from local `./assets/` roots and resolves PNG/SVG icons from
freedesktop-style icon theme directories. The loader maps specific Dock,
desktop, weather, and status icon names so shell code asks for semantic icon
names while the asset layer handles configured themes, inherited themes,
`hicolor` fallback, unthemed pixmaps, and aliases to freedesktop standard icon
names.

Responsibilities:

- keep tracked assets wallpaper-only,
- lazily decode wallpapers and cache output-sized wallpaper surfaces,
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
- prefer user-configured environment variables for terminal/app picker,
- export `GTK_THEME`, `GTK_ICON_THEME`, and `ORANGE_APPEARANCE` for launched
  GTK clients,
- avoid blocking the compositor event loop.

## Data Flow

1. wlroots emits an output frame event.
2. Runtime updates each output's Cairo-backed base shell buffer when persistent
   shell state changes.
3. Runtime updates or clears the transparent overlay buffer when transient
   launcher/menu/notification state changes.
4. The shell and overlay buffers are exposed as wlroots scene buffers below and
   above mapped client surfaces respectively.
5. wlroots scene nodes render shell buffers and mapped client surfaces.
6. Runtime commits the scene output and sends frame callbacks.
7. Pointer hit testing first finds client surfaces; if none, shell hit testing
   handles Dock, desktop, and menu clicks.
8. Clicking or right-clicking the active app title or one of the app menu tabs
   opens a shell-rendered app menu anchored under that tab. The compositor fills
   the menu bar label from the focused view's Dock entry, app ID, or title.
   Common commands are dispatched to the focused keyboard client as standard
   accelerators such as Ctrl+O, Ctrl+S, Ctrl+Z, Ctrl+C, Ctrl+V, Ctrl+F,
   Ctrl+P, F11, and F1; window-level commands use compositor state where
   available.
9. Shell click handlers launch commands from Dock definitions or parsed XDG
   `.desktop` entries. Dock item presses become click-or-drag gestures: a
   click launches, horizontal/vertical movement starts overlay-only drag
   feedback, dropping in the Dock reorders, and dropping clearly off the Dock
   removes the alias when the item is removable. Launcher app presses use the
   same click-or-drag split: a click launches, while dropping a non-duplicate
   app on the Dock inserts a new alias before Trash.
10. Desktop drag state updates the in-memory config while dragging and saves
   `orange.conf` on release.
11. Right-click hit testing opens a shell context menu above a Dock item, near a
   widget, near a desktop item, or at the empty desktop cursor location.
12. Left-clicking status items dispatches by item: Wi-Fi, Sound, Battery,
    Search, Control Center, and Clock each have their own hit rectangle and
    action. Wi-Fi, Sound, and Battery open item-specific status menus; Search
    opens a centered compact glass search pill with four adjacent circular mode
    buttons. Typing remains in the pill. Clicking the first mode button
    transforms the pill into the smaller app launcher panel anchored to the same
    search/header position. The Dock launcher and Super+Space open the centered
    app launcher panel directly. Both launcher surfaces can be dragged by their
    search/header area. Control Center opens quick controls; Clock toggles
    Notification Center.
13. Layout computes a right-edge Notification Center overlay after the base
    shell geometry, hit testing treats the overlay and Edit Widgets button as
    top-level shell targets, and the compositor maps Edit Widgets to Orange
    Settings.

## Failure Modes

- Renderer creation failure: exit with an error.
- Output render initialization failure: disable that output and continue.
- Asset load failure: use procedural background fallback only.
- Missing wallpaper assets: use the procedural background fallback.
- Missing icon theme or icon names: cache the miss and leave that icon slot
  empty while continuing to render.
- Missing cursor theme: wlroots falls back according to xcursor lookup rules.
- Headless `--once` no output: create an explicit headless output.
- Vulkan unavailable: wlroots renderer creation may fail; the log identifies
  the selected renderer when it succeeds.
- Launcher command missing: child exits; compositor remains running.
- GNOME settings launcher unavailable or unsupported outside GNOME/Unity:
  commands skip `gnome-control-center` and prefer Orange Settings or
  desktop-neutral tools.
- Input device absent in headless mode: shell still renders and exits in
  `--once` validation.
- GTK missing at build time: Settings app target is skipped, compositor and
  tests remain buildable.
