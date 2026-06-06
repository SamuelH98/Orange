# Architecture

## Components

### Compositor Runtime

Owns Wayland display setup, wlroots backend creation, renderer/allocator setup,
output lifecycle, and process options.

Responsibilities:

- initialize wlroots,
- create core Wayland globals,
- create xdg-shell support,
- initialize new outputs,
- render frames,
- shut down cleanly in `--once` mode.

### Shell Renderer And Interaction Model

Produces a complete desktop shell image into an ARGB software buffer using
Cairo. The buffer is uploaded to a wlroots texture and composited to the output.

Responsibilities:

- draw wallpaper,
- draw menu bar, widgets, desktop items, dock, and icons,
- load optional local asset overrides,
- expose deterministic layout and hit-test math for tests,
- maintain transient UI state such as the Apple-style menu popover.

### Asset Loader

Loads optional PNG overrides from ignored paths. Missing files are non-fatal and
fall back to procedural art.

Responsibilities:

- keep Apple asset use local-only,
- validate loaded image dimensions,
- avoid failing startup when optional files are absent.

### View Management And Input

xdg-shell support for ordinary clients. New toplevels are centered and rendered
above the shell layer with wlroots scene helpers.

Responsibilities:

- track mapped/unmapped xdg toplevels,
- send basic configure events,
- render client textures,
- focus and raise windows,
- move/resize windows from compositor grabs,
- respond to maximize/fullscreen/close requests,
- dispatch pointer and keyboard input to the focused client.

### Launch Services Shim

Small local process launcher for Dock, desktop, and shortcut commands.

Responsibilities:

- launch commands through `/bin/sh -c` in a child process,
- prefer user-configured environment variables for terminal/app picker,
- avoid blocking the compositor event loop.

## Data Flow

1. wlroots emits an output frame event.
2. Runtime updates each output's Cairo-backed shell buffer when state changes.
3. The shell buffer is exposed as a wlroots scene buffer.
4. wlroots scene nodes render shell buffers and mapped client surfaces.
5. Runtime commits the scene output and sends frame callbacks.
6. Pointer hit testing first finds client surfaces; if none, shell hit testing
   handles Dock, desktop, and menu clicks.

## Failure Modes

- Renderer creation failure: exit with an error.
- Output render initialization failure: disable that output and continue.
- Asset load failure: use fallback drawing.
- Headless `--once` no output: create an explicit headless output.
- Vulkan unavailable: wlroots renderer creation may fail; the log identifies
  the selected renderer when it succeeds.
- Launcher command missing: child exits; compositor remains running.
- Input device absent in headless mode: shell still renders and exits in
  `--once` validation.
