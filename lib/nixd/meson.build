nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')
nix_store = dependency('nix-store')
nix_main = dependency('nix-main')

nixd_server_inc = include_directories('include')
nixd_server_lib = library('nixd-lsp'
, [ 'src/Server.cpp'
  ]
, include_directories: nixd_server_inc
, dependencies: [ llvm
                , nix_expr
                , nix_cmd
                , nix_store
                , nix_main
                , nixd_lsp_server
                ]
)
nixd_server= declare_dependency( link_with: nixd_server_lib
                            , include_directories: nixd_server_inc
                            )
