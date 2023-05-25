nixd_lsp_server_inc = include_directories('include')
nixd_lsp_server_lib = library('nixd-lspserver'
, [ 'src/Connection.cpp'
  , 'src/DraftStore.cpp'
  , 'src/LSPServer.cpp'
  , 'src/Logger.cpp'
  , 'src/Protocol.cpp'
  , 'src/SourceCode.cpp'
  , 'src/URI.cpp'
  ]
, include_directories: nixd_lsp_server_inc
, dependencies: [ llvm ]
)
nixd_lsp_server = declare_dependency( link_with: nixd_lsp_server_lib
                                    , include_directories: nixd_lsp_server_inc
                                    )
