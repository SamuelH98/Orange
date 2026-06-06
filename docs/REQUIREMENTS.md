# Requirements

## Goal

Create a native Linux wlroots compositor prototype in C that recreates the
provided macOS Tahoe 26 desktop screenshot visually and behaves like a usable
macOS-style shell prototype.

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
- Scale menu bar, widgets, desktop items, and Dock geometry from the output
  resolution so the shell remains usable on small and large displays.
- Provide fallback assets implemented in project code, while supporting local
  Apple-provided asset overrides from ignored paths under `assets/apple/`.
- Provide a headless `--once` mode that renders one frame and exits for CI-style
  validation.
- Manage normal Wayland xdg-shell toplevel windows:
  - map/unmap/destroy,
  - focus and raise on click,
  - basic move and resize,
  - maximize/fullscreen requests,
  - close focused window from a shortcut.
- Provide clickable shell areas:
  - Dock launchers for common apps/actions,
  - desktop folder/document launchers,
  - Apple-style menu popover with shell actions.
- Provide keyboard shortcuts modeled after a Mac-like Command key flow:
  - Super/Logo+Return launches a terminal,
  - Super/Logo+Space launches an app picker if available,
  - Super/Logo+Q closes the focused window,
  - Super/Logo+Tab cycles focus.

## Non-Functional Requirements

- Use C, Meson, wlroots, Wayland server APIs, Cairo, Pixman, and Vulkan headers.
- Keep proprietary Apple assets out of Git history.
- Keep code modular enough to evolve into a real shell: compositor lifecycle,
  shell rendering, asset loading, and tests should be separate concerns.
- Compile cleanly with warnings enabled on the installed Ubuntu/wlroots stack.

## Scope Boundaries

- This is a visual/technical shell prototype, not a full macOS implementation.
- Full macOS behavior, proprietary animations, WindowServer compatibility,
  Finder, Notification Center, real Dock app launching, and Apple private APIs
  are out of scope.
- A production Finder replacement, full menu bar app integration, Mission
  Control, Spotlight indexing, real LaunchServices, notarization, iCloud,
  Continuity, and system settings backends are out of scope.
- Exact Apple fonts, shipped wallpaper, official icons, and trademarks are only
  loaded if the user provides local licensed files; the repo ships replacements.

## Acceptance Criteria

- `meson setup build` succeeds.
- `ninja -C build` succeeds.
- Unit tests pass.
- `WLR_BACKENDS=headless build/tahoe-wlroots --headless --once` renders and
  exits successfully.
- Runtime logs include selected renderer type information.
- A user can launch configured apps from the Dock/desktop and interact with
  ordinary Wayland windows.
- `PROJECT_STATE.md` documents status, risks, and continuation steps.
