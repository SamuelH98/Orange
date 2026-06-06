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

### Shell Renderer

Produces a complete desktop shell image into an ARGB software buffer using
Cairo. The buffer is uploaded to a wlroots texture and composited to the output.

Responsibilities:

- draw wallpaper,
- draw menu bar, widgets, desktop items, dock, and icons,
- load optional local asset overrides,
- expose deterministic layout math for tests.

### Asset Loader

Loads optional PNG overrides from ignored paths. Missing files are non-fatal and
fall back to procedural art.

Responsibilities:

- keep Apple asset use local-only,
- validate loaded image dimensions,
- avoid failing startup when optional files are absent.

### View Management

Minimal xdg-shell support for clients. New toplevels are centered and rendered
above the shell layer.

Responsibilities:

- track mapped/unmapped xdg toplevels,
- send basic configure events,
- render client textures.

## Data Flow

1. wlroots emits an output frame event.
2. Runtime asks shell renderer for a buffer matching the output size.
3. Cairo draws shell UI into ARGB memory.
4. wlroots imports the buffer with `wlr_texture_from_pixels`.
5. Runtime renders the shell texture, then mapped client surfaces.
6. Runtime commits the output.

## Failure Modes

- Renderer creation failure: exit with an error.
- Output render initialization failure: disable that output and continue.
- Asset load failure: use fallback drawing.
- Headless `--once` no output: create an explicit headless output.
- Vulkan unavailable: wlroots renderer creation may fail; the log identifies
  the selected renderer when it succeeds.
