project('nixd-common', ['cpp'], default_options : ['cpp_std=gnu++20'], version : 'nightly')

config_h = configuration_data()

git = find_program('git', required : false)
if git.found()
  res = run_command([git, 'describe', '--tags', '--always'], capture : true, check : false)
  describe = res.stdout().strip()
  if describe != ''
    config_h.set_quoted('NIXD_VCS_TAG', describe)
  endif
endif

config_h.set_quoted('NIXD_VERSION', meson.project_version())
config_h.set_quoted('NIXD_LIBEXEC', get_option('prefix') / get_option('libexecdir'))

configure_file(
  output : 'nixd-config.h',
  configuration : config_h,
)

cpp = meson.get_compiler('cpp')

add_project_arguments([
  '-I' + meson.project_build_root(),
  cpp.get_supported_arguments(
    '-Werror=documentation',
    '-Werror=delete-non-abstract-non-virtual-dtor',
    '-Werror=pragma-once-outside-header',
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers'
  ),
], language : 'cpp')

if get_option('buildtype').startswith('debug')
  add_project_arguments([
    cpp.get_supported_arguments('-Werror=return-type'),
  ], language : 'cpp')
endif

if get_option('buildtype').startswith('release')
  add_project_arguments([
    cpp.get_supported_arguments(
      '-Wno-return-type',
      '-Wno-maybe-uninitialized',
      '-Wno-unused-variable'
    ),
  ], language : 'cpp')
endif

llvm = dependency('llvm')
nixd_lsp_server = dependency('nixd-lspserver', fallback : ['nixd-lspserver', 'nixd_lsp_server'])
nixt = dependency('nixt', fallback : ['libnixt', 'nixt'])

common_inc = include_directories('include')

common_deps = [nixd_lsp_server, llvm, nixt]

nixd_common_lib = library(
  'nixd-common',
  'lib/Eval/AttrSetClient.cpp',
  'lib/Eval/AttrSetProvider.cpp',
  'lib/Eval/Spawn.cpp',
  'lib/Protocol/AttrSet.cpp',
  'lib/CommandLine/Options.cpp',
  'lib/Support/AutoCloseFD.cpp',
  'lib/Support/ForkPiped.cpp',
  'lib/Support/StreamProc.cpp',
  include_directories : common_inc,
  dependencies : common_deps,
  install : true,
)

subdir('tools')

nixd_common = declare_dependency(
  include_directories : common_inc,
  link_with : nixd_common_lib,
  dependencies : common_deps,
)

meson.override_dependency('nixd-common', nixd_common)

install_subdir('include', install_dir : 'include')
