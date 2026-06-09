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
- `convert` / ImageMagick: installed and used to generate repo-safe Tahoe
  placeholder PNG assets

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

Apple describes the Tahoe-era design as using "Liquid Glass": translucent UI
materials that adapt to surrounding content and include real-time highlights.
For this prototype, that is approximated with:

- blurred/translucent glass panels,
- rounded widgets and dock,
- bright specular edge highlights,
- softly tinted icon backgrounds,
- wallpaper-colored transparency.

The same material treatment is used for context menus and the Apple menu
popover. Apple's current HIG positions Liquid Glass as the system material for
foreground controls and navigation surfaces, and its menu/action components are
part of that foreground UI layer. The shell therefore renders dock, Apple menu,
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
macOS name and version, with build details revealed from the version text. The
Mac user guide says the Mac model appears at the top, hardware details such as
chip and serial number sit immediately below, and macOS version information
appears below that. Apple's June 3, 2026 Tahoe update notes list macOS Tahoe
`26.5.1`, so the bundled Tahoe About app uses that version string and toggles
to the local Tahoe build string.

The upstream `vinceliuice/MacTahoe-gtk-theme` project describes itself as a
macOS Tahoe-like GTK theme for Linux desktops, based on WhiteSur. Its README
documents source installation and customization flags, and its COPYING file is
MIT-style. The project bundles the upstream source as a Git submodule for
license, commit history, and updates, and expands the upstream `MacTahoe-Light`
and `MacTahoe-Dark` release archives under `themes/` so an installed theme
payload is available without running the installer.

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
- Exact Apple assets cannot be redistributed in this repository.
- The native GTK Settings and About apps are implemented conditionally so the
  compositor can still build on systems without GTK development headers.
- Client-side traffic-light controls depend on GTK client support for CSD and
  theme CSS. The compositor can force client-side preference, but non-GTK
  clients may not honor GTK CSS.

## References

- Apple newsroom: macOS Tahoe design and Liquid Glass announcement.
- Apple macOS Tahoe product page for visual layout reference.
- Apple Human Interface Guidelines: Liquid Glass material guidance and
  menu/action component guidance.
- Apple Support, "Add and customize widgets on Mac":
  https://support.apple.com/guide/mac-help/add-and-customize-widgets-mchl52be5da5/mac
- Apple Support, "Find out which macOS your Mac is using":
  https://support.apple.com/109033
- Apple Support, "What's new in the updates for macOS Tahoe 26":
  https://support.apple.com/en-au/122868
- Apple Support, "Find the Getting Started guide for your Mac":
  https://support.apple.com/guide/mac-help/mchl30bc42cb/mac
- MacTahoe GTK Theme:
  https://github.com/vinceliuice/MacTahoe-gtk-theme
- Apple Human Interface Guidelines, "Liquid Glass":
  https://developer.apple.com/design/human-interface-guidelines/liquid-glass
- Apple Human Interface Guidelines, "Menus":
  https://developer.apple.com/design/human-interface-guidelines/menus
- Apple Newsroom, "Apple introduces a delightful and elegant new software
  design":
  https://www.apple.com/newsroom/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/
- wlroots 0.17 local headers and online renderer documentation.
- Khronos Vulkan documentation for Vulkan instance/rendering concepts.
