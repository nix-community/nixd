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
tree_sitter = dependency('tree-sitter')

cpp = meson.get_compiler('cpp')
backtrace = cpp.find_library('backtrace')

lit = find_program('lit', required: false)

subdir('lspserver')
subdir('tree-sitter-grammars')
subdir('cpp-tree-sitter')
subdir('nixd')
