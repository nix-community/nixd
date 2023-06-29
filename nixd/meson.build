nixfetchers = cpp.find_library('nixfetchers')

nix_all = [ dependency('nix-expr')
          , dependency('nix-cmd')
          , dependency('nix-store')
          , dependency('nix-main')
          , nixfetchers
          ]

nixd_inc = include_directories('include')

subdir('lib')

nixdDeps = [ nixdAST
           , nixdNix
           , nixdParser
           , nixdServer
           , nixdSupport
           ]

subdir('tools')

test_server = executable('test-server'
, [ 'test/ast.cpp'
  , 'test/evalDraftStore.cpp'
  , 'test/expr.cpp'
  , 'test/parser.cpp'
  ]
, lexer
, parser
, dependencies: [ nixdDeps
                , llvm
                , gtest_main
                , nixd_lsp_server
                ] + nix_all
, build_by_default: false
)

test('server', test_server)
