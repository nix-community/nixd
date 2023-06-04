nixd_server_inc = include_directories('include')
nixd_server_lib = library('nixd-lsp'
, [ 'src/AST.cpp'
  , 'src/CallbackExpr.cpp'
  , 'src/Diagnostic.cpp'
  , 'src/EvalDraftStore.cpp'
  , 'src/Expr.cpp'
  , 'src/JSONSerialization.cpp'
  , 'src/ServerController.cpp'
  , 'src/ServerWorker.cpp'
  ]
, include_directories: nixd_server_inc
, dependencies: [ llvm
                , nixd_lsp_server
                ] + nix_all
, install: true
)
nixd_server= declare_dependency( link_with: nixd_server_lib
                            , include_directories: nixd_server_inc
                            )

test_server = executable('test-server'
, [ 'test/ast.cpp'
  , 'test/diagnostic.cpp'
  , 'test/evalDraftStore.cpp'
  , 'test/expr.cpp'
  ]
, dependencies: [ nixd_server
                , llvm
                , gtest_main
                , nixd_lsp_server
                ] + nix_all
, build_by_default: false
)

test('server', test_server)
