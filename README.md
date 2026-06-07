# Tahoe wlroots

A small wlroots compositor prototype that recreates the supplied macOS Tahoe 26
desktop reference on Linux with C rendering code.

The committed project uses original procedural/vector-style assets. It also
supports local Apple-provided asset overrides from `assets/apple/`, which is
ignored by Git. Put licensed wallpaper/icon/logo files there if you want a
closer private run.

## Build

```sh
meson setup build
ninja -C build
```

## Run

From a nested Wayland/X11 session:

```sh
WLR_RENDERER=vulkan build/tahoe-wlroots
```

`WLR_RENDERER=vulkan` requires a backend with a DRM device. Headless validation
uses Pixman because wlroots cannot create its Vulkan renderer without DRM.

On WSLg, run it nested inside the existing WSLg Wayland session:

```sh
cd ~/tahoe-wlroots
WLR_BACKENDS=wayland WLR_RENDERER=pixman ./build/tahoe-wlroots --assets assets --config tahoe.conf --desktop-dir assets/desktop --themes .
```

If your WSLg/Mesa stack exposes what wlroots needs for Vulkan, try:

```sh
cd ~/tahoe-wlroots
WLR_BACKENDS=wayland WLR_RENDERER=vulkan ./build/tahoe-wlroots --assets assets --config tahoe.conf --desktop-dir assets/desktop --themes .
```

For automated/headless validation:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once
```

## Apple Asset Overrides

Optional local files are loaded from `assets/` first, then `assets/apple/`.
`assets/apple/` is ignored by Git.

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

Bundled GTK themes live under `themes/TahoeGTK/` and
`themes/TahoeGTK-dark/`. The compositor exports GTK theme variables for
launched GTK clients and requests client-side decorations.

To populate the GTK icon pack from local assets:

```sh
./tools/populate_icon_theme.sh assets themes/TahoeIcons
```

The generated `themes/TahoeIcons/apps`, `places`, and `status` directories are
ignored so private icon PNGs do not enter Git history.

To render a reference-size PNG for comparison:

```sh
./build/tahoe-render-shell /tmp/tahoe-reference.png
```
