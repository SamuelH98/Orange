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

### Active Feature: macOS 26 Tahoe Spotlight Redesign

The launcher/Spotlight overlay has been redesigned to match the macOS 26 Tahoe
Spotlight appearance:

- **Floating search bar**: A centered glass pill with magnifier icon,
  "Spotlight Search" placeholder, and blinking caret on focus. No panel
  backdrop — the bar floats directly on the wallpaper with the same
  translucent glass material as the Dock.
- **Four mode buttons**: Circular glass buttons to the right of the search
  field with hand-drawn icon glyphs (compass=Apps, folder=Files,
  tools=Actions, clipboard=Clipboard). Active button gets a lighter highlight.
  Same glass material as the Dock.
- **Category tabs**: Horizontal row of pill-shaped category filter buttons
  (Utilities, Productivity & Finance, Social, Entertainment, Photo & Video,
  Information) below the search row in Apps mode with no query. Same glass
  material as the Dock.
- **Mode-specific content views**:
  - Apps mode: Scrollable 4-column app icon grid with hot selection highlight,
    centered labels wrapped to two lines, icon shadows, and rounded corners.
  - Files mode: Placeholder with "Recent Files" header.
  - Actions mode: Placeholder with "Actions" header for future Quick Keys.
  - Clipboard mode: Placeholder with "Clipboard History" header.
- **No panel backdrop**: The old Liquid Glass panel has been removed. The search
  bar and content float directly on the dimmed wallpaper, matching the macOS 26
  Spotlight style.
- **Keyboard shortcuts**: Cmd+1/2/3/4 switches between browse modes. Up/Down
  with scroll, Left/Right for adjacent selection. Escape closes, Enter launches.
- **render-shell support**: Added `--launcher`, `--launcher-mode N`, and
  `--launcher-query TEXT` flags for visual testing. Category tabs render in
  Apps mode by default.

All six tests pass. `orange-render-shell` renders the launcher overlay at every
mode with correct geometry.

### Progress

Current validation passes:

- `ninja -C build`
- `meson test -C build --print-errorlogs` (6/6 tests)
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --launcher /tmp/orange-tahoe-spotlight.png`
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --launcher --launcher-mode 1
  /tmp/orange-tahoe-files.png`
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --launcher --launcher-mode 2
  /tmp/orange-tahoe-actions.png`
- `./build/orange-render-shell --width 1440 --height 900 --assets assets
  --config orange.conf --launcher --launcher-mode 3
  /tmp/orange-tahoe-clipboard.png`
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once
  --width 1440 --height 900 --assets assets --config orange.conf`

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
  defaults place Files before Launchpad, and shell layout and visual tests cover
  the updated behavior.
- **Dock app icons now follow installed desktop entries**: Dock icon selection
  now uses installed `.desktop` `Icon=` fields before Orange role fallbacks, and
  the asset resolver tries exact icon names before semantic aliases. Icon theme
  index parsing now handles Ubuntu's long hicolor `Directories=` lines and the
  large hicolor directory list, so installed GNOME app icons in
  `hicolor/scalable/apps/org.gnome.*.svg` resolve instead of falling through to
  the generic executable icon. Snap and Flatpak export roots are scanned for
  desktop entries and icon themes even when the session omits them from
  `XDG_DATA_DIRS`; dock IDs such as `firefox.desktop` match exported IDs such
  as `firefox_firefox.desktop` or `org.mozilla.firefox.desktop`; and absolute
  desktop-entry icon paths are covered by tests for Snap-style `Icon=/...`
  values. The compositor also refreshes XDG desktop entries every few seconds
  so newly installed dock apps can update without restarting Orange. Launching
  the installed GNOME Settings desktop entry now sets `XDG_CURRENT_DESKTOP=GNOME`
  for `gnome-control-center`, avoiding its GNOME/Unity guard when launched from
  the Dock.
- **Local dock GNOME apps installed through apt**: Ubuntu 24.04 packages were
  installed for `gnome-calendar`, `gnome-contacts`, `gnome-software`, and
  `loupe`. The configured apt sources did not provide packages or desktop
  entries for `org.gnome.Showtime.desktop` or `org.gnome.Decibels.desktop`.

#### Latest Validation

- `ninja -C build` passed after desktop-entry refresh and icon precedence
  changes.
- `./build/test-assets`, `./build/test-shell-visual`, and
  `./build/test-shell-layout` passed.
- `meson test -C build --print-errorlogs` passed.
- `./build/orange-render-shell /tmp/orange-shell.png` passed after installing
  the available GNOME dock apps.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets --config orange.conf /tmp/orange-shell-maps-fixed.png`
  passed and the rendered Dock shows the installed Maps map-pin icon.
- `./build/orange-render-shell --width 1440 --height 900 --assets assets --config orange.conf /tmp/orange-shell-firefox-fixed.png`
  passed and the rendered Dock shows the Snap-installed Firefox icon.
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
