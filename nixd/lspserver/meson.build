nixd_lsp_server_deps = [ llvm ]

nixd_lsp_server_inc = include_directories('include')

nixd_lsp_server_sources = [ 'src/Connection.cpp'
  , 'src/DraftStore.cpp'
  , 'src/LSPServer.cpp'
  , 'src/Logger.cpp'
  , 'src/Protocol.cpp'
  , 'src/SourceCode.cpp'
  , 'src/URI.cpp'
  ]

# A shared library linked to static LLVMSupport would own a separate copy of
# LLVM's globals. Keep the internal libraries static so nixd has one CLI registry.
if llvm_static
  nixd_lsp_server_lib = static_library('nixd-lspserver'
  , nixd_lsp_server_sources
  , include_directories: nixd_lsp_server_inc
  , dependencies: nixd_lsp_server_deps
  )
else
  nixd_lsp_server_lib = shared_library('nixd-lspserver'
  , nixd_lsp_server_sources
  , include_directories: nixd_lsp_server_inc
  , dependencies: nixd_lsp_server_deps
  , install: true
  )
endif

nixd_lsp_server = declare_dependency( link_with: nixd_lsp_server_lib
                                    , include_directories: nixd_lsp_server_inc
                                    , dependencies: nixd_lsp_server_deps
                                    )
