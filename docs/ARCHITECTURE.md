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
- draw shell context menus for Dock items, widgets, desktop items, and empty
  desktop background,
- expose deterministic layout and hit-test math for tests,
- maintain transient UI state such as the system menu popover.

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

- read and write `appearance`, desktop icon, widget, and Dock preferences,
- read and write cursor theme/size, GTK theme names, icon theme name, and
  dragged desktop icon positions,
- provide defaults when config is missing,
- expose one struct consumed by shell layout and rendering.

### GTK Utility Apps

GTK application sources that are conditionally built when GTK development
libraries are present.

Settings app responsibilities include appearance, desktop icon, widget, Dock,
cursor theme, and cursor size controls, writing config changes without a
compositor restart.

About app responsibilities include showing the Orange version/build
affordance, model, real chip and memory values, and theme-style window
controls, launched from the system menu.

### GTK Themes

Launched GTK clients receive `GTK_THEME` from `gtk_theme_light` or
`gtk_theme_dark` in `orange.conf`, depending on the current appearance.
MacTahoe is the current test default, but any installed GTK theme name can be
used.

### Icon Theme

`icon_theme` in `orange.conf` selects the freedesktop icon theme used by the
shell and launched GTK clients. The asset loader checks standard user icon
directories, `$XDG_DATA_DIRS/icons`, inherited themes, `hicolor`, and
`/usr/share/pixmaps`. It caches both successful lookups and missing icon names
so normal redraws do not scan or render whole icon themes.

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
2. Runtime updates each output's Cairo-backed shell buffer when state changes.
3. The shell buffer is exposed as a wlroots scene buffer.
4. wlroots scene nodes render shell buffers and mapped client surfaces.
5. Runtime commits the scene output and sends frame callbacks.
6. Pointer hit testing first finds client surfaces; if none, shell hit testing
   handles Dock, desktop, and menu clicks.
7. Shell click handlers launch commands from Dock definitions or parsed XDG
   `.desktop` entries.
8. Desktop drag state updates the in-memory config while dragging and saves
   `orange.conf` on release.
9. Right-click hit testing opens a shell context menu above a Dock item, near a
   widget, near a desktop item, or at the empty desktop cursor location.

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
- Input device absent in headless mode: shell still renders and exits in
  `--once` validation.
- GTK missing at build time: Settings app target is skipped, compositor and
  tests remain buildable.
