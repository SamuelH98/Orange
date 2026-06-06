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

For automated/headless validation:

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman build/tahoe-wlroots --headless --once
```

## Apple Asset Overrides

Optional local files:

- `assets/apple/wallpaper.png`
- `assets/apple/apple-menu.png`
- `assets/apple/icons/*.png`

These files are intentionally not tracked.
