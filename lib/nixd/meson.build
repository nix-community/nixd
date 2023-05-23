nixd_server_inc = include_directories('include')
nixd_server_lib = library('nixd-lsp'
, [ 'src/Server.cpp'
  , 'src/Diagnostic.cpp'
  , 'src/CallbackExpr.cpp'
  , 'src/AST.cpp'
  , 'src/EvalDraftStore.cpp'
  ]
, include_directories: nixd_server_inc
, dependencies: [ llvm
                , nixd_lsp_server
                ] + nix_all
)
nixd_server= declare_dependency( link_with: nixd_server_lib
                            , include_directories: nixd_server_inc
                            )

test_server = executable('test-server'
, [ 'test/server.cpp'
  , 'test/diagnostic.cpp'
  , 'test/expr.cpp'
  , 'test/evalDraftStore.cpp'
  ]
, dependencies: [ nixd_server
                , llvm
                , gtest_main
                , nixd_lsp_server
                ] + nix_all
, build_by_default: false
)

test('server', test_server)
