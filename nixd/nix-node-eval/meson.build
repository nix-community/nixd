nixd_nix_node_eval_deps = [ libnixdrpc, nixt, libnixdutil, nixd_lsp_server ]

nixd_nix_node_eval_lib = library(
    'nix-node-eval',
    'src/EvalProvider.cpp',
    install: true,
    dependencies: nixd_nix_node_eval_deps
)

nixd_nix_node_eval_lib_dep = declare_dependency(
    link_with: nixd_nix_node_eval_lib,
    dependencies: nixd_nix_node_eval_deps,
)


nixd_nix_node_eval = executable('nix-node-eval',
    'src/Main.cpp',
    install: true,
    install_dir: get_option('libexecdir'),
    dependencies: nixd_nix_node_eval_lib_dep
)
