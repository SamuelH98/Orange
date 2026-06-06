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
when the Vulkan renderer headers are available.

## Visual Reference Notes

Apple describes the Tahoe-era design as using "Liquid Glass": translucent UI
materials that adapt to surrounding content and include real-time highlights.
For this prototype, that is approximated with:

- blurred/translucent glass panels,
- rounded widgets and dock,
- bright specular edge highlights,
- softly tinted icon backgrounds,
- wallpaper-colored transparency.

## Risks

- wlroots APIs are explicitly unstable; code targets 0.17.1 as installed.
- Vulkan renderer availability depends on GPU/driver/runtime environment.
- Running a real compositor outside headless mode may require a nested Wayland
  or X11 session, or a TTY with seat permissions.
- Exact Apple assets cannot be redistributed in this repository.

## References

- Apple newsroom: macOS Tahoe design and Liquid Glass announcement.
- Apple macOS Tahoe product page for visual layout reference.
- wlroots 0.17 local headers and online renderer documentation.
- Khronos Vulkan documentation for Vulkan instance/rendering concepts.
