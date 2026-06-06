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
wlroots compositor, scalable Tahoe-style shell renderer, basic xdg-shell window
management, Dock/menu/desktop launchers, keyboard shortcuts, optional Apple
asset overrides, and headless one-shot validation.

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
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once`
  passed and rendered one headless frame.

#### Tests Added

Shell layout/hit-test/render smoke unit tests.

---

## Current Work

### Active Feature

Prototype complete for this session.

### Progress

Implementation compiles and runs in headless validation mode.

### Remaining Work

Interactive testing under WSLg or a nested Wayland session is still needed.
More faithful behavior would require server-side macOS-style window frames,
real app/menu integration, richer animation, and full desktop services.

---

## Next Actions

1. Run on WSLg with `WLR_BACKENDS=wayland WLR_RENDERER=pixman build/tahoe-wlroots`.
2. If WSLg exposes a suitable DRM/Vulkan path, try
   `WLR_BACKENDS=wayland WLR_RENDERER=vulkan build/tahoe-wlroots`.
3. Add optional local files under `assets/apple/` for private Apple-provided
   wallpaper/icon overrides.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Apple proprietary assets are not included; users must supply local licensed
files under ignored paths for private 1:1 asset replacement.

### Technical Concerns

wlroots 0.17 APIs are unstable and may need porting for newer distros. Vulkan
renderer availability depends on the local GPU/driver and wlroots runtime
support. In this sandbox, `WLR_BACKENDS=headless WLR_RENDERER=vulkan` fails
because wlroots has no DRM FD in headless mode.

---

## Resume Instructions

Continue from the committed implementation. Target wlroots 0.17.1, keep Apple
assets ignored, validate with `meson test -C build --print-errorlogs` and
`WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once`,
then test interactively under WSLg or nested Wayland.
