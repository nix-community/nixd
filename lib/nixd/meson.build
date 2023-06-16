#------------------------------------------------------------------------------#
# Parser & Lexer stuff
flex = find_program('flex')
bison = find_program('bison')

# Args are specified in:
# https://github.com/NixOS/nix/blob/a1cf16563f681b5cb3026f2bbca629996ed36d86/src/libexpr/local.mk#L31

lexer = custom_target('lexer'
, input : [ 'src/nix/Lexer.l' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ flex, '--outfile=@OUTPUT0@', '--header-file=@OUTPUT1@', '@INPUT@' ]
)

parser = custom_target('parser'
, input : [ 'src/nix/Parser.y' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@' ]
)

#------------------------------------------------------------------------------#


nixd_server_inc = include_directories('include')
nixd_server_lib = library('nixd-lsp'
, [ 'src/AST.cpp'
  , 'src/CallbackExpr.cpp'
  , 'src/Diagnostic.cpp'
  , 'src/EvalDraftStore.cpp'
  , 'src/Expr.cpp'
  , 'src/JSONSerialization.cpp'
  , 'src/ServerController.cpp'
  , 'src/ServerEval.cpp'
  , 'src/ServerNixWorker.cpp'
  , 'src/ServerOption.cpp'
  , 'src/nix/EvalState.cpp'
  , 'src/nix/Option.cpp'
  , 'src/nix/Value.cpp'
  ]
, lexer
, parser
, include_directories: nixd_server_inc
, dependencies: [ llvm
                , nixd_lsp_server
                ] + nix_all
, install: true
)

nixd_server = declare_dependency( link_with: nixd_server_lib
                                , include_directories: nixd_server_inc
                                , sources: [ lexer, parser ]
                                )

test_server = executable('test-server'
, [ 'test/ast.cpp'
  , 'test/diagnostic.cpp'
  , 'test/evalDraftStore.cpp'
  , 'test/expr.cpp'
  , 'test/parser.cpp'
  ]
, lexer
, parser
, dependencies: [ nixd_server
                , llvm
                , gtest_main
                , nixd_lsp_server
                ] + nix_all
, build_by_default: false
)

test('server', test_server)
