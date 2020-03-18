project(
  'rudp',
  'c',
  version: '0.3',
  license: 'MIT',
  default_options: [
    'warning_level=2',
    'werror=true',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
  '-Wstrict-prototypes',
  '-Wmissing-prototypes',
  '-Wno-unused-parameter',
]), language: 'c')

ela_dep = dependency('ela', fallback: ['libela', 'ela_dep'])

rudp_files = []
rudp_deps = [
  ela_dep,
]

subdir('include')
subdir('src')

rudp_inc = include_directories('include')

lib_rudp = library(
  meson.project_name(), rudp_files,
  version: meson.project_version(),
  dependencies: rudp_deps,
  include_directories: [rudp_inc],
  install: true,
)

rudp_dep = declare_dependency(
  link_with: lib_rudp,
  dependencies: rudp_deps,
  include_directories : [rudp_inc],
)

if get_option('tests')
  subdir('test')
endif

pkgconfig = import('pkgconfig')
pkgconfig.generate(lib_rudp,
  version: meson.project_version(),
  filebase: meson.project_name(),
  name: meson.project_name(),
  description: 'Reliable UDP library',
)
