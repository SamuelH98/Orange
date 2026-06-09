# PROJECT_STATE

## Project Overview

### Project Name

Tahoe

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
GTK theme/icon theme scaffolding, PNG render export, foreground-only visual
smoke coverage, bundled installed MacTahoe GTK themes plus upstream source, and
headless one-shot validation.

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
- `build/tahoe-render-shell /tmp/tahoe-shell.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe --headless --once`
  passed and rendered one headless frame.

#### Tests Added

Shell layout/hit-test/render smoke unit tests.

---

### Settings, Themes, Assets, And Widgets

#### Validation

- `ninja -C build` passed cleanly.
- `meson test -C build --print-errorlogs` passed.
- `build/tahoe-render-shell /tmp/tahoe-shell.png` passed.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe --headless --once`
  passed and rendered one headless frame.

#### Implemented

- Persistent `tahoe.conf` model for appearance, desktop icon, and Dock settings.
- Conditional GTK4 Settings app source for appearance, desktop icon, and Dock
  controls.
- Conditional GTK4 About This Mac app source launched from Apple menu >
  About Tahoe, with Tahoe version/build, model, chip, memory, serial, graphics,
  and kernel rows.
- Bundled installed MacTahoe release themes as `themes/MacTahoe-Light` and
  `themes/MacTahoe-Dark`.
- Bundled upstream MacTahoe GTK theme source as `themes/MacTahoe-gtk-theme`
  submodule, preserving upstream MIT-style license and update path.
- Persistent widget visibility and small/medium/large widget size settings.
- Light/dark shell appearance switching and MacTahoe GTK theme environment
  export.
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
- Foreground visual testing checks context-menu glass/translucency and scaling
  without requiring a checked-in visual reference image.

### Cursor, Menus, And Tahoe Assets

#### Implemented

- Cursor theme and cursor size settings in `tahoe.conf`.
- GTK Settings app controls for cursor theme and cursor size.
- Config-driven `wlr_xcursor_manager` setup and `XCURSOR_*` environment export.
- Tracked Tahoe placeholder PNG asset pack under `assets/` using `T` branding.
- Asset generator script: `tools/generate_tahoe_assets.sh`.
- Larger white Tahoe menu logo in the transparent menu bar.
- Appearance-aware status icon tinting; status glyphs render light in dark mode.
- Status strip icons now pack leftward from measured clock text in fixed visual
  slots, with tighter generated battery/Wi-Fi/search/control/weather masks.
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

### macOS Tahoe 26 Context Menus & Apple Menu

#### Implemented

- Full macOS Tahoe Apple menu (10 items) with separator lines between groups:
  About Tahoe, System Settings..., App Store..., Recent Items,
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
- Foreground visual test for context-menu glass/scaling.

## Current Work

### Active Feature

Project renamed to Tahoe. Reference-image testing removed. Widget shortcut
menus, context-menu glass, Dock ordering, status icon refinement, bundled
installed MacTahoe themes and source, GTK About app source, and foreground
visual tests complete.

### Progress

Implementation compiles, all 4 Meson tests pass, the non-reference visual test
passes, rendered status-strip crops were checked with both default and private
Apple assets, bundled MacTahoe GTK4 CSS exists, and headless compositor one-shot
validation passes.

### Remaining Work

Interactive testing under WSLg or a nested Wayland session is still needed.
WSLg should use `WLR_BACKENDS=wayland WLR_RENDERER=pixman`; forcing wlroots
Vulkan there can fail with `Cannot create Vulkan renderer: no DRM FD available`
because the nested backend lacks the DRM render-node FD wlroots expects. GTK
utility launch commands set `GSK_RENDERER=cairo` to avoid GTK's WSLg EGL/Zink
path.
Pixel-level 1:1 depends on the ignored private Apple assets. Automated visual
coverage intentionally avoids checked-in reference-image comparison.
More faithful behavior would require real app/menu integration, richer
animation, and full desktop services.

---

## Next Actions

1. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/tahoe`.
2. Replace generated Tahoe placeholder PNGs under `assets/icons/` and
   `assets/status/` with final repo-safe GitHub-sourced Tahoe assets.
3. Populate the GTK icon pack with
   `./tools/populate_icon_theme.sh assets themes/TahoeIcons` after local
   assets are present.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Apple proprietary assets are not included. GTK4 development headers are
installed in this environment now, and both `tahoe-settings` and `tahoe-about`
build here. They remain conditional for systems without GTK4 development files.

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
- **xdg-shell capability assertion fixed**: GTK4 clients can bind xdg-shell v3,
  while `wlr_xdg_toplevel_set_wm_capabilities()` requires the toplevel
  capability protocol version. Tahoe now creates xdg-shell v5 and only advertises
  window-manager capabilities when the client resource version supports them.

### Technical Concerns

wlroots 0.17 APIs are unstable and may need porting for newer distros. Vulkan
renderer availability depends on the local GPU/driver and wlroots runtime
support. In this sandbox, `WLR_BACKENDS=headless WLR_RENDERER=vulkan` fails
because wlroots has no DRM FD in headless mode.
The same `no DRM FD available` failure can occur in WSLg with the Wayland
backend even when the Windows host has a discrete GPU, because WSLg GPU
acceleration does not guarantee the DRM render-node path wlroots Vulkan needs.
The committed procedural background fallback and any missing local PNGs are not
pixel-identical to Apple proprietary assets; exact local visual matching depends
on ignored asset overrides.

---

## Resume Instructions

Continue from the committed implementation. Target wlroots 0.17.1, keep Apple
assets and populated GTK icon PNGs ignored, validate with
`meson test -C build --print-errorlogs` and
`WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe --headless --once`,
then test interactively under WSLg or nested Wayland.
