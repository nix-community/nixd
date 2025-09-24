project('nixd-lspserver', ['cpp'], default_options : ['cpp_std=gnu++20'], version : 'nightly')

cpp = meson.get_compiler('cpp')

add_project_arguments([
  cpp.get_supported_arguments(
    '-Werror=documentation',
    '-Werror=delete-non-abstract-non-virtual-dtor',
    '-Werror=pragma-once-outside-header',
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers'
  ),
], language : 'cpp')

if get_option('buildtype').startswith('release')
  add_project_arguments([
    cpp.get_supported_arguments(
      '-Wno-maybe-uninitialized',
    ),
  ], language: 'cpp')
endif

llvm = dependency('llvm')

nixd_lsp_server_inc = include_directories('include')

nixd_lsp_server_lib = library(
  'nixd-lspserver',
  [
    'src/Connection.cpp',
    'src/DraftStore.cpp',
    'src/LSPServer.cpp',
    'src/Logger.cpp',
    'src/Protocol.cpp',
    'src/SourceCode.cpp',
    'src/URI.cpp',
  ],
  include_directories : nixd_lsp_server_inc,
  dependencies : [llvm],
  install : true,
)

nixd_lsp_server = declare_dependency(
  link_with : nixd_lsp_server_lib,
  include_directories : nixd_lsp_server_inc,
  dependencies : [llvm],
)

meson.override_dependency('nixd-lspserver', nixd_lsp_server)

install_subdir('include', install_dir : 'include')
