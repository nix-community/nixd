project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: 'nightly'
       )

config_h = configuration_data()

config_h.set_quoted('NIXD_VERSION', meson.project_version())
config_h.set_quoted('NIXD_LIBEXEC', get_option('prefix') / get_option('libexecdir'))

configure_file(
  output: 'nixd-config.h',
  configuration: config_h,
)

cpp = meson.get_compiler('cpp')

add_project_arguments([
  cpp.get_supported_arguments(
     '-I' + meson.project_build_root(),
     '-Werror=documentation',
     '-Werror=delete-non-abstract-non-virtual-dtor',
     '-Werror=pragma-once-outside-header',
     '-Wno-unused-parameter',
     '-Wno-missing-field-initializers',
  ),
], language: 'cpp')

if get_option('buildtype').startswith('release')
  add_project_arguments([
    cpp.get_supported_arguments(
      '-Wno-maybe-uninitialized',
    ),
  ], language: 'cpp')
endif

gtest = dependency('gtest')
gtest_main = dependency('gtest_main')
llvm = dependency('llvm')

nixf = dependency('nixf', fallback : ['libnixf', 'nixf'])
nixt = dependency('nixt', fallback : ['libnixt', 'nixt'])
nixd_common = dependency('nixd-common', fallback : ['common', 'nixd_common'])

pkgconfig = import('pkgconfig')


subdir('lib')
subdir('tools')
