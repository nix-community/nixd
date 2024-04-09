libnixd_include = include_directories('include')

libnixd_deps = [ nixd_lsp_server, nixf, llvm, nixt ]

libnixd_lib = library(
    'nixd',
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
    'lib/Controller/LifeTime.cpp',
    'lib/Controller/NixTU.cpp',
    'lib/Controller/Rename.cpp',
    'lib/Controller/SemanticTokens.cpp',
    'lib/Controller/Support.cpp',
    'lib/Controller/TextDocumentSync.cpp',
    'lib/Eval/AttrSetProvider.cpp',
    'lib/Eval/EvalProvider.cpp',
    'lib/Protocol/AttrSet.cpp',
    'lib/Protocol/Protocol.cpp',
    'lib/Support/AutoCloseFD.cpp',
    'lib/Support/AutoRemoveShm.cpp',
    'lib/Support/ForkPiped.cpp',
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

regression_env = environment()

regression_env.append('PATH', meson.current_build_dir())
regression_env.set('MESON_BUILD_ROOT', meson.current_build_dir())

if lit.found()
    test(
        'regression/nixd',
        lit,
        env: regression_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/tools/nixd/test'
        ],
        depends: [ nixd ] )
endif


nix_node_eval = executable(
    'nix-node-eval',
    'tools/nix-node-eval.cpp',
    install: true,
    dependencies: libnixd,
    install_dir: get_option('libexecdir'),
)

nixd_attrset_eval = executable(
    'nixd-attrset-eval',
    'tools/nixd-attrset-eval.cpp',
    install: true,
    dependencies: libnixd,
    install_dir: get_option('libexecdir'),
)

regression_env.set('ASAN_OPTIONS', 'detect_leaks=0')


if lit.found()
    test(
        'regression/nixd-attrset-eval',
        lit,
        env: regression_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/tools/nixd-attrset-eval/test'
        ],
        depends: [ nixd_attrset_eval ] )
endif
