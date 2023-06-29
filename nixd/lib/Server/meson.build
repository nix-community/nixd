libnixdServerDeps = [ nixd_lsp_server
                    , nix_all
                    , nixdAST
                    , nixdNix
                    , nixdSupport
                    ]
libnixdServer = library('nixdServer'
, 'Controller.cpp'
, 'Eval.cpp'
, 'EvalDraftStore.cpp'
, 'Nix.cpp'
, 'Option.cpp'
, 'Static.cpp'
, include_directories: nixd_inc
, dependencies: libnixdServerDeps
, install: true
)


nixdServer = declare_dependency( include_directories: nixd_inc
                               , dependencies: libnixdServerDeps
                               , link_with: libnixdServer
                               )
