libnixdServerDeps = [ nixd_lsp_server
                    , nix_all
                    , nixdAST
                    , nixdNix
                    , nixdSupport
                    ]
libnixdServer = library('nixdServer'
, 'ASTManager.cpp'
, 'ConfigSerialization.cpp'
, 'Controller.cpp'
, 'EvalDraftStore.cpp'
, 'EvalWorker.cpp'
, 'IPCSerialization.cpp'
, 'OptionWorker.cpp'
, include_directories: nixd_inc
, dependencies: libnixdServerDeps
, install: true
)


nixdServer = declare_dependency( include_directories: nixd_inc
                               , dependencies: libnixdServerDeps
                               , link_with: libnixdServer
                               )
