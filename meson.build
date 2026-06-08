project(
  'tahoe-wlroots',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    'werror=false',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments(
  '-D_GNU_SOURCE',
  '-DWLR_USE_UNSTABLE',
  language: 'c',
)

inc = include_directories('include')

cairo_dep = dependency('cairo')
drm_dep = dependency('libdrm')
pixman_dep = dependency('pixman-1')
wayland_server_dep = dependency('wayland-server')
wlroots_dep = dependency('wlroots')
xkbcommon_dep = dependency('xkbcommon')
vulkan_dep = dependency('vulkan', required: false)
if vulkan_dep.found()
  add_project_arguments('-DTAHOE_HAVE_VULKAN=1', language: 'c')
endif
gtk4_dep = dependency('gtk4', required: false)
math_dep = cc.find_library('m', required: true)
wayland_scanner = find_program('wayland-scanner')
wayland_protocols_dir = dependency('wayland-protocols').get_variable(
  pkgconfig: 'pkgdatadir',
)

xdg_shell_protocol = custom_target(
  'xdg-shell-protocol.h',
  input: wayland_protocols_dir / 'stable/xdg-shell/xdg-shell.xml',
  output: 'xdg-shell-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)

tahoe_core_sources = files(
  'src/assets.c',
  'src/config.c',
  'src/desktop_entry.c',
  'src/shell.c',
)

tahoe_core = static_library(
  'tahoe-core',
  tahoe_core_sources,
  include_directories: inc,
  dependencies: [cairo_dep, drm_dep, math_dep],
)

tahoe_core_dep = declare_dependency(
  include_directories: inc,
  link_with: tahoe_core,
  dependencies: [cairo_dep, drm_dep, math_dep],
)

executable(
  'tahoe-wlroots',
  files(
    'src/buffer.c',
    'src/compositor.c',
    'src/main.c',
  ),
  xdg_shell_protocol,
  include_directories: inc,
  dependencies: [
    tahoe_core_dep,
    cairo_dep,
    drm_dep,
    pixman_dep,
    wayland_server_dep,
    wlroots_dep,
    xkbcommon_dep,
    vulkan_dep,
  ],
  install: false,
)

executable(
  'tahoe-render-shell',
  files('tools/render_shell.c'),
  include_directories: inc,
  dependencies: [tahoe_core_dep, cairo_dep],
  install: false,
)

if gtk4_dep.found()
  executable(
    'tahoe-settings',
    files('src/settings_app.c'),
    include_directories: inc,
    dependencies: [tahoe_core_dep, gtk4_dep],
    install: false,
  )
else
  message('gtk4 development files not found; skipping tahoe-settings target')
endif

test_shell_layout = executable(
  'test-shell-layout',
  files('tests/test_shell_layout.c'),
  include_directories: inc,
  dependencies: [tahoe_core_dep],
)

test('shell-layout', test_shell_layout)

test_shell_visual = executable(
  'test-shell-visual',
  files('tests/test_shell_visual.c'),
  include_directories: inc,
  dependencies: [tahoe_core_dep],
)

test('shell-visual', test_shell_visual)

test_config = executable(
  'test-config',
  files('tests/test_config.c'),
  include_directories: inc,
  dependencies: [tahoe_core_dep],
)

test_desktop_entry = executable(
  'test-desktop-entry',
  files('tests/test_desktop_entry.c'),
  include_directories: inc,
  dependencies: [tahoe_core_dep],
)

test('config', test_config)
test('desktop-entry', test_desktop_entry)
