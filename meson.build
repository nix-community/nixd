project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: '2.3.2'
       )

config_h = configuration_data()

git = find_program('git', required : false)
if git.found()
  res = run_command(['git', 'describe', '--tags', '--always'], capture : true, check : false)
  describe = res.stdout().strip()
  config_h.set_quoted('NIXD_VCS_TAG', describe)
endif

config_h.set_quoted('NIXD_VERSION', meson.project_version())
config_h.set_quoted('NIXD_LIBEXEC', get_option('prefix') / get_option('libexecdir'))

configure_file(
  output: 'nixd-config.h',
  configuration: config_h,
)

cpp = meson.get_compiler('cpp')

add_project_arguments([
  '-I' + meson.project_build_root(),
  cpp.get_supported_arguments(
     '-Werror=documentation',
     '-Werror=delete-non-abstract-non-virtual-dtor',
     '-Werror=pragma-once-outside-header',
     '-Wno-unused-parameter',
     '-Wno-missing-field-initializers',
  ),
], language: 'cpp')

if get_option('buildtype').startswith('debug')
  add_project_arguments([
    cpp.get_supported_arguments(
      '-Werror=return-type',
    ),
  ], language: 'cpp')
endif

if get_option('buildtype').startswith('release')
  add_project_arguments([
    cpp.get_supported_arguments(
      '-Wno-return-type',
      '-Wno-maybe-uninitialized',
      '-Wno-unused-variable',
    ),
  ], language: 'cpp')
endif


gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

llvm = dependency('llvm')
boost = dependency('boost')
nlohmann_json = dependency('nlohmann_json')

lit = find_program('lit', required: false)

pkgconfig = import('pkgconfig')


subdir('libnixf/src')
subdir('libnixf/tools')
subdir('libnixf/test')


nix_main = dependency('nix-main')
nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')


subdir('libnixt/lib')
subdir('libnixt/test')

subdir('nixd/lspserver')
subdir('nixd/lib')
subdir('nixd/tools')



