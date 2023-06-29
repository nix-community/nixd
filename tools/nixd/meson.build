nixd = executable('nixd', ['nixd.cpp']
, dependencies: [ backtrace
                , boost
                , llvm
                , nixd_lsp_server
                , nixd_server
                ] + nix_all
, install: true
)
