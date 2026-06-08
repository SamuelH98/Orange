# PROJECT_STATE

## Project Overview

### Project Name

Tahoe wlroots

### Goal

Build a C/wlroots Linux compositor prototype that recreates the supplied macOS
Tahoe 26 desktop reference and behaves like a usable macOS-style shell, with
optional local Apple asset overrides and Vulkan renderer support through
wlroots.

### Current Status

Functional shell prototype is implemented and validated locally. It includes a
wlroots compositor, reference-sized Tahoe shell geometry, transparent menu bar,
compact macOS-style Dock layout, scalable widgets, basic xdg-shell window
management, Dock launchers, XDG `.desktop` desktop launchers with drag/context
menus, keyboard shortcuts, cursor customization, local Tahoe asset sourcing,
GTK theme/icon theme scaffolding, PNG reference export, foreground-only visual
measurement, and headless one-shot validation.

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
- `build/tahoe-render-shell /tmp/tahoe-reference.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once`
  passed and rendered one headless frame.

#### Tests Added

Shell layout/hit-test/render smoke unit tests.

---

### Settings, Themes, Assets, And Widgets

#### Validation

- `ninja -C build` passed cleanly.
- `meson test -C build --print-errorlogs` passed.
- `build/tahoe-render-shell /tmp/tahoe-reference.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once`
  passed and rendered one headless frame.

#### Implemented

- Persistent `tahoe.conf` model for appearance, desktop icon, and Dock settings.
- Conditional GTK4 Settings app source for appearance, desktop icon, and Dock
  controls.
- Persistent widget visibility and small/medium/large widget size settings.
- Light/dark shell appearance switching and GTK theme environment export.
- Bundled GTK CSD theme CSS with traffic-light window control styling.
- Bundled `TahoeIcons` GTK icon theme metadata plus a population script.
- Widget registry layer with Calendar and Weather widgets pinned under client
  windows.
- Deterministic `.desktop` parser and right-side desktop shortcuts from
  `assets/desktop/`.
- Desktop labels wrap and center, including `PDF Documents`.
- Dock Calendar icon day/date overlay uses current shell time.
- Dock indicator dots stay inside the glass container.
- Top menu bar background fill removed so it is transparent over wallpaper.
- Native `2880x1800` foreground visual testing ignores wallpaper differences
  and measures Dock glass bounds plus Dock icon width/center/spacing against
  `tahoe-desktop-reference.png`.

### Cursor, Menus, And Tahoe Assets

#### Implemented

- Cursor theme and cursor size settings in `tahoe.conf`.
- GTK Settings app controls for cursor theme and cursor size.
- Config-driven `wlr_xcursor_manager` setup and `XCURSOR_*` environment export.
- Tracked Tahoe placeholder PNG asset pack under `assets/` using `T` branding.
- Asset generator script: `tools/generate_tahoe_assets.sh`.
- Larger white Tahoe menu logo in the transparent menu bar.
- Appearance-aware status icon tinting; status glyphs render light in dark mode.
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
- Native foreground visual Dock metric test.
- Foreground context-menu glass/translucency and scaling test.

### macOS Tahoe 26 Context Menus & Apple Menu

#### Implemented

- Full macOS Tahoe Apple menu (10 items) with separator lines between groups:
  About Tahoe wlroots, System Settings..., App Store..., Recent Items,
  Force Quit..., Sleep, Restart..., Shut Down..., Lock Screen, Log Out.
- System actions: `systemctl suspend/reboot/poweroff`,
  `xdg-screensaver lock` for Lock Screen, `wl_display_terminate` for Log Out.
- Dock right-click context menu (5 items, 2 separators): Open,
  Show in Finder, Remove from Dock, Open at Login, Dock Settings.
- Desktop background right-click context menu (8 items, 2 separators):
  New Folder, Get Info, Use Stacks, Sort By, Clean Up By,
  Show View Options, Change Desktop Background..., Edit Widgets.
- Widget right-click context menu (5 items, 2 separators): Edit Widget, Small,
  Medium, Large, Remove Widget.
- Desktop icon right-click context menu (9 items, 3 separators):
  Open, Show in Finder, Copy, Get Info, Rename, Duplicate,
  Quick Look, Share, Move to Trash.
- `TAHOE_CONTEXT_MENU_WIDGET` enum for widget menus.
- `TAHOE_HIT_WIDGET` hit kind so widgets no longer behave like wallpaper.
- `TAHOE_CONTEXT_MENU_DESKTOP_ICON` enum for icon vs. background menus.
- `TAHOE_HIT_DESKTOP` hit kind for empty desktop background detection.
- Separator line support (`separator_before[]` arrays with flags in layout).
- Cursor-position-driven desktop background menu placement.
- Context menus and the Apple menu use the same glass material family as the
  Dock, with text and item geometry scaled from output resolution.

#### Tests Added

- Dock context menu hit/layout coverage.
- Widget hit/menu coverage and hidden-widget hit exclusion.
- Desktop icon context menu (DESKTOP_ICON, item 0, 9 items, 3 separators).
- Desktop background context menu at cursor.
- Desktop background hit test (TAHOE_HIT_DESKTOP on empty desktop area).
- Config load/save coverage for widget visibility and size.
- Native `2880x1800` foreground visual test for Dock bounds/icon metrics and
  context-menu glass/scaling.

## Current Work

### Active Feature

Native reference matching, widget shortcut menus, context-menu glass, and
foreground visual tests complete.

### Progress

Implementation compiles, all 4 Meson tests pass, the native visual test passes,
and headless compositor one-shot validation passes.

### Remaining Work

Interactive testing under WSLg or a nested Wayland session is still needed.
Pixel-level 1:1 depends on the ignored private Apple assets. The foreground
visual test intentionally ignores wallpaper and measures shell geometry/glass
against the checked-in reference.
More faithful behavior would require real app/menu integration, richer
animation, and full desktop services.

---

## Next Actions

1. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/tahoe-wlroots`.
2. If WSLg exposes a suitable DRM/Vulkan path, try
   `WLR_BACKENDS=wayland WLR_RENDERER=vulkan build/tahoe-wlroots`.
3. Replace generated Tahoe placeholder PNGs under `assets/icons/` and
   `assets/status/` with final repo-safe GitHub-sourced Tahoe assets.
4. Populate the GTK icon pack with
   `./tools/populate_icon_theme.sh assets themes/TahoeIcons` after local
   assets are present.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Apple proprietary assets are not included. GTK4 development headers are not
installed in this environment, so `tahoe-settings` is skipped here but will
build where `gtk4` is available.

### Fixed

- **Vulkan dependency made optional**: The `vulkan` dependency in `meson.build`
  was hard-required, preventing builds on systems without the Vulkan SDK.
  Changed to `required: false` with a `TAHOE_HAVE_VULKAN` preprocessor guard
  around the `wlr_renderer_is_vk()` runtime check. The compositor now builds
  and runs with any renderer (Pixman, GLES2, Vulkan) without requiring the
  Vulkan SDK at build time.
- **Cursor theme applied on output creation**: `server_apply_cursor_config()`
  was called before `wlr_backend_start()` when no outputs exist, making
  `wlr_cursor_set_xcursor()` a no-op. The cursor theme now gets applied:
  (1) after backend start so all initial outputs receive it, and
  (2) in `handle_new_output()` so hotplugged outputs also get the theme.

### Technical Concerns

wlroots 0.17 APIs are unstable and may need porting for newer distros. Vulkan
renderer availability depends on the local GPU/driver and wlroots runtime
support. In this sandbox, `WLR_BACKENDS=headless WLR_RENDERER=vulkan` fails
because wlroots has no DRM FD in headless mode.
The committed procedural background fallback and any missing local PNGs are not
pixel-identical to Apple proprietary assets; exact local visual matching depends
on ignored asset overrides.

---

## Resume Instructions

Continue from the committed implementation. Target wlroots 0.17.1, keep Apple
assets and populated GTK icon PNGs ignored, validate with
`meson test -C build --print-errorlogs` and
`WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once`,
then test interactively under WSLg or nested Wayland.
