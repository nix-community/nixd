libnixdSemaDeps = [ nixd_lsp_server
                  , nix_all
                  , nixdAST
                  , llvm
                  ]

libnixdSema = library('nixdSema'
, 'CompletionBuilder.cpp'
, include_directories: nixd_inc
, dependencies: libnixdSemaDeps
, install: true
)

nixdSema = declare_dependency( include_directories: nixd_inc
                             , link_with: libnixdSema
                             , dependencies: libnixdSemaDeps
                             )
