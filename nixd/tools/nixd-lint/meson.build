nixd_lint = executable('nixd-lint'
, 'nixd-lint.cpp'
, dependencies: [ boost
                , nixdSyntax
                , nixdSema
                ] + nix_all
, install: true
)


regression_env = environment()

regression_env.append('PATH', meson.current_build_dir())
regression_env.set('MESON_BUILD_ROOT', meson.current_build_dir())

if lit.found()
    test('regression/nixd-lint'
        , lit
        , timeout: 120
        , env: regression_env
        , args: [ '-vv', meson.current_source_dir() + '/test' ]
        , depends: [ nixd_lint ]
        )
endif
