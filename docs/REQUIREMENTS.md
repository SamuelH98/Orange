# Requirements

## Goal

Create Orange, a native Linux wlroots compositor prototype in C that follows
the provided macOS-like desktop screenshot visually and behaves like a usable
macOS-like shell prototype.

## Functional Requirements

- Start a Wayland compositor using wlroots.
- Use the wlroots renderer abstraction and report whether the selected renderer
  is Vulkan when `WLR_RENDERER=vulkan` is requested.
- Render a full-output desktop shell with:
  - clear lake/mountain-style wallpaper,
  - fully transparent menu bar with system menu glyph area and menu
    titles drawn directly over the wallpaper,
  - right-aligned status area and date/time,
  - calendar and weather widgets in the upper-left area,
  - right-side desktop items,
  - translucent bottom dock with app icons, divider, running indicators, and
    trash.
- Keep tracked `assets/` wallpaper-only; shell icons, Dock icons, desktop
  shortcut icons, menu icons, and status icons must come from configured icon
  themes.
- Scale menu bar, widgets, desktop items, and Dock geometry from the output
  resolution so the shell remains usable on small and large displays.
- Match the reference Dock measurements with resolution-scaled constants for
  icon size, icon gaps, glass padding, indicator lane, and bottom edge
  alignment.
- Provide tracked high-resolution Orange light/dark wallpapers plus a
  procedural background fallback.
- Resolve all shell icon imagery through freedesktop icon-theme names, including
  inherited themes and `hicolor` fallback, without storing icons under
  `assets/`.
- Provide a persistent desktop configuration model:
  - global light/dark appearance,
  - desktop shortcut visibility, grid spacing, text label size, and icon scale,
  - Dock scale, icon scale, magnification, and active indicator visibility,
  - widget visibility and small/medium/large sizing,
  - cursor theme name and cursor size,
  - GTK light/dark theme names and icon theme name.
- Provide a root desktop widget system similar in spirit to GNOME Shell:
  - widgets are defined as typed objects with stable IDs, rectangles, and
    visibility flags,
  - the existing Calendar and Weather cards render through this widget layer,
  - widgets expose shortcut-menu edit, size, and remove actions,
  - widgets remain pinned below floating application windows.
- Provide a native GTK Settings application source that controls this
  configuration and uses the configured GTK theme when GTK development libraries
  are available at build time.
- Provide a native GTK About Orange application source that opens from the
  system menu About Orange item when GTK development libraries are available
  at build time. It should present a compact Orange model/version summary
  with real chip and memory values and no serial number row.
- Configure GTK clients launched from the shell using the user-selected GTK
  theme names.
- Use MacTahoe GTK and icon themes as default test values, with final Orange
  GTK and icon themes expected to live in a separate theme project and be
  installed through normal Linux theme locations.
- Resolve Dock, desktop shortcut, and shell status icons through
  freedesktop-style icon-theme names, with semantic aliases for common app IDs
  and standard XDG icon names.
- Keep the menu bar behavior macOS-like at the interaction layer: system menu
  and app menus on the left, status menu icons and clock on the right, with
  status icons opening quick controls; implement those controls with Linux
  launch commands, DBus-readable system state, and freedesktop icon names.
- Disable server-side compositor decorations and prefer client-side window
  decorations for xdg-decoration and KDE server-decoration protocols.
- Replace static desktop shortcut placeholders with XDG `.desktop` entries.
- Allow desktop shortcuts to be dragged to custom persisted positions.
- Provide right-click context menus for desktop shortcuts, widgets, and Dock
  items.
- Fix visible layout bugs:
  - menu bar item spacing,
  - tray glyph replacement with local battery/Wi-Fi/control icons,
  - calendar header padding and centered day grid,
  - weather-condition icon sourced from the configured icon theme,
  - desktop label wrapping and centering,
  - Dock indicator dot bottom padding,
  - Dock Calendar icon day/date sync,
  - Dock active indicators showing only mapped/open applications,
  - top menu logo sizing/color and tighter menu item spacing.
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
  - system menu popover with shell actions.
- Launch Dock and desktop apps from parsed freedesktop `.desktop` entries with
  unsupported or file-specific `Exec` field codes removed for no-file launches.
- Right-clicking the active app/menu title area in the menu bar opens the same
  app context menu used by Dock items so the menu-bar app menu is actionable.
- Provide keyboard shortcuts modeled after a Mac-like Command key flow:
  - Super/Logo+Return launches a terminal,
  - Super/Logo+Space launches an app picker if available,
  - Super/Logo+Q closes the focused window,
  - Super/Logo+Tab cycles focus.

## Non-Functional Requirements

- Use C, Meson, wlroots, Wayland server APIs, Cairo, Pixman, and Vulkan headers.
- Keep proprietary third-party assets out of Git history.
- Keep code modular enough to evolve into a real shell: compositor lifecycle,
  shell rendering, asset loading, and tests should be separate concerns.
- Compile cleanly with warnings enabled on the installed Ubuntu/wlroots stack.

## Scope Boundaries

- This is a visual/technical shell prototype, not a full macOS implementation.
- Full macOS behavior, proprietary animations, WindowServer compatibility,
  proprietary desktop services, real Dock app launching, and platform-private APIs
  are out of scope.
- A production file-manager replacement, full menu bar app integration, Mission
  Control, Spotlight indexing, real LaunchServices, notarization, iCloud,
  Continuity, and system settings backends are out of scope.
- Exact proprietary fonts, shipped wallpapers, official icons, and trademarks
  are only loaded if the user provides or installs licensed local files. The
  runtime is designed to source wallpapers from `./assets/` and icons from
  installed icon themes.

## Acceptance Criteria

- `meson setup build` succeeds.
- `ninja -C build` succeeds.
- Unit tests pass.
- `WLR_BACKENDS=headless build/orange --headless --once` renders and
  exits successfully.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --assets assets --config /tmp/orange-custom.conf`
  renders and exits successfully.
- Runtime logs include selected renderer type information.
- A user can launch configured apps from the Dock/desktop and interact with
  ordinary Wayland windows.
- `.desktop` parser tests pass.
- Config parser tests pass.
- Shell render tests cover both light and dark appearance.
- Shell render tests cover light/dark rendering and foreground context-menu
  glass/scaling without requiring a checked-in visual reference image.
- `PROJECT_STATE.md` documents status, risks, and continuation steps.
