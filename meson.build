project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: 'nightly'
       )

config_h = configuration_data()

git = find_program('git', required : false)
if git.found()
  res = run_command(['git', 'describe', '--tags', '--always'], capture : true, check : false)
  describe = res.stdout().strip()
  config_h.set_quoted('NIXD_VCS_TAG', describe)
endif

config_h.set_quoted('NIXD_VERSION', meson.project_version())

configure_file(
  output: 'nixd-config.h',
  configuration: config_h,
)

add_project_arguments([
  '-I' + meson.project_build_root(),
], language: 'cpp')



gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

llvm = dependency('llvm')
boost = dependency('boost')

cpp = meson.get_compiler('cpp')
backtrace = cpp.find_library('backtrace')
nixfetchers = cpp.find_library('nixfetchers')

nix_all = [ dependency('nix-expr')
          , dependency('nix-cmd')
          , dependency('nix-store')
          , dependency('nix-main')
          , nixfetchers
          ]

lit = find_program('lit')

subdir('lspserver')
subdir('nixd')
