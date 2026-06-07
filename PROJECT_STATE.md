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
macOS-measured Dock layout, scalable widgets, basic xdg-shell window
management, Dock launchers, XDG `.desktop` desktop launchers, keyboard
shortcuts, local asset sourcing, GTK theme/icon theme scaffolding, PNG reference
export, and headless one-shot validation.

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
- Light/dark shell appearance switching and GTK theme environment export.
- Bundled GTK CSD theme CSS with traffic-light window control styling.
- Bundled `TahoeIcons` GTK icon theme metadata plus a population script.
- Widget registry layer with Calendar and Weather widgets pinned under client
  windows.
- Deterministic `.desktop` parser and right-side desktop shortcuts from
  `assets/desktop/`.
- Desktop labels wrap and center, including `PDF Document`.
- Dock Calendar icon day/date overlay uses current shell time.
- Dock indicator dots stay inside the glass container.
- Top menu bar background fill removed so it is transparent over wallpaper.
- Reference Dock measurements are unit-tested at 2048x1153:
  `x=70`, `y=1045`, `w=1908`, `h=108`, 64 px icons, 26 px gaps.

#### Tests Added

- Config load/save parser test.
- `.desktop` parser test.
- Shell dark render smoke test.
- Widget layer existence test.
- `.desktop`-required desktop icon layout test.
- Reference Dock measurement test.

## Current Work

### Active Feature

Settings/theme/widget/icon-pack enhancement complete for this session.

### Progress

Implementation compiles, unit tests pass, PNG render works, and compositor
headless one-shot validation works.

### Remaining Work

Interactive testing under WSLg or a nested Wayland session is still needed.
Pixel-level 1:1 requires user-provided Apple assets under `assets/apple/`.
The local assets currently do not include `assets/apple/status/weather.png`,
`assets/apple/status/control-center.png`, or the later blue abstract wallpaper;
those paths are wired and will render when supplied.
More faithful behavior would require real app/menu integration, richer
animation, and full desktop services.

---

## Next Actions

1. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/tahoe-wlroots`.
2. If WSLg exposes a suitable DRM/Vulkan path, try
   `WLR_BACKENDS=wayland WLR_RENDERER=vulkan build/tahoe-wlroots`.
3. Add optional local files under `assets/apple/` for private Apple-provided
   wallpaper/icon overrides, especially:
   `wallpaper-beach-day.png`, `status/weather.png`, and
   `status/control-center.png`.
4. Populate the GTK icon pack with
   `./tools/populate_icon_theme.sh assets themes/TahoeIcons` after local
   assets are present.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Apple proprietary assets are not included; users must supply local licensed
files under ignored paths for private 1:1 asset replacement. GTK4 development
headers are not installed in this environment, so `tahoe-settings` is skipped
here but will build where `gtk4` is available.

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
