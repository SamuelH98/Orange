# Orange

A small wlroots compositor prototype for Linux with a macOS-like desktop UI
drawn in C.

The committed project ships repo-safe Orange-branded placeholder assets under
`assets/`, using an `O` mark instead of vendor imagery. Private user-provided
assets can still live under ignored `assets/private/` for local experiments.

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
WLR_BACKENDS=wayland WLR_RENDERER=pixman ./build/orange --assets assets --config orange.conf --desktop-dir assets/desktop --themes .
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
WLR_BACKENDS=headless WLR_RENDERER=pixman build/orange --headless --once --width 1440 --height 900 --assets assets --config /tmp/orange-custom.conf --desktop-dir assets/desktop --themes .
```

## Assets

Tracked Orange placeholder files live in:

- `assets/wallpaper.png` and `assets/wallpaper-dark.png` at 5120x3200
- `assets/orange-menu.png`
- `assets/icons/*.png`
- `assets/icons/*-dark.png`
- `assets/status/*.png`

Regenerate them with:

```sh
./tools/generate_orange_assets.sh assets
```

Optional private files can live in `assets/private/`, which is ignored by Git.
To make a run use only that private root, pass `--assets assets/private`.

- `assets/private/wallpaper.png`
- `assets/private/wallpaper-dark.png`
- `assets/private/wallpaper-beach-day.png`
- `assets/private/system-menu.png`
- Dock icons in `assets/private/icons/`:
  `files.png`, `launcher.png`, `browser.png`, `messages.png`, `mail.png`,
  `maps.png`, `photos.png`, `video.png`, `phone.png`, `calendar.png`,
  `contacts.png`, `reminders.png`, `notes.png`, `tv.png`, `music.png`,
  `rocket.png`, `software.png`, `calculator.png`, `settings.png`,
  `desktop-preview.png`, `trash.png`
- Optional dark icon variants in `assets/private/icons/` using `-dark.png`.
- Status icons in `assets/private/status/`:
  `battery.100.png`, `wifi.png`, `magnifyingglass.png`,
  `control-center.png`, `weather.png`

These files are intentionally not tracked.

The optional blue abstract reference wallpaper can live at
`assets/private/wallpaper-beach-day.png`; it is only used when the selected asset
root does not provide the default Orange wallpaper.

## Desktop Entries

Right-side desktop shortcuts are parsed from XDG `.desktop` files in
`assets/desktop/`. The committed entries are:

- `assets/desktop/images.desktop`
- `assets/desktop/pdf-document.desktop`

The shell uses each entry's `Name=`, `Icon=`, and `Exec=` fields. Icon names
must match PNG files in `assets/icons/` or `assets/private/icons/`.

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

Bundled installed MacTahoe GTK themes live under `themes/MacTahoe-Light/` and
`themes/MacTahoe-Dark/` and are the compositor defaults for launched GTK
clients. Lightweight fallback themes remain under `themes/OrangeGTK/` and
`themes/OrangeGTK-dark/`. The upstream MacTahoe source is also bundled as the
`themes/MacTahoe-gtk-theme` submodule, pinned to the upstream MIT-licensed
project for provenance and updates. The compositor exports GTK theme variables
and requests client-side decorations.

To initialize the bundled MacTahoe theme source after cloning:

```sh
git submodule update --init --recursive
```

To populate the GTK icon pack from local assets:

```sh
./tools/populate_icon_theme.sh assets themes/OrangeIcons
```

The generated `themes/OrangeIcons/apps`, `places`, and `status` directories are
ignored so private icon PNGs do not enter Git history.

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
