nixd_next = executable('nixd',
    'src/CodeAction.cpp',
    'src/Convert.cpp',
    'src/Diagnostics.cpp',
    'src/EvalClient.cpp',
    'src/Hover.cpp',
    'src/LifeTime.cpp',
    'src/Main.cpp',
    'src/Support.cpp',
    'src/TextDocumentSync.cpp',
    install: true,
    dependencies: [ nixd_lsp_server, nixf, llvm, libnixdrpc, libnixdutil ]
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
            meson.current_source_dir() + '/test'
        ],
        depends: [ nixd_next ] )
endif
