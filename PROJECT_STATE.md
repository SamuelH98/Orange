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
configuration, lazy freedesktop icon lookup, focused xdg Settings portal and
GNOME DisplayConfig compatibility services for settings clients, scoped
Wayland/freedesktop spec compliance documentation, PNG render export,
foreground-only visual smoke coverage, and headless one-shot validation.

### Current Session - Fast macOS-Style Workspace Switching

Orange workspace switches now use a short macOS-inspired horizontal slide
instead of an instant hide/show. Research used Apple's current Mac User Guide:
Spaces are navigated left/right with gestures or Control+Arrow, and Apple's
Motion accessibility docs call out desktop switching as interface movement that
users may reduce. Orange follows that model with a fast 170 ms cubic ease-out
slide of live scene nodes; client window geometry stays unchanged and the
scene-node offsets are reset exactly when the animation settles.

The animation is compositor-local and respects Orange's hybrid shell model:
menu bar, Dock, and shell overlays remain stable while workspace-scoped windows
slide horizontally. Switching to a higher workspace brings the incoming
workspace from the right and moves the outgoing workspace left; switching lower
does the reverse. The existing GNOME `enable-animations` setting is now treated
as the reduce-motion path for this feature, so workspace switches remain
instant when animations are disabled.

Validation passed with `ninja -C build test-shell-layout orange`,
`./build/test-shell-layout`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs` (11/11),
`git diff --check` on the touched files, and
`WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once`.
The headless smoke still emits the known sandbox dconf warnings for read-only
`/run/user/1000/dconf/user`, but exits successfully.

Previous recent session notes:

GNOME Multitasking Settings Integration:

Orange now treats GNOME Settings' Multitasking panel as real compositor input
instead of launching it with local schema shims. The temporary GNOME extension
schema stub and `GSETTINGS_SCHEMA_DIR` command prefix were removed; Settings
still falls back through `gnome-control-center multitasking`, but Orange now
reads the real `org.gnome.desktop.interface`, `org.gnome.mutter`,
`org.gnome.desktop.wm.preferences`, and `org.gnome.shell.app-switcher` keys.

Implemented behavior keeps Orange's macOS-like shell while adding GNOME-style
multitasking controls: top-left hot corner opens Orange's full launcher
overview, active screen edges snap moved windows to the left/right half or
maximize at the top using the work area, dynamic/fixed workspace settings drive
Orange's workspace count, workspace display policy scopes workspaces to all
outputs or only the primary output, and app switching can include all
workspaces or only the current workspace. Keyboard workspace affordances are
available with Super+Page/Arrow to switch and Super+Shift+Page/Arrow to move
the focused window with the workspace switch.

Validation passed with `ninja -C build test-shell-layout orange`,
`./build/test-shell-layout`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs` (11/11), an
environment-matched `gnome-control-center --list` check showing the
`multitasking` panel, `git diff --check` on the touched files, and
`WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once`.
The headless smoke still emits the known sandbox dconf warnings for read-only
`/run/user/1000/dconf/user`, but exits successfully.

Previous recent session notes:

Fullscreen and Work-Area Window Bounds:

Orange now follows the researched macOS distinction between true fullscreen and
ordinary enlarged windows. Fullscreen xdg-shell toplevels are configured to the
raw output rectangle so videos and games cover the menu bar and Dock, while
maximized windows continue to use the shell work area. Interactive resize now
caps the client rectangle against the same work area before sending the resize
configure, preventing normal windows from being dragged over a visible Dock or
side Dock. The compositor also uses `wlr_output_layout_get_box()` for these
output rectangles so multi-output fullscreen/work-area math uses layout
coordinates consistently. Fullscreen transitions now retain the previous
work-area-safe frame and restore it on exit, with a centered work-area fallback
for clients that launch directly fullscreen.

Validation passed with `ninja -C build orange test-shell-layout`,
`./build/test-shell-layout`, `ninja -C build`, `.venv-wlroots-build/bin/meson
test -C build --print-errorlogs` (11/11), `git diff --check -- src/compositor.c
PROJECT_STATE.md`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange
--headless --once`. The headless smoke still emits the known sandbox dconf
warnings for read-only `/run/user/1000/dconf/user`, but exits successfully.

Previous recent session notes:

Spotlight Launcher Search:

Orange's launcher now follows the macOS Tahoe Spotlight model more closely.
Typing in compact Search expands to the full launcher panel, and query results
switch from the empty-app grid to vertical Spotlight-style rows. The unified
result model can show installed applications, cached Desktop files, built-in
actions, and a web-search row. The full launcher now exposes four browse-mode
chips for Applications, Files, Actions, and Clipboard-style actions; empty
Applications mode keeps the existing app grid and category filters.

The result builder lives in `src/launcher.c` so app/file/action/web ordering is
testable outside the live compositor. The compositor activates rows by kind:
apps use desktop-entry launch, files reuse `gio open`/`xdg-open`, actions reuse
existing shell command helpers, and web rows launch a DuckDuckGo search through
the default browser. Mouse clicks activate list rows immediately, while empty
Applications grid cells keep the existing launcher-to-Dock drag behavior.

Validation passed with `ninja -C build`, `./build/test-shell-layout`,
`./build/test-shell-visual`, `.venv-wlroots-build/bin/meson test -C build
--print-errorlogs` (11/11), `git diff --check`, and `WLR_BACKENDS=headless
WLR_RENDERER=pixman ./build/orange --headless --once`. The headless smoke still
emits the known sandbox dconf warnings for read-only `/run/user/1000/dconf/user`,
but exits successfully.

Previous recent session notes:

Wayland launch, Void defaults, and build directory:

Orange now treats Wayland as the default launch policy for non-game clients:
direct launches export GTK/Qt/SDL/Clutter Wayland backends, Firefox Wayland
mode, Electron Ozone hints, and scoped Chromium/Chrome flag environment when a
Chromium-family browser command is detected. `Categories=Game` desktop entries
are the exception: Orange does not force toolkit Wayland variables for them and
leaves inherited nested-session display and session-bus state available until
Orange has its own Xwayland host.

The legacy GTK menu module path was removed from the package/session/docs
surface. The Void session wrapper clears inherited GTK module injection, the
old sandbox bridge script is gone, Meson no longer syntax-checks it, and the
Void package/VM runtime lists no longer install that module package. Ghostty is
now the packaged/default terminal through the Void dependency list,
`ORANGE_TERMINAL=ghostty`, Dock fallback commands, app icon aliases, and the
default Dock config. README and Void docs now include the local `void-packages`
overlay, `xbps-src -1 pkg orange`, `xbps-rindex`, and `xbps-install -R` flow
for installing Orange.

The development build directory has been simplified to `build`; the wlroots
target remains pinned through the versioned `wlroots-0.20` pkg-config
dependency. README, requirements, Void packaging copy excludes, `.gitignore`,
and handoff commands now use `build`. The local generated build directory was
regenerated at `build` because Meson does not support relocating a configured
build directory cleanly.

Validation passed with `ninja -C build test-desktop-entry
test-shell-layout orange`, `./build/test-desktop-entry`,
`./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
`bash -n scripts/build-vm.sh scripts/build-void-vm.sh scripts/run-vm.sh
scripts/run-void-vm.sh`, `sh -n
packaging/void/srcpkgs/orange/files/orange-session`, `git diff --check`, stale
module reference searches, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`. The standalone headless smoke
still emits the known sandbox dconf warnings for read-only
`/run/user/1000/dconf/user`, but exits successfully.
After the build-directory rename, validation also passed with absolute
wlroots-local paths for `.venv-wlroots-build/bin/meson setup build --reconfigure`,
`ninja -C build`, `.venv-wlroots-build/bin/meson test -C build --print-errorlogs`
(11/11), `git diff --check`, a stale versioned-build-directory reference
search, and the same headless smoke from `./build/orange`.

Optimization audit follow-up reviewed the largest compositor, shell, Dock,
launcher, menubar, asset, script, package, and test surfaces with static
searches for unsafe copy APIs, large stack buffers, repeated synchronous DBus
access, Cairo surface churn, allocation patterns, and filesystem scans. The
interactive app-menu discovery and activation paths now reuse Orange's cached
session bus connection instead of performing fresh synchronous
`g_bus_get_sync()` lookups. Validation passed with `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs` (11/11),
`git diff --check`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`; the headless smoke still emits the known
sandbox dconf warnings for read-only `/run/user/1000/dconf/user`, but exits
successfully.

Older recent session notes:

Dock Trash full icon:

The Dock Trash launcher now reflects the current FreeDesktop trash state.
Orange caches whether `$XDG_DATA_HOME/Trash/files` or
`~/.local/share/Trash/files` contains any entries, initializes that state at
startup, polls it from the compositor timer, and uses dock-only redraws when
the full/empty state is the only visual change. Desktop "Move to Trash" actions
and the Dock "Empty Trash" action request an immediate rescan on the next timer
pass. The Dock renderer now switches the built-in `__trash__` launcher between
`user-trash` and `user-trash-full` through shell state, with visual regression
coverage for both icon states.

Validation passed with `ninja -C build test-shell-visual
test-shell-layout orange`, `./build/test-shell-visual`,
`./build/test-shell-layout`, full `ninja -C
build`, `.venv-wlroots-build/bin/meson test -C
build --print-errorlogs`, targeted `git diff --check`, and
`WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange
--headless --once`. The headless smoke still emits the known sandbox dconf
warnings for read-only `/run/user/1000/dconf/user`, but exits successfully.

Clicking a desktop file or empty desktop now moves shell focus back to the
Files desktop context. The compositor deactivates the previously focused client,
clears keyboard focus, resets imported app-menu/AT-SPI state, and marks the
shell dirty so the menu bar returns to Orange's built-in Files profile. The
same focus transition is applied to desktop right-clicks so desktop context
menus are also Files-scoped. Shell layout now ignores stale native app-menu
models whenever the active app label is Files, with regression coverage for
that fallback. Validation passed with `ninja -C build
test-shell-layout orange`, `./build/test-shell-layout`, full
`ninja -C build`, `.venv-wlroots-build/bin/meson test -C
build --print-errorlogs`, targeted `git diff --check`, and
`WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange
--headless --once`. The headless smoke still emits the known sandbox dconf
warnings for read-only `/run/user/1000/dconf/user`, but exits successfully.

Fixed side-Dock hover redraws erasing desktop icons. Dock hover and
magnification use a clipped redraw path that clears wallpaper inside the Dock
dirty bounds before repainting the Dock. For left/right Docks that dirty band
can cross manually placed desktop icons, so the clipped path now rebuilds the
base shell content in that region before painting the Dock over it. The live
compositor dock-only redraw state now carries the same desktop file, volume,
selection, rename, status, active-app, and app-menu data needed for that clipped
base repaint. Added visual regression coverage that renders a desktop file icon
inside the right-Dock hover dirty band, performs the clipped Dock redraw, and
asserts the icon pixel remains visible. Validation passed with
`ninja -C build test-shell-visual`,
`./build/test-shell-visual`, full
`ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
targeted `git diff --check`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`.

Discord-safe Ozone Wayland hints extended Orange's client launch environment so
Chromium/Electron/Ozone-capable apps are steered toward Wayland in the same
centralized path that already forces GTK, Qt, SDL, Clutter, and Firefox. Direct
child launches and DBus activation exports now set
`ELECTRON_OZONE_PLATFORM_HINT=wayland` and `NIXOS_OZONE_WL=1`. Follow-up from
real Discord testing removed the global
`CHROME_FLAGS`/`CHROMIUM_FLAGS=--ozone-platform=wayland` injection and added a
Discord launch-command compatibility path that keeps
`ELECTRON_OZONE_PLATFORM_HINT=wayland` but unsets `NIXOS_OZONE_WL`,
`CHROME_FLAGS`, and `CHROMIUM_FLAGS`. Discord cannot fall back to X11 under
Orange because the compositor intentionally does not export a parent `DISPLAY`.
Validation passed with `ninja -C build`,
`./build/test-desktop-entry`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
targeted `git diff --check`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`.

Implemented AppIndicator/KStatusNotifierItem tray support through a session-bus
`org.kde.StatusNotifierWatcher`. Orange now accepts
`RegisterStatusNotifierItem`, normalizes object-path and service/path
registrations, caches standard `org.kde.StatusNotifierItem` properties, tracks
item owner disappearance and item update signals, filters Passive/iconless
items out of the visible tray, and exposes immutable tray snapshots to shell
layout/drawing. The menu bar reserves tray slots left of the built-in Wi-Fi,
sound, battery, search, and clock items; tray icons render from theme icon names
with a fallback glyph; hit testing returns tray-item indices; left-click routes
to `Activate` or menu-first `ContextMenu` for AppIndicator-style items, and
right-click routes to `ContextMenu`.

Validation passed with `ninja -C build test-session-services
test-shell-layout test-shell-visual`,
`./build/test-session-services`,
`./build/test-shell-layout`,
`./build/test-shell-visual`, full
`ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
targeted `git diff --check`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`. The headless smoke still emits
the known sandbox dconf warnings for read-only `/run/user/1000/dconf/user`, but
exits successfully.

Refined the empty-desktop right-click context menu to match the current
macOS/Golden Gate command-menu direction: text-first rows, no leading icon
column, New Folder/Paste/Get Info first, the wallpaper/widgets customization
commands grouped directly after Get Info, and Stacks/sort/clean-up/view
commands grouped together. The menu now uses `Change Wallpaper...` and
`Edit Widgets...`, shows the Stacks checkmark on `Use Stacks` instead of
`Get Info`, shows the current sort detail on `Sort By`, and shows `Grid` on
`Clean Up By`. The compositor action switch was updated to preserve the
expected behavior for every visible row.

Validation passed with `ninja -C build test-shell-layout
test-shell-visual`, `./build/test-shell-layout`,
`./build/test-shell-visual`, rebuilt `orange-render-shell`, a
manual render review of `/tmp/orange-menu-after.png`, full
`ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
targeted `git diff --check`, and `WLR_BACKENDS=headless WLR_RENDERER=pixman
./build/orange --headless --once`. The headless smoke still emits
the known sandbox dconf warnings for read-only `/run/user/1000/dconf/user`, but
exits successfully.

Implemented hidden-by-default Dock auto-hide behavior for `dock_auto_hide=true`
while keeping the existing `Automatically Hide and Show Dock` config/menu
surface. The Dock now slides up from the configured screen edge when revealed
and slides down when dismissed, using shared easing math covered by shell layout
tests. Validation completed with `ninja -C build`, `meson test -C
build --print-errorlogs`, and `WLR_BACKENDS=headless
WLR_RENDERER=pixman ./build/orange --headless --once`.
Dock context menus now render as one-piece soft chat-bubble panels without the
bright white edge glow, with a wider integrated tail painted through the same
glass path as the menu body, pointing back to the clicked Dock item/separator,
a shorter rounded lower tip, top-left/top-right side-Dock tail placement, and a
larger off-Dock gap.
Validation for that refinement passed with `ninja -C build` and
`meson test -C build --print-errorlogs`.
Latest menu refinement removes the shared bright white outline and the added
outside shadow from system menu dropdowns, context menus, and side submenus;
those transient surfaces now separate from content through the glass material
itself. Visual coverage rejects bright source pixels at menu edges, asserts no
outside shadow pixels remain, checks the Dock context bubble tail has no
transparent seam at its base, and verifies oversized side-Dock magnification
does not paint into the menu-bar gap. Validation passed with
`ninja -C build test-shell-visual`,
`./build/test-shell-visual`, `ninja -C build`, and
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`.
Side-Dock separator mouse resize now follows the horizontal divider's visible
movement: dragging the separator down grows a left/right Dock and dragging it
up shrinks it, with `ns-resize` used for bottom and side Dock separators. Shell
layout coverage now asserts side-Dock vertical drag math, shrink behavior, and
horizontal-drift stability. Validation passed with
`ninja -C build test-shell-layout`,
`./build/test-shell-layout`, `ninja -C build`, and
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`.
Genie and Scale window minimize effects now animate a temporary screenshot
image between the real window rectangle and its Dock target. The compositor
captures the view from the composed scene before hiding it, retains that
screenshot while minimized, and redraws the effect through the overlay buffer
until completion.
Scale uses fitted screenshot interpolation; the initial Genie pass reused the
screenshot with a tapered curved clip toward the configured Dock edge. Validation
passed with `ninja -C build test-shell-layout`,
`./build/test-shell-layout`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Follow-up minimize refinement makes the retained screenshot a real Dock
thumbnail item immediately left of Trash instead of only a transient animation
target. Minimized thumbnail slots are non-reorderable, click-to-restore the
matching view, and are used as the Genie/Scale target for both minimize and
restore. The animation lifecycle now redraws the Dock after landing, avoids
duplicating the static thumbnail while the flight is active, routes app-icon
restore through the same minimize state machine, and guards Cairo surface
creation to avoid crashes on repeated Genie/minimize draws. Validation passed
with `ninja -C build test-shell-layout`,
`./build/test-shell-layout`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Restore/re-minimize crash follow-up defers focus for Dock-thumbnail restores
until the restore animation completes, instead of focusing a view while its real
scene node is still hidden behind the screenshot animation. Cancelling an
in-flight restore now always re-enables the real scene node before another
minimize capture can run, and starting a new minimize while a restore is active
suppresses the pending restore-focus request. Validation passed with
`ninja -C build`, `./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
The wlroots buffer ownership crash reported as
`wlr_buffer_drop: Assertion '!buffer->dropped' failed` is fixed by releasing
animation-held screenshot references with `wlr_buffer_unlock()` instead of
`wlr_buffer_drop()`. Orange still drops the producer-owned retained thumbnail
buffer when the view no longer needs it. Dock screenshot thumbnails no longer
draw a border or backing frame around the image. Validation passed with
`ninja -C build`, `./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Genie now uses its own screenshot warp renderer instead of sharing the Scale
paint path with only a tapered clip. The edge nearest the configured Dock
position leads while the opposite edge trails, and the overlay damage bounds
cover the warped strip envelope. Scale remains the existing fitted screenshot
interpolation. Validation passed with `ninja -C build`,
`./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Minimized Dock thumbnails now participate in the same Dock magnification visual
rect calculation as launcher and Trash items, including auto-hide overlay Dock
rendering. The Dock hover label for a minimized thumbnail is populated from the
window title, with a generic fallback only when the client has no title. Layout
coverage asserts minimized thumbnail magnification and title labeling.
Validation passed with `ninja -C build`,
`./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Dock screenshot thumbnails now use a larger thumbnail content rect than the
previous inset and paint with Cairo's highest-quality image filter. The
minimize/restore landing target for thumbnail slots uses the same larger rect,
while launcher-icon targets keep their prior inset. Layout coverage asserts the
new thumbnail footprint is larger than the previous `item / 12` inset.
Validation passed with `ninja -C build`,
`./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Long app names such as `Firefox Web Browser` now have more room in the menubar
active-app tab, and Dock hover labels keep their full measured label while
clamping/shrinking only when the bubble would exceed the screen. Layout coverage
asserts the longer Firefox desktop-entry name is preserved for Dock labels and
gets a wider active-app tab than the shorter `Firefox` label. Validation passed
with `ninja -C build`, `./build/test-shell-layout`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Follow-up Dock hover label clipping fix widens incremental Dock redraw bounds
to the full output width so long hover bubbles are not clipped by partial
repaints, including with magnification enabled. Labels that are wider than the
screen now render with an explicit `...` after font shrinking instead of being
hard-clipped. Layout coverage asserts the shared Dock dirty-bounds helper spans
the output width for bottom and side docks. Validation passed with
`ninja -C build test-shell-layout`,
`./build/test-shell-layout`,
`ninja -C build test-shell-visual`,
`./build/test-shell-visual`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build shell-layout shell-visual --print-errorlogs`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check`.
Menu surfaces now scale with the GNOME DisplayConfig/UI scale choices written
through `orange_config_apply_ui_scale`: system-menu dropdowns, app-menu
dropdowns, desktop context menus, Dock menus, and Dock submenus use a
menu-specific layout scale derived from resolution scale times the configured
UI multiplier. Regression coverage checks 100%, 125%, 150%, 175%, and 200%
scale steps. Validation passed with targeted `test-shell-layout` and
`test-shell-visual` builds/binaries, full `ninja -C build`,
targeted and full Meson tests, and targeted `git diff --check`.
Desktop icon grid follow-up gives desktop items a larger, distributed grid
inside the usable work area with near-edge horizontal columns, a screen-edge
inset large enough to keep selected-icon highlights fully on screen, and
compact vertical row spacing while keeping clearance from the menu bar and
Dock. Grid metrics and final clamps now share the same desktop safe area,
including extra clearance for Dock hover magnification on bottom, left, and
right docks, plus coverage for configured Dock-size growth, so icons move out
of the way when the Dock grows.
Follow-up drag refinement keeps `desktop_sort_by=none` saved positions at their
exact dragged coordinates instead of snapping them onto the wide distributed
grid during every drag redraw. Explicit `snap-to-grid` mode still resolves
duplicate saved cells through the grid. This prevents icons from jumping large
horizontal distances during side-to-side moves while preserving the wider
default desktop spacing.
Runtime drag follow-up switches the desktop back to free placement on the first
real drag movement when an automatic/snap sort mode was active, so dragging an
icon after using Clean Up By no longer remains locked to the wide snap columns.
Overlap follow-up keeps free-placement desktop icons from stacking their
selection/highlight hit regions on top of each other. The later icon is nudged
to the nearest non-overlapping local position inside the same safe work area,
instead of snapping to a far grid column.
Validation passed with `ninja -C build test-shell-layout`,
`./build/test-shell-layout`, `ninja -C build`,
`.venv-wlroots-build/bin/meson test -C build --print-errorlogs`,
and targeted `git diff --check -- src/compositor.c src/shell.c tests/test_shell_layout.c PROJECT_STATE.md`.
The system `meson test` binary is incompatible with this build directory's
metadata; use the repo-local `.venv-wlroots-build/bin/meson` for this build.

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
  Adwaita for the packaged Void desktop.
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
- Slightly larger desktop items with wider dark translucent selected/drag
  highlights, double-click desktop item opening, and progress cursor feedback
  while desktop item open requests are pending.
- Finder-style inline rename for selected desktop files/folders by clicking
  their label, with Return/click-away commit and Escape cancel.
- Wider Dock separator context menu sizing so the Minimize Using effect detail
  does not clip.
- Image-file desktop previews use the normal desktop-file right-click menu even
  when their enlarged desktop target overlaps widget space.
- Desktop folder right-clicks take priority over neighboring icons' widened
  transparent hit padding, and stale context menus are cleared before resolving
  the next right-click target. Desktop item right-clicks are routed to the shell
  before client surfaces can intercept them, so folders keep their file context
  menu.
- Desktop background New Folder now creates exactly one unique folder directly
  through the compositor instead of launching a shell loop, and the desktop
  background menu includes Paste Item for file clipboard contents.
- Desktop file Copy and multi-selection Copy now publish GNOME Files-compatible
  `x-special/gnome-copied-files` clipboard payloads with file URIs.
- Desktop inline rename editing now lives in `src/desktop.c`/`orange/desktop.h`;
  file renames select the filename stem before the extension, while directories
  and hidden dotfiles select the full name.
- Desktop file context menus now include a highlighted side Open With submenu
  populated from freedesktop MIME associations through GIO `GAppInfo`; it shows
  up to 16 associated app choices and choosing one launches the selected desktop
  app ID with the file URI.
- GNOME Files/Nautilus launch paths from the Dock, Trash, Show in Files, and
  Properties actions now unset Orange's GTK theme/icon variables and advertise a
  GNOME desktop environment so Nautilus can use the user's GNOME/Adwaita icon
  theme instead of Orange's shell theme.
- Menu shortcut labels now use Linux-style ASCII chords such as `Ctrl`,
  `Alt`, and `Ctrl+Alt` instead of macOS modifier labels or glyphs.
- The active app menu bar shows Orange's built-in File/Edit/View/Go/Window/Help
  shell menu profile for Files. Every other app shows detected native menu tabs
  only; when none are detected, the left side shows only the bold focused app
  name.
- Unknown focused apps now receive a conservative macOS-style app-menu fallback
  with App/File/Edit/View/Window/Help headings while native DBus and AT-SPI menu
  discovery retries briefly after focus changes.
- Orange registers session services from `src/session_services.c`: the xdg
  Settings portal color-scheme bridge and a focused
  `org.gnome.Mutter.DisplayConfig` compatibility service for GNOME Control
  Center's Displays panel. Settings launches stay on Orange's session bus
  instead of being wrapped in private per-launch DBus sessions. Generic
  Settings launchers prefer direct GNOME Settings panels before the bare GNOME
  Control Center fallback, avoiding slow WWAN/ModemManager startup on generic
  launches. DisplayConfig monitor and mode property dictionaries match GNOME
  Control Center 46's expected GVariant shapes for privacy-screen state and
  refresh-rate mode, and `ApplyMonitorsConfig` validates and applies output
  scale/orientation changes through wlroots output state commits while also
  persisting the chosen UI scale to Orange's `desktop_icon_scale`,
  `dock_scale`, and `dock_icon_scale` config keys.
- Window placement barriers for the menu bar and Dock now use wlroots effective
  output coordinates directly after output scaling. Move/resize clamping,
  maximize/fullscreen sizing, and initial app placement no longer divide the
  shell work area by output scale a second time.
- The initial desktop-volume scan avoids GIO/GVFS work during compositor
  startup; external/removable volume probing is deferred to the normal timer
  after Wayland and DBus services are live.
- Dock and desktop app launches export the compositor's Wayland client
  environment to DBus activation after the socket is created, and the 0.20
  pointer button handler uses the Wayland button-state enum so Dock releases
  are handled on the wlroots 0.20 event type.
- Direct nested runs such as `./build/orange` now launch clients
  with Orange's `WAYLAND_DISPLAY`, force common toolkits to Wayland, unset the
  parent `DISPLAY`, and use private per-launch DBus sessions by default so
  GNOME/GApplication single-instance apps do not activate an already-running
  parent-session app. `ORANGE_CLIENT_PRIVATE_DBUS=0` disables the private DBus
  wrapper for debugging.
- The launcher child resets `SIGCHLD` to default and clears the inherited
  signal mask before `exec`, preventing GLib/Tracker helpers from failing with
  `waitid ... No child processes` after inheriting Orange's compositor-side
  `SA_NOCLDWAIT` setup.
- Startup now creates missing XDG user-directory defaults and an empty
  `.gtk-bookmarks` placeholder without overwriting existing files, reducing
  Nautilus/Tracker warnings in sparse direct-run or VM home directories.
- Right-click context menus for desktop shortcuts, widgets, Dock items, and
  empty desktop background.

#### Validation

- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" ninja -C build`
  passed, with existing wlroots/compositor warnings.
- Reconfigured and rebuilt `build/orange` with the repo-local
  wlroots 0.20 environment after the Settings launch and DisplayConfig fixes;
  the rebuilt binary contains no `build/orange-settings` launch strings and
  uses the GNOME-first `gnome-control-center display`, then
  `gnome-control-center system` fallback order. `meson test -C
  build --print-errorlogs` passed (11/11 tests).
- A `dbus-run-session` headless smoke test called
  `org.gnome.Mutter.DisplayConfig.ApplyMonitorsConfig` with GNOME's scale and
  orientation payload. Verify and temporary apply returned `()`, and a
  subsequent `GetCurrentState` reported scale `1.25` and transform `uint32 1`.
  The smoke config file also contained `desktop_icon_scale=1.25`,
  `dock_scale=1.25`, and `dock_icon_scale=1.25`.
- Rebuilt `build/orange` after the scaled window-barrier fix.
  `meson test -C build --print-errorlogs` passed (11/11 tests).
  A scaled `dbus-run-session` DisplayConfig smoke applied scale `1.25`,
  returned `()`, and reported scale `1.25` in `GetCurrentState`.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build shell-layout shell-visual --print-errorlogs`
  passed.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (9/9 tests).
- `meson compile -C build` passed.
- `meson test -C build --print-errorlogs` passed (10/10 tests).
- `.venv-wlroots-build/bin/meson setup --reconfigure build`
  passed after adding `src/desktop.c` and `tests/test_desktop.c`.
- `.venv-wlroots-build/bin/meson compile -C build` passed cleanly.
- `.venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (11/11 tests).
- `git diff --check` passed for the files touched by the desktop rename,
  Open With, GNOME Files launch, and shortcut-label changes.
- `dbus-run-session` headless smoke passed: Orange owned
  `org.gnome.Mutter.DisplayConfig`, `GetCurrentState` returned the headless
  monitor, and `Properties.GetAll` returned `ApplyMonitorsConfigAllowed=true`,
  `NightLightSupported=false`, `PanelOrientationManaged=false`, and
  `PowerSaveMode=0`.

#### Tests Added

- Cursor config load/save coverage.
- Persisted desktop icon position load/save coverage.
- Custom desktop position layout coverage.
- Desktop default item sizing, rename label hit target, Dock separator menu
  width, image-file/folder context-menu hit behavior including stale-menu
  clearing and client-surface interception, and dark
  selected/marquee highlight coverage.
- Desktop rename unit coverage for file stem replacement, directory/hidden-file
  full-name selection, delete/backspace behavior, UTF-8 backspace, and invalid
  slash input.
- Shell layout and visual coverage for the desktop file Open With side submenu
  count, fallback label, submenu affordance, shifted separators, shifted
  shortcuts, hit testing, overlay bounds, and Linux-style `Ctrl`/`Alt` shortcut
  labels.
- App menu layout coverage for no synthetic fallback tabs, native imported tab
  preservation, and desktop Paste Item menu metadata.
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
- Desktop background right-click context menu (9 items, 2 separators):
  New Folder, Paste Item, Get Info, Change Wallpaper..., Edit Widgets...,
  Use Stacks, Sort By, Clean Up By, Show View Options.
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
  expands the pill into the launcher panel and shows unified local/web results.
- **Mode-button transform**: Clicking any adjacent mode button transforms the
  pill into a smaller glass app-launcher panel anchored to the same
  search/header position and narrows results to that browse mode.
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
  desktop "Change Wallpaper..." action now launch
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

- **GNOME Control Center crash on launch fixed**: `gnome-control-center` crashed
  with assertion `G_IS_OBJECT(object)` in `cc_object_storage_add_object` when
  launched from the Dock. The `FAST_ENV` macro set
  `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/tmp/orange-no-system-bus`, which blocked
  system bus connections. The network panel's `nm_client_new(NULL, NULL)` returned
  NULL, and passing NULL to `cc_object_storage_add_object` triggered the assertion.
  Removed the `DBUS_SYSTEM_BUS_ADDRESS` override so gnome-control-center accesses
  the real system bus.
- **Vulkan dependency made optional**: The `vulkan` dependency in `meson.build`

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

### wlroots 0.20 Dev Install

#### Implemented

- Installed wlroots 0.20.1 into the repo-local development prefix
  `.local/wlroots-0.20`; the pkg-config dependency name is `wlroots-0.20`.
- Added `.local/`, `.venv-wlroots-build/`, and `build/` to the
  local ignore list.
- Updated Orange's Meson dependency to `dependency('wlroots-0.20')`.
- Ported Orange directly to wlroots 0.20 APIs without a compatibility wrapper:
  texture readback replaces renderer readback, backend/layout constructors use
  the 0.20 signatures, xdg-shell geometry uses current state, pointer axis
  notifications pass relative direction, and tablet input uses the 0.20 device
  enum.
- Added explicit teardown for server-owned wlroots listeners before destroying
  wlroots objects, which fixes wlroots 0.20 listener-list assertions during
  headless shutdown.
- Reinstalled the local wlroots shared libraries with `-Wl,-rpath,$ORIGIN` so
  `libwlroots-0.20.so` can resolve private prefix libraries such as
  `libdisplay-info.so.4` without `LD_LIBRARY_PATH` or a runtime wrapper.

#### Validation

- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig pkg-config --modversion wlroots-0.20`
  returned `0.20.1`.
- `env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" ninja -C build`
  passed.
- `env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (8/8 tests).
- `ldd build/orange` resolves `libdisplay-info.so.4` from
  `.local/wlroots-0.20/lib/x86_64-linux-gnu` with no missing libraries.
- `WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/orange --headless --once --width 800 --height 600`
  exited 0 after the local runpath fix.

### Void Package And KVM VM

#### Implemented

- Added a local `packaging/void/srcpkgs/orange/template` that builds Orange with
  Meson from a copied working tree and installs `/usr/bin/orange`,
  `/usr/bin/orange-session`, `/usr/share/wayland-sessions/orange.desktop`,
  `/usr/share/orange/orange.conf`, and the tracked wallpaper assets.
- The Orange Void package is now intended to be installable as the user's
  desktop entrypoint after adding the local repository. Installing `orange`
  pulls `gdm`, `gnome-core`, `gnome-apps`, Nautilus/GVFS, Firefox, Ghostty,
  Adwaita icons/fonts/backgrounds, and Adwaita GTK light/dark theme packages.
  The package also installs bundled wallpapers under
  `/usr/share/backgrounds/orange`.
- `/usr/bin/orange-session` sets Adwaita GTK/icon/cursor defaults, clears
  inherited GTK module injection, enables Firefox Wayland mode, exports
  Wayland toolkit backends, exports Chromium/Electron Ozone hints, and defaults
  `ORANGE_TERMINAL` to `ghostty`.
- The Orange Void package now depends on Void's packaged `wlroots0.20-devel`
  binary package instead of the `wlroots-devel` metapackage. The metapackage
  template in the current `void-packages` checkout points at `wlroots0.20` while
  that source target is not present locally, so using the concrete binary
  package avoids accidentally building or resolving the broken metapackage.
- Added `scripts/build-void-vm.sh`, which clones/uses `void-packages`, overlays
  only the local `orange` template, bootstraps `xbps-src`, builds the Orange
  package, downloads and verifies Void's official minimal `ROOTFS`
  tarball, installs the VM runtime package delta plus Orange into that base,
  installs GRUB, and exports a bootable VM disk image.
- The Void VM builder now targets glibc explicitly with `ORANGE_VOID_ARCH`
  defaulting to `x86_64` and `ORANGE_VOID_REPO` defaulting to Void's
  `current` repository. It rejects `*-musl` arch values and `current/musl`
  repositories for this path.
- On non-Void hosts, the VM builder downloads Void's official xbps-static host
  tools under `~/.cache/orange-void-vm/xbps-static` before running `xbps-src`.
  Those tools are host bootstrap helpers only; the guest/package ABI remains
  glibc.
- The Void VM builder accepts pinned/mirrored rootfs inputs through
  `ORANGE_VOID_LIVE_BASE`, `ORANGE_VOID_ROOTFS_URL`,
  `ORANGE_VOID_ROOTFS_TARBALL`, and `ORANGE_VOID_ROOTFS_SHA256`.
- After extracting the minimal rootfs, the VM builder updates the rootfs `xbps`
  package, then updates the full rootfs base package set before installing the
  runtime package set and Orange. This handles older rootfs snapshots whose
  installed `xbps` cannot read current Void repository metadata and prevents new
  runtime packages from linking against stale base libraries.
- The VM runtime install list now treats Void's minimal rootfs as the base
  system: it no longer explicitly reinstalls rootfs-provided `bash`, `shadow`,
  `dhcpcd`, or `openssh`; it uses Void's current `qemu-ga` package plus
  `/etc/sv/qemu-ga` service name for the QEMU guest agent; it uses Void's
  `dejavu-fonts-ttf` font package name; it installs Void's `gdm` package
  rather than Debian's `gdm3` package name; it includes `gnome-core`,
  `gnome-apps`, Firefox, Ghostty, Nautilus/GVFS, and Adwaita packages; and
  it installs Void's `grub` package for guest bootloader setup.
- The VM builder now writes `/etc/dracut.conf.d/orange-vm.conf` inside the guest,
  disables host-only initramfs generation, includes QEMU virtio/storage/display
  drivers, detects the installed `/usr/lib/modules/*` kernel version, and runs
  `dracut --force` before copying kernel artifacts out of `/boot`.
- The VM builder now creates an MBR disk with one ext4 root partition, records
  the root filesystem UUID in `/etc/fstab`, writes `/boot/grub/grub.cfg` with
  a UUID-based root argument, installs GRUB for BIOS boot, and converts the raw
  disk to the selected output format.
- The Void VM image format is selectable with `ORANGE_VOID_IMAGE_FORMAT`.
  `qcow`/`qcow2` produce qcow2, `vmdk` produces VMDK, and `vdi` produces
  VirtualBox VDI. `vti` is accepted as an alias for `vdi` because QEMU does not
  expose a literal `vti` output format.
- The guest now starts Orange through GDM autologin as the `orange` user instead
  of a root-owned `orange` runit service. The builder writes
  `/etc/gdm/custom.conf` with `AutomaticLogin=orange`, writes
  `/var/lib/AccountsService/users/orange` with `Session=orange`, enables
  `dbus`, `elogind`, `seatd`, `polkitd`, `qemu-ga`, `sshd`, and `gdm`, and
  patches the GDM runit script to wait for `dbus`, `elogind`, and `polkitd` when
  present. This gives wlroots a real login session/seat for DRM startup.
- Added `scripts/run-void-vm.sh`, which boots that image through GRUB with QEMU, KVM
  acceleration, virtio GPU/input/network devices, and a GTK display suitable for
  WSLg. The guest includes OpenSSH and QEMU forwards host TCP port 2222 to the
  guest's port 22. The run script now creates an explicit `qemu-xhci` USB
  controller before attaching `usb-tablet`, avoiding QEMU's missing `usb-bus`
  error while keeping absolute pointer input available. It uses
  `ORANGE_VOID_IMAGE_FORMAT` when set and otherwise infers the QEMU drive format
  from the image extension.
- Kept `scripts/build-vm.sh` and `scripts/run-vm.sh` as compatibility entry
  points that delegate to the Void-specific scripts.
- Documented the package-first VM flow in `docs/VOID_VM.md`.
- Documented the normal Void install path and the required one-time runit
  service links for `dbus`, `elogind`, `seatd`, `polkitd`, and `gdm`.

#### Validation

- `bash -n scripts/build-vm.sh scripts/run-vm.sh scripts/build-void-vm.sh scripts/run-void-vm.sh`
  passed after adding GRUB boot, partitioned image creation, image-format
  normalization, and new package-copy image excludes.
- Extracting the guest chroot heredoc from `scripts/build-void-vm.sh` and piping
  it to `/bin/sh -n` passed after adding the GRUB install and `grub.cfg`
  generation path.
- `env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson setup build --reconfigure`
  passed and registered the `vm-script-syntax` Meson test.
- `env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build vm-script-syntax --print-errorlogs`
  passed.
- `ORANGE_VOID_IMAGE_FORMAT=vti ORANGE_VOID_IMAGE=/tmp/orange-missing.vti ORANGE_VM_ALLOW_TCG=1 scripts/run-void-vm.sh`
  reached the expected missing-image guard, confirming the `vti` alias is
  accepted by the runner rather than rejected as an unknown format.
- Local `qemu-img --help` lists `qcow`, `qcow2`, `vdi`, and `vmdk`, but not
  `vti`; the scripts therefore map `vti` to QEMU's `vdi` format.
- The cached Void package tree has `srcpkgs/grub/template`; on x86_64 it uses
  native `pc` platform support, matching `grub-install --target=i386-pc`.
- `bash -n scripts/build-vm.sh scripts/run-vm.sh scripts/build-void-vm.sh scripts/run-void-vm.sh`
  passed after adding the GDM autologin setup.
- Extracting the guest chroot heredoc from `scripts/build-void-vm.sh` and piping
  it to `/bin/sh -n` passed after adding the GDM autologin setup.
- `git diff --check -- scripts packaging docs/VOID_VM.md PROJECT_STATE.md`
  passed after adding the GDM autologin setup.
- `xbps-install.static -h` shows package names are optional (`[PKGNAME...]`) and
  `-u` is the update mode, matching the rootfs-wide `xbps-install -Suy -r
  "$mount_dir"` call added after the `xbps` self-update.
- `timeout 3s qemu-system-x86_64 -machine q35,accel=tcg -display none -nodefaults -device qemu-xhci,id=usb0 -device usb-tablet,bus=usb0.0 -S`
  ran until timeout without the previous `No 'usb-bus' bus found for device
  'usb-tablet'` error, confirming the controller/tablet pairing is accepted by
  local QEMU.
- `rg -n "[[:blank:]]$" scripts/build-void-vm.sh scripts/run-void-vm.sh docs/VOID_VM.md packaging/void/README.md PROJECT_STATE.md`
  returned no matches, covering the untracked VM files that `git diff --check`
  does not inspect.
- `env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (8/8 tests) after the minimal-rootfs builder update, with existing
  wlroots 0.20 compiler warnings in `src/compositor.c`.
- The rootfs checksum verifier was fixed so status output from the checksum
  helper cannot contaminate the captured expected hash, and checksum download
  failures cannot fall through to a stale local checksum file.
- A controlled cached-rootfs dry path with local checksum input reached the
  stubbed `xbps-src` stage, proving the known-good rootfs hash is accepted.
- A controlled cached-rootfs dry path with a deliberately wrong checksum stopped
  with the expected checksum mismatch before `xbps-src`.
- `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -s wlroots0.20`
  returned `wlroots0.20-0.20.1_1` and `wlroots0.20-devel-0.20.1_1`,
  confirming Void provides the desired packaged wlroots development dependency.
- `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -s elogind-devel`
  returned `elogind-devel-252.39_1`, confirming the explicit build dependency
  for Orange's optional `libsystemd`/elogind integration.
- A controlled cached-rootfs dry path with a stubbed `xbps-src` showed the VM
  builder now runs `binary-bootstrap`, `bootstrap-update`, `clean orange`, and
  `-1 pkg orange`, with no `pkg wlroots0.20`. The `bootstrap-update` step keeps
  the cached masterdir `xbps` package current with Void repository metadata, the
  `-1` flag forces missing dependencies to fail instead of building dependency
  sources, and the script removes a stale local `srcpkgs/wlroots0.20` overlay
  from the script-managed checkout.
- After adding a Meson feature check for `sd_session_get_start_time()` and
  cleaning stale Orange xbps build state before packaging, a real
  `./xbps-src -A x86_64 -1 pkg orange` pass succeeded. Void/elogind reported
  `sd_session_get_start_time` unavailable, built with the fallback session time
  path, linked against packaged `wlroots0.20`, and created
  `orange-0.1.0_1.x86_64.xbps`.
- `./xbps-src -A x86_64 bootstrap-update` passed in the cached `void-packages`
  checkout after the repository reported that the `xbps` package must be
  updated.
- A subsequent `./xbps-src -A x86_64 clean orange` and
  `./xbps-src -A x86_64 -1 pkg orange` pass succeeded after `bootstrap-update`.
- The VM image population step now runs `xbps-install -Suy -r "$mount_dir" xbps`
  against the extracted rootfs before installing runtime packages, addressing the
  reported post-package error where the minimal rootfs asked for `xbps` to be
  updated first. It then runs a full `xbps-install -Suy -r "$mount_dir"` rootfs
  update before installing runtime packages.
- The cached `void-x86_64-ROOTFS-20250202.tar.xz` contains `/usr/bin/bash`,
  `/usr/bin/useradd`, `/etc/shadow`, and the XBPS package database; querying the
  extracted package database showed `bash-5.2.32_1`, `shadow-4.8.1_3`,
  `dhcpcd-10.1.0_1`, and `openssh-9.9p1_1`, confirming those packages are part
  of the minimal base and should not be reinstalled as runtime deltas.
- `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -s qemu-ga`
  returned `qemu-ga-11.0.1_1`; the same query for `qemu-guest-agent` returned no
  package. `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -f qemu-ga`
  showed `/etc/sv/qemu-ga/run`, confirming the runit service name.
- `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -s dejavu`
  returned `dejavu-fonts-ttf-2.37_3`; the same query for `font-dejavu` returned
  no package.
- Exact `xbps-query.static -R -M --repository=https://repo-default.voidlinux.org/current -p pkgver`
  checks passed for the remaining remote runtime packages: `linux`, `qemu-ga`,
  `dbus`, `elogind`, `seatd`, `ghostty`, `adwaita-icon-theme`,
  `hicolor-icon-theme`, `dejavu-fonts-ttf`, `gdm`, `polkit`, and `mesa-dri`.
- Void current provides `gdm-48.0_2`, while queries for `gdm3` returned no
  package. `xbps-query --files gdm` showed `/etc/sv/gdm/run`, and the cached
  Void `gdm` template shows it is built with `-Dlogind-provider=elogind`.
- `xbps-query --cat=/etc/gdm/custom.conf gdm` confirmed the packaged GDM
  configuration uses a `[daemon]` section; `xbps-query
  --cat=/usr/share/accountsservice/user-templates/standard accountsservice`
  confirmed AccountsService user files use `Session=`.
- The cached Void templates show `linux` depends on `linux-base`, `linux-base`
  depends on `dracut`, and dracut's kernel post-install hook creates
  `boot/initramfs-${VERSION}.img`. The build script now invokes dracut
  explicitly in the guest chroot so `/boot/initramfs-*` exists before the copy
  step.
- The cached rootfs package database showed `kmod-31_1` and `libkmod-31_1`,
  while Void current provides `kmod-34.2_1`, `libkmod-34.2_1`, and
  `dracut-109_1`. The full rootfs update is required so dracut's helper binaries
  do not fail with missing `LIBKMOD_33` symbols from stale rootfs libraries.
- `./scripts/run-void-vm.sh` fails early with the expected missing-image message
  when `scripts/build-void-vm.sh` has not been run.
- The full `./scripts/build-void-vm.sh` flow was not run to completion in this
  sandbox because image assembly needs sudo, loop mounts, and chroot access.
- The official `xbps-static-latest.x86_64-musl.tar.xz` archive was inspected and
  contains both unsuffixed `xbps-*` commands and `.static` aliases, matching the
  PATH-based host bootstrap used by the VM builder.
- QEMU tooling exists on this host: `qemu-system-x86_64` 8.2.2,
  `qemu-img` 8.2.2, and `mkfs.ext4` 1.47.0.

### wlroots 0.20 xdg-shell Configure Crash Fix

#### Implemented

- Centralized xdg-toplevel configure-producing calls in guarded helpers that
  only call wlroots setters once `wlr_xdg_surface.initialized` is true.
- Deferred initial xdg-toplevel configure from the surface commit listener to a
  Wayland idle callback when wlroots has not finished initializing the xdg
  surface yet. This addresses the wlroots 0.20 assertion at
  `wlr_xdg_surface_schedule_configure: surface->initialized`.
- Removed Orange's direct `wlr_xdg_surface_schedule_configure()` call from
  minimize handling. Minimize now only updates Orange scene/focus state and uses
  normal activation configure when the surface is ready.
- Routed resize, focus activation, maximize, fullscreen, initial sizing, and WM
  capability configure requests through the guarded helpers.
- Added empty-output-list checks around maximize/fullscreen fallback output
  selection so window-state paths do not call `wl_container_of()` on an empty
  list.
- Made xdg-toplevel listener teardown idempotent with `orange_listener_remove()`
  and cancel pending initial-configure idle callbacks before freeing a view.
- Converted zero-vararg `wlr_log()` calls in `src/compositor.c` to explicit
  `%s` format calls, removing GCC ISO C variadic macro warnings.

#### Validation

- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:/home/samuel/orange-wlroots/.venv-wlroots-build/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin ninja -C build -t clean`
  cleaned the local wlroots 0.20 build outputs.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:/home/samuel/orange-wlroots/.venv-wlroots-build/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin .venv-wlroots-build/bin/meson setup build --reconfigure`
  passed and repaired stale Meson metadata. The reconfigure was rerun with
  absolute `.local/wlroots-0.20/bin` in `PATH` so Ninja records an executable
  absolute `wayland-scanner` path.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:/home/samuel/orange-wlroots/.venv-wlroots-build/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin ninja -C build`
  passed from a clean rebuild with no compiler warnings.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:/home/samuel/orange-wlroots/.venv-wlroots-build/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (9/9 tests).
- `git diff --check -- src/compositor.c` passed.
- `rg -n 'strcpy\(|strcat\(|sprintf\(|gets\(|wlr_xdg_surface_schedule_configure|wl_container_of\([^\n]*(outputs|views|keyboards)\.next' src include tests tools`
  found no unsafe string-copy calls or direct xdg-surface schedule calls. The
  remaining `outputs.next` matches are guarded by `wl_list_empty()` checks.
- A manual headless xdg-shell smoke test launched Orange with wlroots 0.20.1,
  connected two Wayland terminal clients to the generated `WAYLAND_DISPLAY`, waited for
  both to map/exit, and found no assertion, abort, `surface->initialized`, or
  failure markers in `/tmp/orange-xdg-smoke.log`. The first run was blocked by
  sandbox socket restrictions, then passed outside the sandbox.

#### Current Blockers

- This sandbox did not expose `/dev/kvm` during validation. WSL2 can expose KVM
  when nested virtualization is enabled, so validate the KVM run path in the
  user's interactive WSL environment.
- This sandbox did not expose `/dev/loop-control`, so the root image mount path
  cannot be validated here.
- `sudo -n true` fails in this sandbox, so the image assembly path cannot mount
  or chroot here.

### Global App Menu Discovery

#### Implemented

- Fixed client-surface click focus/raise by storing the owning
  `orange_view` on mapped toplevel root surfaces. Pointer hit-testing now
  resolves clicked client surfaces back to the real view object before calling
  `focus_view()`, and popup parenting resolves through the popup root's
  toplevel view instead of relying on root surface data to hold a scene tree.
- Added an `orange_app_menu_registry` helper for AppMenu registrations, with
  update/unregister/generation tracking and PID-based lookup.
- Orange now owns `com.canonical.AppMenu.Registrar` at
  `/com/canonical/AppMenu/Registrar`, accepts Fildem/appmenu-style
  `RegisterWindow`, `UnregisterWindow`, `GetMenuForWindow`, and `GetMenus`,
  and invalidates the app-menu cache when registrations change.
- Focused-client global menu import now prefers registered DBusMenu exporters
  whose DBus sender PID matches the focused Wayland client PID, parses the
  exported `com.canonical.dbusmenu.GetLayout` tree, and invokes imported items
  through `com.canonical.dbusmenu.Event`.
- GTK/GMenu probing remains as a secondary exporter path, with a few additional
  conventional object paths checked from the application bus name.
- Added a no-module fallback through AT-SPI DBus probing. When no
  DBusMenu/GMenu exporter is found, Orange connects to the accessibility bus,
  matches the focused Wayland client PID to an AT-SPI application, scans a
  bounded accessible subtree, and exposes named actionable controls as an
  `Actions` tab. Clicking those items dispatches `org.a11y.atspi.Action.DoAction`.
- Current Orange app-menu discovery does not install or inject GTK exporter
  modules. It uses DBusMenu/GMenu/GAction exporters that clients expose
  themselves, then AT-SPI and built-in fallback profiles.
- Orange now falls back from AppMenu registrar entries to PID-matched GMenu
  discovery: it lists well-known session bus names owned by the focused Wayland
  client process and probes their `org.gtk.Menus`/`org.gtk.Actions` exports.
  This handles GTK/GApplication apps whose menus are exported on DBus but never
  call the old `RegisterWindow` AppMenu registrar path.
- If no GMenu model is exported, Orange now also imports useful focused-PID
  `GAction` names from `/org/gtk/Actions` or `/org/gtk/actions` into an
  Actions app-menu tab. This provides an automatic no-module path for GTK4 and
  libadwaita apps that expose actions but no traditional menubar.
- Failed native app-menu discovery is cached by focused app id, focused PID,
  and AppMenu registry generation, not by the changing window title/label. A
  missing DBusMenu/GMenu/GAction export is retried at most once per second for
  five seconds after focus so late GTK exports can appear without probing DBus
  on every redraw; AT-SPI fallback remains one-shot per focus target.
- Apps without a native exporter now get a cheap built-in fallback profile with
  App, File, Edit, View, Window, and Help tabs. Browser and Files profiles keep
  their specialized History/Bookmarks/Go labels. The macOS-style system menu
  remains unchanged; no separate Display Settings row was added.
- Earlier local sandbox bridge experiments were removed from the repository.
  Sandboxed apps now participate only when their permissions allow DBusMenu,
  GMenu, GAction, or AT-SPI access to the relevant session services.

#### Validation

- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" ninja -C build`
  passed with no compiler warnings after the app-menu changes.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" ninja -C build`
  passed after the click focus/raise fix.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build app-menu shell-layout headless-custom-startup --print-errorlogs`
  passed (3/3 tests).
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH="/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (10/10 tests).
- A private `dbus-run-session` smoke test launched Orange headless, introspected
  `com.canonical.AppMenu.Registrar`, called `RegisterWindow 42 /com/example/Menu`,
  and verified `GetMenus` returned `/com/example/Menu`. The sandbox blocked the
  private DBus socket on the first run; the same smoke passed outside the
  sandbox.
- `meson compile -C build` passed after reconfiguring the build
  with the repo-local wlroots 0.20 pkg-config path.
- `meson test -C build app-menu shell-layout headless-custom-startup --print-errorlogs`
  passed (3/3 tests).
- A temporary headless Orange instance on the real session bus owned
  `com.canonical.AppMenu.Registrar`; host and Flatpak `gdbus` calls to
  `org.freedesktop.DBus.Peer.Ping` and `GetMenus` succeeded.
- Live diagnosis against the running Orange registrar showed
  `GetMenus` returning an empty map without a direct registration. A
  temporary GTK3 `GtkApplication` exported `org.gtk.Menus` at
  `/org/orange/MenuTest/menus/menubar`, confirming that direct GMenu discovery
  is needed on Wayland when `RegisterWindow` is not emitted.
- The Void package template does not depend on GTK menu exporter modules; the
  baseline Orange app-menu mechanism is Wayland/client-exporter focused.
- `env -u GTK_THEME -u GTK_ICON_THEME XDG_CURRENT_DESKTOP=GNOME:Unity:ubuntu XDG_SESSION_DESKTOP=gnome DESKTOP_SESSION=gnome GNOME_DESKTOP_SESSION_ID=this-is-deprecated gnome-control-center --list`
  succeeds locally and lists the `display` panel on GNOME Control Center 46.7;
  Orange's settings commands keep using that wrapper when launching GNOME
  Control Center under Orange.
- `meson compile -C build` passed after the fallback-menu and
  discovery-throttle changes.
- `meson test -C build --print-errorlogs` passed (10/10 tests)
  after the fallback-menu and discovery-throttle changes.

#### Known Limits

- Without an appmenu/DBusMenu/GMenu exporter there is no complete native menu
  protocol on Wayland. The AT-SPI fallback can find exposed buttons/actions, but
  completeness depends on each app's accessibility tree and action metadata.
- Snap/Flatpak appmenu support depends on GTK3 apps loading the copied
  Ubuntu appmenu module bundle. GTK4/headerbar-only apps and apps without a
  traditional `GtkMenuShell` menu may still expose little or no native app menu.

### GNOME Displays Panel And Fractional Scaling

#### Implemented

- The development build directory is named `build` while the dependency target
  stays pinned to the versioned `wlroots-0.20` pkg-config package.
- The compositor now pumps the default GLib context from a 10ms Wayland timer
  and drains all pending work each tick. GNOME Settings DisplayConfig calls no
  longer wait for Orange's 1-second clock timer, which was the source of the
  2-3 second Displays panel load and orientation dropdown stalls.
- Orange advertises GNOME DisplayConfig fractional scale choices
  `1.0`, `1.25`, `1.5`, `1.75`, and `2.0`, keeps
  `global-scale-required=false`, and no longer writes GNOME Mutter's
  `scale-monitor-framebuffer` experimental feature. The GNOME Settings
  fractional-scaling toggle can therefore remain off when the user turns it
  off.
- DisplayConfig global properties now match GNOME Control Center 46.7's parsed
  types: `layout-mode` remains `uint32`, while
  `legacy-ui-scaling-factor` is signed `int32`. Mode and monitor property maps
  also include GNOME-probed keys such as `is-extra` and `min-refresh-rate`,
  preventing the GLib criticals seen in the Displays logs.
- Orange creates wlroots 0.20 `wp_viewporter` and
  `wp_fractional_scale_manager_v1` globals. Since client windows are attached
  with `wlr_scene_xdg_surface_create()`, wlroots' scene helper can advertise
  and notify fractional-scale-capable Wayland clients once the protocols are
  enabled.
- GTK dropdown popups are now handled through wlroots 0.20's dedicated
  `wlr_xdg_shell.events.new_popup` signal rather than a role check in
  `new_surface`. Orange now tracks each xdg popup with commit/destroy
  listeners, sends the initial popup configure only after wlroots marks the
  popup xdg_surface initialized, and resolves popup hit-testing back to the
  owning toplevel view so dropdown pointer/keyboard events are delivered to
  GNOME Settings correctly.
- Popup bookkeeping no longer stores `struct orange_popup` in generic
  `wlr_surface.data`; popup xdg-surface data owns the popup record while the
  root Wayland surface points back to the owning toplevel view. This keeps the
  normal focus/hit-test paths from casting popup records as windows after a GTK
  combobox dropdown is dismissed.
- While an xdg popup grab is active, no-surface pointer motion and button
  events are left in wlroots' popup-grab path instead of being converted into
  Orange desktop hover/click handling. Clicking the desktop with the GNOME
  orientation dropdown open now forwards the button to wlroots for popup
  dismissal instead of racing Orange's shell state underneath the popup.

#### Validation

- `ninja -C build` passed after the DisplayConfig type fixes and
  again after enabling `wp_viewporter`/`wp_fractional_scale_manager_v1`.
- `env PKG_CONFIG_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:/home/samuel/orange-wlroots/.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=/home/samuel/orange-wlroots/.local/wlroots-0.20/bin:/home/samuel/orange-wlroots/.venv-wlroots-build/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin .venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (10/10 tests).
- A private `dbus-run-session` headless smoke called
  `org.gnome.Mutter.DisplayConfig.GetCurrentState`; the returned payload
  included fractional scales, `layout-mode=<uint32 1>`,
  `legacy-ui-scaling-factor=<1>`, `is-extra=false`,
  `min-refresh-rate=0.0`, and the expected orientation properties.
- A private `dbus-run-session` headless smoke called verify-only
  `ApplyMonitorsConfig` with scale `1.25` and transform `uint32 1`; it returned
  `()`, covering the GNOME orientation dropdown payload without committing a
  permanent config change.
- A private `dbus-run-session` real-client smoke launched
  `gnome-control-center display` against Orange's headless Wayland socket for
  six seconds. GNOME Control Center stayed open until timeout and Orange logs
  contained no `wlr_xdg_surface_schedule_configure`, `surface->initialized`,
  `Assertion`, or `Aborted` markers.
- The popup lifecycle fix was rebuilt with `ninja -C build`;
  `meson test -C build --print-errorlogs` passed (10/10 tests).
  A second private `dbus-run-session` real-client smoke launched
  `gnome-control-center display` against Orange's headless Wayland socket for
  six seconds with no `wlr_xdg_surface_schedule_configure`,
  `surface->initialized`, `Assertion`, or `Aborted` markers. This sandbox has
  no Wayland input automation tool installed, so physically clicking the
  orientation dropdown still needs interactive validation in the live session.
- After the popup-grab outside-click guard, `ninja -C build` and
  `git diff --check -- src/compositor.c` passed.
- After the popup-grab outside-click guard, the full test suite passed:
  `meson test -C build --print-errorlogs` reported 10/10 tests.
- A corrected private `dbus-run-session` headless DisplayConfig smoke applied
  all transform values `0..7` through `ApplyMonitorsConfig`; Orange stayed
  alive and the log contained no `Assertion`, `Aborted`,
  `surface->initialized`, or GLib critical markers. The non-interactive
  `gnome-control-center display` harness could not be repeated after the latest
  patch because this sandbox has no Wayland input automation tool and the
  private Control Center process exited before exercising the panel.

### GNOME Files Icon Theme Propagation

#### Implemented

- GTK settings-file fallback now writes `gtk-theme-name`,
  `gtk-icon-theme-name`, `gtk-cursor-theme-name`, and
  `gtk-cursor-theme-size` into both `gtk-4.0/settings.ini` and
  `gtk-3.0/settings.ini`, using the active `orange.conf` theme settings.
- Empty theme values remove the corresponding GTK settings-file key instead of
  leaving a stale theme such as MacTahoe behind.

#### Validation

- `.venv-wlroots-build/bin/meson compile -C build` passed.
- An isolated headless run with copied config and temporary `HOME`,
  `XDG_CONFIG_HOME`, and `XDG_RUNTIME_DIR` wrote
  `gtk-icon-theme-name=Adwaita` to both GTK 4 and GTK 3 settings files.
- `.venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (10/10 tests).

### Dock Auto-Hide, GNOME Apps Settings, And Icon Alpha

#### Implemented

- Added persisted `dock_auto_hide` config and a Dock separator context-menu
  toggle whose stable label is `Automatically Hide and Show Dock` with a
  right-side `On`/`Off` detail.
- Expanded the Dock separator context menu to mirror the backed Desktop & Dock
  options: magnification, hiding, Dock size, magnification size, position,
  minimize effect, opening animation, open-app indicators, and Dock Settings.
- Dock hiding is obstruction-driven: the Dock remains visible and reserves work
  area unless a mapped, non-minimized app window overlaps its normal rectangle
  or edge guard band. Blocked layouts move the Dock offscreen, expose a small
  reveal strip on the configured Dock edge, and stop reserving work area.
- Fullscreen app state now also blocks the Dock on the app's output even before
  the client has committed its fullscreen-sized buffer, so Dock auto-hide takes
  effect immediately when a window enters fullscreen.
- The compositor reveals a blocked Dock on edge hover, keeps it in the overlay
  layer above clients while revealed, and dispatches left/right Dock clicks to
  shell handlers even when a client surface is underneath.
- GNOME Settings launch wrappers use a GNOME/Unity-compatible session identity
  for the overview and app-settings paths, and app preferences fall back to the
  GNOME Applications panel wrapper.
- Status bar icons and context/Open With menu icons now paint the actual themed
  icon surface instead of forcing a white/text-colored alpha silhouette.

#### Validation

- `ninja -C build test-config test-shell-layout` passed.
- `./build/test-config` passed.
- `./build/test-shell-layout` passed.
- `ninja -C build orange` passed.
- `ninja -C build test-shell-visual` passed.
- `./build/test-shell-visual` passed.
- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig ninja -C build` passed after the fullscreen Dock auto-hide fix.
- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig .venv-wlroots-build/bin/meson test -C build --print-errorlogs` passed (11/11 tests).

### Dock Keep In Dock And Context Menu Cleanup

#### Implemented

- Preserved Dock context-menu temporary-app state when opening the `Options`
  submenu, so `Keep in Dock` can pin running apps that are not already saved in
  `dock_apps`.
- Default Dock app insertion now appends before Trash instead of inserting at
  the front of the Dock, matching the existing drag-to-Dock clamp.
- Dock command-menu separators were reduced to real command groups: normal and
  running app menus separate only the remove/quit row, Trash separates only
  Dock Settings, and the Dock separator menu separates only Dock Settings.
- Dock Options submenu is now text-first like the other command menus, so
  `Keep in Dock` can render as checked for saved Dock aliases and unchecked for
  temporary running apps.
- Dock separator menu draw labels now show the next action (`Turn Hiding On`,
  `Turn Hiding Off`, `Turn Magnification On`, or `Turn Magnification Off`)
  instead of a generic `On/Off` detail.
- Dock context menus no longer draw the Dock-only speech-bubble tail, and menu
  panel/highlight radii were tightened so Dock settings menus read as compact
  command menus instead of chat bubbles.
- Temporary running Dock apps now flow through an explicit layout API instead
  of being copied into the layout before `orange_shell_layout_compute()` clears
  it, fixing running apps disappearing before Trash.
- Public Dock behavior research checked Apple's current Mac User Guide: the
  Dock can show recent apps only when they are not already in the Dock, and the
  Desktop & Dock setting describes suggested/recent apps as excluded when
  already included. Orange now applies that stricter filter to its launcher.
- Temporary Dock apps are now session-scoped recents: launching an unpinned app
  from the launcher inserts it into the Dock immediately, keeps it there after
  the window exits until Orange restarts, and prunes it only when the app is
  pinned with Keep in Dock or explicitly removed while closed.
- Launcher app results now exclude both saved Dock aliases and session-temporary
  Dock apps, so a launched Calculator or any other temporary Dock item no
  longer stays available in the launcher grid at the same time.
- Temporary Dock icons route clicks, running-app actions, and relaunches by app
  identity instead of visible pinned indices, preventing temporary slots near
  Trash from dispatching to the wrong Dock app.
- Dock running-state recomputation is dirty-gated and invalidated by view
  map/unmap, `xdg_toplevel.set_title`, `xdg_toplevel.set_app_id`, desktop-entry
  reloads, and Dock config mutations. Temporary app identity now falls back to
  a matched desktop entry or window title when `app_id` is delayed or empty.
- Desktop-entry reload now uses the actual `desktop_entries` array capacity,
  avoiding an over-large hard-coded limit in the same app-id matching path.
- Research verified GNOME Calculator and GNOME Maps are distinct apps
  (`org.gnome.Calculator.desktop` launches `gnome-calculator`;
  `org.gnome.Maps.desktop` launches `gapplication launch org.gnome.Maps`).
  The failure was Orange's stale fallback indices: Calculator fallback still
  returned Dock slot 3, which is Maps in the current default Dock. Fallback
  matching now asks the Dock config for the actual pinned slot and leaves
  unpinned Calculator as a temporary running Dock item.

#### Validation

- `.venv-wlroots-build/bin/meson test -C build shell-layout --print-errorlogs`
  passed.
- `.venv-wlroots-build/bin/meson test -C build shell-visual --print-errorlogs`
  passed.
- `.venv-wlroots-build/bin/meson test -C build --print-errorlogs`
  passed (11/11 tests). The build still reports existing unused-helper warnings
  for `cycle_dock_size` and `cycle_dock_magnification_size`.
- `git diff --check -- include/orange/dock.h include/orange/launcher.h include/orange/shell.h src/dock.c src/launcher.c src/compositor.c tests/test_shell_layout.c`
  passed.
- `git diff --check -- src/compositor.c src/dock.c src/shell.c src/menubar.c include/orange/shell.h include/orange/dock.h tests/test_shell_layout.c PROJECT_STATE.md docs/REQUIREMENTS.md docs/RESEARCH.md docs/ARCHITECTURE.md`
  passed.

### Dock Separator Resize And Hover Lag

#### Implemented

- Hovering the Dock separator now uses the divider-axis resize cursor:
  `ns-resize` for bottom and side Docks.
- Left-dragging the separator resizes the Dock live with position-aware drag
  direction: bottom Docks grow upward, while side Docks grow when the horizontal
  separator is dragged downward. The new `dock_scale` is clamped to `0.60..2.00`
  and saved to `orange.conf` on release.
- Separator hover no longer also selects the nearest Dock icon for hover
  magnification.
- Dock hover magnification, hover labels, and bounce animation now dirty a
  conservative Dock rectangle and redraw the clipped Dock area instead of
  repainting/damaging the full output. Full shell redraws remain in place for
  actual Dock-size/layout changes.
- Auto-hidden Dock reveal keeps using overlay redraws while revealed, and the
  Dock remains revealed during separator resize.

#### Validation

- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig meson setup --reconfigure build`
  passed.
- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig meson compile -C build`
  passed without project warnings.
- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig meson test -C build`
  passed (11/11 tests).
- `git diff --check -- include/orange/shell.h src/shell.c src/compositor.c tests/test_shell_layout.c`
  passed.

### Firefox Snap Wrapper And Launcher Filtering

#### Implemented

- Firefox Dock aliases now resolve stale snap IDs such as `firefox_firefox`
  through the built-in Firefox command before consulting desktop entries, so a
  leftover `~/.local/share/applications/firefox_firefox.desktop` that points at
  `firefox-snap-wrapper` no longer blocks a non-snap Firefox install.
- Desktop entries now parse `Hidden`, `NoDisplay`, `OnlyShowIn`, `NotShowIn`,
  `TryExec`, `StartupWMClass`, `X-SnapInstanceName`, `X-SnapAppName`, and
  `X-Flatpak`.
- The app launcher applies GNOME/GIO-style menu visibility filtering before
  search/category display, hiding MIME helpers, deleted entries,
  environment-specific entries, and shortcuts whose `TryExec` target is not
  installed while keeping the full desktop-entry table available for Dock and
  app-ID resolution.
- Package-specific desktop entries are treated as unavailable when their snap
  or flatpak install directory is gone. Dock and compositor app matching now
  rank available entries instead of taking the first fuzzy match, so an old
  snap ID can fall through to an installed non-snap or other equivalent entry.

#### Validation

- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig meson compile -C build`
  passed.
- `PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig meson test -C build`
  passed (11/11 tests).
- `git diff --check -- include/orange/desktop_entry.h src/desktop_entry.c src/launcher.c src/dock.c tests/test_desktop_entry.c tests/test_shell_layout.c`
  passed.
- After stale package detection was added, the same `meson compile`,
  `meson test`, and `git diff --check` commands passed again.

### Glass App Launcher Rewrite

#### Implemented

- The full app launcher now renders as a separate floating search/action pill
  above a translucent glass app-library panel instead of embedding the search
  header inside the panel.
- The app grid now uses six columns with a medium glass panel footprint,
  upper-middle screen placement, and tuned icon sizing after visual iteration.
- Full-launcher overlay bounds include the separate search/action pill, action
  button, and app panel, so the pill is not clipped when overlays are redrawn.
- Launcher category tabs now default to: All, Utilities, Photo & Video,
  Productivity & Finance, Creativity, Entertainment, Social, and Information &
  Reading.
- Category filtering maps those new tab names to freedesktop desktop-entry
  categories while preserving older aliases such as Productivity, Media, and
  Info.

#### Validation

- `ninja -C build test-shell-layout` passed.
- `./build/test-shell-layout` passed.
- `ninja -C build test-shell-visual` passed.
- `./build/test-shell-visual` passed.
- `ninja -C build` passed.
- `meson test -C build --print-errorlogs` did not run because
  the build directory's Meson metadata was generated by an older Meson and
  requires `meson setup --reconfigure`.

### Technical Concerns

wlroots APIs are unstable; the current development target is wlroots 0.20.1
through the versioned `wlroots-0.20` pkg-config dependency. Vulkan
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

Continue from the current implementation. Target wlroots 0.20.1 directly, keep
private wallpapers and local theme experiments ignored, and validate the 0.20
dev build with the repo-local prefix:

`env PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig LD_LIBRARY_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu PATH=".local/wlroots-0.20/bin:$PATH" .venv-wlroots-build/bin/meson test -C build --print-errorlogs`

To build and run the Void VM, first enable WSL2 nested virtualization and make
sure `/dev/kvm` and loop devices are visible in the distro. Then run
`scripts/build-void-vm.sh` followed by `scripts/run-void-vm.sh`.
