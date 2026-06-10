# Tahoe

A small wlroots compositor prototype that recreates the supplied macOS Tahoe 26
desktop reference on Linux with C rendering code.

The committed project ships repo-safe Tahoe-branded placeholder assets under
`assets/`, using a `T` mark instead of Apple imagery. Private Apple-provided
assets can still live under ignored `assets/apple/` for local experiments.

## Build

```sh
meson setup build
ninja -C build
```

## Run

From a nested Wayland/X11 session:

```sh
WLR_RENDERER=vulkan build/tahoe
```

`WLR_RENDERER=vulkan` requires a backend that can hand wlroots a DRM render
node. Headless validation uses Pixman because wlroots cannot create its Vulkan
renderer without DRM.

On WSLg, run it nested inside the existing WSLg Wayland session:

```sh
cd ~/tahoe
WLR_BACKENDS=wayland WLR_RENDERER=pixman ./build/tahoe --assets assets --config tahoe.conf --desktop-dir assets/desktop --themes .
```

When this command logs `running on Wayland display wayland-N`, startup
succeeded. The compositor is then serving that nested display and keeps running
until you stop it.

Do not force `WLR_RENDERER=vulkan` on WSLg unless `vulkaninfo` works and the
nested Wayland backend exposes a DRM render node. If wlroots logs `Cannot
create Vulkan renderer: no DRM FD available`, use Pixman for WSLg. That error
means renderer setup failed before Tahoe can fall back.

For automated/headless validation:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe --headless --once
```

For a custom-size startup smoke test that exits on its own:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe --headless --once --width 1440 --height 900 --assets assets --config /tmp/tahoe-custom.conf --desktop-dir assets/desktop --themes .
```

## Assets

Tracked Tahoe placeholder files live in:

- `assets/tahoe-menu.png`
- `assets/icons/*.png`
- `assets/icons/*-dark.png`
- `assets/status/*.png`

Regenerate them with:

```sh
./tools/generate_tahoe_assets.sh assets
```

Optional private files can live in `assets/apple/`, which is ignored by Git.
To make a run use only that private root, pass `--assets assets/apple`.

- `assets/apple/wallpaper.png`
- `assets/apple/wallpaper-dark.png`
- `assets/apple/wallpaper-beach-day.png`
- `assets/apple/apple-menu.png`
- Dock icons in `assets/apple/icons/`:
  `finder.png`, `launchpad.png`, `safari.png`, `messages.png`, `mail.png`,
  `maps.png`, `photos.png`, `facetime.png`, `phone.png`, `calendar.png`,
  `contacts.png`, `reminders.png`, `notes.png`, `tv.png`, `music.png`,
  `rocket.png`, `app-store.png`, `calculator.png`, `settings.png`,
  `desktop-preview.png`, `trash.png`
- Optional dark icon variants in `assets/apple/icons/` using `-dark.png`.
- Status icons in `assets/apple/status/`:
  `battery.100.png`, `wifi.png`, `magnifyingglass.png`,
  `control-center.png`, `weather.png`

These files are intentionally not tracked.

The later blue abstract reference wallpaper should be placed at
`assets/apple/wallpaper-beach-day.png` until a separate wallpaper selector is
added.

## Desktop Entries

Right-side desktop shortcuts are parsed from XDG `.desktop` files in
`assets/desktop/`. The committed entries are:

- `assets/desktop/images.desktop`
- `assets/desktop/pdf-document.desktop`

The shell uses each entry's `Name=`, `Icon=`, and `Exec=` fields. Icon names
must match PNG files in `assets/icons/` or `assets/apple/icons/`.

## Settings And Themes

The compositor reloads `tahoe.conf` once per second. The GTK Settings app source
is included and will build as `build/tahoe-settings` when GTK4 development
headers are installed.

The GTK About This Mac app source is included and will build as
`build/tahoe-about` when GTK4 development headers are installed. It opens from
Apple menu > About Tahoe and shows model, real chip and memory values, and
Tahoe version/build details.

Settings currently covers appearance, desktop icon visibility/scale/labels,
widget visibility/size, Dock size/icon/magnification/indicator controls, and
cursor theme/size. Cursor settings write `cursor_theme=` and `cursor_size=` to
`tahoe.conf`.

Desktop icons can be dragged; custom positions are persisted in `tahoe.conf` as
`desktop_icon_N_position=x,y`. Right-click desktop icons or Dock items to open
their shell context menus. Right-click Calendar or Weather widgets for
widget-specific edit, size, and remove actions.

Bundled installed MacTahoe GTK themes live under `themes/MacTahoe-Light/` and
`themes/MacTahoe-Dark/` and are the compositor defaults for launched GTK
clients. Lightweight fallback themes remain under `themes/TahoeGTK/` and
`themes/TahoeGTK-dark/`. The upstream MacTahoe source is also bundled as the
`themes/MacTahoe-gtk-theme` submodule, pinned to the upstream MIT-licensed
project for provenance and updates. The compositor exports GTK theme variables
and requests client-side decorations.

To initialize the bundled MacTahoe theme source after cloning:

```sh
git submodule update --init --recursive
```

To populate the GTK icon pack from local assets:

```sh
./tools/populate_icon_theme.sh assets themes/TahoeIcons
```

The generated `themes/TahoeIcons/apps`, `places`, and `status` directories are
ignored so private icon PNGs do not enter Git history.

To render a native-size PNG for manual inspection:

```sh
./build/tahoe-render-shell /tmp/tahoe-shell.png
```

The render tool defaults to the native `2880x1800` shell size. Use
foreground-only output to measure shell geometry without wallpaper differences:

```sh
./build/tahoe-render-shell --foreground-only /tmp/tahoe-foreground.png
```

Context menus can be rendered directly for glass/text checks:

```sh
./build/tahoe-render-shell --foreground-only --context-menu desktop --context-x 1440 --context-y 900 /tmp/tahoe-menu.png
```
