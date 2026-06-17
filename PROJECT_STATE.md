# PROJECT_STATE

## Project Overview

### Project Name

Orange

### Goal

Build Orange, a C/wlroots Linux compositor prototype that follows the supplied
macOS-like desktop reference and behaves like a usable macOS-like shell,
with optional local private asset overrides and Vulkan renderer support through
wlroots.

### Current Status

Functional macOS-like shell prototype with volume-based desktop icons (drives
only, like macOS), grid layout with snap-to-grid positioning, translucent menu
bar with dock-style glass effect, compact Dock with even glass transparency,
wlroots compositor, scalable widgets, basic xdg-shell window management, Dock
launchers, keyboard shortcuts, cursor customization, GTK/icon theme
configuration, lazy freedesktop icon lookup, PNG render export, foreground-only
visual smoke coverage, and headless one-shot validation.

---

## Completed Features

### Project Planning

#### Validation

Local dependency versions were checked with `pkg-config`, `meson`, `ninja`, and
`cc`.

#### Tests Added

`tests/test_shell_layout.c` covers Dock/menu/desktop hit testing, resolution
scaling, and a render smoke test.

### Functional Compositor Prototype

#### Validation

- `meson setup build` passed.
- `ninja -C build` passed.
- `meson test -C build --print-errorlogs` passed.
- `build/orange-render-shell /tmp/orange-shell.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once`
  passed and rendered one headless frame.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --assets assets --config /tmp/orange-custom.conf`
  passed and rendered one headless custom-size frame.

#### Tests Added

Shell layout/hit-test/render smoke unit tests.
Meson startup smoke test for custom headless compositor arguments.

---

### Settings, Themes, Assets, And Widgets

#### Validation

- `ninja -C build` passed cleanly.
- `meson test -C build --print-errorlogs` passed.
- `build/orange-render-shell /tmp/orange-shell.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once`
  passed and rendered one headless frame.

#### Implemented

- Persistent `orange.conf` model for appearance, desktop icon, and Dock settings.
- Conditional GTK4 Settings app source for appearance, desktop icon, and Dock
  controls, with GTK headerbar controls requested on the left and default size
  clamped to monitor geometry.
- Conditional GTK4 About Orange app source launched from system menu >
  About Orange, using a portrait About-style reference layout with custom
  traffic-light window controls (close/minimize/maximize), no GTK title
  bar, top area draggable via gesture-initiated surface move, laptop
  illustration, centered model/year, compact local chip/memory rows, More Info
  action, and regulatory footer. It caps at 72% of the reference design size
  and still scales down to monitor height on smaller displays.
- Config-driven GTK light/dark theme names and icon theme name, defaulting to
  MacTahoe for current testing.
- Theme assets are external to this compositor repo; installed GTK/icon theme
  names are selected through `orange.conf`.
- Persistent widget visibility and small/medium/large widget size settings.
- Light/dark shell appearance switching with `GTK_THEME`, `GTK_ICON_THEME`,
  and `ORANGE_APPEARANCE` environment export.
- Bundled GTK CSD theme CSS with traffic-light window control styling.
- Lazy freedesktop icon-theme lookup with inherited themes, `hicolor` fallback,
  semantic aliases, positive cache, and miss cache.
- Lazy wallpaper decode plus output-sized wallpaper cache.
- Widget registry layer with Calendar and Weather widgets pinned under client
  windows.
- Deterministic `.desktop` parser and right-side desktop shortcuts from
  XDG data directories.
- Desktop labels wrap and center, including `PDF Documents`.
- Dock Calendar icon day/date overlay uses current shell time.
- Dock indicator dots stay inside the glass container.
- Top menu bar background fill removed so it is transparent over wallpaper.
- Foreground visual testing checks context-menu glass/translucency and scaling
  without requiring a checked-in visual reference image.

### Cursor, Menus, And Orange Assets

#### Implemented

- Cursor theme and cursor size settings in `orange.conf`.
- GTK Settings app controls for cursor theme and cursor size.
- Config-driven `wlr_xcursor_manager` setup and `XCURSOR_*` environment export.
- `assets/` is now wallpaper-only; shell/Dock/status/desktop/menu icons come
  from configured icon themes.
- Generated 5120x3200 light/dark Orange wallpapers.
- Menu bar falls back to a text `O` if the configured icon theme has no
  `orange-menu` equivalent.
- Appearance-aware status icon tinting; status glyphs render light in dark mode.
- Status strip icons now pack leftward from measured clock text in fixed visual
  slots, including Wi-Fi, Sound, Battery, Spotlight/Search, and Control Center
  glyphs sourced from the configured icon theme.
- Tighter menu bar spacing.
- Compact Dock icon sizing and spacing to better match the latest Dock crop.
- Dock active indicators now reflect mapped/open client windows only.
- Desktop shortcut dragging with persisted `desktop_icon_N_position=x,y`.
- Right-click context menus for desktop shortcuts, widgets, Dock items, and
  empty desktop background.

#### Tests Added

- Cursor config load/save coverage.
- Persisted desktop icon position load/save coverage.
- Custom desktop position layout coverage.
- Context menu layout and hit-test coverage.

#### Tests Added

- Config load/save parser test.
- `.desktop` parser test.
- Shell dark render smoke test.
- Widget layer existence test.
- `.desktop`-required desktop icon layout test.
- Foreground context-menu glass/translucency and scaling test.

### macOS-Like Context Menus & System Menu

#### Implemented

- Full macOS-like system menu for Orange (10 items) with separator lines between groups:
  About Orange, System Settings..., App Store..., Recent Items,
  Force Quit..., Sleep, Restart..., Shut Down..., Lock Screen, Log Out.
- System actions: `systemctl suspend/reboot/poweroff`,
  `xdg-screensaver lock` for Lock Screen, `wl_display_terminate` for Log Out.
- Dock right-click context menu (5 items, 2 separators): Open,
  Show in Files, Remove from Dock, Open at Login, Dock Settings.
- Desktop background right-click context menu (8 items, 2 separators):
  New Folder, Get Info, Use Stacks, Sort By, Clean Up By,
  Show View Options, Change Desktop Background..., Edit Widgets.
- Widget right-click context menu (5 items, 2 separators): Edit Widget, Small,
  Medium, Large, Remove Widget.
- Desktop icon right-click context menu (9 items, 3 separators):
  Open, Show in Files, Copy, Get Info, Rename, Duplicate,
  Quick Look, Share, Move to Trash.
- `ORANGE_CONTEXT_MENU_WIDGET` enum for widget menus.
- `ORANGE_HIT_WIDGET` hit kind so widgets no longer behave like wallpaper.
- `ORANGE_CONTEXT_MENU_DESKTOP_ICON` enum for icon vs. background menus.
- `ORANGE_HIT_DESKTOP` hit kind for empty desktop background detection.
- Separator line support (`separator_before[]` arrays with flags in layout).
- Cursor-position-driven desktop background menu placement.
- Context menus and the system menu use the same glass material family as the
  Dock, with text and item geometry scaled from output resolution.
- Context menus and the system menu switch to a dark translucent material and
  light text in dark appearance mode.
- Status-area menu is modeled as a Control Center-style quick-control list:
  Wi-Fi, Bluetooth, AirDrop, Focus, Sound, Screen Mirroring, Display, Battery,
  Keyboard Brightness, and Control Center Settings.
- Desktop shortcuts clamp saved positions into the visible desktop and draw a
  visible generated fallback tile if the installed icon theme cannot resolve a
  usable icon.

#### Tests Added

- Dock context menu hit/layout coverage.
- Widget hit/menu coverage and hidden-widget hit exclusion.
- Desktop icon context menu (DESKTOP_ICON, item 0, 9 items, 3 separators).
- Desktop background context menu at cursor.
- Desktop background hit test (ORANGE_HIT_DESKTOP on empty desktop area).
- Config load/save coverage for widget visibility and size.
- Foreground visual test for context-menu glass/scaling.

## Current Work

### Active Feature

Desktop icons now show volumes (drives) instead of XDG `.desktop` entries,
matching macOS Finder behavior: the root filesystem and `/home` are discovered
from `/proc/mounts`, removable media and external volumes are monitored through
`GVolumeMonitor`. Icons are laid out on a snap-to-grid with configurable
spacing (`desktop_grid_spacing`) and per-volume visibility toggles in Settings.

The menu bar has been given the same glass translucency as the Dock using the
shared `draw_dock_glass` function, with the Apple icon enlarged to 40×40 and
menu text baselines adjusted for a more macOS-like appearance. Status icons are
drawn in original icon-theme colors (no tinting) and clicking them no longer
opens any menu — each icon slot is reserved for its future feature.

`assets/` remains wallpaper-only, GTK/icon theme names are config-driven, theme
payloads are not stored under a repo `themes/` directory, and shell icons
continue to come from installed freedesktop icon themes.

### Progress

Current validation passes:

- `ninja -C build`
- `meson test -C build --print-errorlogs`
- `./build/test-assets`
- `./build/test-shell-visual` covers volume desktop icon drawing.
- `./build/test-shell-layout` covers snap-to-grid, minimum slots, volume
  positioning, and resolution scaling.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf /tmp/orange-shell.png`
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --foreground-only /tmp/orange-shell-foreground.png`
  passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once
  --width 1440 --height 900 --assets assets --config orange.conf`
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once
  --width 1440 --height 900 --assets assets --config orange.conf`

Core desktop icons are now volume-driven with snap-to-grid, drag-to-reorder,
and a macOS-style desktop icon context menu (Open, Get Info, Eject) plus
background context menu (Clean Up, Sort By, Show View Options, etc.).

Volume detection combines `/proc/mounts` for fixed filesystems with
`GVolumeMonitor` for removable drives, deduplicating by mount path. The
Settings app includes per-volume visibility toggles, Sort By dropdown, and
Label Position dropdown. Grid layout respects `desktop_grid_spacing` and
occupies a minimum 2×2 slot area even without volumes.

### Top-Left App Menu

The menu-bar app title is no longer a static `Files` placeholder. Shell state
now carries an active app label from the focused view, resolved through Dock
metadata when possible and falling back to app ID/title text. The menu bar now
lays out the active app title plus File, Edit, View, Go, Window, and Help as
separate hit targets. Clicking or right-clicking each tab opens a shell-rendered
menu anchored beneath that tab.

Generic app commands are routed to the focused client through standard keyboard
accelerators where xdg-shell gives Orange a real mechanism: New/Open/Close/
Save/Save As/Print, Undo/Redo/Cut/Copy/Paste/Select All/Find, Zoom, Full
Screen, Help, and Quit. Window menu commands use compositor behavior where
available. Arbitrary native app menu-tree extraction remains future work behind
a DBusMenu/global-menu backend, because xdg-shell does not expose app menu
models or native menu callbacks.

The status-side Control Center slot and quick-controls menu were restored so
the existing macOS-like status tests pass again.

Validation from this pass:

- `ninja -C build` passed.
- `./build/test-shell-layout` passed.
- `meson test -C build --print-errorlogs` passed 6/6 tests.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --foreground-only --active-app Photoshop --context-menu
  app /tmp/orange-shell-photoshop-app-menu.png` passed.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --foreground-only --active-app Photoshop --context-menu
  app-file /tmp/orange-shell-photoshop-file-menu.png` passed and visually shows
  `Photoshop File Edit View Go Window Help` with File menu entries anchored
  under the File tab.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once
  --width 1440 --height 900 --assets assets --config orange.conf` passed,
  with expected sandbox dconf/DBus warnings.

### Dock Icon Alias Fix

Completed a focused Dock icon fix for the default Image Viewer, Notes, and
Video Player launchers. The Dock now requests configured role icons before
scanned `.desktop` icon fields, using `image-viewer` and `video-player` for
MacTahoe-style Image Viewer/Video Player artwork while keeping `org.gnome.Notes`
for the Notes role. Generic image/video/text aliases continue to resolve
through their own fallback lists.

Validation:

- `ninja -C build test-assets test-shell-visual`
- `./build/test-assets`
- `./build/test-shell-visual`
- `meson test -C build --print-errorlogs`
- `ninja -C build orange-render-shell`
- `./build/orange-render-shell --width 1440 --height 900 --assets assets --config orange.conf /tmp/orange-assets-icons-check.png`

### Orange Settings App

Reworked `orange-settings` from a single scrolling form into a macOS-like
System Settings shell: full-height translucent rounded sidebar well with drop
shadow, About-style Cairo-drawn traffic-light window controls, rounded/clipped
resizable app frame, rounded back/forward arrow capsule, searchable local
account
sidebar, Appearance-first landing pane, grouped rounded settings cards, and
separate panels for the visible macOS-style categories. The Appearance pane now
persists accent color, text highlight mode, icon/widget style, folder color
mode, sidebar icon size, wallpaper tinting, and scroll-bar visibility alongside
the existing light/dark, GTK theme, icon theme, desktop, Dock, widget, and
cursor settings. Settings now applies the same dark-mode detection order as
About Orange (`ORANGE_APPEARANCE`, `GTK_THEME`, GTK dark preference, then local
config) and uses dark colors from the About palette.

The launcher argument warning is fixed by keeping `orange.conf` as the app's
own config-path argument while only passing the executable name through
`g_application_run()`, so GTK no longer treats the config file as a document to
open.

Validation:

- `ninja -C build orange-settings test-config`
- `./build/test-config`
- `git diff --check`
- `ninja -C build`
- `meson test -C build --print-errorlogs` still reaches the same two known
  non-settings failures listed below.

### GTK Theme Rounding, Files Icons, And Cursors

About Orange and Orange Settings no longer hardcode a top-level app-window
corner radius. Their custom root widgets inherit the GTK window theme's
computed radius while keeping transparent app windows and clipped custom
content.

The compositor now propagates the configured GTK theme, icon theme, cursor
theme, and cursor size to `org.gnome.desktop.interface` when a GNOME settings
schema and session bus are available, so GTK apps such as Files can pick up the
same theme/icon/cursor settings. Orange still exports `GTK_THEME`,
`GTK_ICON_THEME`, `XCURSOR_THEME`, and `XCURSOR_SIZE` for launched clients.
If `cursor_theme` is empty, Orange now falls back to the configured
`icon_theme` as the cursor theme, which lets local icon themes such as
MacTahoe provide their bundled `cursors/` directory without duplicating config.
`XCURSOR_PATH` is initialized to the standard user and system icon roots when
unset.

Orange's own icon cache is reloaded when `icon_theme` changes. Asset coverage
now includes resolving folder icons from a theme `places/scalable` directory,
matching where themes commonly store Files/folder artwork.

Validation:

- `ninja -C build orange orange-about orange-settings test-assets test-config`
- `./build/test-assets`
- `./build/test-config`
- `git diff --check`
- `meson test -C build --print-errorlogs` still reaches the same two known
  local failures in `shell-layout` and `shell-visual`.

### Dock Glass And Files-First Defaults

The Dock now defaults to Files first and Launchpad second, matching the checked
in `orange.conf` order. Dock glass samples and scales the existing framebuffer
behind the Dock before applying a lighter translucent shade, with tighter
horizontal padding, a slimmer trash separator, larger running indicators, and
updated tests for the reordered Launchpad hit target and role-icon lookup.
Light/dark wallpapers were regenerated for the new default appearance pass.

Menu bar and Dock share the same `draw_dock_glass` rendering path for
consistent translucency. Dock outer padding = 22, corner radius = 44 × s,
top padding = 16, bottom padding = 10, bottom margin = 8. Menu bar height =
48px, Apple icon 40×40 at y = scaled_i(4, s), text baseline at
scaled_i(36, s). Status icons render with original icon-theme colors
(no white tinting) and no context menu on click.

### Remaining Work

Interactive testing under WSLg or a nested Wayland session is still needed.
WSLg should use `WLR_BACKENDS=wayland WLR_RENDERER=pixman`; forcing wlroots
Vulkan there can fail with `Cannot create Vulkan renderer: no DRM FD available`
because the nested backend lacks the DRM render-node FD wlroots expects. GTK
utility launch commands set `GSK_RENDERER=cairo` to avoid GTK's WSLg EGL/Zink
path.
Interactive nested Wayland runs are expected to keep running after logging
`running on Wayland display wayland-N`; use `--headless --once` for startup
tests that should exit by themselves.
Pixel-level matching depends on the ignored private user assets. Automated visual
coverage intentionally avoids checked-in reference-image comparison.
More faithful behavior would require real app/menu integration, richer
animation, and full desktop services.

Individual status-icon features (Wi-Fi dropdown, volume slider, search, etc.)
still need to be wired up.

---

## Next Actions

1. Wire up individual status-icon features (Wi-Fi, Sound, Battery, Spotlight,
   Control Center).
2. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/orange`.
3. Test Dock and desktop interactively under WSLg/nested Wayland.
4. Keep Orange GTK/icon theme assets in a separate project and install them
   into normal user/system GTK and icon theme directories for testing.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Proprietary third-party assets are not included. GTK4 development headers are
installed in this environment now, and both `orange-settings` and `orange-about`
build here. They remain conditional for systems without GTK4 development files.

### Fixed

- **Vulkan dependency made optional**: The `vulkan` dependency in `meson.build`
  was hard-required, preventing builds on systems without the Vulkan SDK.
  Changed to `required: false` with a `ORANGE_HAVE_VULKAN` preprocessor guard
  around the `wlr_renderer_is_vk()` runtime check. The compositor now builds
  and runs with any renderer (Pixman, GLES2, Vulkan) without requiring the
  Vulkan SDK at build time.
- **Cursor theme applied on output creation**: `server_apply_cursor_config()`
  was called before `wlr_backend_start()` when no outputs exist, making
  `wlr_cursor_set_xcursor()` a no-op. The cursor theme now gets applied:
  (1) after backend start so all initial outputs receive it, and
  (2) in `handle_new_output()` so hotplugged outputs also get the theme.
- **xdg-shell capability assertion fixed**: GTK4 clients can bind xdg-shell v3,
  while `wlr_xdg_toplevel_set_wm_capabilities()` requires the toplevel
  capability protocol version. Orange now creates xdg-shell v5 and only advertises
  window-manager capabilities when the client resource version supports them.
- **GTK utility controls moved left**: Orange's GTK4 utility apps request
  `close,minimize,maximize:` through GTK decoration layout; the Settings app
  uses a GTK headerbar with left-aligned decoration layout, while the About app
  uses custom macOS-like traffic-light buttons (no GTK title bar) with
  gesture-driven window move for a more native appearance.
- **GTK utility sizing made monitor-aware**: The About app scales its compact
  reference layout to the current monitor (up to 2x on high-resolution displays)
  instead of opening too tall on small WSLg sessions, and Settings clamps its
  default size to the monitor bounds.
- **Custom-argument startup validation added**: Meson now runs
  `headless-custom-startup` with `--headless --once --width 1440 --height 900`
  and explicit asset/config/desktop/theme arguments. Output initialization uses
  wlroots' expected render-init-before-commit order and falls back to fixed
  output modes when a backend rejects the requested custom size.
- **About app portrait layout matched**: The About Orange window now uses the
  tall portrait reference proportions, with the laptop, title, local hardware
  rows, More Info button, and footer spaced vertically like the supplied
  screenshot. Its custom traffic-light buttons use 16px button geometry, a 23px
  step, active red/gray/gray colors, no hover effect on the gray buttons, gray
  inactive/backdrop state, and MacTahoe dark GTK colors.
- **About hardware rows made local**: The About app reads the local CPU label
  from `/proc/cpuinfo`, memory from `sysinfo(2)`, ellipsizes long chip names,
  and removes the serial-number row.
- **Resolution-scaled shell surfaces tightened**: Menu bar, Dock, desktop
  icons, system menu dropdowns, and context menus are covered by layout tests
  comparing 1440x900 and 2880x1800 geometry.
- **Dock drag blank slot fixed**: Visible Dock order entries are normalized to
  the launcher count, drag insert targets are clamped to visible items, and the
  layout no longer creates zero-size invisible Dock slots.
- **Dock hover and context menu behavior matched closer to macOS**: Hover uses
  icon magnification plus a label bubble instead of a circular halo, and Dock
  context menus are placed above the Dock container so the Dock glass no longer
  cuts off the menu.
- **Output swapchain pressure reduced**: Output frames are not committed while
  a previous commit is still pending presentation; dirty shell updates schedule
  the next frame once the backend presents.
- **Dock launcher icon regression fixed**: `view-app-grid` no longer aliases to
  `start-here` first. The app launcher and the menu icon now use separate
  freedesktop alias chains so local macOS-style icon themes can provide a
  Launchpad-style Dock icon and a separate menu/start icon.
- **GNOME-only settings fallback removed**: Orange no longer launches
  `gnome-control-center` from shell fallback commands, avoiding the
  "only supported under GNOME and Unity" exit path when running inside Orange.
  Settings fallbacks now prefer `orange-settings`, KDE/Xfce/MATE settings
  shells, or standalone tools such as `nm-connection-editor`, `pavucontrol`,
  `blueman-manager`, `wdisplays`, and `arandr`.
- **macOS-like Notification Center and status item behavior added**: The
  date/time item opens a right-edge Notification Center card stack with missed
  notification cards, Calendar/Screen Time/Weather widget cards, and an Edit
  Widgets button that opens Orange Settings. Wi-Fi, Sound, and Battery now have
  separate hit targets and item-specific status menus; Search opens the app
  picker; Control Center opens the existing quick-control menu.
- **Notification/status validation added**: `test-shell-layout` covers status
  item hit targets, item-specific status menus, Notification Center layout,
  Edit Widgets hit testing, scaling, and the absence of
  `gnome-control-center` in Dock fallback commands. `test-shell-visual` checks
  Notification Center panel/card/button rendering. `orange-render-shell` now
  supports `--notification-center` and `status-wifi` context-menu rendering.
- **Settings theme dropdowns and themed sidebar icons added**: Orange Settings
  now uses dropdowns for installed GTK, icon, and cursor themes, preserving a
  saved custom value when it is not discoverable. The Settings sidebar renders
  direct `GtkImage` icons from the active GTK icon theme instead of local
  color-backed symbolic tiles, with fallback names for common freedesktop and
  Adwaita/MacTahoe icon names. Cursor theme discovery now also normalizes
  `XCURSOR_PATH` entries that point directly at a theme directory or its
  `cursors/` directory, expands `~/...` entries, and checks local cursor roots
  such as `~/.local/share/icons`, `~/.icons`, `~/.local/share/cursors`, and
  `~/.cursors`.
- **Dock glass and default order tuned**: Dock rendering now includes a
  downsampled framebuffer backdrop behind the translucent shade, the checked-in
  defaults place Files before Launchpad, role icons are preferred before
  `.desktop` icon fields for Image Viewer and Video Player, and shell layout
  and visual tests cover the updated behavior.

#### Latest Validation

- `ninja -C build` passed.
- `meson test -C build --print-errorlogs` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once --width 1440 --height 900 --assets assets --config orange.conf` passed; DBus status probes logged sandbox connection errors but did not fail startup.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets --config orange.conf --notification-center /tmp/orange-notification-center.png` passed and was visually inspected.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets --config orange.conf --foreground-only --context-menu status-wifi /tmp/orange-status-wifi.png` passed and was visually inspected.

#### Current GTK Settings Validation

- `ninja -C build orange-settings orange-about` passed for GTK theme dropdown,
  icon theme, and CSD/icon changes.
- `ninja -C build orange-settings` passed after switching the Settings sidebar
  to direct icon-theme images.
- `ninja -C build orange-settings` passed after tightening local cursor theme
  discovery.
- `ninja -C build` passed.
- `git diff --check -- src/settings_app.c` passed.
- A later Dock/default-order validation pass updates the stale shell assertions
  and returns the full Meson suite to green.

### Technical Concerns

wlroots 0.17 APIs are unstable and may need porting for newer distros. Vulkan
renderer availability depends on the local GPU/driver and wlroots runtime
support. In this sandbox, `WLR_BACKENDS=headless WLR_RENDERER=vulkan` fails
because wlroots has no DRM FD in headless mode.
The same `no DRM FD available` failure can occur in WSLg with the Wayland
backend even when the Windows host has a discrete GPU, because WSLg GPU
acceleration does not guarantee the DRM render-node path wlroots Vulkan needs.
The committed procedural background fallback and any missing local PNGs are not
pixel-identical to proprietary third-party assets; exact local visual matching
depends on ignored asset overrides.

---

## Resume Instructions

Continue from the committed implementation. Target wlroots 0.17.1, keep private
wallpapers and local theme experiments ignored, validate with
`meson test -C build --print-errorlogs` and
`WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once`,
plus the custom-size one-shot command documented in README, then test
interactively under WSLg or nested Wayland.
