# PROJECT_STATE

## Project Overview

### Project Name

Orange

### Goal

Build Orange, a C/wlroots Linux compositor that follows the supplied
macOS-like desktop reference and behaves like a usable macOS-like shell,
with optional local private asset overrides and Vulkan renderer support through
wlroots.

### Current Status

Functional macOS-like shell compositor with desktop files and volume icons,
Dock-aware two-axis desktop grid layout with snap-to-grid positioning,
Finder-like image-file desktop previews, desktop marquee selection, translucent menu
bar with dock-style glass effect, launcher glass matching Dock/menu/context
transparency, compact Dock with even glass transparency,
wlroots compositor, scalable widgets, basic xdg-shell window management, Dock
launchers, keyboard shortcuts, cursor customization, GTK/icon theme
configuration, lazy freedesktop icon lookup, scoped Wayland/freedesktop spec
compliance documentation, PNG render export, foreground-only visual smoke
coverage, and headless one-shot validation.

---

## Completed Features

### Project Planning

#### Validation

Local dependency versions were checked with `pkg-config`, `meson`, `ninja`, and
`cc`.

#### Tests Added

`tests/test_shell_layout.c` covers Dock/menu/desktop hit testing, resolution
scaling, and a render smoke test.

### Functional Compositor

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

### Shared Glass Renderer & Bounded Overlay

#### Implemented

- Shared `orange_glass_draw()` in `src/glass.c`/`include/orange/glass.h` used by
  Dock, menu bar, menu panels, launcher/search surfaces, and shell liquid panels
  instead of local duplicate blur code.
- Glass blur uses a sliding-window box blur with downscaled backdrop, upscaled
  to device resolution for clean rounded-edge antialiasing, with padded backdrop
  sampling and radius-zero rectangle fallback.
- Overlay buffer separation: transient launcher/menu/notification UI draws only
  in a transparent overlay scene buffer; base shell buffer skips transient
  overlays. Overlay clears when no transient surface remains active.
- Overlay glass renders against the current base shell buffer before unblending
  back to a transparent layer, so launcher/search glass opacity matches
  Dock/menu-bar glass.
- Conservative overlay bounds computed from the union of all active transient
  rects; compositor clears, redraws, unblends, and damages only that union
  instead of the full output.
- Overlay-only dirty marking for launcher/menu/notification open, close, drag,
  scroll, and keyboard navigation paths avoids unnecessary wallpaper, widget,
  desktop icon, and Dock redraws.

#### Bug Fixes

- Fixed stale launcher/menu pixels by clearing and damaging the overlay buffer
  when no transient overlay is active.
- Fixed first-use menu/dropdown lag by warming the small menu-bar/system-menu/
  status icon set after asset loading, caching icon lookup misses during preload,
  and probing the discovered theme path first before scanning secondary XDG icon
  bases for the same theme name.
- Replaced the old Cairo blur pass with a real sliding-window blur for direct
  CPU cost reduction on context menus, dropdowns, launcher glass, and Dock glass.
- Restored dropdowns, context menus, and launcher surfaces to the shared base
  glass material used by the Dock and menu bar instead of the brighter raised
  panel material, preventing the overlay buffer from producing a washed-out
  white source layer on light backdrops.
- Overlay glass now captures a bounded composed-scene backdrop with the overlay
  node hidden before drawing transient menus/launcher surfaces, so blur samples
  desktop shell content and client app windows underneath the menu. If readback
  is unavailable, it falls back to the shell buffer backdrop.

#### Tests Added

- Base shell transient overlay suppression test (`skip_transient_overlays`).
- Overlay clear test (draw with no active overlay produces transparent buffer).
- Menu overlay bounds are tight and exclude most of the output.
- Backdrop overlay composition matches direct launcher rendering to within
  3 color values for full and search-only launcher modes.
- Split-theme icon lookup test in `test_assets`.
- Icon warm/preload test in `test_assets`.

### macOS 26 Tahoe Search And Launcher Refinement

#### Implemented

- **Compact Search pill**: Menu-bar Search and Help search open a centered
  Spotlight-style glass pill with four adjacent round mode buttons. Typing
  stays in the pill and does not expose hidden launch results.
- **First-button transform**: Clicking the first adjacent mode button transforms
  the pill into a smaller glass app-launcher panel anchored to the same
  search/header position.
- **Smaller app launcher**: Dock launcher and Super+Space open the centered app
  launcher panel directly at a reduced scale.
- **Draggable placement**: Compact pill and expanded/full launcher panels can be
  dragged by the search/header area. Drag position is session-local and clears
  when the launcher closes.
- **Header/icons**: Left launcher icon renders as a normal themed icon. Compact
  mode uses adjacent circular themed icon buttons. Right-side options affordance
  uses a themed horizontal-more icon with a drawn fallback.
- **Filters**: Compact pill filters (`All`, `Utilities`, `Productivity`,
  `Social`, `Media`, `Info`) appear in the full Apps launcher and use parsed
  `.desktop` `Categories=` metadata to filter the app list.
- **Scrollable app grid**: Five-column theme-icon grid, white labels, no
  generated gray icon backplates, tight side/bottom insets, label ellipsizing
  for long app names, and scrollbar tied to row-scroll state.

#### Tests Added

- Full launcher layout scrolls all apps, hit-tests app cells, categories, and
  scrollbar.
- Search mode layout transforms to overlay on mode-button click.
- Category filter coverage for Utilities/Productivity/Media/Social groups.

### Responsive Dock Dragging & Separator Controls

#### Implemented

- Dock drag feedback uses the overlay buffer: moving icon ghost and Remove
  affordance draw over the active damage union; base Dock redraws only when the
  insertion target changes.
- Reordering uses icon displacement/gaps instead of a blue insertion line.
  Launcher-to-Dock drags part the Dock around the target slot.
- Dragging a removable Dock app off the Dock shows a `Remove` bubble; release
  removes only the Dock alias without uninstalling the app.
- Apps can be dragged from the launcher onto the Dock to create a new alias.
  Duplicate aliases are rejected; launcher and Trash are permanent.
- Remove/reorder/insert commands share centralized Dock config mutation helpers
  that compact `dock_apps` and reset visible order consistently.
- Right-clicking the Dock separator opens a Dock-wide glass bubble context menu
  with Magnification On/Off, Dock Size (small/medium/large), Position on Screen
  (bottom/left/right), Minimize Using (Genie/Scale), and Dock Settings.
- Magnification toggles and Dock size/position/minimize-effect choices persist
  to `orange.conf`.
- xdg-toplevel minimize requests hide the view, clear focus/grabs, keep the app
  indicated as open in the Dock, and restore the minimized view on Dock launch.
- Side Dock positions (left/right) with vertical item geometry, rotated separator
  drawing, side-aware drag insertion, and maximize bounds that reserve the Dock
  strip.
- Shared Dock-aware work-area rectangle for app placement, dragged windows,
  maximized windows, fullscreen work areas, and dragged desktop icons.

#### Tests Added

- Dock separator hit target and context-menu geometry.
- Left/right Dock position layout, work-area clamping, and context-menu placement.
- `dock_position`/`minimize_effect` config round-tripping.
- Dock alias removal compacts `dock_apps` and protects launcher/Trash.
- Launcher-to-Dock insertion clamps before Trash, rejects duplicates.
- Dock reorder keeps permanent items fixed.

### Desktop Entry Categories

#### Implemented

- `.desktop` parser now reads `Categories=` field and stores it in the entry
  struct. Categories filtering via `orange_launcher_filter()` for launcher
  category tabs.

#### Tests Added

- Desktop entry parser test covers `Categories=` field.
- `test_launcher_category_filter` covers category-based filtering.

### Dock Bounce Animation

#### Implemented

- Per-app icon bounce animation when a Dock item is clicked to launch, matching
  the macOS behavior. The icon performs three diminishing hops (upward Y offset
  with decaying amplitude) over 800ms.
- Bounce uses a decaying sine wave per hop: amplitudes 1.0, 0.50, 0.18 for the
  three bounces, 22px initial amplitude at 1x layout scale, scaled by layout
  scale factor.
- The bounce is driven through the present handler: each output present event
  checks if the bounce is still active and re-arms a shell redraw if so, so
  the animation runs at the display's native refresh rate without a separate
  animation timer.
- Config key `animate_opening_applications=true|false` (default true) controls
  the feature, matching the macOS "Animate opening applications" setting.
- Built-in permanent Dock items (launcher, Trash) never bounce.

#### Tests Added

- `test_dock_bounce_offset` in `test_shell_layout` verifies bounce starts at 0,
  goes negative (upward) mid-animation, returns to 0 after duration, stays 0
  for inactive bounces.
- Config round-trip test for `animate_opening_applications`.

### Validation

- `ninja -C build` and `meson test -C build --print-errorlogs` (6/6) all pass.
- Bounce offset math verified by unit test.

## Current Work

- Desktop icon placement, image previews, marquee selection, multi-selection
  context menus, real drive labels, GNOME Files Trash opening, and the Nautilus
  Settings-portal crash fix are complete.
  Continue with interactive WSLg/nested-Wayland verification and then resume
  status-icon feature work as needed.

### Dark/Light Appearance Advertising

- Written `color-scheme` to GSettings `org.gnome.desktop.interface` schema so
  GTK4/libadwaita apps (Firefox, GNOME apps) receive the correct theme hint.
- Written `gtk-application-prefer-dark-theme` and `color-scheme` to
  `~/.config/gtk-3.0/settings.ini` and `~/.config/gtk-4.0/settings.ini` at
  runtime so GTK3 apps started from the compositor session (including Firefox
  in GTK fallback mode) receive the dark/light preference through the
  traditional GTK settings file path.
- Orange now also reads `org.gnome.desktop.interface color-scheme` while
  running so GNOME Settings' Style selector can change Orange between
  light/default and dark appearance. The compositor pumps the default GLib
  context from its timer so GSettings notifications are serviced without a
  separate GLib main loop.
- Implemented `org.freedesktop.impl.portal.Settings` D-Bus backend at
  `/org/freedesktop/portal/desktop` with bus name
  `org.freedesktop.impl.portal.desktop.orange`. The backend responds to `Read`
  and `ReadAll` calls for `org.freedesktop.appearance.color-scheme` and emits
  `SettingChanged` signals when the appearance toggles at runtime. This lets
  Firefox and other portal-aware clients receive the color-scheme through the
  xdg-desktop-portal chain.
- `GTK_THEME`, `GTK_ICON_THEME`, and `ORANGE_APPEARANCE` environment variables
  continue to be exported for apps started from the compositor session.
- Extended the Settings portal surface to also answer the
  `org.freedesktop.portal.Settings` frontend interface, including `Read`,
  `ReadOne`, `ReadAll`, the `version` property, and `SettingChanged` for the
  `org.freedesktop.appearance.color-scheme` key.

### GNOME Control Center And Wallpapers

- Settings, status-panel actions, GNOME Settings desktop entries, and the
  desktop "Change Desktop Background..." action now launch
  `gnome-control-center` through a GNOME/Unity-compatible environment wrapper
  when it is installed, with KDE/Xfce/MATE/standalone fallbacks retained.
- The wrapper advertises `XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu`,
  `XDG_SESSION_DESKTOP=gnome`, and `DESKTOP_SESSION=gnome`, and unsets forced
  GTK theme environment variables so GNOME Settings follows system appearance.
- The compositor reads `org.gnome.desktop.background` at startup and polls it
  while running. Light/dark picture URIs, picture options, opacity, solid
  colors, and gradient colors feed the wallpaper asset cache.
- The asset layer can compose GNOME-selected wallpapers before shell rendering,
  including `none`, `wallpaper`, `centered`, `scaled`, `stretched`, `zoom`, and
  `spanned` placement. Bundled Orange wallpapers remain the fallback when GNOME
  background settings are unavailable.
- Light-mode system menus, context menus, and dropdowns now use white text,
  white separators, and white-tinted icons, matching the dark-mode transient
  menu foreground treatment.

---

## Next Actions

1. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/orange`.
3. Test Dock and desktop interactively under WSLg/nested Wayland.
4. Keep Orange GTK/icon theme assets in a separate project and install them
   into normal user/system GTK and icon theme directories for testing.
5. Verify dark/light follow-through with Firefox (appearance set to "System"),
   GTK4 apps (GTK_THEME / GSettings path), and portal-aware clients.

---

## Risks

### Open Questions

None blocking for the compositor.

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
  separate hit targets and item-specific status menus; Search opens the compact
  Spotlight-style pill; Control Center opens the existing quick-control menu.
- **Notification/status validation added**: `test-shell-layout` covers status
  item hit targets, item-specific status menus, Notification Center layout,
  Edit Widgets hit testing, scaling, and the absence of
  `gnome-control-center` in Dock fallback commands. `test-shell-visual` checks
  Notification Center panel/card/button rendering. `orange-render-shell` now
  supports `--notification-center` and `status-wifi` context-menu rendering.
- **Notification Center now uses real DBus notifications**: The compositor owns
  the freedesktop `org.freedesktop.Notifications` service on the session bus,
  stores recent notifications in an in-memory capped store, supports
  replacement and close requests, emits `NotificationClosed`, and renders
  captured app notifications above the widget cards instead of hard-coded
  placeholder notification text.
- **Overlay glass backdrop includes client windows**: Overlay glass rendering
  now builds an offscreen backdrop from the base shell buffer plus visible
  client scene buffers below the overlay when import/readback is available.
  This fixes menu/Notification Center glass over app windows sampling only the
  wallpaper/shell backdrop, with a shell-only fallback for unsupported
  renderer/buffer combinations.
- **Menu popover glass strengthened over app content**: System and context
  menus now use the raised glass panel material instead of bare low-opacity
  glass, and light menus use dark text/icons. This prevents high-contrast app
  UI underneath a menu from remaining readable through the overlay.
- **Launcher glass strengthened and invalidated on client repaint**: Full
  launcher panels, compact search fields, and launcher mode buttons now use the
  same raised glass panel material as menu popovers. When a mapped client
  commits a new buffer while a backdrop-dependent overlay is visible, the
  compositor marks the overlay dirty so the glass backdrop is refreshed against
  the current app content.
- **Overlay glass keeps material alpha over light apps**: Transparent overlay
  unblending now renders a separate alpha mask for launcher/menu/notification
  overlays. This prevents white or near-white app content from canceling out
  the panel source alpha, matching macOS-style glass behavior where the app
  behind the UI is frosted instead of readable through the panel.
- **Widget data and rendering split out of shell.c**: Calendar, Screen Time,
  and Weather widgets now render through `src/widgets.c`, while
  `src/widget_data.c` gathers real local data. Calendar reads GNOME/Evolution
  `.ics` files, Screen Time uses systemd-logind session start when available
  with an Orange-session fallback, and Weather reads a local weather file plus
  GNOME Weather's configured location instead of showing fixed demo values.
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
- **Dock and menu-bar right-click hit testing fixed**: Empty Dock glass and
  empty menu-bar chrome now return shell chrome hit kinds instead of empty
  desktop hits, preventing right-clicks through those surfaces from opening the
  desktop context menu.
- **Running Dock menus added**: Dock item right-click dispatch now selects a
  running-app context menu when the launcher is open. The menu adds Show All
  Windows, Hide, and Quit while preserving existing file/settings actions.
  Show All Windows raises/unminimizes matching toplevels, Hide minimizes them,
  and Quit sends xdg-toplevel close requests.
- **Side Dock bounce direction fixed**: Dock opening bounce now maps the same
  waveform to edge-aware displacement: up for bottom Dock, right for left Dock,
  and left for right Dock.
- **Modern macOS-like menu pass completed**: System/app menu dropdowns and
  regular context menus for Dock items, the Dock separator, widgets, desktop
  background, desktop files/volumes, and desktop app icons are now text-first
  command menus with separator grouping, trailing shortcut/detail text, and
  drawn checkmarks for selected/toggled state. Status/Control Center menus
  remain icon-leading. Native imported app-menu section separators are
  preserved. Desktop background menu actions now update real local state:
  Use Stacks toggles config, Sort By cycles the implemented None/Name/Kind
  modes, and Clean Up By clears saved manual positions to snap icons back to
  the grid.
- **Desktop item and Dock built-in menus completed**: Desktop file context
  menus now expose functional Open, Show in Files, Copy, Get Info,
  Rename/select, Duplicate, Quick Look/open, Share, and Move to Trash actions.
  Desktop files and volumes open from left-click using the sorted layout item
  metadata, drag positions persist across the full 64-item desktop capacity,
  and file/volume shortcut menus open at the cursor instead of over labels.
  Dock built-ins now have distinct menus: Launchpad shows Open Launchpad and
  Dock Settings, while Trash shows Open Trash, Empty Trash, and Dock Settings.
  The normal Dock menu width and draw clipping were expanded so `Remove from
  Dock` no longer clips.
- **Desktop icon placement and previews fixed**: Desktop item layout now
  computes after Dock geometry, snaps dragged positions in both axes across the
  usable work-area grid, reserves scaled clearance from bottom/left/right Docks,
  enforces a wider minimum cell gap, and resolves duplicate saved cells to the
  nearest free slot. Image desktop files render decoded Finder-like previews
  with themed icon fallback. Empty desktop dragging draws a marquee selection
  rectangle and selected icons receive a wider blue selection treatment; moving
  an icon no longer adds its own blue drag highlight. Root/home disk entries
  now use real GIO mount names or device-backed labels instead of the old
  `Macintosh HD` placeholder.
- **Desktop multi-selection menus added**: Right-clicking inside a highlighted
  multi-selection now opens a selection-aware command menu. File-only
  selections keep file actions such as Show in Files, Quick Look, and Move to
  Trash. Mixed selections that include volumes use a smaller Open, Copy, Get
  Info, and Share menu so mounted volumes are not offered file-only actions.
  Right-clicking an unselected desktop item switches the selection to that
  single target before opening its normal item menu.
- **Trash and Nautilus portal crash fixed**: The built-in Trash Dock item and
  Trash context-menu Open action now prefer `nautilus trash:///`, with
  `gio open trash:///` and `xdg-open trash:///` fallbacks. Orange's Settings
  portal now returns the extra variant layer required by the deprecated
  frontend `Read` method while keeping `ReadOne`, backend `Read`, `ReadAll`,
  and `SettingChanged` on the normal single-variant shape. This fixes Nautilus
  crashes that logged `GVariant format string 'v'` against a raw `u` value.

#### Latest Validation

- `meson compile -C build` passed after the desktop icon placement/preview
  pass, again after the Trash/portal fix, and again after the multi-selection
  context-menu pass.
- `./build/test-shell-layout` passed with coverage for horizontal desktop
  placement, duplicate saved-position separation, side-Dock clearance,
  selection-aware desktop context-menu metadata, and the GNOME Files Trash
  command fallback.
- `./build/test-shell-visual` passed with coverage for decoded image previews,
  marquee drawing, and selected-icon highlighting.
- `meson test -C build --print-errorlogs` passed (8/8 tests).
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --assets assets --config orange.conf`
  exited 0. The sandbox emitted the known dconf/GIO session-bus warnings
  because `/run/user/1000/dconf` is read-only and session-bus access is
  restricted.
- `git diff --check` passed.
- `meson compile -C build` passed cleanly after the desktop/Dock context-menu
  pass.
- `./build/test-shell-layout` passed after adding coverage for text-first menu
  metadata, native app-menu separators, desktop Name/Kind sorting, Dock
  built-in menu variants, widened Dock menus, and desktop file menu rows.
- `./build/test-shell-visual` passed after updating the light menu text palette
  assertion to detect bright label pixels instead of dark translucent panel
  pixels.
- `meson test -C build --print-errorlogs` passed (8/8 tests).
- `./build/orange-render-shell --foreground-only --context-menu dock --context-index 0 /tmp/orange-dock-menu.png`
  passed and showed `Remove from Dock` fully visible.
- `./build/orange-render-shell --foreground-only --context-menu dock-launcher --context-index 0 /tmp/orange-dock-launcher-menu.png`
  passed and showed the Launchpad-specific menu.
- `./build/orange-render-shell --foreground-only --context-menu dock-trash --context-index 6 /tmp/orange-dock-trash-menu.png`
  passed and showed the Trash-specific menu.
- `./build/orange-render-shell --foreground-only --context-menu desktop-file --context-index 0 --context-x 1960 --context-y 360 /tmp/orange-desktop-file-menu.png`
  passed and showed the expanded desktop file menu away from the icon label.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --config /tmp/orange-menu-validation.conf`
  exited 0. The sandbox emitted dconf/GIO session-bus warnings because
  `/run/user/1000/dconf` is read-only and session-bus access is restricted.
- `build/orange-render-shell --foreground-only --context-menu desktop --context-x 1440 --context-y 900 /tmp/orange-menu-debug.png`
  passed and was used to inspect the updated desktop context menu rendering.
- `ninja -C build` passed after GNOME Control Center wrapper, Settings portal,
  and GNOME wallpaper bridge changes.
- `meson test -C build --print-errorlogs` passed (8/8 tests).
- `test-shell-visual` now checks that light-mode context menu labels do not
  render dark glyph pixels.
- `XDG_CURRENT_DESKTOP=Orange gnome-control-center --list` exited 1 with the
  GNOME/Unity support guard, confirming the local guard behavior.
- `env -u GTK_THEME -u GTK_ICON_THEME XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu
  XDG_SESSION_DESKTOP=gnome DESKTOP_SESSION=gnome
  GNOME_DESKTOP_SESSION_ID=this-is-deprecated gnome-control-center --list`
  exited 0 and listed panels including `background`, `wifi`, `network`,
  `notifications`, `display`, `sound`, `power`, and `keyboard`.
- `gsettings get org.gnome.desktop.background picture-uri` and
  `picture-uri-dark` returned local `file://` wallpaper URIs under
  `/usr/share/backgrounds` in this environment.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once`
  exited 0. The sandbox emitted dconf/GIO session-bus warnings because
  `/run/user/1000/dconf` is read-only and session-bus access is restricted, but
  wlroots selected Pixman, enabled the headless output, rendered, and exited.
- A temporary-config style-sync smoke run also exited 0, but this sandbox could
  not prove live GNOME Settings toggling because dconf writes are blocked.
- `meson test -C build` passed (8/8 tests) after DBus notifications, composed
  overlay backdrop readback, menu popover material fixes, transient glass
  material alignment, client-commit overlay invalidation, alpha-preserving
  overlay unblending, and real widget data/rendering split.
- `build/orange-render-shell --foreground-only --context-menu desktop --context-x 720 --context-y 450 /tmp/orange-context-menu.png` passed.
- `build/orange-render-shell --foreground-only --launcher /tmp/orange-launcher.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --launcher-search /tmp/orange-search-pill.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --launcher-search --launcher-query cal /tmp/orange-search-pill-typed.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --launcher /tmp/orange-launcher-small.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --launcher /tmp/orange-shared-glass-launcher-final.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --foreground-only --context-menu desktop /tmp/orange-context-bounded-overlay.png` passed.
- `build/orange-render-shell --width 1440 --height 900 --foreground-only --context-menu app /tmp/orange-menubar-bounded-overlay.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once --width 1440 --height 900 --assets assets --config orange.conf` exited 0 with sandbox dconf/GIO warnings.
- Headless one-shot smoke checks with `dock_position=left` and `dock_position=right` configs exited 0.
- `./build/test-assets`, `./build/test-shell-layout`, `./build/test-shell-visual`, `./build/test-config`, and `./build/test-desktop-entry` all passed.

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
