project(
  'orange',
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
  '-Wno-gnu-zero-variadic-macro-arguments',
  language: 'c',
)

inc = include_directories('include')

cairo_dep = dependency('cairo')
librsvg_dep = dependency('librsvg-2.0')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')
gio_dep = dependency('gio-2.0')
drm_dep = dependency('libdrm')
pixman_dep = dependency('pixman-1')
wayland_server_dep = dependency('wayland-server')
wlroots_dep = dependency('wlroots-0.20')
xkbcommon_dep = dependency('xkbcommon')
systemd_dep = dependency('libsystemd', required: false)
if systemd_dep.found()
  add_project_arguments('-DORANGE_HAVE_SYSTEMD=1', language: 'c')
  if cc.has_function(
    'sd_session_get_start_time',
    dependencies: systemd_dep,
    prefix: '#include <systemd/sd-login.h>',
  )
    add_project_arguments('-DORANGE_HAVE_SD_SESSION_GET_START_TIME=1', language: 'c')
  endif
endif
vulkan_dep = dependency('vulkan', required: false)
if vulkan_dep.found()
  add_project_arguments('-DORANGE_HAVE_VULKAN=1', language: 'c')
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

orange_core_sources = files(
  'src/assets.c',
  'src/config.c',
  'src/desktop_entry.c',
  'src/desktop.c',
  'src/dock.c',
  'src/glass.c',
  'src/launcher.c',
  'src/menubar.c',
  'src/notifications.c',
  'src/session_services.c',
  'src/shell.c',
  'src/widget_data.c',
  'src/widgets.c',
)

orange_core = static_library(
  'orange-core',
  orange_core_sources,
  include_directories: inc,
  dependencies: [cairo_dep, drm_dep, math_dep, librsvg_dep, gdk_pixbuf_dep, gio_dep, systemd_dep],
)

orange_core_dep = declare_dependency(
  include_directories: inc,
  link_with: orange_core,
  dependencies: [cairo_dep, drm_dep, math_dep, librsvg_dep, gdk_pixbuf_dep, gio_dep, systemd_dep],
)

orange_exe = executable(
  'orange',
  files(
    'src/buffer.c',
    'src/compositor.c',
    'src/main.c',
  ),
  xdg_shell_protocol,
  include_directories: inc,
  dependencies: [
    orange_core_dep,
    cairo_dep,
    drm_dep,
    pixman_dep,
    gio_dep,
    wayland_server_dep,
    wlroots_dep,
    xkbcommon_dep,
    vulkan_dep,
  ],
  install: false,
)

headless_startup_env = environment()
headless_startup_env.set('WLR_BACKENDS', 'headless')
headless_startup_env.set('WLR_RENDERER', 'pixman')
test(
  'headless-custom-startup',
  orange_exe,
  args: [
    '--headless',
    '--once',
    '--width', '1440',
    '--height', '900',
    '--config', meson.current_build_dir() / 'headless-custom-startup.conf',
  ],
  env: headless_startup_env,
  timeout: 15,
)

executable(
  'orange-render-shell',
  files('tools/render_shell.c'),
  include_directories: inc,
  dependencies: [orange_core_dep, cairo_dep],
  install: false,
)

if gtk4_dep.found()
  # orange-settings and orange-about will be rewritten
else
  message('gtk4 development files not found')
endif

test_shell_layout = executable(
  'test-shell-layout',
  files('tests/test_shell_layout.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test('shell-layout', test_shell_layout)

test_shell_visual = executable(
  'test-shell-visual',
  files('tests/test_shell_visual.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test('shell-visual', test_shell_visual)

test_config = executable(
  'test-config',
  files('tests/test_config.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_assets = executable(
  'test-assets',
  files('tests/test_assets.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_desktop_entry = executable(
  'test-desktop-entry',
  files('tests/test_desktop_entry.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_desktop = executable(
  'test-desktop',
  files('tests/test_desktop.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_notifications = executable(
  'test-notifications',
  files('tests/test_notifications.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_session_services = executable(
  'test-session-services',
  files('tests/test_session_services.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test_widget_data = executable(
  'test-widget-data',
  files('tests/test_widget_data.c'),
  include_directories: inc,
  dependencies: [orange_core_dep],
)

test('config', test_config)
test('assets', test_assets)
test('desktop', test_desktop)
test('desktop-entry', test_desktop_entry)
test('notifications', test_notifications)
test('session-services', test_session_services)
test('widget-data', test_widget_data)

bash_prog = find_program('bash', required: false)
if bash_prog.found()
  test(
    'vm-script-syntax',
    bash_prog,
    args: [
      '-n',
      files(
        'scripts/build-vm.sh',
        'scripts/build-void-vm.sh',
        'scripts/orange-setup.sh',
        'scripts/run-vm.sh',
        'scripts/run-void-vm.sh',
      ),
    ],
  )
endif
