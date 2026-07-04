# Orange

A wlroots compositor for Linux with a macOS-like desktop UI drawn in C.

The committed `assets/` directory is wallpaper-only. Shell, Dock, desktop, menu,
and status icons are resolved from the configured freedesktop icon theme.

## Build

```sh
PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig \
  .venv-wlroots-build/bin/meson setup build-wlroots-0.20
PKG_CONFIG_PATH=.local/wlroots-0.20/lib/x86_64-linux-gnu/pkgconfig:.local/wlroots-0.20/share/pkgconfig \
  ninja -C build-wlroots-0.20
```

Orange targets wlroots 0.20 through the versioned `wlroots-0.20` pkg-config
dependency. The older unversioned `wlroots` package is not a supported build
target.

## Run

From a nested Wayland/X11 session:

```sh
WLR_RENDERER=vulkan ./build-wlroots-0.20/orange
```

`WLR_RENDERER=vulkan` requires a backend that can hand wlroots a DRM render
node. Headless validation uses Pixman because wlroots cannot create its Vulkan
renderer without DRM.

On WSLg, run it nested inside the existing WSLg Wayland session:

```sh
cd ~/orange-wlroots
WLR_BACKENDS=wayland WLR_RENDERER=pixman ./build-wlroots-0.20/orange --assets assets --config orange.conf
```

When this command logs `running on Wayland display wayland-N`, startup
succeeded. The compositor is then serving that nested display and keeps running
until you stop it.

Do not force `WLR_RENDERER=vulkan` on WSLg unless `vulkaninfo` works and the
nested Wayland backend exposes a DRM render node. If wlroots logs `Cannot
create Vulkan renderer: no DRM FD available`, use Pixman for WSLg. That error
means renderer setup failed before Orange can fall back.

For automated/headless validation:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman ./build-wlroots-0.20/orange --headless --once
```

For a custom-size startup smoke test that exits on its own:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman ./build-wlroots-0.20/orange --headless --once --width 1440 --height 900 --assets assets --config /tmp/orange-custom.conf
```

## Assets

Tracked assets are limited to:

- `assets/wallpaper.png`
- `assets/wallpaper-dark.png`

Optional private wallpapers can live in `assets/private/`, which is ignored by
Git. To make a run use only that private wallpaper root, pass
`--assets assets/private`.

Icons are looked up by name from the configured installed icon theme using
freedesktop theme directories, inherited themes, and `hicolor` fallback. Orange
checks standard user/system icon locations such as `$HOME/.local/share/icons`,
`$HOME/.icons`, and `$XDG_DATA_DIRS/icons`.

## Desktop Entries

Right-side desktop shortcuts are parsed from XDG `.desktop` files installed on
the system. The shell scans `$XDG_DATA_HOME/applications/` and each path in
`$XDG_DATA_DIRS/applications/` for `.desktop` entries. Dock items are configured
via `dock_apps` in `orange.conf` (comma-separated desktop file IDs).

## Settings And Themes

The compositor reloads `orange.conf` once per second. GNOME Settings integration
through Orange's session services is the active settings path for the 0.20
build. The older in-tree GTK Settings and About utilities are currently disabled
in Meson while they are rewritten, so `build-wlroots-0.20/orange-settings` and
`build-wlroots-0.20/orange-about` are not expected build outputs.

Theme and cursor settings are still stored in `orange.conf`; cursor settings
write `cursor_theme=` and `cursor_size=`.

Desktop icons can be dragged; custom positions are persisted in `orange.conf` as
`desktop_icon_N_position=x,y`. Right-click desktop icons or Dock items to open
their shell context menus. Right-click Calendar or Weather widgets for
widget-specific edit, size, and remove actions.

Click the date/time text to open the right-edge Notification Center overlay.
Click it again, click the desktop, or press Escape to close it. Wi-Fi, sound,
and battery open item-specific status menus; search opens a centered draggable
Spotlight-style glass pill with adjacent round buttons; the first button opens
the app launcher. Control Center opens quick controls. The Edit Widgets button
opens Orange Settings for widget preferences.

Theme names are config-driven:

```ini
icon_theme=MacTahoe
gtk_theme_light=MacTahoe-Light
gtk_theme_dark=MacTahoe-Dark
```

Any installed GTK/icon theme can be used by changing those values in
`orange.conf` or through the Settings dropdowns. MacTahoe is only the current
test default.

Install or develop GTK/icon themes outside this repository, then set the theme
names in `orange.conf`. The compositor exports `GTK_THEME`, `GTK_ICON_THEME`,
`ORANGE_APPEARANCE`, Wayland toolkit backends, and Chromium/Electron Ozone
Wayland hints before launching child clients. Discord launch commands suppress
global Chromium wrapper flags while keeping the Electron Wayland hint because
Orange does not provide an X11 `DISPLAY`.

To render a native-size PNG for manual inspection:

```sh
./build-wlroots-0.20/orange-render-shell /tmp/orange-shell.png
```

The render tool defaults to the native `2880x1800` shell size. Use
foreground-only output to measure shell geometry without wallpaper differences:

```sh
./build-wlroots-0.20/orange-render-shell --foreground-only /tmp/orange-foreground.png
```

Notification Center and context menus can be rendered directly for glass/text
checks:

```sh
./build-wlroots-0.20/orange-render-shell --notification-center /tmp/orange-notification-center.png
```

```sh
./build-wlroots-0.20/orange-render-shell --foreground-only --context-menu status-wifi /tmp/orange-status-wifi.png
```

```sh
./build-wlroots-0.20/orange-render-shell --foreground-only --context-menu desktop --context-x 1440 --context-y 900 /tmp/orange-menu.png
```
