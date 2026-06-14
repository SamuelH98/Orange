# Research Summary

## Local Environment

- `wlroots`: 0.17.1
- `wayland-server`: 1.22.0
- `vulkan`: 1.3.275
- `cairo`: 1.18.0
- `pixman`: 0.42.2
- `meson`: 1.3.2
- `ninja`: 1.11.1
- `gcc`: 13.3.0
- `gtk4`: 4.14.5
- `gtk+-3.0`: not installed in this environment
- `convert` / ImageMagick: installed and used to generate repo-safe Orange
  wallpaper PNG assets

## wlroots Notes

The installed wlroots uses the unstable 0.17 renderer API:

- `wlr_backend_autocreate`
- `wlr_renderer_autocreate`
- `wlr_allocator_autocreate`
- `wlr_output_attach_render`
- `wlr_renderer_begin`
- `wlr_render_texture_with_matrix`
- `wlr_output_commit`

The Vulkan renderer can be selected through wlroots with
`WLR_RENDERER=vulkan`. The project checks `wlr_renderer_is_vk()` at runtime
when the Vulkan renderer headers are available. On this machine, the headless
backend cannot create a Vulkan renderer because wlroots has no DRM file
descriptor in that mode; headless validation uses Pixman.

## Visual Reference Notes

The visual reference uses Apple's Liquid Glass-era design language:
translucent UI materials that adapt to surrounding content and include
real-time highlights. For this prototype, that is approximated with:

- blurred/translucent glass panels,
- rounded widgets and dock,
- bright specular edge highlights,
- softly tinted icon backgrounds,
- wallpaper-colored transparency.

The same material treatment is used for context menus and the system menu
popover. Apple's current HIG positions Liquid Glass as the system material for
foreground controls and navigation surfaces, and its menu/action components are
part of that foreground UI layer. The shell therefore renders dock, system menu,
and context menu panels through glass material helpers instead of opaque menu
panels.

Apple's Mac widget guide describes desktop widget customization from the
desktop and widget shortcut menus: the desktop shortcut menu exposes
`Edit Widgets`, while a widget shortcut menu exposes widget editing, size
selection, and removal. The shell models that surface with widget hit targets,
widget context menus, persistent widget visibility, and small/medium/large
size settings.

Pixel validation renders at the native `2880x1800` shell size and can ignore
wallpaper/background differences for manual inspection. Automated shell render
coverage keeps light/dark smoke tests plus a foreground-only desktop context
menu check for scaled geometry and translucent glass, without comparing against
a checked-in visual reference image.

Apple documents About This Mac as an Apple-menu window that always includes the
system name and version, with build details revealed from the version text. The
Mac user guide says the Mac model appears at the top, hardware details such as
chip and serial number sit immediately below, and version information appears
below that. The bundled Orange About app follows that structure while showing
the local Orange build string instead of claiming to be an Apple OS release.

The upstream `vinceliuice/MacTahoe-gtk-theme` project describes itself as a
macOS Tahoe-like GTK theme for Linux desktops, based on WhiteSur. Its README
documents source installation, `./install.sh`, destination selection with
`-d/--dest`, and naming with `-n/--name`. The companion
`vinceliuice/MacTahoe-icon-theme` README documents the same install-script
shape for icons, defaulting icon installation to `$HOME/.local/share/icons`.
Orange keeps MacTahoe as config-selected test values when those themes are
installed locally; theme payloads live outside this compositor repository.

Freedesktop's Desktop Entry Specification defines `Exec` as the command line
used by launchers and reserves field codes such as `%f`, `%F`, `%u`, and `%U`
for file or URL arguments. For a no-file Dock or desktop launch, those field
codes must be removed rather than passed literally to a shell. This avoids real
desktop entries failing when they include `%U` or similar launch placeholders.
It also defines the `Icon` key as either an absolute path or a themed icon name
resolved through the Icon Theme Specification.

Freedesktop's Icon Theme Specification defines icon lookup as a mapping from an
icon name and size to theme files, searched through base directories, theme
inheritance, and finally `hicolor`. Orange keeps a compact in-memory cache but
should still honor the practical directory shapes used by XDG themes:
`$XDG_DATA_DIRS/icons/<theme>/...`, `$HOME/.icons/<theme>/...`,
and `/usr/share/pixmaps`.
The Freedesktop Icon Naming Specification provides standard icon names for the
Linux-facing shell contract, including `start-here`, `system-search`,
`system-shutdown`, `system-lock-screen`, `network-wireless`, `network-offline`,
`audio-volume-high`, `battery`, `user-trash`, and `weather-clear`.

Apple's Mac User Guide describes the menu bar as a top-screen strip with the
Apple/system menu and app menus on the left, and status menu icons plus date
and time on the right. Status icons are interactive: clicking a status item
opens details and quick controls such as Wi-Fi options. Orange should preserve
that user-facing shape while resolving images and launch targets through
freedesktop desktop entries, icon themes, and standard Linux control commands.
The same guide describes Control Center as a menu-bar entry that exposes common
settings such as Wi-Fi, AirDrop, Focus, Sound, Screen Mirroring, Display, and
related customization. Orange models the status-area menu as this
Control-Center-style grouping while launching Linux settings panes and tools.

Apple's current appearance documentation says light/dark appearance applies to
the menu bar, Dock, windows, and built-in apps. For Orange this means context
menus and the system menu must use the dark material palette when
`appearance=dark`, not only the Dock/widgets.

Apple's Apple-menu documentation lists About, System Settings, App Store,
Recent Items, Force Quit, Sleep, Restart, Shut Down, Lock Screen, and Log Out.
Orange keeps those visible menu labels but maps actions to local Orange apps,
standard Linux power commands, and installed software-center tools.

GTK's `GtkIconTheme` documentation describes a named icon theme as a shared
database and notes that direct lookup takes an icon name, size, and scale.
The shell renderer cannot depend on a GTK display object inside the wlroots
compositor path, so Orange implements the same practical model locally: parse
`index.theme`, probe only the requested icon name through exact/closest size
lookup, cache successful surfaces, and cache misses. This avoids rendering
hundreds of theme icons during startup or every shell redraw.

Freedesktop's StatusNotifierItem family is the DBus tray/status-area protocol:
a StatusNotifierHost represents graphical StatusNotifierItem instances and is
registered on the session bus. Orange's current menu-bar status strip is still
static compositor chrome, so this pass fixes its icon-theme/local asset lookup;
a live DBus StatusNotifier host remains a separate feature.

Orange's custom About app controls use 16px geometry, a 23px button step,
active red/gray/gray colors, a hover/pressed glyph only on the red close
button, and gray inactive/backdrop colors, drawing the states directly without
depending on external SVG assets at runtime. The About dark palette follows
the installed MacTahoe dark GTK named colors used during testing:
`window_bg_color #333333`, `window_fg_color #dadada`, and `view_bg_color #242424`.

Apple's Desktop & Dock settings documentation describes Dock size as
slider-controlled, Dock magnification as icon magnification when the pointer
moves over icons, and open applications as indicated by a small dot below the
app icon. Dock behavior references also describe dynamic resizing to fit and
labels appearing on pointer hover. The shell Dock therefore avoids a circular
hover halo, keeps the small running indicator dot, and uses icon magnification
plus a compact label bubble for hover feedback.

## Risks

- wlroots APIs are explicitly unstable; code targets 0.17.1 as installed.
- Vulkan renderer availability depends on GPU/driver/runtime environment.
- wlroots' Vulkan renderer needs a DRM render-node path from the backend. WSLg
  can expose GPU acceleration to Linux GUI clients without exposing the DRM FD
  needed by `WLR_BACKENDS=wayland WLR_RENDERER=vulkan`; in that case wlroots
  fails during renderer creation with `no DRM FD available`, and Pixman is the
  expected WSLg fallback.
- Running a real compositor outside headless mode may require a nested Wayland
  or X11 session, or a TTY with seat permissions.
- Exact proprietary vendor assets cannot be redistributed in this repository.
- The native GTK Settings and About apps are implemented conditionally so the
  compositor can still build on systems without GTK development headers.
- Client-side traffic-light controls depend on GTK client support for CSD and
  theme CSS. The compositor can force client-side preference, but non-GTK
  clients may not honor GTK CSS.

## References

- Apple newsroom: Liquid Glass software design announcement.
- Apple macOS design/product pages for visual layout reference.
- Apple Human Interface Guidelines: Liquid Glass material guidance and
  menu/action component guidance.
- Apple Support, "Add and customize widgets on Mac":
  https://support.apple.com/guide/mac-help/add-and-customize-widgets-mchl52be5da5/mac
- Apple Support, "What’s in the menu bar on Mac?":
  https://support.apple.com/guide/mac-help/whats-in-the-menu-bar-mchlp1446/mac
- Apple Support, "What’s in the Apple menu on Mac?":
  https://support.apple.com/guide/mac-help/whats-in-the-apple-menu-mchlp1130/mac
- Apple Support, "Use Control Center on Mac":
  https://support.apple.com/guide/mac-help/quickly-change-settings-mchl50f94f8f/mac
- Apple Support, "Use a light or dark appearance on your Mac":
  https://support.apple.com/guide/mac-help/use-a-light-or-dark-appearance-mchl52e1c2d2/mac
- Apple Support, "Find out which macOS your Mac is using":
  https://support.apple.com/109033
- Apple Support, macOS update notes:
  https://support.apple.com/en-au/122868
- Apple Support, "Find the Getting Started guide for your Mac":
  https://support.apple.com/guide/mac-help/mchl30bc42cb/mac
- MacTahoe GTK Theme:
  https://github.com/vinceliuice/MacTahoe-gtk-theme
- MacTahoe Icon Theme:
  https://github.com/vinceliuice/MacTahoe-icon-theme
- GTK4 `GtkIconTheme`:
  https://docs.gtk.org/gtk4/class.IconTheme.html
- Freedesktop Icon Theme Specification:
  https://specifications.freedesktop.org/icon-theme-spec/latest/
- Freedesktop Icon Naming Specification:
  https://specifications.freedesktop.org/icon-naming-spec/latest/
- Freedesktop Desktop Entry Specification:
  https://specifications.freedesktop.org/desktop-entry-spec/latest/
- Freedesktop StatusNotifierItem:
  https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
- Apple Support, "Change Desktop & Dock settings on Mac":
  https://support.apple.com/guide/mac-help/change-desktop-dock-settings-mchlp1119/mac
- Dock (macOS) behavior reference:
  https://en.wikipedia.org/wiki/Dock_(macOS)
- Apple Human Interface Guidelines, "Liquid Glass":
  https://developer.apple.com/design/human-interface-guidelines/liquid-glass
- Apple Human Interface Guidelines, "Menus":
  https://developer.apple.com/design/human-interface-guidelines/menus
- Apple Newsroom, "Apple introduces a delightful and elegant new software
  design":
  https://www.apple.com/newsroom/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/
- wlroots 0.17 local headers and online renderer documentation.
- Khronos Vulkan documentation for Vulkan instance/rendering concepts.
