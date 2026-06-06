# Requirements

## Goal

Create a native Linux wlroots compositor prototype in C that recreates the
provided macOS Tahoe 26 desktop screenshot as closely as practical in one
iteration.

## Functional Requirements

- Start a Wayland compositor using wlroots.
- Use the wlroots renderer abstraction and report whether the selected renderer
  is Vulkan when `WLR_RENDERER=vulkan` is requested.
- Render a full-output desktop shell with:
  - clear lake/mountain-style wallpaper,
  - transparent menu bar with Apple-style menu glyph area and menu titles,
  - right-aligned status area and date/time,
  - calendar and weather widgets in the upper-left area,
  - right-side desktop items,
  - translucent bottom dock with app icons, divider, running indicators, and
    trash.
- Provide fallback assets implemented in project code, while supporting local
  Apple-provided asset overrides from ignored paths under `assets/apple/`.
- Provide a headless `--once` mode that renders one frame and exits for CI-style
  validation.

## Non-Functional Requirements

- Use C, Meson, wlroots, Wayland server APIs, Cairo, Pixman, and Vulkan headers.
- Keep proprietary Apple assets out of Git history.
- Keep code modular enough to evolve into a real shell: compositor lifecycle,
  shell rendering, asset loading, and tests should be separate concerns.
- Compile cleanly with warnings enabled on the installed Ubuntu/wlroots stack.

## Scope Boundaries

- This is a visual/technical prototype, not a full macOS clone.
- Full macOS behavior, proprietary animations, WindowServer compatibility,
  Finder, Notification Center, real Dock app launching, and Apple private APIs
  are out of scope.
- Exact Apple fonts, shipped wallpaper, official icons, and trademarks are only
  loaded if the user provides local licensed files; the repo ships replacements.

## Acceptance Criteria

- `meson setup build` succeeds.
- `ninja -C build` succeeds.
- Unit tests pass.
- `WLR_BACKENDS=headless build/tahoe-wlroots --headless --once` renders and
  exits successfully.
- Runtime logs include selected renderer type information.
- `PROJECT_STATE.md` documents status, risks, and continuation steps.
