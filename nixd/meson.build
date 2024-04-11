libnixd_include = include_directories('include')

libnixd_deps = [ nixd_lsp_server, nixf, llvm, nixt ]

libnixd_lib = library(
    'nixd',
    'lib/CommandLine/Options.cpp',
    'lib/Controller/AST.cpp',
    'lib/Controller/CodeAction.cpp',
    'lib/Controller/Completion.cpp',
    'lib/Controller/Convert.cpp',
    'lib/Controller/Definition.cpp',
    'lib/Controller/Diagnostics.cpp',
    'lib/Controller/DocumentHighlight.cpp',
    'lib/Controller/DocumentSymbol.cpp',
    'lib/Controller/EvalClient.cpp',
    'lib/Controller/FindReferences.cpp',
    'lib/Controller/Hover.cpp',
    'lib/Controller/InlayHints.cpp',
    'lib/Controller/LifeTime.cpp',
    'lib/Controller/NixTU.cpp',
    'lib/Controller/Rename.cpp',
    'lib/Controller/SemanticTokens.cpp',
    'lib/Controller/Support.cpp',
    'lib/Controller/TextDocumentSync.cpp',
    'lib/Eval/AttrSetClient.cpp',
    'lib/Eval/AttrSetProvider.cpp',
    'lib/Eval/EvalProvider.cpp',
    'lib/Protocol/AttrSet.cpp',
    'lib/Protocol/Protocol.cpp',
    'lib/Support/AutoCloseFD.cpp',
    'lib/Support/AutoRemoveShm.cpp',
    'lib/Support/ForkPiped.cpp',
    'lib/Support/StreamProc.cpp',
    dependencies: libnixd_deps,
    include_directories: libnixd_include,
    install: true
)

libnixd = declare_dependency(
    include_directories: libnixd_include,
    link_with: libnixd_lib,
    dependencies: libnixd_deps
)

nixd = executable(
    'nixd',
    'tools/nixd.cpp',
    install: true,
    dependencies: libnixd
)


nixd_attrset_eval = executable(
    'nixd-attrset-eval',
    'tools/nixd-attrset-eval.cpp',
    install: true,
    dependencies: libnixd,
    install_dir: get_option('libexecdir'),
)

regression_controller_env = environment()

regression_controller_env.append('PATH', meson.current_build_dir())
regression_controller_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
regression_controller_env.set('NIXD_ATTRSET_EVAL', nixd_attrset_eval.path())


if lit.found()
    test(
        'regression/nixd',
        lit,
        env: regression_controller_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/tools/nixd/test'
        ],
        depends: [ nixd, nixd_attrset_eval ] )
endif

regression_worker_env = environment()

regression_worker_env.append('PATH', meson.current_build_dir())
regression_worker_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
regression_worker_env.set('ASAN_OPTIONS', 'detect_leaks=0')

nix_node_eval = executable(
    'nix-node-eval',
    'tools/nix-node-eval.cpp',
    install: true,
    dependencies: libnixd,
    install_dir: get_option('libexecdir'),
)





if lit.found()
    test(
        'regression/nixd-attrset-eval',
        lit,
        env: regression_worker_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/tools/nixd-attrset-eval/test'
        ],
        depends: [ nixd_attrset_eval ] )
endif
