libnixdASTDeps = [ nixd_lsp_server
                 , nix_all
                 , nixdExpr
                 , nixdSyntax
                 , nixdNix
                 , llvm
                 ]

libnixdAST = library('nixdAST'
, 'AttrLocator.cpp'
, 'EvalAST.cpp'
, 'ParseAST.cpp'
, include_directories: nixd_inc
, dependencies: libnixdASTDeps
, install: true
)

nixdAST = declare_dependency( include_directories: nixd_inc
                            , link_with: libnixdAST
                            , dependencies: libnixdASTDeps
                            )
