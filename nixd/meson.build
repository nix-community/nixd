project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: '2.3.0'
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

gtest = dependency('gtest')
gtest_main = dependency('gtest_main')
nixf = dependency('nixf')
nixt = dependency('nixt')
llvm = dependency('llvm')

pkgconfig = import('pkgconfig')


subdir('lspserver')
subdir('lib')
subdir('tools')
