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
WLR_BACKENDS=wayland WLR_RENDERER=pixman build/tahoe-wlroots
```

If your WSLg/Mesa stack exposes what wlroots needs for Vulkan, try:

```sh
WLR_BACKENDS=wayland WLR_RENDERER=vulkan build/tahoe-wlroots
```

For automated/headless validation:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once
```

## Apple Asset Overrides

Optional local files:

- `assets/apple/wallpaper.png`
- `assets/apple/apple-menu.png`
- Dock icons in `assets/apple/icons/`:
  `finder.png`, `launchpad.png`, `safari.png`, `messages.png`, `mail.png`,
  `maps.png`, `photos.png`, `facetime.png`, `phone.png`, `calendar.png`,
  `contacts.png`, `reminders.png`, `notes.png`, `tv.png`, `music.png`,
  `rocket.png`, `app-store.png`, `calculator.png`, `settings.png`,
  `desktop-preview.png`, `trash.png`

These files are intentionally not tracked.

To render a reference-size PNG for comparison:

```sh
build/tahoe-render-shell /tmp/tahoe-reference.png
```
