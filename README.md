# Orange

A small wlroots compositor prototype for Linux with a macOS-like desktop UI
drawn in C.

The committed `assets/` directory is wallpaper-only. Shell, Dock, desktop, menu,
and status icons are resolved from the configured freedesktop icon theme.

## Build

```sh
meson setup build
ninja -C build
```

## Run

From a nested Wayland/X11 session:

```sh
WLR_RENDERER=vulkan build/orange
```

`WLR_RENDERER=vulkan` requires a backend that can hand wlroots a DRM render
node. Headless validation uses Pixman because wlroots cannot create its Vulkan
renderer without DRM.

On WSLg, run it nested inside the existing WSLg Wayland session:

```sh
cd ~/orange-wlroots
WLR_BACKENDS=wayland WLR_RENDERER=pixman ./build/orange --assets assets --config orange.conf
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
WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once
```

For a custom-size startup smoke test that exits on its own:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --assets assets --config /tmp/orange-custom.conf
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

The compositor reloads `orange.conf` once per second. The GTK Settings app source
is included and will build as `build/orange-settings` when GTK4 development
headers are installed.

The GTK About Orange app source is included and will build as
`build/orange-about` when GTK4 development headers are installed. It opens from
the system menu > About Orange and shows model, real chip and memory values, and
Orange version/build details.

Settings currently covers appearance, desktop icon visibility/scale/labels,
widget visibility/size, Dock size/icon/magnification/indicator controls, and
cursor theme/size. Cursor settings write `cursor_theme=` and `cursor_size=` to
`orange.conf`.

Desktop icons can be dragged; custom positions are persisted in `orange.conf` as
`desktop_icon_N_position=x,y`. Right-click desktop icons or Dock items to open
their shell context menus. Right-click Calendar or Weather widgets for
widget-specific edit, size, and remove actions.

Theme names are config-driven:

```ini
icon_theme=MacTahoe
gtk_theme_light=MacTahoe-Light
gtk_theme_dark=MacTahoe-Dark
```

Any installed GTK/icon theme can be used by changing those values in
`orange.conf`. MacTahoe is only the current test default.

Install or develop GTK/icon themes outside this repository, then set the theme
names in `orange.conf`. The compositor exports `GTK_THEME`, `GTK_ICON_THEME`,
and `ORANGE_APPEARANCE` before launching child GTK clients.

To render a native-size PNG for manual inspection:

```sh
./build/orange-render-shell /tmp/orange-shell.png
```

The render tool defaults to the native `2880x1800` shell size. Use
foreground-only output to measure shell geometry without wallpaper differences:

```sh
./build/orange-render-shell --foreground-only /tmp/orange-foreground.png
```

Context menus can be rendered directly for glass/text checks:

```sh
./build/orange-render-shell --foreground-only --context-menu desktop --context-x 1440 --context-y 900 /tmp/orange-menu.png
```
