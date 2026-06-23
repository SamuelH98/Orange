# Requirements

## Goal

Create Orange, a native Linux wlroots compositor in C that follows the provided
macOS-like desktop screenshot visually and behaves like a usable macOS-like
shell.

## Functional Requirements

- Start a Wayland compositor using wlroots.
- Use the wlroots renderer abstraction and report whether the selected renderer
  is Vulkan when `WLR_RENDERER=vulkan` is requested.
- Follow the relevant Wayland and freedesktop specifications for surfaces that
  Orange implements: stable xdg-shell toplevel handling, xdg-decoration
  client-side preference, Desktop Entry launch parsing, Icon Theme lookup,
  Icon Naming names, and XDG data-directory discovery.
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
  - Dock scale, icon scale, magnification, screen position, window minimize
    effect preference, and active indicator visibility,
  - widget visibility and small/medium/large sizing,
  - cursor theme name and cursor size,
  - GTK light/dark theme names and icon theme name.
- Provide a root desktop widget system similar in spirit to GNOME Shell:
  - widgets are defined as typed objects with stable IDs, rectangles, and
    visibility flags,
  - the existing Calendar and Weather cards render through this widget layer,
  - widget painting is isolated from the main shell renderer,
  - Calendar, Screen Time, and Weather widget cards must use real local data
    when available rather than fixed demo text,
  - Calendar reads local GNOME/Evolution calendar files, Screen Time reports
    the current login or Orange session duration, and Weather reads a local
    weather data file plus GNOME Weather's configured location when available,
  - widgets expose shortcut-menu edit, size, and remove actions,
  - widgets remain pinned below floating application windows.
- Provide a native GTK Settings application source that controls this
  configuration and uses the configured GTK theme when GTK development libraries
  are available at build time.
- Settings must expose installed GTK theme, icon theme, and cursor theme names
  as dropdown choices while preserving any saved custom value that is not
  currently installed.
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
- Clicking the date/time/status area opens a macOS-like Notification Center
  overlay from the right edge; clicking outside it or pressing Escape closes it.
- Individual status items expose distinct actions: Wi-Fi, Sound, and Battery
  open item-specific status menus with settings actions; Search opens a
  centered compact Spotlight-style glass pill with adjacent round mode buttons;
  typing stays in the pill; the first adjacent button transforms it into the
  app-launcher overlay; Control Center opens quick controls; and the clock/date
  opens Notification Center.
- The Search pill and launcher overlay must be draggable by the search/header
  area. The Dock launcher and Super/Logo+Space open a smaller centered
  app-launcher overlay; the menu-bar Search item starts in compact search mode.
- Dock icons must support responsive drag interactions: app aliases can be
  reordered within the app section, dragged clearly off the Dock to remove the
  alias without uninstalling the app, and dragged from the Tahoe-style
  Apps/Spotlight launcher back into the Dock. Permanent shell affordances such
  as the launcher and Trash remain available. Insertion feedback should use
  macOS-like icon displacement/gaps rather than a static line marker.
- Right-clicking the Dock separator must open a compact glass bubble context
  menu with Dock-wide controls: turn magnification on/off, adjust Dock size,
  change screen position, choose the minimize effect preference, and open Dock
  settings. Dock size, position, magnification, and minimize-effect choices
  must persist to `orange.conf`.
- Right-clicking empty Dock glass or non-interactive menu-bar chrome must be
  consumed by shell chrome hit testing and must not fall through to the empty
  desktop background context menu.
- Dock item shortcut menus must be stateful. A non-running Dock item uses the
  normal launcher menu, while a running Dock item exposes window/app controls
  including Show All Windows, Hide, and Quit, with Quit closing mapped windows
  that match that Dock launcher.
- Dock opening bounce animation must move away from the Dock edge: upward for
  a bottom Dock, rightward for a left Dock, and leftward for a right Dock.
- Notification Center must render real missed-notification cards received
  through the freedesktop `org.freedesktop.Notifications` DBus service, keep
  widget cards below the notifications, show an empty state when no app
  notifications have arrived, and include an Edit Widgets affordance that opens
  Orange Settings for widget preferences.
- Launch GNOME Control Center through a GNOME-compatible environment wrapper
  when it is installed, including the main Settings app and direct panel
  shortcuts for Background, Wi-Fi/Network, Bluetooth, Notifications, Sound,
  Display, Power, and Keyboard. Keep desktop-neutral fallbacks for systems
  without `gnome-control-center`.
- Follow GNOME Background settings from `org.gnome.desktop.background` so
  wallpapers selected in GNOME Control Center apply to the compositor shell,
  including light/dark picture URIs, placement mode, opacity, and background
  colors.
- Advertise system dark/light preference through GNOME interface GSettings,
  GTK settings files, and the standardized Settings portal color-scheme key.
- Consume GNOME interface `color-scheme` changes so GNOME Settings' Style
  selector maps `default`/light to Orange light appearance and `prefer-dark`
  to Orange dark appearance.
- Context menus and dropdown menus must render white text and white tinted
  icons in both light and dark appearance modes.
- Match macOS dark appearance behavior for menus: system menu and context menu
  panels must switch to a dark translucent material when global appearance is
  dark.
- Disable server-side compositor decorations and prefer client-side window
  decorations for xdg-decoration and KDE server-decoration protocols.
- Replace static desktop shortcut placeholders with XDG `.desktop` entries.
- Allow desktop shortcuts to be dragged to custom persisted positions.
- Provide right-click context menus for desktop shortcuts, widgets, Dock items,
  the Dock separator, and empty desktop background.
- Fix visible layout bugs:
  - menu bar item spacing,
  - tray glyph replacement with local battery/Wi-Fi/control icons,
  - calendar header padding and centered day grid,
  - weather-condition icon sourced from the configured icon theme,
  - desktop label wrapping and centering,
  - desktop shortcuts remaining visible when a theme icon is missing or a saved
    desktop position is outside the current output,
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
- The left side of the menu bar must be active-app driven, matching macOS'
  single-menu-bar model: the title next to the system menu reflects the focused
  application, and File/Edit/View/Go/Window/Help menus sit to its right.
- App menu commands must operate on the focused client where the compositor has
  a real mechanism. For generic Wayland clients, Orange dispatches standard
  focused-client keyboard accelerators for common commands such as Open, Save,
  Undo, Copy, Paste, Find, Print, Full Screen, Help, and Quit. Toolkit-exported
  native menu introspection remains a separate DBusMenu/global-menu backend
  feature because xdg-shell does not expose arbitrary app menu trees.
- Provide keyboard shortcuts modeled after a Mac-like Command key flow:
  - Super/Logo+Return launches a terminal,
  - Super/Logo+Space opens the centered app launcher overlay,
  - Super/Logo+Q closes the focused window,
  - Super/Logo+Tab cycles focus.

## Non-Functional Requirements

- Use C, Meson, wlroots, Wayland server APIs, Cairo, Pixman, and Vulkan headers.
- Keep proprietary third-party assets out of Git history.
- Keep code modular enough to evolve into a real shell: compositor lifecycle,
  shell rendering, asset loading, and tests should be separate concerns.
- Compile cleanly with warnings enabled on the installed Ubuntu/wlroots stack.

## Scope Boundaries

- This is a visual/technical shell implementation, not a full macOS
  implementation.
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
- "All freedesktop specs" is out of scope as a blanket claim. Orange documents
  and tests the subset it implements; protocols such as StatusNotifierHost,
  global-menu DBus backends, layer-shell, output-management, portals, and
  accessibility remain separate feature tracks.

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
- Shell render tests cover missing desktop shortcut icon fallback drawing.
- Shell render tests cover live-compositor overlay separation so launcher and
  menu dismissal cannot leave stale scene-buffer pixels behind.
- Dock add/remove/reorder tests cover alias compaction, duplicate prevention,
  and keeping permanent Dock affordances available.
- Shell layout tests cover menu-bar and Dock chrome background hit targets,
  running Dock context-menu options, and side-aware Dock bounce direction.
- Documentation lists the Wayland/freedesktop specs implemented in the current
  surface area and explicitly scopes non-implemented specs.
- `PROJECT_STATE.md` documents status, risks, and continuation steps.
